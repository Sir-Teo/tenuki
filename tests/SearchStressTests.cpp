#include "TestUtils.hpp"
#include "go/Board.hpp"
#include "search/Search.hpp"

void test_search_short_random_game_stability() {
    go::Rules rules;
    rules.board_size = 5;
    go::Board board(rules);

    search::SearchConfig cfg;
    cfg.enable_playout_cap_randomization = false;
    cfg.max_playouts = 16;
    cfg.dirichlet_epsilon = 0.0f;
    cfg.num_threads = 4;
    auto eval = search::make_uniform_evaluator();
    search::SearchAgent agent(cfg, eval);

    go::Player to_move = go::Player::Black;
    for (int i = 0; i < 30; ++i) {
        go::Move m = agent.select_move(board, to_move, i);
        TENUKI_EXPECT(board.play_move(to_move, m));
        agent.notify_move(m, board, board.to_play());
        to_move = board.to_play();
    }
}

void run_search_stress_tests() {
    test_search_short_random_game_stability();
}
