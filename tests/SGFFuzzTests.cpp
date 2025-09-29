#include "TestUtils.hpp"
#include "sgf/SGF.hpp"

#include <sstream>
#include <string>

static void load_should_not_throw(const std::string& data) {
    std::istringstream iss(data);
    sgf::GameTree game = sgf::load(iss);
    TENUKI_EXPECT(game.board_size <= 25);
}

void test_sgf_malformed_inputs_do_not_throw() {
    load_should_not_throw("");
    load_should_not_throw("(;)\n");
    load_should_not_throw("(;SZ[])\n");
    load_should_not_throw("(;KM[])\n");
    load_should_not_throw("(;SZ[abc])\n");
    load_should_not_throw("(;KM[foo])\n");
    load_should_not_throw("(;B[aa];W[bb];B[cc])");
    load_should_not_throw("(;B[];W[aa])");
    load_should_not_throw("(;C[unterminated comment\n ;B[aa])");
    load_should_not_throw("(;SZ[19]KM[7.5];B[qq])");
}

void run_sgf_fuzz_tests() {
    test_sgf_malformed_inputs_do_not_throw();
}

