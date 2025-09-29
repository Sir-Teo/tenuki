#include "TestUtils.hpp"
#include "go/Board.hpp"

using go::Board;
using go::KoRule;
using go::Move;
using go::Player;
using go::PointState;
using go::Rules;

namespace {

void surround_center(Board& board) {
    TENUKI_EXPECT(board.play_move(Player::Black, Move(3)));
    TENUKI_EXPECT(board.play_move(Player::White, Move(0)));
    TENUKI_EXPECT(board.play_move(Player::Black, Move(1)));
    TENUKI_EXPECT(board.play_move(Player::White, Move(2)));
    TENUKI_EXPECT(board.play_move(Player::Black, Move(5)));
    TENUKI_EXPECT(board.play_move(Player::White, Move(6)));
    TENUKI_EXPECT(board.play_move(Player::Black, Move(7)));
}

} // namespace

void test_simple_capture() {
    Rules rules;
    rules.board_size = 3;
    Board board(rules);

    TENUKI_EXPECT(board.play_move(Player::Black, Move(1)));
    TENUKI_EXPECT(board.play_move(Player::White, Move(4)));
    TENUKI_EXPECT(board.play_move(Player::Black, Move(3)));
    TENUKI_EXPECT(board.play_move(Player::White, Move::Pass()));
    TENUKI_EXPECT(board.play_move(Player::Black, Move(5)));
    TENUKI_EXPECT(board.play_move(Player::White, Move::Pass()));
    TENUKI_EXPECT(board.play_move(Player::Black, Move(7)));

    TENUKI_EXPECT_EQ(board.point_state(4), PointState::Empty);
}

void test_neutral_point_no_territory() {
    Rules rules;
    rules.board_size = 3;
    rules.komi = 0.0;
    Board board(rules);

    TENUKI_EXPECT(board.play_move(Player::Black, Move(3)));
    TENUKI_EXPECT(board.play_move(Player::White, Move(1)));
    TENUKI_EXPECT(board.play_move(Player::Black, Move(5)));
    TENUKI_EXPECT(board.play_move(Player::White, Move(7)));

    auto score = board.tromp_taylor_score();
    TENUKI_EXPECT_EQ(score.black_points, 2.0);
    TENUKI_EXPECT_EQ(score.white_points, 2.0);
}

void test_simple_ko() {
    Rules rules;
    rules.board_size = 5;
    Board board(rules);

    TENUKI_EXPECT(board.play_move(Player::Black, Move(7)));   // C1
    TENUKI_EXPECT(board.play_move(Player::White, Move(8)));   // D1
    TENUKI_EXPECT(board.play_move(Player::Black, Move(12)));  // C2
    TENUKI_EXPECT(board.play_move(Player::White, Move(17)));  // D3
    TENUKI_EXPECT(board.play_move(Player::Black, Move(13)));  // C3
    TENUKI_EXPECT(board.play_move(Player::White, Move(18)));  // D4
    TENUKI_EXPECT(board.play_move(Player::Black, Move(19)));  // C4 capture creating ko

    TENUKI_EXPECT_FALSE(board.is_legal(Player::White, Move(18)));
    TENUKI_EXPECT(board.play_move(Player::White, Move::Pass()));
    TENUKI_EXPECT(board.play_move(Player::Black, Move::Pass()));
}

void test_positional_superko_prevents_cycle() {
    Rules rules;
    rules.board_size = 5;
    rules.ko_rule = KoRule::PositionalSuperko;
    Board board(rules);

    TENUKI_EXPECT(board.play_move(Player::Black, Move(7)));
    TENUKI_EXPECT(board.play_move(Player::White, Move(8)));
    TENUKI_EXPECT(board.play_move(Player::Black, Move(12)));
    TENUKI_EXPECT(board.play_move(Player::White, Move(17)));
    TENUKI_EXPECT(board.play_move(Player::Black, Move(13)));
    TENUKI_EXPECT(board.play_move(Player::White, Move(18)));
    TENUKI_EXPECT(board.play_move(Player::Black, Move(19)));

    // Ko recapture after passes repeats a previous board and should be rejected.
    TENUKI_EXPECT(board.play_move(Player::White, Move::Pass()));
    TENUKI_EXPECT(board.play_move(Player::Black, Move::Pass()));
    TENUKI_EXPECT_FALSE(board.play_move(Player::White, Move(18)));
}

void test_tromp_taylor_score() {
    Rules rules;
    rules.board_size = 3;
    rules.komi = 0.0;
    Board board(rules);

    TENUKI_EXPECT(board.play_move(Player::Black, Move(0)));
    TENUKI_EXPECT(board.play_move(Player::White, Move(1)));
    TENUKI_EXPECT(board.play_move(Player::Black, Move(3)));
    TENUKI_EXPECT(board.play_move(Player::White, Move(4)));
    TENUKI_EXPECT(board.play_move(Player::Black, Move(6)));

    auto score = board.tromp_taylor_score();
    TENUKI_EXPECT_EQ(score.black_points, 3.0);
    TENUKI_EXPECT_EQ(score.white_points, 2.0);
}

void test_suicide_rule_respected() {
    Rules base;
    base.board_size = 3;

    Rules no_suicide = base;
    no_suicide.allow_suicide = false;
    Board board_no(no_suicide);
    surround_center(board_no);
    TENUKI_EXPECT_FALSE(board_no.play_move(Player::White, Move(4)));

    Rules allow_suicide = base;
    allow_suicide.allow_suicide = true;
    Board board_yes(allow_suicide);
    surround_center(board_yes);
    TENUKI_EXPECT(board_yes.play_move(Player::White, Move(4)));
}

void test_state_key_includes_side_to_move() {
    Rules rules;
    rules.board_size = 5;
    Board board(rules);

    const auto key_black = board.state_key();
    board.set_to_play(Player::White);
    const auto key_white = board.state_key();
    TENUKI_EXPECT_NE(key_black, key_white);

    TENUKI_EXPECT(board.play_move(Player::White, Move(12)));
    const auto after_move = board.state_key();

    Board copy = board;
    copy.set_to_play(Player::White);
    TENUKI_EXPECT_NE(after_move, copy.state_key());
}

void run_board_tests() {
    test_simple_capture();
    test_neutral_point_no_territory();
    test_simple_ko();
    test_positional_superko_prevents_cycle();
    test_tromp_taylor_score();
    test_suicide_rule_respected();
    test_state_key_includes_side_to_move();
}

