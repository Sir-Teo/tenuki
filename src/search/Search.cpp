#include "search/Search.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>
#include <numeric>
#include <random>
#include <thread>
#include <unordered_map>

namespace search {
namespace {

constexpr float kEpsilon = 1e-8f;

} // namespace

EvaluationResult UniformEvaluator::evaluate(const go::Board& board, go::Player /*to_play*/) {
    const std::size_t board_size = board.board_size();
    const std::size_t total_moves = board_size * board_size + 1; // include pass
    EvaluationResult result;
    result.policy.assign(total_moves, 1.0f / static_cast<float>(total_moves));
    result.value = 0.0f;
    return result;
}

SearchAgent::SearchAgent(SearchConfig config, std::shared_ptr<Evaluator> evaluator)
    : config_(config), evaluator_(std::move(evaluator)), root_(nullptr), rng_(config.seed) {
    if (!evaluator_) {
        evaluator_ = std::make_shared<UniformEvaluator>();
    }
}

std::shared_ptr<Evaluator> make_uniform_evaluator() {
    return std::make_shared<UniformEvaluator>();
}

void SearchAgent::ensure_root(const go::Board& board, go::Player to_play) {
    const std::uint64_t key = state_key(board, to_play);
    if (!root_ || !root_ready_ || root_hash_ != key) {
        root_ = std::make_unique<Node>();
        root_->to_play = to_play;
        root_->expanded = false;
        root_->noise_applied = false;
        root_->visit_count = 0;
        root_->value_sum = 0.0f;
        root_->children.clear();
        root_->move_to_index.clear();
        root_->virtual_loss_count = 0;
        root_->expanding = false;
        root_hash_ = key;
        root_player_ = to_play;
        root_ready_ = true;
    } else {
        std::scoped_lock lock(root_->mutex);
        root_->to_play = to_play;
    }

    if (root_) {
        if (!root_->expanded) {
            float unused_value = 0.0f;
            (void)try_expand(*root_, board, unused_value);
        }

        if (config_.dirichlet_epsilon > 0.0f) {
            bool needs_noise = false;
            {
                std::unique_lock<std::mutex> lock(root_->mutex);
                if (!root_->noise_applied && !root_->children.empty()) {
                    root_->noise_applied = true;
                    needs_noise = true;
                }
            }
            if (needs_noise) {
                apply_dirichlet_noise(*root_, rng_);
            }
        }
    }
}

go::Move SearchAgent::select_move(const go::Board& board, go::Player to_play, int move_number) {
    ensure_root(board, to_play);

    int playouts = std::max(1, config_.max_playouts);
    if (config_.enable_playout_cap_randomization && config_.random_playouts_max > config_.random_playouts_min) {
        std::uniform_int_distribution<int> dist(config_.random_playouts_min, config_.random_playouts_max);
        playouts = dist(rng_);
    }

    const int thread_count = std::max(1, config_.num_threads);
    if (thread_count <= 1) {
        for (int i = 0; i < playouts; ++i) {
            run_simulation(board, rng_);
        }
    } else {
        std::atomic<int> counter{0};
        std::vector<std::thread> workers;
        workers.reserve(static_cast<std::size_t>(thread_count));
        for (int t = 0; t < thread_count; ++t) {
            const unsigned int seed_offset = static_cast<unsigned int>(t + 1) * 0x9e3779b9u;
            const unsigned int seed = config_.seed ^ seed_offset ^ static_cast<unsigned int>(move_number * 17 + playouts);
            workers.emplace_back([this, &board, playouts, &counter, seed]() {
                std::mt19937 local_rng(seed);
                while (true) {
                    const int idx = counter.fetch_add(1, std::memory_order_relaxed);
                    if (idx >= playouts) {
                        break;
                    }
                    run_simulation(board, local_rng);
                }
            });
        }
        for (auto& worker : workers) {
            worker.join();
        }
    }

    return select_move_from_root(move_number, rng_);
}

void SearchAgent::notify_move(go::Move move, const go::Board& board_after_move, go::Player to_play) {
    const std::uint64_t new_hash = state_key(board_after_move, to_play);

    if (!root_ || !root_ready_) {
        root_hash_ = new_hash;
        root_player_ = to_play;
        root_ready_ = false;
        return;
    }

    const int move_key = move.is_pass() ? -1 : move.vertex;
    std::unique_ptr<Node> next_root;

    auto it = root_->move_to_index.find(move_key);
    if (it != root_->move_to_index.end()) {
        SearchAgent::Child& child = root_->children[it->second];
        if (child.node) {
            next_root = std::move(child.node);
        }
    }

    if (next_root) {
        root_ = std::move(next_root);
        root_->to_play = to_play;
        root_->noise_applied = false;
        root_->virtual_loss_count = 0;
        for (auto& child : root_->children) {
            child.virtual_loss_count = 0;
        }
        root_hash_ = new_hash;
        root_player_ = to_play;
        root_ready_ = true;
    } else {
        root_.reset();
        root_hash_ = new_hash;
        root_player_ = to_play;
        root_ready_ = false;
    }
}

void SearchAgent::reset() {
    root_.reset();
    root_hash_ = 0;
    root_player_ = go::Player::Black;
    root_ready_ = false;
}

void SearchAgent::apply_dirichlet_noise(Node& node, std::mt19937& rng) {
    std::unique_lock<std::mutex> lock(node.mutex);
    if (node.children.empty()) {
        return;
    }

    const float alpha = config_.dirichlet_alpha;
    std::gamma_distribution<float> gamma(alpha, 1.0f);
    std::vector<float> noise(node.children.size(), 0.0f);
    float sum = 0.0f;
    for (float& value : noise) {
        value = gamma(rng);
        sum += value;
    }
    if (sum <= kEpsilon) {
        const float uniform = 1.0f / static_cast<float>(noise.size());
        for (float& value : noise) {
            value = uniform;
        }
    } else {
        for (float& value : noise) {
            value /= sum;
        }
    }

    for (std::size_t i = 0; i < node.children.size(); ++i) {
        node.children[i].prior = node.children[i].prior * (1.0f - config_.dirichlet_epsilon) + config_.dirichlet_epsilon * noise[i];
    }
}

float SearchAgent::run_simulation(const go::Board& root_board, std::mt19937& rng) {
    go::Board board_copy(root_board);
    return simulate(std::move(board_copy), *root_, rng);
}

float SearchAgent::simulate(go::Board board_copy, Node& node, std::mt19937& rng) {
    Node* current = &node;
    std::vector<Node*> path;
    std::vector<int> child_indices;
    path.push_back(current);

    while (true) {
        float expansion_value = 0.0f;
        if (try_expand(*current, board_copy, expansion_value)) {
            backpropagate(path, child_indices, expansion_value);
            return expansion_value;
        }

        {
            std::unique_lock<std::mutex> lock(current->mutex);
            if (current->children.empty()) {
                lock.unlock();
                backpropagate(path, child_indices, 0.0f);
                return 0.0f;
            }
        }

        const int child_index = select_child(*current, rng);
        const std::size_t child_pos = static_cast<std::size_t>(child_index);

        SearchAgent::Child* child_ptr = nullptr;
        {
            std::unique_lock<std::mutex> lock(current->mutex);
            child_ptr = &current->children[child_pos];
            if (!child_ptr->node) {
                child_ptr->node = std::make_unique<Node>();
                child_ptr->node->to_play = go::other(current->to_play);
            }
        }

        go::Move move = child_ptr->move == -1 ? go::Move::Pass() : go::Move(child_ptr->move);
        const bool legal = board_copy.play_move(current->to_play, move);
        if (!legal) {
            std::unique_lock<std::mutex> lock(current->mutex);
            revert_virtual_loss(*current, child_pos);
            child_ptr->prior = 0.0f;
            child_ptr->visit_count = 0;
            child_ptr->value_sum = 0.0f;
            child_ptr->node.reset();
            continue;
        }

        current = child_ptr->node.get();
        child_indices.push_back(child_index);
        path.push_back(current);
    }
}

int SearchAgent::select_child(Node& node, std::mt19937& rng) {
    std::unique_lock<std::mutex> lock(node.mutex);
    const float sqrt_total = std::sqrt(static_cast<float>(node.visit_count) + 1.0f);
    const float parent_q = node.visit_count > 0 ? node.value_sum / static_cast<float>(node.visit_count) : 0.0f;
    float best_score = -std::numeric_limits<float>::infinity();
    std::size_t best_index = 0;

    for (std::size_t idx = 0; idx < node.children.size(); ++idx) {
        SearchAgent::Child& child = node.children[idx];
        float q = child.visit_count > 0 ? child.value_sum / static_cast<float>(child.visit_count)
                                        : parent_q - config_.fpu_reduction;
        q = std::clamp(q, -1.0f, 1.0f);
        const float u = config_.cpuct * child.prior * sqrt_total /
                        (1.0f + static_cast<float>(child.visit_count));
        const float score = q + u;
        const float noisy_score = score + 1e-6f * std::generate_canonical<float, 10>(rng);
        if (noisy_score > best_score) {
            best_score = noisy_score;
            best_index = idx;
        }
    }

    apply_virtual_loss(node, best_index);
    return static_cast<int>(best_index);
}

bool SearchAgent::try_expand(Node& node, const go::Board& board, float& value) {
    {
        std::unique_lock<std::mutex> lock(node.mutex);
        if (node.expanded) {
            return false;
        }
        while (node.expanding) {
            node.cv.wait(lock);
            if (node.expanded) {
                return false;
            }
        }
        node.expanding = true;
    }

    EvaluationResult eval = evaluator_->evaluate(board, node.to_play);
    const std::size_t board_area = board.board_size() * board.board_size();
    const std::size_t expected_policy_size = board_area + 1;

    if (eval.policy.size() != expected_policy_size) {
        eval.policy.assign(expected_policy_size, 1.0f / static_cast<float>(expected_policy_size));
    }

    std::vector<int> legal_moves;
    legal_moves.reserve(board_area + 1);
    std::vector<float> priors;
    priors.reserve(board_area + 1);

    double prior_sum = 0.0;
    for (std::size_t vertex = 0; vertex < board_area; ++vertex) {
        if (board.point_state(vertex) != go::PointState::Empty) {
            continue;
        }
        go::Move move(static_cast<int>(vertex));
        if (!board.is_legal(node.to_play, move)) {
            continue;
        }
        const float prior = std::max(eval.policy[vertex], 0.0f);
        legal_moves.push_back(static_cast<int>(vertex));
        priors.push_back(prior);
        prior_sum += prior;
    }

    const float pass_prior = std::max(eval.policy.back(), 0.0f);
    legal_moves.push_back(-1);
    priors.push_back(pass_prior);
    prior_sum += pass_prior;

    if (prior_sum <= kEpsilon) {
        const float uniform = 1.0f / static_cast<float>(priors.size());
        std::fill(priors.begin(), priors.end(), uniform);
    } else {
        for (float& prior : priors) {
            prior = prior / static_cast<float>(prior_sum);
        }
    }

    std::vector<Child> children;
    std::unordered_map<int, std::size_t> move_to_index;
    children.reserve(legal_moves.size());

    for (std::size_t i = 0; i < legal_moves.size(); ++i) {
        Child child;
        child.move = legal_moves[i];
        child.prior = priors[i];
        child.value_sum = 0.0f;
        child.visit_count = 0;
        child.virtual_loss_count = 0;
        child.node.reset();
        move_to_index[child.move] = i;
        children.push_back(std::move(child));
    }

    {
        std::unique_lock<std::mutex> lock(node.mutex);
        node.children = std::move(children);
        node.move_to_index = std::move(move_to_index);
        node.expanded = true;
        node.noise_applied = false;
        node.expanding = false;
        node.cv.notify_all();
    }

    value = eval.value;
    return true;
}

void SearchAgent::apply_virtual_loss(Node& node, std::size_t child_index) {
    if (!config_.use_virtual_loss) {
        return;
    }
    Child& child = node.children[child_index];
    child.virtual_loss_count += 1;
    child.visit_count += config_.virtual_loss_visits;
    child.value_sum -= config_.virtual_loss;
    node.virtual_loss_count += 1;
    node.visit_count += config_.virtual_loss_visits;
    node.value_sum -= config_.virtual_loss;
}

void SearchAgent::revert_virtual_loss(Node& node, std::size_t child_index) {
    if (!config_.use_virtual_loss) {
        return;
    }
    Child& child = node.children[child_index];
    if (child.virtual_loss_count > 0) {
        child.virtual_loss_count -= 1;
        child.visit_count = std::max(0, child.visit_count - config_.virtual_loss_visits);
        child.value_sum += config_.virtual_loss;
    }
    if (node.virtual_loss_count > 0) {
        node.virtual_loss_count -= 1;
        node.visit_count = std::max(0, node.visit_count - config_.virtual_loss_visits);
        node.value_sum += config_.virtual_loss;
    }
}

void SearchAgent::backpropagate(const std::vector<Node*>& path, const std::vector<int>& child_indices, float value) {
    float current_value = value;
    for (std::size_t idx = path.size(); idx-- > 0;) {
        Node* node = path[idx];
        backpropagate_on_node(*node, current_value);
        if (idx > 0) {
            Node* parent = path[idx - 1];
            const std::size_t child_index = static_cast<std::size_t>(child_indices[idx - 1]);
            backpropagate_on_edge(*parent, child_index, current_value);
        }
        current_value = -current_value;
    }
}

void SearchAgent::backpropagate_on_node(Node& node, float value) {
    std::unique_lock<std::mutex> lock(node.mutex);
    if (config_.use_virtual_loss && node.virtual_loss_count > 0) {
        node.virtual_loss_count -= 1;
        node.visit_count = std::max(0, node.visit_count - config_.virtual_loss_visits);
        node.value_sum += config_.virtual_loss;
    }
    node.visit_count += 1;
    node.value_sum += value;
}

void SearchAgent::backpropagate_on_edge(Node& parent, std::size_t child_index, float value) {
    std::unique_lock<std::mutex> lock(parent.mutex);
    Child& child = parent.children[child_index];
    if (config_.use_virtual_loss && child.virtual_loss_count > 0) {
        child.virtual_loss_count -= 1;
        child.visit_count = std::max(0, child.visit_count - config_.virtual_loss_visits);
        child.value_sum += config_.virtual_loss;
    }
    child.visit_count += 1;
    child.value_sum += value;
}

go::Move SearchAgent::select_move_from_root(int move_number, std::mt19937& rng) const {
    if (!root_ || root_->children.empty()) {
        return go::Move::Pass();
    }

    float temperature = config_.temperature;
    if (move_number >= config_.temperature_move_cutoff) {
        temperature = 0.0f;
    }

    if (temperature <= kEpsilon) {
        int best_index = 0;
        int best_visits = -1;
        for (std::size_t idx = 0; idx < root_->children.size(); ++idx) {
            const SearchAgent::Child& child = root_->children[idx];
            if (child.visit_count > best_visits) {
                best_visits = child.visit_count;
                best_index = static_cast<int>(idx);
            }
        }
        const SearchAgent::Child& best_child = root_->children[static_cast<std::size_t>(best_index)];
        return best_child.move == -1 ? go::Move::Pass() : go::Move(best_child.move);
    }

    std::vector<float> weights;
    weights.reserve(root_->children.size());
    float sum = 0.0f;
    for (const SearchAgent::Child& child : root_->children) {
        const float visit = static_cast<float>(child.visit_count);
        float weight = std::pow(visit + kEpsilon, 1.0f / temperature);
        weights.push_back(weight);
        sum += weight;
    }

    if (sum <= kEpsilon) {
        const float uniform = 1.0f / static_cast<float>(weights.size());
        std::fill(weights.begin(), weights.end(), uniform);
    } else {
        for (float& w : weights) {
            w /= sum;
        }
    }

    std::discrete_distribution<int> dist(weights.begin(), weights.end());
    const int idx = dist(rng);
    const SearchAgent::Child& choice = root_->children[static_cast<std::size_t>(idx)];
    return choice.move == -1 ? go::Move::Pass() : go::Move(choice.move);
}

std::uint64_t SearchAgent::state_key(const go::Board& board, go::Player) const {
    return board.state_key();
}

} // namespace search
