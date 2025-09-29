#include <iostream>

void run_board_tests();
void run_search_tests();
void run_sgf_tests();

int main() {
    run_board_tests();
    run_search_tests();
    run_sgf_tests();
    std::cout << "All tests passed\n";
    return 0;
}
