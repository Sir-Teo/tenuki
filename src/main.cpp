#include "gtp/GTP.hpp"
#include "go/Rules.hpp"
#include "search/Search.hpp"

#include <iostream>

int main() {
    go::Rules rules;
    rules.board_size = 19;
    go::Board board(rules);

    search::SearchConfig search_config;
    search_config.max_playouts = 160;
    search_config.random_playouts_min = 128;
    search_config.random_playouts_max = 256;
    search_config.dirichlet_epsilon = 0.1f;

    auto evaluator = search::make_uniform_evaluator();

    gtp::Server server(std::move(board), std::cin, std::cout, search_config, evaluator);
    server.run();
    return 0;
}
