#ifndef FILESCHUNK_HPP
#define FILESCHUNK_HPP

#include <cstddef>
#include <cstdint>
#include <vector>
#include <stdexcept>
#include "spdlog/spdlog.h"
#include "Utils/ByteSwap.hpp"

namespace ntt
{
    class FilesChunk
    {
        public:
            FilesChunk(const std::vector<std::byte> &_fileBuffer, const std::size_t &_fileBufferSize);
            ~FilesChunk();

            void setChunkHeader(const ptrdiff_t headerOffset);
            void parseChunk();

            void addFile(const std::uint16_t parentId, const std::uint16_t id, const std::string &fileName, const std::uint64_t addr);

        private:
            const std::vector<std::byte> &_fileBuffer;
            const std::size_t &_fileBufferSize;
            struct FileInfo {
                bool _isDir;
                std::uint16_t _parentDirId;
                std::uint16_t _dirId;
                std::string _pathName;
                std::string _fileName;
                std::uint64_t _dataAddr;

                bool operator==(const std::uint16_t parentDirId) const {
                    return _parentDirId == parentDirId;
                }
            };

            std::size_t _headerOffset;
            std::uint32_t _chunkSize;
            std::uint32_t _archiveRemainingSize; // The EOF offset from curr offset
            std::uint32_t _ChunkVersion;
            std::uint32_t _FileCount;
            std::vector<FileInfo> _files;
    };

    FilesChunk::FilesChunk(const std::vector<std::byte> &fileBuffer, const std::size_t &fileBufferSize) :
        _fileBuffer(fileBuffer), _fileBufferSize(fileBufferSize)
    {
    }

    FilesChunk::~FilesChunk()
    {
    }

    void FilesChunk::setChunkHeader(const ptrdiff_t headerOffset)
    {
        _headerOffset = headerOffset;

        if (_headerOffset < 4 || _headerOffset > _fileBufferSize) {
            throw std::out_of_range("Invalid offset provided!");
        }
        std::memcpy(&_archiveRemainingSize, &_fileBuffer[_headerOffset - 0x4], sizeof(std::uint32_t));
        std::memcpy(&_ChunkVersion, &_fileBuffer[_headerOffset + 0xC], sizeof(std::uint32_t));
        std::memcpy(&_FileCount, &_fileBuffer[_headerOffset + 0x10], sizeof(std::uint32_t));
        std::memcpy(&_chunkSize, &_fileBuffer[_headerOffset + 0x18], sizeof(std::uint32_t));

        if (utils::isLittleEndian()) {
            _archiveRemainingSize = utils::byteswap(_archiveRemainingSize);
            _chunkSize = utils::byteswap(_chunkSize);
            _ChunkVersion = utils::byteswap(_ChunkVersion);
            _FileCount = utils::byteswap(_FileCount);
        }

        spdlog::info("Remaining chunk size {:X}", _chunkSize);
    }

    void FilesChunk::parseChunk()
    {
        std::uint32_t readIndex = 0u;
        // std::pair<std::uint64_t, std::uint64_t> defaultKey = {0ull, 0ull}; // NOT IMPLEMENTED
        std::string fileName;
        std::uint32_t fileIndex = 1u;

        std::uint32_t begDummyId = 0;
        std::uint32_t fileNameOffset = 1;
        std::uint16_t fileDirectoryId = 0;
        std::uint16_t someDummyId = 0;
        std::uint16_t someId = 0;
        std::uint16_t fileId = 0;

        std::memcpy(&begDummyId, &_fileBuffer[_headerOffset + 0x1C + _chunkSize + (fileIndex * 0x4)], sizeof(std::int32_t));
        while (readIndex < _chunkSize - 0x2) {
            if (static_cast<char>(_fileBuffer[_headerOffset + 0x1C + readIndex]) != '\0') {
                fileName.push_back(static_cast<char>(_fileBuffer[_headerOffset + 0x1C + readIndex]));
            } else {
                if (!fileName.empty()) {
                    std::memcpy(&fileNameOffset, &_fileBuffer[_headerOffset + 0x1C + _chunkSize + (fileIndex * 0xC) + 0x4], sizeof(std::uint32_t));
                    std::memcpy(&fileDirectoryId, &_fileBuffer[_headerOffset + 0x1C + _chunkSize + (fileIndex * 0xC) + 0x8], sizeof(std::uint16_t));
                    std::memcpy(&someDummyId, &_fileBuffer[_headerOffset + 0x1C + _chunkSize + (fileIndex * 0xC) + 0xA], sizeof(std::uint16_t));
                    std::memcpy(&someId, &_fileBuffer[_headerOffset + 0x1C + _chunkSize + (fileIndex * 0xC) + 0xC], sizeof(std::uint16_t));
                    std::memcpy(&fileId, &_fileBuffer[_headerOffset + 0x1C + _chunkSize + (fileIndex * 0xC) + 0xE], sizeof(std::uint16_t));
                    if (utils::isLittleEndian()) {
                        fileNameOffset = utils::byteswap(fileNameOffset);
                        fileDirectoryId = utils::byteswap(fileDirectoryId);
                        someDummyId = utils::byteswap(someDummyId);
                        someId = utils::byteswap(someId);
                        fileId = utils::byteswap(fileId);
                    }
                    // _datFilesData[{readIndex, 0}] = fileName;
                    addFile(fileDirectoryId, fileIndex, fileName, 0x0ULL /* ADDR NOT IMPLEMENTED */);
                    // spdlog::info("{:08X} {:08X} {:08X} {:08X} {:08X} {:08X} {}", fileIndex, fileNameOffset, fileDirectoryId, someDummyId, someId, fileId, fileName);
                    fileName.clear();
                    fileIndex += 1;
                } else {
                    spdlog::warn("The extracted file name was empty");
                }
                readIndex += 0x1;
            }
            readIndex += 0x1;
        }
        spdlog::info("Extracted {} files successfully", _FileCount);
    }

    void FilesChunk::addFile(const std::uint16_t parentId, const std::uint16_t id, const std::string &fileName, const std::uint64_t addr)
    {
        bool isDir = false;
        std::string pathName;

        if (fileName.find('.') == std::string::npos) {
            isDir = true;
        }

        std::uint16_t currentParentId = parentId;
        std::vector<std::string> pathComponents;

        while (currentParentId != 0) {
            auto it = std::find_if(_files.begin(), _files.end(), [currentParentId](const FileInfo &file) {
                return file._dirId == currentParentId;
            });

            if (it != _files.end()) {
                pathComponents.push_back(it->_fileName);
                currentParentId = it->_parentDirId;
            } else {
                break;
            }
        }
        std::reverse(pathComponents.begin(), pathComponents.end());
        for (const auto& component : pathComponents) {
            pathName += component + "/";
        }
        pathName += fileName;
        spdlog::info("{:#016x} {}", addr, pathName);
        _files.push_back({isDir, parentId, id, pathName, fileName, addr});
    }

} // namespace nxg


#endif // FILESCHUNK_HPP