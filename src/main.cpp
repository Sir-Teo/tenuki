#include "gtp/GTP.hpp"
#include "go/Rules.hpp"

#include <iostream>

int main() {
    go::Rules rules;
    rules.board_size = 19;
    go::Board board(rules);
    gtp::Server server(std::move(board), std::cin, std::cout);
    server.run();
    return 0;
}

