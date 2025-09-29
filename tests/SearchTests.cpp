#include "TestUtils.hpp"
#include "go/Board.hpp"
#include "search/Search.hpp"

#include <algorithm>
#include <memory>
#include <random>

namespace {

class BiasedEvaluator : public search::Evaluator {
public:
    explicit BiasedEvaluator(int preferred_vertex, float value)
        : preferred_vertex_(preferred_vertex), value_(value) {}

    search::EvaluationResult evaluate(const go::Board& board, go::Player) override {
        const std::size_t area = board.board_size() * board.board_size();
        search::EvaluationResult result;
        result.policy.assign(area + 1, 1.0f);
        if (preferred_vertex_ >= 0 && preferred_vertex_ < static_cast<int>(area)) {
            result.policy[preferred_vertex_] = 10.0f;
        }
        result.value = value_;
        return result;
    }

private:
    int preferred_vertex_ = -1;
    float value_ = 0.0f;
};

class CountingEvaluator : public search::Evaluator {
public:
    search::EvaluationResult evaluate(const go::Board& board, go::Player) override {
        ++calls;
        const std::size_t area = board.board_size() * board.board_size();
        search::EvaluationResult result;
        result.policy.assign(area + 1, 1.0f);
        result.value = 0.0f;
        return result;
    }

    int calls = 0;
};

go::Move choose_alternate_move(const go::Board& board, const go::Move& primary) {
    const std::size_t area = board.board_size() * board.board_size();
    for (std::size_t idx = 0; idx < area; ++idx) {
        if (!primary.is_pass() && static_cast<std::size_t>(primary.vertex) == idx) {
            continue;
        }
        return go::Move(static_cast<int>(idx));
    }
    return go::Move::Pass();
}

} // namespace

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
    TENUKI_EXPECT(board.is_legal(go::Player::Black, move));
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
    TENUKI_EXPECT(board.play_move(go::Player::Black, first));
    agent.notify_move(first, board, board.to_play());

    TENUKI_EXPECT(board.play_move(go::Player::White, go::Move::Pass()));
    agent.notify_move(go::Move::Pass(), board, board.to_play());

    go::Move second = agent.select_move(board, go::Player::Black, 2);
    TENUKI_EXPECT(board.is_legal(go::Player::Black, second));
}

void test_search_prefers_high_prior_move() {
    go::Rules rules;
    rules.board_size = 3;
    go::Board board(rules);

    search::SearchConfig config;
    config.max_playouts = 32;
    config.enable_playout_cap_randomization = false;
    config.dirichlet_epsilon = 0.0f;
    config.temperature = 0.0f;
    config.temperature_move_cutoff = 0;

    auto evaluator = std::make_shared<BiasedEvaluator>(0, 0.3f);
    search::SearchAgent agent(config, evaluator);

    const go::Move move = agent.select_move(board, go::Player::Black, 0);
    TENUKI_EXPECT_FALSE(move.is_pass());
    TENUKI_EXPECT_EQ(move.vertex, 0);
}

void test_search_returns_pass_when_no_legal_moves() {
    go::Rules rules;
    rules.board_size = 1;
    rules.allow_suicide = true;
    go::Board board(rules);

    TENUKI_EXPECT(board.play_move(go::Player::Black, go::Move(0)));

    search::SearchConfig config;
    config.max_playouts = 8;
    config.enable_playout_cap_randomization = false;
    config.dirichlet_epsilon = 0.0f;
    config.temperature = 0.0f;
    config.temperature_move_cutoff = 0;

    search::SearchAgent agent(config, search::make_uniform_evaluator());
    const go::Move move = agent.select_move(board, board.to_play(), 4);
    TENUKI_EXPECT(move.is_pass());
}

void test_search_uses_randomized_playout_cap_when_enabled() {
    go::Rules rules;
    rules.board_size = 3;
    go::Board board(rules);

    auto evaluator = std::make_shared<CountingEvaluator>();

    search::SearchConfig config;
    config.max_playouts = 1;
    config.enable_playout_cap_randomization = true;
    config.random_playouts_min = 2;
    config.random_playouts_max = 4;
    config.dirichlet_epsilon = 0.0f;
    config.temperature = 0.0f;
    config.temperature_move_cutoff = 0;

    std::mt19937 rng(config.seed);
    std::uniform_int_distribution<int> dist(config.random_playouts_min, config.random_playouts_max);
    const int expected_playouts = dist(rng);

    search::SearchAgent agent(config, evaluator);
    agent.select_move(board, go::Player::Black, 0);

    TENUKI_EXPECT_EQ(evaluator->calls, expected_playouts + 1);
}

void test_notify_move_resets_tree_when_child_unexpanded() {
    go::Rules rules;
    rules.board_size = 3;
    go::Board board(rules);

    auto evaluator = std::make_shared<CountingEvaluator>();

    search::SearchConfig config;
    config.max_playouts = 1;
    config.enable_playout_cap_randomization = false;
    config.dirichlet_epsilon = 0.0f;
    config.temperature = 0.0f;
    config.temperature_move_cutoff = 0;

    search::SearchAgent agent(config, evaluator);

    const go::Move chosen = agent.select_move(board, go::Player::Black, 0);
    const int calls_after_first = evaluator->calls;

    const go::Move alternate = choose_alternate_move(board, chosen);
    TENUKI_EXPECT(board.play_move(go::Player::Black, alternate));
    agent.notify_move(alternate, board, board.to_play());

    const int calls_before_second = evaluator->calls;
    agent.select_move(board, board.to_play(), 1);

    const int playouts = std::max(1, config.max_playouts);
    TENUKI_EXPECT_EQ(evaluator->calls, calls_before_second + playouts + 1);
    TENUKI_EXPECT(evaluator->calls > calls_after_first);
}

void run_search_tests() {
    test_search_generates_legal_move();
    test_tree_reuse_after_moves();
    test_search_prefers_high_prior_move();
    test_search_returns_pass_when_no_legal_moves();
    test_search_uses_randomized_playout_cap_when_enabled();
    test_notify_move_resets_tree_when_child_unexpanded();
}
