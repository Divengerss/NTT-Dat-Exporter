#ifndef FILESCHUNK_HPP
#define FILESCHUNK_HPP

#include <cstddef>
#include <cstdint>
#include <vector>
#include <stdexcept>
#include <filesystem>
#include <fstream>
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
            void getFilesOffset();

            void addFile(bool isDir, const std::uint16_t parentId, const std::uint16_t id, const std::string &fileName, const std::uint32_t addr);

            void createFiles(bool isDir, const std::string &path) const;

        private:
            const std::vector<std::byte> &_fileBuffer;
            const std::size_t &_fileBufferSize;
            struct FileInfo {
                bool _isDir;
                std::uint16_t _parentDirId;
                std::uint16_t _dirId;
                std::string _pathName;
                std::string _fileName;
                std::uint32_t _dataAddr;

                bool operator==(const std::uint16_t parentDirId) const {
                    return _parentDirId == parentDirId;
                }
            };

            std::size_t _headerOffset;
            std::uint32_t _chunkSize;
            std::uint32_t _archiveRemainingSize; // The EOF offset from curr offset
            std::uint32_t _ChunkVersion;
            std::uint32_t _FileCount;
            std::uint32_t _DirCount;
            std::vector<FileInfo> _files;
    };

    FilesChunk::FilesChunk(const std::vector<std::byte> &fileBuffer, const std::size_t &fileBufferSize) :
        _fileBuffer(fileBuffer), _fileBufferSize(fileBufferSize), _headerOffset(0ull), _chunkSize(0u), _archiveRemainingSize(0u), _ChunkVersion(0u), _FileCount(0u), _DirCount(0u), _files({})
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
        std::string fileName;
        std::uint32_t fileIndex = 1u;

        std::uint32_t begDummyId = 0u;
        std::uint32_t fileNameOffset = 1u;
        std::uint16_t fileDirectoryId = 0u;
        std::uint16_t someDummyId = 0u;
        std::uint16_t someId = 0u;
        std::uint16_t fileId = 0u;
        std::uint32_t fileAddr = 0ull;

        bool isDir = false;

        std::memcpy(&begDummyId, &_fileBuffer[_headerOffset + 0x1C + _chunkSize + (fileIndex * 0x4)], sizeof(std::int32_t));
        while (readIndex < _chunkSize - 0x2) {
            if (static_cast<char>(_fileBuffer[_headerOffset + 0x1C + readIndex]) != '\0') {
                fileName.push_back(static_cast<char>(_fileBuffer[_headerOffset + 0x1C + readIndex]));
            } else {
                if (!fileName.empty()) {
                    if (fileName.find('.') == std::string::npos) {
                        isDir = true;
                        fileAddr = 0x0u;
                        _DirCount += 1u;
                    } 
                    // else {
                        // std::memcpy(&fileAddr, &_fileBuffer[_headerOffset + 0x1C + _chunkSize + 0x4AEF0], sizeof(std::uint64_t));
                        // spdlog::warn("{:X} {}", _headerOffset + 0x1C + _chunkSize + 0x10 + (0xC * (_FileCount + 1067)), _FileCount);
                    // }
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
                        if (fileAddr != 0x0u)
                            fileAddr = utils::byteswap(fileAddr);
                    }
                    addFile(isDir, fileDirectoryId, fileIndex, fileName, fileAddr);
                    // spdlog::info("{:08X} {:08X} {:08X} {:08X} {:08X} {:08X} {}", fileIndex, fileNameOffset, fileDirectoryId, someDummyId, someId, fileId, fileName);
                    fileName.clear();
                    fileIndex += 1;
                } else {
                    spdlog::warn("The file name was empty");
                }
                readIndex += 0x1;
            }
            readIndex += 0x1;
            isDir = false;
        }
        spdlog::info("Found {} files", _FileCount);
    }

    void FilesChunk::getFilesOffset()
    {
        for (std::size_t fileIndex = 0ull; fileIndex < _files.size(); ++fileIndex)
        {
            std::memcpy(&_files[fileIndex]._dataAddr, &_fileBuffer[_headerOffset + 0x1C + _chunkSize + 0x10 + 0xC * (_FileCount + _DirCount) + (fileIndex * 0xC)], sizeof(std::uint32_t));
            if (utils::isLittleEndian()) {
                _files[fileIndex]._dataAddr = utils::byteswap(_files[fileIndex]._dataAddr);
            }
            if (!_files[fileIndex]._isDir)
                spdlog::info("{:08x} {}", _files[fileIndex]._dataAddr, _files[fileIndex]._pathName);
            // spdlog::info("{:08x} {}", _headerOffset + 0x1C + _chunkSize + 0x10 + 0xC * (_FileCount + _DirCount) + (fileIndex * 0xC), _files[fileIndex]._pathName);
        }
    }

    void FilesChunk::addFile(bool isDir, const std::uint16_t parentId, const std::uint16_t id, const std::string &fileName, const std::uint32_t addr)
    {
        std::string pathName;
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
        // if (!isDir)
        //     spdlog::info("{:#016x} {}", addr, pathName);
        // createFiles(isDir, pathName);
        _files.push_back({isDir, parentId, id, pathName, fileName, addr});
    }

    void FilesChunk::createFiles(bool isDir, const std::string &path) const
    {
        std::error_code errCode;
        std::string relativePath = "./Content/" + path;

        if (std::filesystem::exists(relativePath)) {
            return;
        } else if (isDir && !std::filesystem::create_directories(relativePath)) {
            spdlog::error("Could not create the files/directories for {}: {}", path, errCode.message());
            errCode.clear();
        } else if (!isDir) {
            std::ofstream file(relativePath);
            file.close();
        }
    }

} // namespace nxg


#endif // FILESCHUNK_HPP