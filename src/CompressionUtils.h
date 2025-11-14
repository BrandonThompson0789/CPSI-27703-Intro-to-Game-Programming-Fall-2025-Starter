#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <zlib.h>

// Compression utilities for network data
class CompressionUtils {
public:
    // Compress a string using zlib deflate
    // Returns compressed data, or empty vector on failure
    static std::vector<uint8_t> Compress(const std::string& data, int compressionLevel = Z_DEFAULT_COMPRESSION);
    
    // Decompress data using zlib inflate
    // Returns decompressed string, or empty string on failure
    static std::string Decompress(const std::vector<uint8_t>& compressedData);
    
    // Compress a string and return as string (for convenience)
    static std::string CompressToString(const std::string& data, int compressionLevel = Z_DEFAULT_COMPRESSION);
    
    // Decompress a string
    static std::string DecompressFromString(const std::string& compressedData);
};

