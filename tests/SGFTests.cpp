#include "sgf/SGF.hpp"
#include "go/Board.hpp"

#include <cassert>
#include <sstream>
#include <string>

static void test_sgf_roundtrip_simple() {
    sgf::GameTree game;
    game.board_size = 9;
    game.komi = 6.5;
    game.moves.push_back({go::Player::Black, go::Move(0)});
    game.moves.push_back({go::Player::White, go::Move(1)});
    game.moves.push_back({go::Player::Black, go::Move::Pass()});

    std::ostringstream oss;
    sgf::save(game, oss);

    std::istringstream iss(oss.str());
    sgf::GameTree loaded = sgf::load(iss);

    assert(loaded.board_size == 9);
    assert(loaded.komi == 6.5);
    assert(loaded.moves.size() == 3);
    assert(loaded.moves[0].player == go::Player::Black);
    assert(!loaded.moves[0].move.is_pass());
}

static void test_sgf_load_minimal() {
    const std::string data = "(;SZ[5]KM[0.5];B[aa];W[bb];B[])";
    std::istringstream iss(data);
    sgf::GameTree loaded = sgf::load(iss);
    assert(loaded.board_size == 5);
    assert(loaded.moves.size() == 3);
}

void run_sgf_tests() {
    test_sgf_roundtrip_simple();
    test_sgf_load_minimal();
}

