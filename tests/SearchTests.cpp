#include "go/Board.hpp"
#include "search/Search.hpp"

#include <cassert>

void test_search_generates_legal_move() {
    go::Rules rules;
    rules.board_size = 5;
    go::Board board(rules);

    search::SearchConfig config;
    config.max_playouts = 16;
    config.enable_playout_cap_randomization = false;
    config.dirichlet_epsilon = 0.0f;
    auto evaluator = search::make_uniform_evaluator();

    search::SearchAgent agent(config, evaluator);
    const go::Move move = agent.select_move(board, go::Player::Black, 0);
    assert(board.is_legal(go::Player::Black, move));
}

void test_tree_reuse_after_moves() {
    go::Rules rules;
    rules.board_size = 5;
    go::Board board(rules);

    search::SearchConfig config;
    config.max_playouts = 8;
    config.enable_playout_cap_randomization = false;
    config.dirichlet_epsilon = 0.0f;

    search::SearchAgent agent(config, search::make_uniform_evaluator());

    go::Move first = agent.select_move(board, go::Player::Black, 0);
    bool applied = board.play_move(go::Player::Black, first);
    assert(applied);
    agent.notify_move(first, board, board.to_play());

    // Opponent passes.
    applied = board.play_move(go::Player::White, go::Move::Pass());
    assert(applied);
    agent.notify_move(go::Move::Pass(), board, board.to_play());

    go::Move second = agent.select_move(board, go::Player::Black, 2);
    assert(board.is_legal(go::Player::Black, second));
}

void run_search_tests() {
    test_search_generates_legal_move();
    test_tree_reuse_after_moves();
}
