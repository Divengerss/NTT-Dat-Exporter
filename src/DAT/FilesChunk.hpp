#ifndef FILESCHUNK_HPP
#define FILESCHUNK_HPP

#include <cstddef>
#include <cstdint>
#include <vector>
#include <stdexcept>
#include <filesystem>
#include <fstream>
#include <array>
#include "spdlog/spdlog.h"
#include "Utils/Utils.hpp"
#include "BaseHandler.hpp"
#include "ZipX.hpp"
#include "LZ2K.hpp"

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
            void defineCRCdatabase();
            void computeCRC();
            void readFilesOffsetBuffer();
            void decompressFiles();

        private:
            const std::vector<std::byte> &_fileBuffer;
            const std::size_t &_fileBufferSize;
            struct CRCInfo {
                std::uint32_t _dataAddr;
                std::uint32_t _fileSize;
                std::uint32_t _fileZsize;
                std::uint32_t _packedVer;
                std::string _crcPath;
                std::uint32_t _crcValue;
            };
            struct FileInfo {
                bool _isDir;
                std::uint16_t _parentDirId;
                std::uint16_t _dirId;
                std::string _pathName;
                std::string _fileName;
                CRCInfo _CRC;
                std::vector<std::byte> _dataBuffer;

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
            std::vector<CRCInfo> _CRCs;

            std::string _normalizeFilename(const std::string &fullname) const;
            void _createFile(FileInfo &fileInfo) const;
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
                    fileNameOffset = utils::assignFromMemory(fileNameOffset, _fileBuffer[_headerOffset + 0x1C + _chunkSize + (fileIndex * 0xC) + 0x4], sizeof(std::uint32_t), true);
                    fileDirectoryId = utils::assignFromMemory(fileDirectoryId, _fileBuffer[_headerOffset + 0x1C + _chunkSize + (fileIndex * 0xC) + 0x8], sizeof(std::uint16_t), true);
                    someDummyId = utils::assignFromMemory(someDummyId, _fileBuffer[_headerOffset + 0x1C + _chunkSize + (fileIndex * 0xC) + 0xA], sizeof(std::uint16_t), true);
                    someId = utils::assignFromMemory(someId, _fileBuffer[_headerOffset + 0x1C + _chunkSize + (fileIndex * 0xC) + 0xC], sizeof(std::uint16_t), true);
                    fileId = utils::assignFromMemory(fileId, _fileBuffer[_headerOffset + 0x1C + _chunkSize + (fileIndex * 0xC) + 0xE], sizeof(std::uint16_t), true);

                    addFile(isDir, fileDirectoryId, fileIndex, fileName, fileAddr);
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
        CRCInfo crc = {};

        std::uint32_t typeBOH = 0u;
        std::uint32_t fileCount2 = 0u; // Data from the archive

        typeBOH = utils::assignFromMemory(typeBOH, _fileBuffer[_filesChunkOffset], sizeof(std::uint32_t), true);
        fileCount2 = utils::assignFromMemory(fileCount2, _fileBuffer[_filesChunkOffset + 0x4], sizeof(std::uint32_t), true);

        if (_FileCount != fileCount2)
            spdlog::warn("The number of files read from the archive differ from last check.");

        for (std::size_t fileIndex = 0ull; fileIndex < _FileCount + _DirCount; ++fileIndex)
        {
            crc = {};
            if (!_files[fileIndex]._isDir) {
                crc._packedVer = utils::assignFromMemory(crc._packedVer, _fileBuffer[_filesChunkOffset + 0x8 + (fileOffset * 0x10)], sizeof(std::uint32_t), true);
                crc._dataAddr = utils::assignFromMemory(crc._dataAddr, _fileBuffer[_filesChunkOffset + 0xC + (fileOffset * 0x10)], sizeof(std::uint32_t), true);
                crc._fileZsize = utils::assignFromMemory(crc._fileZsize, _fileBuffer[_filesChunkOffset + 0x10 + (fileOffset * 0x10)], sizeof(std::uint32_t), true);
                crc._fileSize = utils::assignFromMemory(crc._fileSize, _fileBuffer[_filesChunkOffset + 0x14 + (fileOffset * 0x10)], sizeof(std::uint32_t), true);
                fileOffset += 1;
            }
            _CRCs.push_back(crc);
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
        _files.push_back({isDir, parentId, id, pathName, fileName, {}, {}});
    }

    void FilesChunk::_createFile(FileInfo &fileInfo) const
    {
        std::string relativePath = "./Content/" + fileInfo._pathName;

        if (std::filesystem::exists(relativePath)) {
            spdlog::warn("Already exists: {}", relativePath);
            return;
        }

        if (fileInfo._isDir) {
            std::error_code errCode;
            if (!std::filesystem::create_directories(relativePath, errCode)) {
                spdlog::error("Could not create directories for {}: {}", fileInfo._pathName, errCode.message());
            }
        } else {
            std::error_code errCode;
            auto parentPath = std::filesystem::path(relativePath).parent_path();
            if (!parentPath.empty() && !std::filesystem::exists(parentPath)) {
                if (!std::filesystem::create_directories(parentPath, errCode)) {
                    spdlog::error("Could not create parent directories for file {}: {}", fileInfo._pathName, errCode.message());
                    return;
                }
            }

            std::ofstream file(relativePath, std::ios::out);
            if (!file) {
                spdlog::error("Failed to create file: {}", relativePath);
            } else {
                if (!fileInfo._dataBuffer.empty()) {
                    file.write(reinterpret_cast<const char *>(fileInfo._dataBuffer.data()), fileInfo._dataBuffer.size());
                    if (!file) {
                        spdlog::error("Failed to write data to file: {}", relativePath);
                    } else {
                        spdlog::info("{:08x} {:<8} {} {}", fileInfo._CRC._dataAddr, fileInfo._CRC._fileZsize, fileInfo._CRC._fileSize, fileInfo._pathName);
                    }
                } else {
                    spdlog::warn("{:08x} {:<8} {}", fileInfo._CRC._dataAddr, 0, fileInfo._pathName);
                }
                file.close();
            }
        }
    }

    void FilesChunk::defineCRCdatabase()
    {
        std::size_t chunkOffset = _filesChunkOffset + (_FileCount * 0x10) + 0x8;
        std::uint32_t crc = 0u;
        std::uint32_t fileOffset = 0u;

        for (std::uint32_t fileIndex = 0ull; fileIndex < _FileCount + _DirCount; ++fileIndex)
        {
            crc = 0xFFFFFFFF;
            if (!_files[fileIndex]._isDir) {
                crc = utils::assignFromMemory(crc, _fileBuffer[chunkOffset + fileOffset * 0x4], sizeof(std::uint32_t), true);
                fileOffset += 1;
            }
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
        std::string normalizedPath;
        CRCInfo crcData = {};

        for (std::uint32_t fileIndex = 0ull; fileIndex < _FileCount + _DirCount; ++fileIndex)
        {
            crc = CRC_FNV_OFFSET;
            normalizedPath.clear();
            if (!_files[fileIndex]._isDir && _crcDatabase[fileIndex] != 0xFFFFFFFF) {
                normalizedPath = _normalizeFilename(_files[fileIndex]._pathName);
                for (char c : normalizedPath) {
                    crc ^= static_cast<std::uint8_t>(c);
                    crc *= CRC_FNV_PRIME;
                }

                auto it = std::find(_crcDatabase.begin(), _crcDatabase.end(), crc);
                if (it != _crcDatabase.end()) {
                    idx = std::distance(_crcDatabase.begin(), it);
                    crcData._crcValue = crc;
                    crcData._crcPath = normalizedPath;
                    crcData._dataAddr = _CRCs[idx]._dataAddr;
                    crcData._fileZsize = _CRCs[idx]._fileZsize;
                    crcData._fileSize = _CRCs[idx]._fileSize;
                    crcData._dataAddr = _CRCs[idx]._dataAddr;
                    _files[fileIndex]._CRC = crcData;
                } else {
                    spdlog::warn("The CRC of the file {} has not been found.", _files[fileIndex]._pathName);
                }
            }
        }
    }

    void FilesChunk::readFilesOffsetBuffer()
    {
        for (FileInfo &file : _files) {
            if (!file._isDir) {
                if (file._CRC._fileSize != file._CRC._fileZsize) { // File is compressed
                    file._dataBuffer.resize(file._CRC._fileZsize);
                    std::memcpy(file._dataBuffer.data(), &_fileBuffer[file._CRC._dataAddr], static_cast<std::size_t>(file._CRC._fileZsize));
                } else {
                    file._dataBuffer.resize(file._CRC._fileSize);
                    std::memcpy(file._dataBuffer.data(), &_fileBuffer[file._CRC._dataAddr], static_cast<std::size_t>(file._CRC._fileSize));
                }
            }
            // _createFile(file);
        }
    }

    void FilesChunk::decompressFiles()
    {
        std::unordered_map<std::string, std::function<std::unique_ptr<ntt::BaseHandler>(const std::vector<std::byte> &)>> handlerFactories = {
            {"ZIPX", [](const std::vector<std::byte> &buffer) { return std::make_unique<zipx::ZipX>(buffer); }},
            {"LZ2K", [](const std::vector<std::byte> &buffer) { return std::make_unique<lz2k::LZ2K>(buffer); }},
        };
        std::string fileSign;

        for (FileInfo &file : _files)
        {
            if (!file._isDir && file._CRC._fileSize != file._CRC._fileZsize) {
                if (file._dataBuffer.size() >= 4ull) {
                    fileSign = std::string(reinterpret_cast<const char*>(&file._dataBuffer[0]), 4);
                    auto it = handlerFactories.find(fileSign);
                    if (it != handlerFactories.end()) {
                        auto handler = it->second(file._dataBuffer);
                        handler->handle();
                    } else {
                        spdlog::warn("{} with signature {} is unknown.", file._fileName, fileSign);
                    }
                } else {
                    spdlog::warn("File {} has insufficient data for signature extraction", file._fileName);
                }
            }
            // _createFile(file);
        }
    }

} // namespace nxg

#endif // FILESCHUNK_HPP