#ifndef FILESCHUNK_HPP
#define FILESCHUNK_HPP

#include <cstddef>
#include <cstdint>
#include <vector>
#include <stdexcept>
#include <filesystem>
#include <fstream>
#include "spdlog/spdlog.h"
#include "Utils/Utils.hpp"

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

            void defineCRCdatabase();

            void computeCRC();

        private:
            const std::vector<std::byte> &_fileBuffer;
            const std::size_t &_fileBufferSize;
            struct FileInfo {
                bool _isDir;
                std::uint16_t _parentDirId;
                std::uint16_t _dirId;
                std::string _pathName;
                std::string _fileName;
                std::string _crcPath;
                std::uint32_t _dataAddr;
                std::uint32_t _fileSize;
                std::uint32_t _fileZsize;
                std::uint32_t _packedVer;
                std::uint32_t _crcValue;

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
            std::size_t _filesChunkOffset;
            std::vector<std::uint32_t> _crcDatabase;

            std::string _normalizeFilename(const std::string &fullname) const;
    };

    FilesChunk::FilesChunk(const std::vector<std::byte> &fileBuffer, const std::size_t &fileBufferSize) :
        _fileBuffer(fileBuffer), _fileBufferSize(fileBufferSize), _headerOffset(0ull), _chunkSize(0u), _archiveRemainingSize(0u), _ChunkVersion(0u), _FileCount(0u), _DirCount(0u), _files({}), _filesChunkOffset(0ull), _crcDatabase({})
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
        _archiveRemainingSize = utils::assignFromMemory(_archiveRemainingSize, _fileBuffer[_headerOffset - 0x4], sizeof(std::uint32_t), true);
        _ChunkVersion = utils::assignFromMemory(_ChunkVersion, _fileBuffer[_headerOffset + 0xC], sizeof(std::uint32_t), true);
        _FileCount = utils::assignFromMemory(_FileCount, _fileBuffer[_headerOffset + 0x10], sizeof(std::uint32_t), true);
        _chunkSize = utils::assignFromMemory(_chunkSize, _fileBuffer[_headerOffset + 0x18], sizeof(std::uint32_t), true);

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

        begDummyId = utils::assignFromMemory(begDummyId, _fileBuffer[_headerOffset + 0x1C + _chunkSize + (fileIndex * 0x4)], sizeof(std::uint32_t), false);
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
                    fileNameOffset = utils::assignFromMemory(fileNameOffset, _fileBuffer[_headerOffset + 0x1C + _chunkSize + (fileIndex * 0xC) + 0x4], sizeof(std::uint32_t), true);
                    fileDirectoryId = utils::assignFromMemory(fileDirectoryId, _fileBuffer[_headerOffset + 0x1C + _chunkSize + (fileIndex * 0xC) + 0x8], sizeof(std::uint16_t), true);
                    someDummyId = utils::assignFromMemory(someDummyId, _fileBuffer[_headerOffset + 0x1C + _chunkSize + (fileIndex * 0xC) + 0xA], sizeof(std::uint16_t), true);
                    someId = utils::assignFromMemory(someId, _fileBuffer[_headerOffset + 0x1C + _chunkSize + (fileIndex * 0xC) + 0xC], sizeof(std::uint16_t), true);
                    fileId = utils::assignFromMemory(fileId, _fileBuffer[_headerOffset + 0x1C + _chunkSize + (fileIndex * 0xC) + 0xE], sizeof(std::uint16_t), true);

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
        _filesChunkOffset = _headerOffset + 0x1C + _chunkSize + 0x10 + 0xC * (_FileCount + _DirCount);
        std::size_t fileOffset = 0ull;

        std::uint32_t typeBOH = 0u;
        std::uint32_t fileCount2 = 0u; // Data from the archive

        typeBOH = utils::assignFromMemory(typeBOH, _fileBuffer[_filesChunkOffset], sizeof(std::uint32_t), true);
        fileCount2 = utils::assignFromMemory(fileCount2, _fileBuffer[_filesChunkOffset + 0x4], sizeof(std::uint32_t), true);
        
        if (_FileCount != fileCount2)
            spdlog::warn("The number of files read from the archive differ from last check.");

        for (std::size_t fileIndex = 0ull; fileIndex < _FileCount + _DirCount; ++fileIndex)
        {
            if (!_files[fileIndex]._isDir) {
                _files[fileIndex]._packedVer = utils::assignFromMemory(_files[fileIndex]._packedVer, _fileBuffer[_filesChunkOffset + 0x8 + (fileOffset * 0x10)], sizeof(std::uint32_t), true);
                _files[fileIndex]._dataAddr = utils::assignFromMemory(_files[fileIndex]._dataAddr, _fileBuffer[_filesChunkOffset + 0xC + (fileOffset * 0x10)], sizeof(std::uint32_t), true);
                _files[fileIndex]._fileZsize = utils::assignFromMemory(_files[fileIndex]._fileZsize, _fileBuffer[_filesChunkOffset + 0x10 + (fileOffset * 0x10)], sizeof(std::uint32_t), true);
                _files[fileIndex]._fileSize = utils::assignFromMemory(_files[fileIndex]._fileSize, _fileBuffer[_filesChunkOffset + 0x14 + (fileOffset * 0x10)], sizeof(std::uint32_t), true);
                fileOffset += 1;
                // spdlog::info("{:08x} {}", _files[fileIndex]._dataAddr, _files[fileIndex]._pathName);
            }
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
        _files.push_back({isDir, parentId, id, pathName, fileName, pathName, addr, 0x0u});
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

    void FilesChunk::defineCRCdatabase()
    {
        std::size_t chunkOffset = _filesChunkOffset + (_FileCount * 0x10) + 0x8;
        std::uint32_t crc = 0u;

        spdlog::info("offset = {:08x}", chunkOffset);
        for (std::uint32_t fileIndex = 0ull; fileIndex < _FileCount; ++fileIndex)
        {
            crc = utils::assignFromMemory(crc, _fileBuffer[chunkOffset + fileIndex * 0x4], sizeof(std::uint32_t), true);
            // spdlog::info("crc {:08x}", crc);
            _crcDatabase.push_back(crc);
        }
    }

    std::string FilesChunk::_normalizeFilename(const std::string &fullname) const {
        std::string normalized = fullname;

        std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](char c) {
            if (c == '/') {
                return '\\';
            }
            return static_cast<char>(std::toupper(c));
        });

        return normalized;
    }

    void FilesChunk::computeCRC() // FNV-a1
    {
        const std::uint32_t CRC_FNV_OFFSET = 0x811c9dc5;
        const std::uint32_t CRC_FNV_PRIME = 0x199933;
        std::uint32_t crc = CRC_FNV_OFFSET;
        std::ptrdiff_t idx = 0;

        for (std::uint32_t fileIndex = 0ull; fileIndex < _FileCount; ++fileIndex)
        {
            crc = CRC_FNV_OFFSET;
            if (!_files[fileIndex]._isDir) {
                _files[fileIndex]._crcPath = _normalizeFilename(_files[fileIndex]._pathName);
                for (char c : _files[fileIndex]._crcPath) {
                    crc ^= static_cast<std::uint8_t>(c);
                    crc *= CRC_FNV_PRIME;
                }
                _files[fileIndex]._crcValue = crc;

                auto it = std::find(_crcDatabase.begin(), _crcDatabase.end(), _files[fileIndex]._crcValue);
                if (it != _crcDatabase.end()) {
                    idx = std::distance(_crcDatabase.begin(), it);
                } else {
                    spdlog::warn("The CRC of the file {} has not been found.", _files[fileIndex]._crcPath);
                }
                spdlog::info("CRC = {:08x} {} {:08x} {}", _files[fileIndex]._crcValue, idx, _files[idx]._dataAddr, _files[fileIndex]._crcPath);
            }
        }

    }

} // namespace nxg

#endif // FILESCHUNK_HPP