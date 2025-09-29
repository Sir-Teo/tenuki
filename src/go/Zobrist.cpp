#include "go/Zobrist.hpp"

#include <random>

namespace {

std::mt19937_64& rng() {
    static std::mt19937_64 gen(0x5eedbadull);
    return gen;
}

std::uint64_t random_uint64() {
    static std::uniform_int_distribution<std::uint64_t> dist;
    return dist(rng());
}

} // namespace

ZobristTable::ZobristTable(std::size_t board_size) : board_size_(board_size) {
    const std::size_t total = board_size_ * board_size_;
    black_hashes_.resize(total);
    white_hashes_.resize(total);
    ko_hashes_.resize(total);

    for (std::size_t i = 0; i < total; ++i) {
        black_hashes_[i] = random_uint64();
        white_hashes_[i] = random_uint64();
        ko_hashes_[i] = random_uint64();
    }

    side_to_move_ = random_uint64();
}

std::uint64_t ZobristTable::black_stone_hash(std::size_t vertex) const {
    return black_hashes_.at(vertex);
}

std::uint64_t ZobristTable::white_stone_hash(std::size_t vertex) const {
    return white_hashes_.at(vertex);
}

std::uint64_t ZobristTable::ko_hash(std::size_t vertex) const {
    return ko_hashes_.at(vertex);
}

