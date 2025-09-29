#include "go/Board.hpp"
#include "search/Search.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <memory>
#include <numeric>
#include <vector>

namespace {

constexpr float kEpsilon = 1e-6f;

struct ScenarioMove {
    go::Player player;
    int vertex; // -1 for pass
};

struct Scenario {
    const char* name;
    std::vector<ScenarioMove> history;
    go::Player to_play;
    std::vector<int> optimal_moves; // -1 for pass
};

class OneStepScoreEvaluator : public search::Evaluator {
public:
    search::EvaluationResult evaluate(const go::Board& board, go::Player to_play) override {
        const std::size_t area = board.board_size() * board.board_size();
        search::EvaluationResult result;
        result.policy.assign(area + 1, 0.0f);

        std::vector<float> raw(area + 1, -1e9f);
        float best = -1e9f;
        float worst = 1e9f;

        auto score_after_move = [&](const go::Board& start, go::Player player, go::Move move) {
            go::Board copy(start);
            if (!copy.play_move(player, move)) {
                return -1e6f;
            }
            auto score = copy.tromp_taylor_score();
            double diff = (player == go::Player::Black)
                ? (score.black_points - score.white_points)
                : (score.white_points - score.black_points);
            return static_cast<float>(diff);
        };

        for (std::size_t idx = 0; idx < area; ++idx) {
            if (board.point_state(idx) != go::PointState::Empty) {
                continue;
            }
            float score = score_after_move(board, to_play, go::Move(static_cast<int>(idx)));
            if (score <= -1e5f) {
                continue;
            }
            raw[idx] = score;
            best = std::max(best, score);
            worst = std::min(worst, score);
        }

        float pass_score = score_after_move(board, to_play, go::Move::Pass());
        raw.back() = pass_score;
        best = std::max(best, pass_score);
        worst = std::min(worst, pass_score);

        if (best <= -1e8f) {
            // No legal moves recorded; uniform fallback.
            std::fill(result.policy.begin(), result.policy.end(), 1.0f / static_cast<float>(area + 1));
            result.value = 0.0f;
            return result;
        }

        const float offset = best == worst ? 0.0f : -worst;
        float sum = 0.0f;
        for (std::size_t i = 0; i < raw.size(); ++i) {
            float val = raw[i];
            if (val <= -1e8f) {
                continue;
            }
            float shifted = val + offset;
            float weight = shifted <= 0.0f ? kEpsilon : shifted;
            result.policy[i] = weight;
            sum += weight;
        }

        if (sum <= kEpsilon) {
            std::fill(result.policy.begin(), result.policy.end(), 1.0f / static_cast<float>(area + 1));
        } else {
            for (float& w : result.policy) {
                w /= sum;
            }
        }

        result.value = best;
        result.value = std::clamp(result.value / 10.0f, -1.0f, 1.0f);
        return result;
    }
};

go::Board apply_history(const Scenario& scenario) {
    go::Rules rules;
    rules.board_size = 5;
    go::Board board(rules);
    for (const ScenarioMove& m : scenario.history) {
        go::Move move = (m.vertex < 0) ? go::Move::Pass() : go::Move(m.vertex);
        const bool ok = board.play_move(m.player, move);
        assert(ok);
    }
    board.set_to_play(scenario.to_play);
    return board;
}

bool is_expected_move(const Scenario& scenario, const go::Move& move) {
    const int vertex = move.is_pass() ? -1 : move.vertex;
    return std::find(scenario.optimal_moves.begin(), scenario.optimal_moves.end(), vertex) != scenario.optimal_moves.end();
}

float evaluate_model_on_scenarios(const std::vector<Scenario>& scenarios) {
    int successes = 0;
    int total = 0;

    search::SearchConfig cfg;
    cfg.enable_playout_cap_randomization = false;
    cfg.max_playouts = 64;
    cfg.dirichlet_epsilon = 0.0f;
    cfg.temperature = 0.0f;
    cfg.temperature_move_cutoff = 0;

    for (const Scenario& scenario : scenarios) {
        go::Board board = apply_history(scenario);
        auto evaluator = std::make_shared<OneStepScoreEvaluator>();
        search::SearchAgent agent(cfg, evaluator);
        go::Move predicted = agent.select_move(board, scenario.to_play, static_cast<int>(scenario.history.size()));
        if (is_expected_move(scenario, predicted)) {
            ++successes;
        }
        ++total;
    }

    return total == 0 ? 0.0f : static_cast<float>(successes) / static_cast<float>(total);
}

std::vector<Scenario> build_scenarios() {
    return {
        {
            "capture_atari",
            {
                {go::Player::Black, 1},
                {go::Player::White, 6},
                {go::Player::Black, 5},
                {go::Player::White, -1},
                {go::Player::Black, 7},
                {go::Player::White, -1},
            },
            go::Player::Black,
            {11}
        },
        {
            "mirror_capture",
            {
                {go::Player::White, 1},
                {go::Player::Black, 6},
                {go::Player::White, 5},
                {go::Player::Black, -1},
                {go::Player::White, 7},
                {go::Player::Black, -1},
            },
            go::Player::White,
            {11}
        }
    };
}

} // namespace

void test_model_quality_harness_scores_simple_positions() {
    const auto scenarios = build_scenarios();
    const float accuracy = evaluate_model_on_scenarios(scenarios);
    assert(accuracy >= 1.0f - kEpsilon);
}

void run_model_quality_tests() {
    test_model_quality_harness_scores_simple_positions();
}
