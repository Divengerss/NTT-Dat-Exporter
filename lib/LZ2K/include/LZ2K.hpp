#ifndef LZ2K_HPP_
#define LZ2K_HPP_

#include <vector>
#include <cstddef>
#include "BaseHandler.hpp"

namespace lz2k
{
    class LZ2K : public ntt::BaseHandler
    {
        public:
            explicit LZ2K(const std::vector<std::byte> &buffer);
            ~LZ2K() = default;

            void handle() const override;

        private:
            const std::vector<std::byte> &_compressedBuffer;
    };
} // namespace zipx

#endif // LZ2K_HPP_
