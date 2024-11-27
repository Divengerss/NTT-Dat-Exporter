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

    static const std::uint32_t byteswap(std::uint32_t value) {
        return ((value & 0x000000FF) << 24) |
            ((value & 0x0000FF00) << 8)  |
            ((value & 0x00FF0000) >> 8)  |
            ((value & 0xFF000000) >> 24);
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
        std::string currentString;

        while (readIndex < _datCC4_chunkSize - 0x2) {
            if (static_cast<char>(_fileBuffer[_datCC4_offset + 0x1C + readIndex]) != '\0') {
                currentString.push_back(static_cast<char>(_fileBuffer[_datCC4_offset + 0x1C + readIndex]));
            } else {
                if (!currentString.empty()) {
                    _datFilesData[{readIndex, 0}] = currentString;
                    spdlog::info("Extracting file {}", currentString);
                    currentString.clear();
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
