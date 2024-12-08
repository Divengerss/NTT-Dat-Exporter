#include "ZipX.hpp"
#include <spdlog/spdlog.h>

namespace zipx
{
    ZipX::ZipX(const std::vector<std::byte> &buffer)
        : _compressedBuffer(buffer)
    {
    }

    void ZipX::handle() const
    {
        spdlog::info("Handling a ZIPX file with size: {}", _compressedBuffer.size());
    }

} // namespace zipx
