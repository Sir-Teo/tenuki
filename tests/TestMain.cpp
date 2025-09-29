#include <iostream>

void run_board_tests();
void run_search_tests();
void run_sgf_tests();
void run_sgf_fuzz_tests();
void run_search_stress_tests();
void run_model_quality_tests();

int main() {
    run_board_tests();
    run_search_tests();
    run_sgf_tests();
    run_sgf_fuzz_tests();
    run_search_stress_tests();
    run_model_quality_tests();
    std::cout << "All tests passed\n";
    return 0;
}
