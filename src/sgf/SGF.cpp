#include "sgf/SGF.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

std::string read_all(std::istream& input) {
    std::ostringstream oss;
    oss << input.rdbuf();
    return oss.str();
}

std::string extract_property(const std::string& data, const std::string& prop) {
    auto pos = data.find(prop + "[");
    if (pos == std::string::npos) {
        return {};
    }
    pos += prop.size() + 1;
    std::string value;
    while (pos < data.size() && data[pos] != ']') {
        char c = data[pos++];
        if (c == '\\' && pos < data.size()) {
            value.push_back(data[pos++]);
        } else {
            value.push_back(c);
        }
    }
    return value;
}

int decode_coord(char c) {
    if (c < 'a' || c > 'z') {
        throw std::invalid_argument("Invalid SGF coordinate");
    }
    return c - 'a';
}

char encode_coord(int value) {
    if (value < 0 || value >= 26) {
        throw std::invalid_argument("Coordinate out of range for SGF");
    }
    return static_cast<char>('a' + value);
}

bool try_parse_size_t(const std::string& s, std::size_t& out) {
    std::string t = s;
    t.erase(std::remove_if(t.begin(), t.end(), [](unsigned char c){ return std::isspace(c) != 0; }), t.end());
    if (t.empty()) return false;
    try {
        std::size_t idx = 0;
        unsigned long v = std::stoul(t, &idx);
        if (idx != t.size()) return false;
        out = static_cast<std::size_t>(v);
        return true;
    } catch (...) {
        return false;
    }
}

bool try_parse_double(const std::string& s, double& out) {
    std::string t = s;
    t.erase(std::remove_if(t.begin(), t.end(), [](unsigned char c){ return std::isspace(c) != 0; }), t.end());
    if (t.empty()) return false;
    try {
        std::size_t idx = 0;
        double v = std::stod(t, &idx);
        if (idx != t.size()) return false;
        out = v;
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace

namespace sgf {

GameTree load(std::istream& input) {
    GameTree game;
    std::string data = read_all(input);
    if (data.empty()) {
        return game;
    }

    auto stripped = data;
    stripped.erase(std::remove_if(stripped.begin(), stripped.end(), [](unsigned char c) { return std::isspace(c) != 0; }), stripped.end());

    std::string sz_value = extract_property(stripped, "SZ");
    std::size_t parsed_size = 0;
    if (!sz_value.empty() && try_parse_size_t(sz_value, parsed_size)) {
        game.board_size = parsed_size;
    }
    std::string km_value = extract_property(stripped, "KM");
    double parsed_komi = 0.0;
    if (!km_value.empty() && try_parse_double(km_value, parsed_komi)) {
        game.komi = parsed_komi;
    }

    std::size_t pos = 0;
    while (pos < stripped.size()) {
        if (stripped[pos] == ';' && pos + 4 < stripped.size()) {
            char color_char = stripped[pos + 1];
            pos += 2;
            if (stripped[pos] != '[') {
                ++pos;
                continue;
            }
            ++pos;
            std::string value;
            while (pos < stripped.size() && stripped[pos] != ']') {
                value.push_back(stripped[pos++]);
            }
            ++pos; // skip ']'

            go::Player player;
            if (color_char == 'B' || color_char == 'b') {
                player = go::Player::Black;
            } else if (color_char == 'W' || color_char == 'w') {
                player = go::Player::White;
            } else {
                continue;
            }

            go::Move move = go::Move::Pass();
            if (value.size() == 2u) {
                int x = decode_coord(value[0]);
                int y = decode_coord(value[1]);
                int vertex = y * static_cast<int>(game.board_size) + x;
                move = go::Move(vertex);
            }
            game.moves.push_back({player, move});
        } else {
            ++pos;
        }
    }

    return game;
}

void save(const GameTree& game, std::ostream& output) {
    output << "(;";
    output << "SZ[" << game.board_size << "]";
    output << "KM[" << game.komi << "]";
    for (const auto& record : game.moves) {
        output << ';';
        output << (record.player == go::Player::Black ? 'B' : 'W');
        output << '[';
        if (!record.move.is_pass()) {
            int size = static_cast<int>(game.board_size);
            int x = record.move.vertex % size;
            int y = record.move.vertex / size;
            output << encode_coord(x) << encode_coord(y);
        }
        output << ']';
    }
    output << ")";
}

} // namespace sgf
