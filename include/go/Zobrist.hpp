#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

class ZobristTable {
public:
    ZobristTable() = default;
    explicit ZobristTable(std::size_t board_size);

    std::uint64_t black_stone_hash(std::size_t vertex) const;
    std::uint64_t white_stone_hash(std::size_t vertex) const;
    std::uint64_t ko_hash(std::size_t vertex) const;
    std::uint64_t side_to_move_hash() const noexcept { return side_to_move_; }

    std::size_t size() const noexcept { return board_size_; }

private:
    std::size_t board_size_ = 0;
    std::vector<std::uint64_t> black_hashes_;
    std::vector<std::uint64_t> white_hashes_;
    std::vector<std::uint64_t> ko_hashes_;
    std::uint64_t side_to_move_ = 0;
};

