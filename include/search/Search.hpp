#pragma once

#include "go/Board.hpp"

#include <memory>
#include <random>
#include <unordered_map>
#include <vector>

namespace search {

struct SearchConfig {
    int max_playouts = 256;
    float cpuct = 1.6f;
    float dirichlet_alpha = 0.03f;
    float dirichlet_epsilon = 0.25f;
    float temperature = 1.0f;
    int temperature_move_cutoff = 30;
    bool enable_playout_cap_randomization = true;
    int random_playouts_min = 192;
    int random_playouts_max = 384;
    unsigned int seed = 0x5eed1234u;
};

struct EvaluationResult {
    std::vector<float> policy; // policy logits/probabilities for each vertex plus pass.
    float value = 0.0f;        // value estimate from the perspective of the current player.
};

class Evaluator {
public:
    virtual ~Evaluator() = default;
    virtual EvaluationResult evaluate(const go::Board& board, go::Player to_play) = 0;
};

class UniformEvaluator : public Evaluator {
public:
    EvaluationResult evaluate(const go::Board& board, go::Player to_play) override;
};

class SearchAgent {
public:
    SearchAgent(SearchConfig config, std::shared_ptr<Evaluator> evaluator);

    go::Move select_move(const go::Board& board, go::Player to_play, int move_number);

    void notify_move(go::Move move, const go::Board& board_after_move, go::Player to_play);

    void reset();

    const SearchConfig& config() const noexcept { return config_; }

private:
    struct Node {
        struct Child {
            int move = -1; // -1 denotes pass
            float prior = 0.0f;
            float value_sum = 0.0f;
            int visit_count = 0;
            std::unique_ptr<Node> node;
        };

        go::Player to_play = go::Player::Black;
        bool expanded = false;
        bool noise_applied = false;
        int visit_count = 0;
        float value_sum = 0.0f;
        std::vector<Child> children;
        std::unordered_map<int, std::size_t> move_to_index;
    };

    using Child = Node::Child;

    void ensure_root(const go::Board& board, go::Player to_play);
    go::Move select_move_from_root(int move_number, std::mt19937& rng) const;
    float expand(Node& node, const go::Board& board);
    float run_simulation(const go::Board& root_board, std::mt19937& rng);
    float simulate(go::Board board_copy, Node& node, std::mt19937& rng);
    int select_child(const Node& node, std::mt19937& rng) const;
    void backpropagate(std::vector<Node*> path, std::vector<int> child_indices, float value);
    void apply_dirichlet_noise(Node& node, std::mt19937& rng);
    std::uint64_t state_key(const go::Board& board, go::Player to_play) const;

    SearchConfig config_{};
    std::shared_ptr<Evaluator> evaluator_;
    std::unique_ptr<Node> root_;
    std::uint64_t root_hash_ = 0;
    go::Player root_player_ = go::Player::Black;
    bool root_ready_ = false;
    std::mt19937 rng_;
};

std::shared_ptr<Evaluator> make_uniform_evaluator();

} // namespace search
