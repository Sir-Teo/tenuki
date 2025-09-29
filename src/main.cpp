#include "gtp/GTP.hpp"
#include "go/Rules.hpp"
#include "search/Search.hpp"

#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace {

bool read_env_int(const char* name, int& out) {
    const char* value = std::getenv(name);
    if (!value || *value == '\0') {
        return false;
    }

    char* end = nullptr;
    errno = 0;
    const long parsed = std::strtol(value, &end, 10);
    if (end == value || (end && *end != '\0') || errno == ERANGE) {
        return false;
    }
    if (parsed < std::numeric_limits<int>::min() || parsed > std::numeric_limits<int>::max()) {
        return false;
    }

    out = static_cast<int>(parsed);
    return true;
}

void apply_env_overrides(search::SearchConfig& config) {
    int max_playouts = 0;
    if (read_env_int("TENUKI_MAX_PLAYOUTS", max_playouts) && max_playouts > 0) {
        config.max_playouts = max_playouts;
        config.enable_playout_cap_randomization = false;
        config.random_playouts_min = max_playouts;
        config.random_playouts_max = max_playouts;
    }

    int min_playouts = 0;
    int max_random_playouts = 0;
    const bool has_min = read_env_int("TENUKI_RANDOM_PLAYOUTS_MIN", min_playouts) && min_playouts > 0;
    const bool has_max = read_env_int("TENUKI_RANDOM_PLAYOUTS_MAX", max_random_playouts) && max_random_playouts > 0;

    if (has_min || has_max) {
        if (has_min) {
            config.random_playouts_min = min_playouts;
        }
        if (has_max) {
            config.random_playouts_max = max_random_playouts;
        }
        if (config.random_playouts_max < config.random_playouts_min) {
            config.random_playouts_max = config.random_playouts_min;
        }
        config.enable_playout_cap_randomization = true;
    }
}

} // namespace

int main() {
    go::Rules rules;
    rules.board_size = 19;
    go::Board board(rules);

    search::SearchConfig search_config;
    search_config.max_playouts = 160;
    search_config.random_playouts_min = 128;
    search_config.random_playouts_max = 256;
    search_config.dirichlet_epsilon = 0.1f;

    apply_env_overrides(search_config);

    auto evaluator = search::make_uniform_evaluator();

    gtp::Server server(std::move(board), std::cin, std::cout, search_config, evaluator);
    server.run();
    return 0;
}
