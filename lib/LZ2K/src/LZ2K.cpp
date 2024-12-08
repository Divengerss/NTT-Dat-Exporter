#include "LZ2K.hpp"
#include <spdlog/spdlog.h>

namespace lz2k
{
    LZ2K::LZ2K(const std::vector<std::byte> &buffer)
        : _compressedBuffer(buffer)
    {
    }

    void LZ2K::handle() const
    {
        spdlog::info("Handling a LZ2K file with size: {}", _compressedBuffer.size());
    }

} // namespace zipx
