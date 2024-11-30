#ifndef BYTESWAP_HPP
#define BYTESWAP_HPP

#include <cstddef>
#include <utility>

namespace utils
{
    struct PairHash {
        template <typename T1, typename T2>
        std::size_t operator()(const std::pair<T1, T2>& pair) const {
            auto hash1 = std::hash<T1>{}(pair.first);
            auto hash2 = std::hash<T2>{}(pair.second);
            return hash1 ^ (hash2 + 0x9e3779b9 + (hash1 << 6) + (hash1 >> 2));
        }
    };

    template <typename T>
    static T byteswap(T value) {
        static_assert(std::is_integral<T>::value, "T must be an integral type");
        static_assert(sizeof(T) <= 8, "T must be at most 64 bits");

        if constexpr (sizeof(T) == 1) {
            return value; // No swapping needed for 8-bit integers
        } else if constexpr (sizeof(T) == 2) {
            return (T)((value & 0x00FF) << 8 |
                    (value & 0xFF00) >> 8);
        } else if constexpr (sizeof(T) == 4) {
            return (T)((value & 0x000000FF) << 24 |
                    (value & 0x0000FF00) << 8  |
                    (value & 0x00FF0000) >> 8  |
                    (value & 0xFF000000) >> 24);
        } else if constexpr (sizeof(T) == 8) {
            return (T)((value & 0x00000000000000FFULL) << 56 |
                    (value & 0x000000000000FF00ULL) << 40 |
                    (value & 0x0000000000FF0000ULL) << 24 |
                    (value & 0x00000000FF000000ULL) << 8  |
                    (value & 0x000000FF00000000ULL) >> 8  |
                    (value & 0x0000FF0000000000ULL) >> 24 |
                    (value & 0x00FF000000000000ULL) >> 40 |
                    (value & 0xFF00000000000000ULL) >> 56);
        }
        return value; // Fallback, should never be reached
    }

    static const bool isLittleEndian() {
        std::uint16_t test = 1;
        return *reinterpret_cast<std::uint8_t*>(&test) == 1;
    }
} // namespace utils

#endif // BYTESWAP_HPP

