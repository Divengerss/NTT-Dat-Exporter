#ifndef DAT_HPP
#define DAT_HPP

#include <string>
#include <fstream>
#include <vector>
#include <iterator>
#include <stdexcept>
#include <iomanip>
#include <sstream>
#include <unordered_map>
#include <cstddef>
#include <cstring>
#include <memory>
#include "spdlog/spdlog.h"
#include "Utils/ByteSwap.hpp"
#include "FilesChunk.hpp"

namespace ntt
{
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

        std::ptrdiff_t getFilesChunkOffset(const std::string &chunkSign) const;

        void setFilesChunkHeader(const ptrdiff_t headerOffset) { _filesChunk->setChunkHeader(headerOffset); };
        void parseFilesChunk() { _filesChunk->parseChunk(); };

    private:
        std::string _datFilePath;
        std::ifstream _datFile;
        std::size_t _fileSize{0};
        std::vector<std::byte> _fileBuffer;
        std::unordered_map<std::string, std::function<void()>> _magicSign;
        std::unique_ptr<FilesChunk> _filesChunk;

        void _readFile();
        void _initializeMagicSignMap();
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
            _readFile();
        }

        _initializeMagicSignMap();
        _filesChunk = std::make_unique<FilesChunk>(_fileBuffer, _fileSize);
    }

    void Dat::_readFile()
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

    void Dat::_initializeMagicSignMap()
    {
        _magicSign = {
            {"LZ2K", [this]() { extractLZ2K(); }}};
    }

    void Dat::extractLZ2K()
    {
        return;
    }

    // Get the .CC40TAD offset
    std::ptrdiff_t Dat::getFilesChunkOffset(const std::string &chunkSign) const
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
