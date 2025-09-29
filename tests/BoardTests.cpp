#include "go/Board.hpp"

#include <cassert>

using go::Board;
using go::Move;
using go::Player;
using go::PointState;
using go::Rules;

void test_simple_capture() {
    Rules rules;
    rules.board_size = 3;
    Board board(rules);

    board.play_move(Player::Black, Move(1));
    board.play_move(Player::White, Move(4));
    board.play_move(Player::Black, Move(3));
    board.play_move(Player::White, Move::Pass());
    board.play_move(Player::Black, Move(5));
    board.play_move(Player::White, Move::Pass());
    board.play_move(Player::Black, Move(7));

    assert(board.point_state(4) == PointState::Empty);
}

void test_simple_ko() {
    Rules rules;
    rules.board_size = 5;
    Board board(rules);

    board.play_move(Player::Black, Move(7));   // C1
    board.play_move(Player::White, Move(8));   // D1
    board.play_move(Player::Black, Move(12));  // C2
    board.play_move(Player::White, Move(17));  // D3
    board.play_move(Player::Black, Move(13));  // C3
    board.play_move(Player::White, Move(18));  // D4
    board.play_move(Player::Black, Move(19));  // C4 capture creating ko

    assert(!board.is_legal(Player::White, Move(18)));
    board.play_move(Player::White, Move::Pass());
    assert(board.is_legal(Player::Black, Move::Pass()));
}

void test_tromp_taylor_score() {
    Rules rules;
    rules.board_size = 3;
    rules.komi = 0.0;
    Board board(rules);

    board.play_move(Player::Black, Move(0));
    board.play_move(Player::White, Move(1));
    board.play_move(Player::Black, Move(3));
    board.play_move(Player::White, Move(4));
    board.play_move(Player::Black, Move(6));

    auto score = board.tromp_taylor_score();
    assert(score.black_points == 3);
    assert(score.white_points == 2);
}

void run_board_tests() {
    test_simple_capture();
    test_simple_ko();
    test_tromp_taylor_score();
}
