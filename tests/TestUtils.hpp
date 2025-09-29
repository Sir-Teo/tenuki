#pragma once

#include <cmath>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace tenuki::test {

template <typename T>
concept Streamable = requires(std::ostream& os, const T& value) {
    os << value;
};

template <typename T>
std::string repr(const T& value) {
    if constexpr (std::is_same_v<T, bool>) {
        return value ? "true" : "false";
    } else if constexpr (std::is_enum_v<T>) {
        using Underlying = std::underlying_type_t<T>;
        std::ostringstream oss;
        oss << static_cast<Underlying>(value);
        return oss.str();
    } else if constexpr (Streamable<T>) {
        std::ostringstream oss;
        oss << value;
        return oss.str();
    } else {
        return "<unprintable>";
    }
}

inline void expect(bool condition, const char* expr, const char* file, int line) {
    if (!condition) {
        std::ostringstream oss;
        oss << "Expectation failed: " << expr << " at " << file << ":" << line;
        throw std::runtime_error(oss.str());
    }
}

template <typename Lhs, typename Rhs>
inline void expect_eq(const Lhs& lhs,
                      const Rhs& rhs,
                      const char* lhs_expr,
                      const char* rhs_expr,
                      const char* file,
                      int line) {
    if (!(lhs == rhs)) {
        std::ostringstream oss;
        oss << "Expectation failed: " << lhs_expr << " == " << rhs_expr
            << " (" << repr(lhs) << " vs " << repr(rhs) << ") at "
            << file << ":" << line;
        throw std::runtime_error(oss.str());
    }
}

template <typename Lhs, typename Rhs>
inline void expect_ne(const Lhs& lhs,
                      const Rhs& rhs,
                      const char* lhs_expr,
                      const char* rhs_expr,
                      const char* file,
                      int line) {
    if (lhs == rhs) {
        std::ostringstream oss;
        oss << "Expectation failed: " << lhs_expr << " != " << rhs_expr
            << " (both " << repr(lhs) << ") at " << file << ":" << line;
        throw std::runtime_error(oss.str());
    }
}

template <typename Lhs, typename Rhs>
inline void expect_near(const Lhs& lhs,
                        const Rhs& rhs,
                        double tolerance,
                        const char* lhs_expr,
                        const char* rhs_expr,
                        const char* file,
                        int line) {
    const double diff = std::fabs(static_cast<double>(lhs) - static_cast<double>(rhs));
    if (diff > tolerance) {
        std::ostringstream oss;
        oss << "Expectation failed: |" << lhs_expr << " - " << rhs_expr << "| <= "
            << tolerance << " (diff=" << diff << ") at " << file << ":" << line;
        throw std::runtime_error(oss.str());
    }
}

} // namespace tenuki::test

#define TENUKI_EXPECT(expr) \
    ::tenuki::test::expect(static_cast<bool>(expr), #expr, __FILE__, __LINE__)

#define TENUKI_EXPECT_FALSE(expr) \
    ::tenuki::test::expect(!(expr), "!(" #expr ")", __FILE__, __LINE__)

#define TENUKI_EXPECT_EQ(lhs, rhs) \
    ::tenuki::test::expect_eq((lhs), (rhs), #lhs, #rhs, __FILE__, __LINE__)

#define TENUKI_EXPECT_NE(lhs, rhs) \
    ::tenuki::test::expect_ne((lhs), (rhs), #lhs, #rhs, __FILE__, __LINE__)

#define TENUKI_EXPECT_NEAR(lhs, rhs, tolerance) \
    ::tenuki::test::expect_near((lhs), (rhs), (tolerance), #lhs, #rhs, __FILE__, __LINE__)

