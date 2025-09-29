#pragma once

#include "go/Board.hpp"

#include <iosfwd>
#include <optional>
#include <string>
#include <vector>

namespace sgf {

struct MoveRecord {
    go::Player player;
    go::Move move;
};

struct GameTree {
    std::size_t board_size = 19;
    double komi = 7.5;
    std::vector<MoveRecord> moves;
};

GameTree load(std::istream& input);
void save(const GameTree& game, std::ostream& output);

} // namespace sgf

