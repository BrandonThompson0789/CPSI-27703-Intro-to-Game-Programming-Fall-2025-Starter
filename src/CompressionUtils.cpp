#include "CompressionUtils.h"
#include <iostream>
#include <cstring>
#include <cstdint>

std::vector<uint8_t> CompressionUtils::Compress(const std::string& data, int compressionLevel) {
    if (data.empty()) {
        return std::vector<uint8_t>();
    }
    
    z_stream zs;
    memset(&zs, 0, sizeof(zs));
    
    if (deflateInit(&zs, compressionLevel) != Z_OK) {
        std::cerr << "CompressionUtils: Failed to initialize deflate" << std::endl;
        return std::vector<uint8_t>();
    }
    
    zs.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(data.data()));
    zs.avail_in = static_cast<uInt>(data.size());
    
    std::vector<uint8_t> output;
    output.resize(data.size()); // Start with same size, will grow if needed
    
    int ret;
    do {
        if (zs.total_out >= output.size()) {
            output.resize(output.size() * 2);
        }
        
        zs.next_out = output.data() + zs.total_out;
        zs.avail_out = static_cast<uInt>(output.size() - zs.total_out);
        
        ret = deflate(&zs, Z_FINISH);
    } while (ret == Z_OK);
    
    deflateEnd(&zs);
    
    if (ret != Z_STREAM_END) {
        std::cerr << "CompressionUtils: Compression failed: " << ret << std::endl;
        return std::vector<uint8_t>();
    }
    
    output.resize(zs.total_out);
    return output;
}

std::string CompressionUtils::Decompress(const std::vector<uint8_t>& compressedData) {
    if (compressedData.empty()) {
        return std::string();
    }
    
    z_stream zs;
    memset(&zs, 0, sizeof(zs));
    
    if (inflateInit(&zs) != Z_OK) {
        std::cerr << "CompressionUtils: Failed to initialize inflate" << std::endl;
        return std::string();
    }
    
    zs.next_in = const_cast<Bytef*>(compressedData.data());
    zs.avail_in = static_cast<uInt>(compressedData.size());
    
    std::vector<uint8_t> output;
    output.resize(compressedData.size() * 4); // Estimate: compressed data is usually smaller
    
    int ret;
    do {
        if (zs.total_out >= output.size()) {
            output.resize(output.size() * 2);
        }
        
        zs.next_out = output.data() + zs.total_out;
        zs.avail_out = static_cast<uInt>(output.size() - zs.total_out);
        
        ret = inflate(&zs, Z_NO_FLUSH);
    } while (ret == Z_OK);
    
    inflateEnd(&zs);
    
    if (ret != Z_STREAM_END) {
        std::cerr << "CompressionUtils: Decompression failed: " << ret << std::endl;
        return std::string();
    }
    
    output.resize(zs.total_out);
    return std::string(reinterpret_cast<const char*>(output.data()), output.size());
}

std::string CompressionUtils::CompressToString(const std::string& data, int compressionLevel) {
    std::vector<uint8_t> compressedVec = Compress(data, compressionLevel);
    if (compressedVec.empty()) {
        return std::string();
    }
    return std::string(reinterpret_cast<const char*>(compressedVec.data()), compressedVec.size());
}

std::string CompressionUtils::DecompressFromString(const std::string& compressedData) {
    std::vector<uint8_t> compressedVec(compressedData.begin(), compressedData.end());
    return Decompress(compressedVec);
}

