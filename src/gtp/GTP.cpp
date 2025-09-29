#include "gtp/GTP.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <optional>
#include <sstream>

namespace {

std::string trim_copy(const std::string& input) {
    auto first = std::find_if_not(input.begin(), input.end(), ::isspace);
    auto last = std::find_if_not(input.rbegin(), input.rend(), ::isspace).base();
    if (first >= last) {
        return {};
    }
    return std::string(first, last);
}

std::string to_lower_copy(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

} // namespace

namespace gtp {

Server::Server(go::Board board,
               std::istream& in,
               std::ostream& out,
               search::SearchConfig search_config,
               std::shared_ptr<search::Evaluator> evaluator)
    : board_(std::move(board)),
      in_(in),
      out_(out),
      search_agent_(std::make_unique<search::SearchAgent>(search_config, std::move(evaluator))),
      search_config_(search_config) {
    register_handlers();
    reset_search();
}

void Server::run() {
    std::string line;
    while (std::getline(in_, line)) {
        auto comment_pos = line.find('#');
        if (comment_pos != std::string::npos) {
            line = line.substr(0, comment_pos);
        }
        line = trim_copy(line);
        if (line.empty()) {
            continue;
        }

        std::istringstream iss(line);
        std::string token;
        if (!(iss >> token)) {
            continue;
        }

        std::string id;
        std::string command = token;
        if (!token.empty() && std::isdigit(token.front())) {
            id = token;
            if (!(iss >> command)) {
                out_ << format_failure(id, "missing_command") << std::flush;
                continue;
            }
        }

        std::string args;
        std::getline(iss, args);
        args = trim_copy(args);

        std::string command_lower = to_lower_copy(command);
        auto it = handlers_.find(command_lower);
        HandlerResult result;
        if (it != handlers_.end()) {
            result = it->second(args);
        } else {
            result = {false, "unknown_command"};
        }

        out_ << (result.first ? format_success(id, result.second) : format_failure(id, result.second));
        out_.flush();

        if (command_lower == "quit") {
            break;
        }
    }
}

Server::HandlerResult Server::handle_protocol_version(const std::string&) {
    return {true, "2"};
}

Server::HandlerResult Server::handle_name(const std::string&) {
    return {true, "Tenuki"};
}

Server::HandlerResult Server::handle_version(const std::string&) {
    return {true, "0.1"};
}

Server::HandlerResult Server::handle_boardsize(const std::string& args) {
    if (args.empty()) {
        return {false, "boardsize requires argument"};
    }
    int size = std::stoi(args);
    if (size <= 0 || size > 25) {
        return {false, "invalid boardsize"};
    }
    go::Rules rules = board_.rules();
    rules.board_size = static_cast<std::size_t>(size);
    board_ = go::Board(rules);
    reset_search();
    return {true, ""};
}

Server::HandlerResult Server::handle_clear_board(const std::string&) {
    board_.clear();
    board_.set_to_play(go::Player::Black);
    reset_search();
    return {true, ""};
}

Server::HandlerResult Server::handle_komi(const std::string& args) {
    if (args.empty()) {
        return {false, "komi requires value"};
    }
    double komi = std::stod(args);
    go::Rules rules = board_.rules();
    rules.komi = komi;
    board_ = go::Board(rules);
    reset_search();
    return {true, ""};
}

Server::HandlerResult Server::handle_play(const std::string& args) {
    std::istringstream iss(args);
    std::string color_token;
    std::string vertex_token;
    if (!(iss >> color_token >> vertex_token)) {
        return {false, "play requires color and vertex"};
    }

    auto color_char = static_cast<char>(std::tolower(color_token.front()));
    go::Player color;
    if (color_char == 'b') {
        color = go::Player::Black;
    } else if (color_char == 'w') {
        color = go::Player::White;
    } else {
        return {false, "invalid color"};
    }

    go::Move move = go::Move::Pass();
    if (to_lower_copy(vertex_token) != "pass") {
        auto parsed = parse_vertex(vertex_token);
        if (!parsed.first) {
            return {false, "invalid vertex"};
        }
        move = parsed.second;
    }

    board_.set_to_play(color);
    if (!board_.play_move(color, move)) {
        return {false, "illegal move"};
    }
    ++move_number_;
    search_agent_->notify_move(move, board_, board_.to_play());
    return {true, ""};
}

Server::HandlerResult Server::handle_genmove(const std::string& args) {
    go::Player color = board_.to_play();
    if (!args.empty()) {
        auto token = to_lower_copy(args);
        if (token.front() == 'b') {
            color = go::Player::Black;
        } else if (token.front() == 'w') {
            color = go::Player::White;
        } else {
            return {false, "invalid color"};
        }
    }

    board_.set_to_play(color);

    go::Move move = search_agent_->select_move(board_, color, move_number_);
    if (!board_.play_move(color, move)) {
        return {false, "genmove failed"};
    }

    ++move_number_;
    search_agent_->notify_move(move, board_, board_.to_play());

    if (move.is_pass()) {
        return {true, "pass"};
    }
    return {true, vertex_to_string(move.vertex)};
}

Server::HandlerResult Server::handle_final_score(const std::string&) {
    auto score = board_.tromp_taylor_score();
    double diff = score.black_points - score.white_points;
    std::ostringstream oss;
    if (std::abs(diff) < 1e-6) {
        oss << "0";
    } else if (diff > 0) {
        oss << "B+" << std::fixed << std::setprecision(1) << diff;
    } else {
        oss << "W+" << std::fixed << std::setprecision(1) << std::abs(diff);
    }
    return {true, oss.str()};
}

Server::HandlerResult Server::handle_showboard(const std::string&) {
    std::ostringstream oss;
    const std::size_t size = board_.board_size();
    oss << "  ";
    for (std::size_t x = 0; x < size; ++x) {
        char letter = static_cast<char>('A' + x);
        if (letter >= 'I') {
            letter += 1;
        }
        oss << letter << ' ';
    }
    oss << '\n';

    for (std::size_t y = 0; y < size; ++y) {
        oss << std::setw(2) << (size - y) << ' ';
        for (std::size_t x = 0; x < size; ++x) {
            int vertex = static_cast<int>(y * size + x);
            auto state = board_.point_state(vertex);
            char symbol = '.';
            if (state == go::PointState::Black) {
                symbol = 'X';
            } else if (state == go::PointState::White) {
                symbol = 'O';
            }
            oss << symbol << ' ';
        }
        oss << (size - y) << '\n';
    }

    oss << "  ";
    for (std::size_t x = 0; x < size; ++x) {
        char letter = static_cast<char>('A' + x);
        if (letter >= 'I') {
            letter += 1;
        }
        oss << letter << ' ';
    }
    return {true, oss.str()};
}

Server::HandlerResult Server::handle_quit(const std::string&) {
    return {true, ""};
}

std::pair<bool, go::Move> Server::parse_vertex(const std::string& vertex) const {
    if (vertex.empty()) {
        return {false, go::Move::Pass()};
    }

    std::string upper = vertex;
    std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });

