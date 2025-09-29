#pragma once

#include <cstddef>

namespace go {

enum class KoRule {
    PositionalSuperko,
    SimpleKo
};

enum class ScoringRule {
    TrompTaylorArea,
    Territory
};

struct Rules {
    std::size_t board_size = 19;
    double komi = 7.5;
    bool allow_suicide = false;
    KoRule ko_rule = KoRule::PositionalSuperko;
    ScoringRule scoring_rule = ScoringRule::TrompTaylorArea;
};

} // namespace go

