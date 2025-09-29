#include "go/Board.hpp"
#include "search/Search.hpp"

#include <cassert>

void test_search_short_random_game_stability() {
    go::Rules rules;
    rules.board_size = 5; // keep small for speed
    go::Board board(rules);

    search::SearchConfig cfg;
    cfg.enable_playout_cap_randomization = false;
    cfg.max_playouts = 16;
    cfg.dirichlet_epsilon = 0.0f;
    auto eval = search::make_uniform_evaluator();
    search::SearchAgent agent(cfg, eval);

    go::Player to_move = go::Player::Black;
    for (int i = 0; i < 30; ++i) { // short game
        go::Move m = agent.select_move(board, to_move, i);
        bool ok = board.play_move(to_move, m);
        assert(ok);
        agent.notify_move(m, board, board.to_play());
        to_move = board.to_play();
    }
}

void run_search_stress_tests() {
    test_search_short_random_game_stability();
}

