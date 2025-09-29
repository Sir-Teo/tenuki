#pragma once

#include "go/Rules.hpp"
#include "go/Zobrist.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <unordered_set>
#include <vector>

namespace go {

enum class Player : std::uint8_t {
    Black = 0,
    White = 1
};

enum class PointState : std::uint8_t {
    Empty = 0,
    Black = 1,
    White = 2
};

struct Move {
    static Move Pass() { return Move{-1}; }

    explicit Move(int idx) : vertex(idx) {}

    bool is_pass() const { return vertex < 0; }

    int vertex;
};

struct ScoreResult {
    double black_points = 0.0;
    double white_points = 0.0;
};

class Board {
public:
    explicit Board(const Rules& rules = {});

    void clear();

    bool play_move(Player player, Move move);
    bool is_legal(Player player, Move move) const;

    std::size_t board_size() const noexcept { return rules_.board_size; }
    const Rules& rules() const noexcept { return rules_; }

    PointState point_state(std::size_t vertex) const;
    Player to_play() const noexcept { return to_play_; }
    void set_to_play(Player player);
    std::optional<int> ko_vertex() const noexcept { return ko_vertex_; }

    std::uint64_t position_hash() const noexcept { return position_hash_; }
    const std::unordered_set<std::uint64_t>& seen_positions() const noexcept { return position_history_; }

    ScoreResult tromp_taylor_score() const;

private:
    std::size_t index(std::size_t x, std::size_t y) const noexcept;
    bool in_bounds(int x, int y) const noexcept;
    std::vector<int> neighbors(int vertex) const;
    int count_liberties(int vertex, PointState color, std::vector<bool>& visited, std::vector<bool>& liberty_visited) const;
    int collect_group(int vertex, PointState color, std::vector<int>& out_vertices) const;

    bool violates_superko(std::uint64_t prospective_hash) const;

    void place_stone(int vertex, PointState color);
    void remove_stone(int vertex);
    void set_ko(std::optional<int> vertex);

    Rules rules_{};
    std::size_t board_len_ = 0;
    std::vector<PointState> board_;

    Player to_play_ = Player::Black;
    std::optional<int> ko_vertex_;

    ZobristTable zobrist_;
    std::uint64_t position_hash_ = 0;
    std::unordered_set<std::uint64_t> position_history_;
    std::vector<std::uint64_t> history_stack_;
};

Player other(Player p);
PointState to_point(Player p);
Player to_player(PointState state);

} // namespace go

