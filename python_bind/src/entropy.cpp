#include "entropy.h"

namespace codec {

// -------- Encoder (LZMA-style range coder) --------

void RangeEncoder::shiftLow() {
    if ((uint32_t)(low_ >> 32) != 0 || (uint32_t)low_ < 0xFF000000u) {
        uint8_t temp = cache_;
        do {
            out_.push_back((uint8_t)(temp + (uint8_t)(low_ >> 32)));
            temp = 0xFF;
        } while (--cacheSize_ != 0);
        cache_ = (uint8_t)((uint32_t)low_ >> 24);
    }
    cacheSize_++;
    low_ = (uint32_t)low_ << 8;
}

void RangeEncoder::encodeBit(BitModel& m, int bit) {
    uint32_t bound = (range_ >> kNumBitModelTotalBits) * m.p;
    if (bit == 0) {
        range_ = bound;
        m.p = (Prob)(m.p + ((kBitModelTotal - m.p) >> kNumMoveBits));
    } else {
        low_   += bound;
        range_ -= bound;
        m.p = (Prob)(m.p - (m.p >> kNumMoveBits));
    }
    while (range_ < kTopValue) {
        range_ <<= 8;
        shiftLow();
    }
}

void RangeEncoder::flush() {
    for (int i = 0; i < 5; i++) shiftLow();
}

// -------- Decoder --------

RangeDecoder::RangeDecoder(const uint8_t* data, size_t size)
    : data_(data), size_(size) {
    for (int i = 0; i < 5; i++)
        code_ = (code_ << 8) | readByte();
}

uint8_t RangeDecoder::readByte() {
    return (pos_ < size_) ? data_[pos_++] : 0;
}

int RangeDecoder::decodeBit(BitModel& m) {
    uint32_t bound = (range_ >> kNumBitModelTotalBits) * m.p;
    int bit;
    if (code_ < bound) {
        range_ = bound;
        m.p = (Prob)(m.p + ((kBitModelTotal - m.p) >> kNumMoveBits));
        bit = 0;
    } else {
        range_ -= bound;
        code_  -= bound;
        m.p = (Prob)(m.p - (m.p >> kNumMoveBits));
        bit = 1;
    }
    while (range_ < kTopValue) {
        range_ <<= 8;
        code_ = (code_ << 8) | readByte();
    }
    return bit;
}

} // namespace codec