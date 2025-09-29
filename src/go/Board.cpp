#include "go/Board.hpp"

#include <algorithm>
#include <array>
#include <queue>
#include <stdexcept>

namespace go {

namespace {
constexpr std::array<int, 4> kDx{1, -1, 0, 0};
constexpr std::array<int, 4> kDy{0, 0, 1, -1};
}

Board::Board(const Rules& rules) : rules_(rules) {
    if (rules.board_size == 0 || rules.board_size > 25) {
        throw std::invalid_argument("Board size must be between 1 and 25");
    }
    board_len_ = rules_.board_size * rules_.board_size;
    board_.assign(board_len_, PointState::Empty);
    zobrist_ = ZobristTable(rules_.board_size);
    clear();
}

void Board::clear() {
    std::fill(board_.begin(), board_.end(), PointState::Empty);
    to_play_ = Player::Black;
    set_ko(std::nullopt);
    position_hash_ = 0;
    position_history_.clear();
    history_stack_.clear();
    position_history_.insert(position_hash_);
    history_stack_.push_back(position_hash_);
}

void Board::set_to_play(Player player) {
    to_play_ = player;
}

PointState Board::point_state(std::size_t vertex) const {
    if (vertex >= board_.size()) {
        throw std::out_of_range("vertex out of range");
    }
    return board_[vertex];
}

bool Board::play_move(Player player, Move move) {
    if (move.is_pass()) {
        set_ko(std::nullopt);
        to_play_ = other(player);
        history_stack_.push_back(position_hash_);
        position_history_.insert(position_hash_);
        return true;
    }

    if (move.vertex < 0 || static_cast<std::size_t>(move.vertex) >= board_len_) {
        return false;
    }

    const std::size_t move_index = static_cast<std::size_t>(move.vertex);

    if (board_[move_index] != PointState::Empty) {
        return false;
    }

    if (ko_vertex_.has_value() && ko_vertex_.value() == move.vertex) {
        return false;
    }

    PointState stone = to_point(player);
    PointState opponent = to_point(other(player));

    auto previous_ko = ko_vertex_;
    place_stone(move.vertex, stone);

    std::vector<int> captured;
    captured.reserve(4 * rules_.board_size);

    for (int neighbor : neighbors(move.vertex)) {
        const std::size_t neighbor_index = static_cast<std::size_t>(neighbor);
        if (board_[neighbor_index] != opponent) {
            continue;
        }
        std::vector<int> group;
        if (collect_group(neighbor, opponent, group) == 0) {
            for (int v : group) {
                remove_stone(v);
                captured.push_back(v);
            }
        }
    }

    std::vector<bool> visited(board_len_, false);
    std::vector<bool> liberty_seen(board_len_, false);
    int liberties = count_liberties(move.vertex, stone, visited, liberty_seen);
    if (liberties == 0 && captured.empty() && !rules_.allow_suicide) {
        remove_stone(move.vertex);
        for (int v : captured) {
            place_stone(v, opponent);
        }
        set_ko(previous_ko);
        return false;
    }

    std::optional<int> new_ko;
    if (captured.size() == 1) {
        // Check for simple ko shape: newly placed stone must have exactly one liberty.
        std::fill(visited.begin(), visited.end(), false);
        std::fill(liberty_seen.begin(), liberty_seen.end(), false);
        if (count_liberties(move.vertex, stone, visited, liberty_seen) == 1) {
            new_ko = captured.front();
        }
    }
    set_ko(new_ko);

    if (violates_superko(position_hash_)) {
        // Undo move
        set_ko(previous_ko);
        remove_stone(move.vertex);
        for (int v : captured) {
            place_stone(v, opponent);
        }
        return false;
    }

    to_play_ = other(player);
    history_stack_.push_back(position_hash_);
    position_history_.insert(position_hash_);
    return true;
}

bool Board::is_legal(Player player, Move move) const {
    Board copy(*this);
    return copy.play_move(player, move);
}

std::size_t Board::index(std::size_t x, std::size_t y) const noexcept {
    return y * rules_.board_size + x;
}

bool Board::in_bounds(int x, int y) const noexcept {
    return x >= 0 && y >= 0 && x < static_cast<int>(rules_.board_size) && y < static_cast<int>(rules_.board_size);
}

std::vector<int> Board::neighbors(int vertex) const {
    std::vector<int> result;
    result.reserve(4);
    const int board_size = static_cast<int>(rules_.board_size);
    const int x = vertex % board_size;
    const int y = vertex / board_size;
    for (int dir = 0; dir < 4; ++dir) {
        const std::size_t dir_index = static_cast<std::size_t>(dir);
        const int nx = x + kDx[dir_index];
        const int ny = y + kDy[dir_index];
        if (in_bounds(nx, ny)) {
            const std::size_t neighbor_index = index(static_cast<std::size_t>(nx), static_cast<std::size_t>(ny));
            result.push_back(static_cast<int>(neighbor_index));
        }
    }
    return result;
}

int Board::count_liberties(int vertex, PointState color, std::vector<bool>& visited, std::vector<bool>& liberty_visited) const {
    std::queue<int> q;
    q.push(vertex);
    const std::size_t vertex_index = static_cast<std::size_t>(vertex);
    visited[vertex_index] = true;
    int liberties = 0;

    while (!q.empty()) {
        const int v = q.front();
        q.pop();
        for (int n : neighbors(v)) {
            const std::size_t neighbor_index = static_cast<std::size_t>(n);
            if (board_[neighbor_index] == PointState::Empty) {
                if (!liberty_visited[neighbor_index]) {
                    liberty_visited[neighbor_index] = true;
                    ++liberties;
                }
            } else if (board_[neighbor_index] == color && !visited[neighbor_index]) {
                visited[neighbor_index] = true;
                q.push(n);
            }
        }
    }
    return liberties;
}

int Board::collect_group(int vertex, PointState color, std::vector<int>& out_vertices) const {
    std::queue<int> q;
    std::vector<bool> visited(board_len_, false);
    q.push(vertex);
    visited[static_cast<std::size_t>(vertex)] = true;
    int liberties = 0;

    while (!q.empty()) {
        const int v = q.front();
        q.pop();
        out_vertices.push_back(v);
        for (int n : neighbors(v)) {
            const std::size_t neighbor_index = static_cast<std::size_t>(n);
            if (board_[neighbor_index] == PointState::Empty) {
                ++liberties;
            } else if (board_[neighbor_index] == color && !visited[neighbor_index]) {
                visited[neighbor_index] = true;
                q.push(n);
            }
        }
    }
    return liberties;
}

bool Board::violates_superko(std::uint64_t prospective_hash) const {
    if (rules_.ko_rule != KoRule::PositionalSuperko) {
        return false;
    }
    return position_history_.find(prospective_hash) != position_history_.end();
}

void Board::place_stone(int vertex, PointState color) {
    const std::size_t vertex_index = static_cast<std::size_t>(vertex);
    board_[vertex_index] = color;
    if (color == PointState::Black) {
        position_hash_ ^= zobrist_.black_stone_hash(vertex_index);
    } else if (color == PointState::White) {
        position_hash_ ^= zobrist_.white_stone_hash(vertex_index);
    }
}

void Board::remove_stone(int vertex) {
    const std::size_t vertex_index = static_cast<std::size_t>(vertex);
    PointState color = board_[vertex_index];
    if (color == PointState::Black) {
        position_hash_ ^= zobrist_.black_stone_hash(vertex_index);
    } else if (color == PointState::White) {
        position_hash_ ^= zobrist_.white_stone_hash(vertex_index);
    }
    board_[vertex_index] = PointState::Empty;
}

void Board::set_ko(std::optional<int> vertex) {
    if (ko_vertex_) {
        position_hash_ ^= zobrist_.ko_hash(static_cast<std::size_t>(*ko_vertex_));
    }
    ko_vertex_ = vertex;
    if (ko_vertex_) {
        position_hash_ ^= zobrist_.ko_hash(static_cast<std::size_t>(*ko_vertex_));
    }
}

ScoreResult Board::tromp_taylor_score() const {
    ScoreResult result;
    std::vector<bool> visited(board_len_, false);

    for (int v = 0; v < static_cast<int>(board_len_); ++v) {
        const std::size_t vertex_index = static_cast<std::size_t>(v);
        if (board_[vertex_index] == PointState::Black) {
            result.black_points += 1.0;
        } else if (board_[vertex_index] == PointState::White) {
            result.white_points += 1.0;
        } else if (!visited[vertex_index]) {
            std::queue<int> q;
            q.push(v);
            visited[vertex_index] = true;
            bool borders_black = false;
            bool borders_white = false;
            int region_size = 0;

            while (!q.empty()) {
                int cur = q.front();
                q.pop();
                ++region_size;
                for (int n : neighbors(cur)) {
                    const std::size_t neighbor_index = static_cast<std::size_t>(n);
                    if (board_[neighbor_index] == PointState::Empty && !visited[neighbor_index]) {
                        visited[neighbor_index] = true;
                        q.push(n);
                    } else if (board_[neighbor_index] == PointState::Black) {
                        borders_black = true;
                    } else if (board_[neighbor_index] == PointState::White) {
                        borders_white = true;
                    }
                }
            }

            if (borders_black && !borders_white) {
                result.black_points += region_size;
            } else if (borders_white && !borders_black) {
                result.white_points += region_size;
            }
        }
    }

    result.white_points += rules_.komi;
    return result;
}

Player other(Player p) {
    return p == Player::Black ? Player::White : Player::Black;
}

PointState to_point(Player p) {
    return p == Player::Black ? PointState::Black : PointState::White;
}

Player to_player(PointState state) {
    return state == PointState::Black ? Player::Black : Player::White;
}

std::uint64_t Board::state_key() const noexcept {
    std::uint64_t key = position_hash_;
    if (to_play_ == Player::White) {
        key ^= zobrist_.side_to_move_hash();
    }
    return key;
}

} // namespace go
