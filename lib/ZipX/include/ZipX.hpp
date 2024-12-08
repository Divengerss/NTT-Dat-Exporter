#ifndef ZIPX_HPP_
#define ZIPX_HPP_

#include <vector>
#include <cstddef>
#include "BaseHandler.hpp"
#include "zlib.h"

namespace zipx
{
    class ZipX : public ntt::BaseHandler
    {
        public:
            explicit ZipX(const std::vector<std::byte> &buffer);
            ~ZipX() = default;

            void handle() const override;

        private:
            const std::vector<std::byte> &_compressedBuffer;
    };
} // namespace zipx

#endif // ZIPX_HPP_
