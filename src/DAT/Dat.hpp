#ifndef DAT_HPP
#define DAT_HPP

#include <string>
#include <fstream>
#include <cstddef>
#include <vector>
#include <iterator>
#include <stdexcept>
#include <iomanip>
#include <sstream>
#include <unordered_map>
#include <cstddef>
#include <cstring>
#include "spdlog/spdlog.h"

namespace ntt
{
    struct PairHash {
        template <typename T1, typename T2>
        std::size_t operator()(const std::pair<T1, T2>& pair) const {
            auto hash1 = std::hash<T1>{}(pair.first);
            auto hash2 = std::hash<T2>{}(pair.second);
            // Combine the two hashes
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


    class Dat
    {
    public:
        explicit Dat(const std::string &inputFile);
        ~Dat();

        const std::string &getFilePath() const noexcept { return _datFilePath; }
        const std::size_t getFileSize() const noexcept { return _fileSize; }
        const std::vector<std::byte> &getFileBuffer() const noexcept { return _fileBuffer; }

        std::string readBytesInHex(std::size_t offset, std::size_t n) const;

        void readMagicHeader();

        void extractLZ2K();

        std::ptrdiff_t findFileListChunkOffset(const std::string &chunkSign) const;
        void defineCC4_chunkData(const ptrdiff_t offset);
        void readCC4_chunkData();

    private:
        std::string _datFilePath;
        std::ifstream _datFile;
        std::size_t _fileSize{0};
        std::vector<std::byte> _fileBuffer;
        std::unordered_map<std::string, std::function<void()>> _magicSign;

        // Relative to the DAT0CC4 chunk
        std::size_t _datCC4_offset;
        std::uint32_t _datCC4_chunkSize;
        std::uint32_t _datRemainingSize;
        std::uint32_t _datCC4_ver;
        std::uint32_t _datCC4_FileCount;

        //                             addr              size         filepath
        std::unordered_map<std::pair<std::uint64_t, std::uint64_t>, std::string, PairHash> _datFilesData;

        void readFile();
        void initializeMagicSignMap();
    };

    Dat::Dat(const std::string &inputFile)
        : _datFilePath(inputFile), _datFile(inputFile, std::ios::binary | std::ios::ate)
    {
        if (!_datFile)
        {
            throw std::ios_base::failure("Failed to open file: " + _datFilePath);
        }

        spdlog::info("Reading file {}", _datFilePath);

        _fileSize = _datFile.tellg();
        if (_fileSize == 0)
        {
            spdlog::warn("File is empty: {}", _datFilePath);
        }
        else
        {
            _fileBuffer.resize(_fileSize);
            _datFile.seekg(std::ios::beg);
            readFile();
        }

        initializeMagicSignMap();
    }

    void Dat::readFile()
    {
        _datFile.seekg(0, std::ios::beg);
        _datFile.read(reinterpret_cast<char *>(_fileBuffer.data()), _fileSize);

        if (!_datFile)
        {
            throw std::ios_base::failure("Error while reading file: " + _datFilePath);
        }
        spdlog::info("Successfully read {} bytes from {}", _fileSize, _datFilePath);
    }

    std::string Dat::readBytesInHex(std::size_t offset, std::size_t n) const
    {
        if (offset >= _fileBuffer.size())
        {
            throw std::out_of_range("Offset is beyond the end of the file buffer.");
        }

        n = std::min(n, _fileBuffer.size() - offset);
        std::ostringstream hexStream;
        hexStream << std::hex << std::uppercase << std::setfill('0');

        for (std::size_t i = 0; i < n; ++i)
        {
            hexStream << std::setw(2) << static_cast<unsigned int>(std::to_integer<unsigned char>(_fileBuffer[offset + i]));
            if (i != n - 1)
            {
                hexStream << " ";
            }
        }

        return hexStream.str();
    }

    void Dat::readMagicHeader()
    {
        spdlog::info("Magic header: {}", readBytesInHex(0x0, 0x7));
    }

    void Dat::initializeMagicSignMap()
    {
        _magicSign = {
            {"LZ2K", [this]() { extractLZ2K(); }}};
    }

    void Dat::extractLZ2K()
    {
        return;
    }

    // Get the .CC40TAD offset
    std::ptrdiff_t Dat::findFileListChunkOffset(const std::string &chunkSign) const
    {
        if (chunkSign.empty() || _fileBuffer.size() < chunkSign.size())
        {
            return -1;
        }

        for (std::vector<std::byte>::difference_type i = _fileBuffer.size() - chunkSign.size(); i >= 0; --i)
        {
            bool found = true;
            for (std::size_t j = 0; j < chunkSign.size(); ++j)
            {
                if (_fileBuffer[i + j] != static_cast<std::byte>(chunkSign[j]))
                {
                    found = false;
                    break;
                }
            }
            if (found)
            {
                return i;
            }
        }

        return -1;
    }

    void Dat::defineCC4_chunkData(const ptrdiff_t offset)
    {
        _datCC4_offset = offset;

        if (_datCC4_offset < 4 || _datCC4_offset > _fileBuffer.size()) {
            throw std::out_of_range("Invalid offset provided!");
        }
        std::memcpy(&_datRemainingSize, &_fileBuffer[_datCC4_offset - 0x4], sizeof(std::uint32_t));
        std::memcpy(&_datCC4_ver, &_fileBuffer[_datCC4_offset + 0xC], sizeof(std::uint32_t));
        std::memcpy(&_datCC4_FileCount, &_fileBuffer[_datCC4_offset + 0x10], sizeof(std::uint32_t));
        std::memcpy(&_datCC4_chunkSize, &_fileBuffer[_datCC4_offset + 0x18], sizeof(std::uint32_t));

        if (isLittleEndian()) {
            _datRemainingSize = byteswap(_datRemainingSize);
            _datCC4_chunkSize = byteswap(_datCC4_chunkSize);
            _datCC4_ver = byteswap(_datCC4_ver);
            _datCC4_FileCount = byteswap(_datCC4_FileCount);
        }

        spdlog::info("Remaining chunk size {:X}", _datCC4_chunkSize);
    }

    void Dat::readCC4_chunkData()
    {
        std::uint32_t readIndex = 0u;
        // std::pair<std::uint64_t, std::uint64_t> defaultKey = {0ull, 0ull}; // NOT IMPLEMENTED
        std::string fileName;
        std::uint32_t fileIndex = 0u;

        std::uint32_t begDummyId = 0;
        std::uint32_t fileNameOffset = 1;
        std::uint16_t fileDirectoryId = 0;
        std::uint16_t someDummyId = 0;
        std::uint16_t someId = 0;
        std::uint16_t fileId = 0;

        std::memcpy(&begDummyId, &_fileBuffer[_datCC4_offset + 0x1C + _datCC4_chunkSize + (fileIndex * 0x4)], sizeof(std::int32_t));
        while (readIndex < _datCC4_chunkSize - 0x2) {
            if (static_cast<char>(_fileBuffer[_datCC4_offset + 0x1C + readIndex]) != '\0') {
                fileName.push_back(static_cast<char>(_fileBuffer[_datCC4_offset + 0x1C + readIndex]));
            } else {
                if (!fileName.empty()) {
                    std::memcpy(&fileNameOffset, &_fileBuffer[_datCC4_offset + 0x1C + _datCC4_chunkSize + (fileIndex * 0xC) + 0x4], sizeof(std::uint32_t));
                    std::memcpy(&fileDirectoryId, &_fileBuffer[_datCC4_offset + 0x1C + _datCC4_chunkSize + (fileIndex * 0xC) + 0x8], sizeof(std::uint16_t));
                    std::memcpy(&someDummyId, &_fileBuffer[_datCC4_offset + 0x1C + _datCC4_chunkSize + (fileIndex * 0xC) + 0xA], sizeof(std::uint16_t));
                    std::memcpy(&someId, &_fileBuffer[_datCC4_offset + 0x1C + _datCC4_chunkSize + (fileIndex * 0xC) + 0xC], sizeof(std::uint16_t));
                    std::memcpy(&fileId, &_fileBuffer[_datCC4_offset + 0x1C + _datCC4_chunkSize + (fileIndex * 0xC) + 0xE], sizeof(std::uint16_t));
                    fileNameOffset = byteswap(fileNameOffset);
                    fileDirectoryId = byteswap(fileDirectoryId);
                    someDummyId = byteswap(someDummyId);
                    someId = byteswap(someId);
                    fileId = byteswap(fileId);
                    _datFilesData[{readIndex, 0}] = fileName;
                    spdlog::info("{:08X} {}", fileDirectoryId, fileName);
                    fileName.clear();
                    fileIndex += 1;
                } else {
                    spdlog::warn("The extracted file name was empty");
                }
                readIndex += 0x1;
            }
            readIndex += 0x1;
        }
        spdlog::info("Extracted {} files successfully", _datCC4_FileCount);
    }

    Dat::~Dat()
    {
        if (_datFile.is_open())
        {
            _datFile.close();
            spdlog::info("Closed file {}", _datFilePath);
        }
    }

} // namespace ntt

#endif // DAT_HPP
