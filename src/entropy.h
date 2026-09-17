#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>

namespace codec {

using Prob = uint16_t;
constexpr int      kNumBitModelTotalBits = 11;
constexpr uint32_t kBitModelTotal        = 1u << kNumBitModelTotalBits; // 2048
constexpr int      kNumMoveBits          = 5;
constexpr uint32_t kTopValue             = 1u << 24;

struct BitModel {
    Prob p = kBitModelTotal / 2;
};

class RangeEncoder {
public:
    void encodeBit(BitModel& m, int bit);
    void flush();
    std::vector<uint8_t>&       data()       { return out_; }
    const std::vector<uint8_t>& data() const { return out_; }
private:
    void shiftLow();
    uint64_t low_       = 0;
    uint32_t range_     = 0xFFFFFFFFu;
    uint8_t  cache_     = 0;
    uint64_t cacheSize_ = 1;
    std::vector<uint8_t> out_;
};

class RangeDecoder {
public:
    RangeDecoder(const uint8_t* data, size_t size);
    int decodeBit(BitModel& m);
private:
    uint8_t readByte();
    uint32_t range_ = 0xFFFFFFFFu;
    uint32_t code_  = 0;
    const uint8_t* data_;
    size_t size_;
    size_t pos_ = 0;
};

} // namespace codec