    char column_char = upper.front();
    if (column_char < 'A' || column_char > 'Z') {
        return {false, go::Move::Pass()};
    }

    int column = column_char - 'A';
    if (column_char >= 'I') {
        column -= 1; // skip the letter I
    }

    std::string row_str = upper.substr(1);
    if (row_str.empty()) {
        return {false, go::Move::Pass()};
    }
    int row = std::stoi(row_str);
    if (row <= 0 || row > static_cast<int>(board_.board_size())) {
        return {false, go::Move::Pass()};
    }

    if (column < 0 || column >= static_cast<int>(board_.board_size())) {
        return {false, go::Move::Pass()};
    }

    int x = column;
    int y = static_cast<int>(board_.board_size()) - row;
    int vertex_index = y * static_cast<int>(board_.board_size()) + x;
    return {true, go::Move(vertex_index)};
}

std::string Server::vertex_to_string(int vertex) const {
    int size = static_cast<int>(board_.board_size());
    int x = vertex % size;
    int y = vertex / size;
    int row = size - y;
    char column = static_cast<char>('A' + x);
    if (column >= 'I') {
        column += 1;
    }
    std::ostringstream oss;
    oss << column << row;
    return oss.str();
}

std::string Server::format_success(const std::string& id, const std::string& payload) const {
    std::ostringstream oss;
    oss << '=';
    if (!id.empty()) {
        oss << id;
    }
    if (!payload.empty()) {
        oss << ' ' << payload;
    }
    oss << "\n\n";
    return oss.str();
}

std::string Server::format_failure(const std::string& id, const std::string& message) const {
    std::ostringstream oss;
    oss << '?';
    if (!id.empty()) {
        oss << id;
    }
    if (!message.empty()) {
        oss << ' ' << message;
    }
    oss << "\n\n";
    return oss.str();
}

void Server::register_handlers() {
    handlers_.clear();
    handlers_["protocol_version"] = [this](const std::string& args) { return handle_protocol_version(args); };
    handlers_["name"] = [this](const std::string& args) { return handle_name(args); };
    handlers_["version"] = [this](const std::string& args) { return handle_version(args); };
    handlers_["boardsize"] = [this](const std::string& args) { return handle_boardsize(args); };
    handlers_["clear_board"] = [this](const std::string& args) { return handle_clear_board(args); };
    handlers_["komi"] = [this](const std::string& args) { return handle_komi(args); };
    handlers_["play"] = [this](const std::string& args) { return handle_play(args); };
    handlers_["genmove"] = [this](const std::string& args) { return handle_genmove(args); };
    handlers_["final_score"] = [this](const std::string& args) { return handle_final_score(args); };
    handlers_["showboard"] = [this](const std::string& args) { return handle_showboard(args); };
    handlers_["quit"] = [this](const std::string& args) { return handle_quit(args); };
}

void Server::reset_search() {
    move_number_ = 0;
    if (search_agent_) {
        search_agent_->reset();
    } else {
        search_agent_ = std::make_unique<search::SearchAgent>(search_config_, nullptr);
    }
}

} // namespace gtp
