#include "TestUtils.hpp"
#include "sgf/SGF.hpp"
#include "go/Board.hpp"

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

    TENUKI_EXPECT_EQ(loaded.board_size, 9u);
    TENUKI_EXPECT_NEAR(loaded.komi, 6.5, 1e-6);
    TENUKI_EXPECT_EQ(loaded.moves.size(), static_cast<std::size_t>(3));
    TENUKI_EXPECT_EQ(loaded.moves[0].player, go::Player::Black);
    TENUKI_EXPECT_FALSE(loaded.moves[0].move.is_pass());
}

static void test_sgf_load_minimal() {
    const std::string data = "(;SZ[5]KM[0.5];B[aa];W[bb];B[])";
    std::istringstream iss(data);
    sgf::GameTree loaded = sgf::load(iss);
    TENUKI_EXPECT_EQ(loaded.board_size, 5u);
    TENUKI_EXPECT_EQ(loaded.moves.size(), static_cast<std::size_t>(3));
    TENUKI_EXPECT(loaded.moves.back().move.is_pass());
}

void run_sgf_tests() {
    test_sgf_roundtrip_simple();
    test_sgf_load_minimal();
}

