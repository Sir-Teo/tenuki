#include "go/Board.hpp"
#include "search/Search.hpp"

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct Options {
    std::size_t board_size = 19;
    int playouts = 512;
    int iterations = 16;
    unsigned int seed = 0x5eed1234u;
    std::vector<int> thread_counts{1, 2, 4};
};

bool parse_int(const char* value, int& out) {
    char* end = nullptr;
    long parsed = std::strtol(value, &end, 10);
    if (end == value || *end != '\0') {
        return false;
    }
    if (parsed < std::numeric_limits<int>::min() || parsed > std::numeric_limits<int>::max()) {
        return false;
    }
    out = static_cast<int>(parsed);
    return true;
}

bool parse_size_t(const char* value, std::size_t& out) {
    char* end = nullptr;
    unsigned long parsed = std::strtoul(value, &end, 10);
    if (end == value || *end != '\0') {
        return false;
    }
    out = static_cast<std::size_t>(parsed);
    return true;
}

bool parse_threads(const char* value, std::vector<int>& out) {
    std::vector<int> counts;
    std::istringstream ss(value);
    std::string token;
    while (std::getline(ss, token, ',')) {
        if (token.empty()) {
            continue;
        }
        int parsed = 0;
        if (!parse_int(token.c_str(), parsed) || parsed <= 0) {
            return false;
        }
        counts.push_back(parsed);
    }
    if (counts.empty()) {
        return false;
    }
    out = std::move(counts);
    return true;
}

Options parse_options(int argc, char** argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];
        if (std::strcmp(arg, "--board-size") == 0 && i + 1 < argc) {
            std::size_t value = 0;
            if (!parse_size_t(argv[++i], value) || value == 0) {
                throw std::invalid_argument("Invalid value for --board-size");
            }
            options.board_size = value;
        } else if (std::strcmp(arg, "--playouts") == 0 && i + 1 < argc) {
            int value = 0;
            if (!parse_int(argv[++i], value) || value <= 0) {
                throw std::invalid_argument("Invalid value for --playouts");
            }
            options.playouts = value;
        } else if (std::strcmp(arg, "--iterations") == 0 && i + 1 < argc) {
            int value = 0;
            if (!parse_int(argv[++i], value) || value <= 0) {
                throw std::invalid_argument("Invalid value for --iterations");
            }
            options.iterations = value;
        } else if (std::strcmp(arg, "--seed") == 0 && i + 1 < argc) {
            int value = 0;
            if (!parse_int(argv[++i], value)) {
                throw std::invalid_argument("Invalid value for --seed");
            }
            options.seed = static_cast<unsigned int>(value);
        } else if (std::strcmp(arg, "--threads") == 0 && i + 1 < argc) {
            if (!parse_threads(argv[++i], options.thread_counts)) {
                throw std::invalid_argument("Invalid value for --threads");
            }
        } else if (std::strcmp(arg, "--help") == 0) {
            throw std::invalid_argument("");
        } else {
            std::ostringstream oss;
            oss << "Unknown option: " << arg;
            throw std::invalid_argument(oss.str());
        }
    }
    return options;
}

void print_usage() {
    std::cout << "Usage: search_benchmark [options]\n"
              << "  --board-size N      Board size (default 19)\n"
              << "  --playouts N        Playouts per search (default 512)\n"
              << "  --iterations N      Number of searches per measurement (default 16)\n"
              << "  --threads a,b,c     Comma separated thread counts (default 1,2,4)\n"
              << "  --seed N            RNG seed (default 0x5eed1234)\n";
}

} // namespace

int main(int argc, char** argv) {
    Options options;
    try {
        options = parse_options(argc, argv);
    } catch (const std::invalid_argument& ex) {
        if (std::strlen(ex.what()) > 0) {
            std::cerr << ex.what() << "\n";
        }
        print_usage();
        return std::strlen(ex.what()) > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
    }

    go::Rules rules;
    rules.board_size = options.board_size;
    go::Board board(rules);

    std::cout << "# Tenuki Search Benchmark\n";
    std::cout << "# board_size=" << options.board_size
              << " playouts=" << options.playouts
              << " iterations=" << options.iterations
              << " seed=" << options.seed << "\n";
    std::cout << "threads,seconds,total_playouts,playouts_per_second\n";

    for (int thread_count : options.thread_counts) {
        search::SearchConfig config;
        config.max_playouts = options.playouts;
        config.enable_playout_cap_randomization = false;
        config.dirichlet_epsilon = 0.0f;
        config.temperature = 0.0f;
        config.temperature_move_cutoff = 0;
        config.num_threads = thread_count;
        config.seed = options.seed;

        search::SearchAgent agent(config, search::make_uniform_evaluator());

        auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < options.iterations; ++i) {
            agent.reset();
            board.clear();
            board.set_to_play(go::Player::Black);
            agent.select_move(board, board.to_play(), 0);
        }
        auto end = std::chrono::steady_clock::now();

        std::chrono::duration<double> elapsed = end - start;
        const double total_playouts = static_cast<double>(options.iterations) * static_cast<double>(options.playouts);
        const double seconds = elapsed.count();
        const double playouts_per_second = seconds > 0.0 ? total_playouts / seconds : 0.0;

        std::cout << thread_count << ','
                  << std::fixed << std::setprecision(6) << seconds << ','
                  << static_cast<long long>(total_playouts) << ','
                  << std::setprecision(2) << std::fixed << playouts_per_second << '\n';
    }

    return EXIT_SUCCESS;
}

