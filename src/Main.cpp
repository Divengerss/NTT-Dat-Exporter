#include "DAT/Dat.hpp"
#include "spdlog/spdlog.h"

int main(int argc, const char *argv[]) {
    if (argc < 2) {
        spdlog::error("Error: No file provided at command line.");
        return 1;
    }

    try {
        for (std::size_t fileIndex = 1; fileIndex < argc; ++fileIndex) {
            ntt::Dat datFile(argv[fileIndex]);

            datFile.readMagicHeader();
            std::ptrdiff_t offset = datFile.findFileListChunkOffset(".CC40TAD");

            datFile.setFilesChunkHeader(offset);
            datFile.parseFilesChunk();
        }
        return 0;
    } catch (const std::ios_base::failure &e) {
        spdlog::error("Error: {}", e.what());
    } catch (const std::out_of_range &e) {
        spdlog::error("Error: {}", e.what());
    }
    return 1;
}