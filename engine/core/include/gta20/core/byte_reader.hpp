#pragma once

#include <cstring>
#include <span>
#include <string>

#include "gta20/core/types.hpp"

namespace gta20 {

/// Bounds-checked little-endian reader over immutable bytes. Failure is sticky:
/// after the first out-of-range read every read returns zero and failed() is true,
/// so parsers can read a whole record and check once.
class ByteReader {
public:
    explicit ByteReader(std::span<const std::byte> data) : data_(data) {}

    [[nodiscard]] bool failed() const { return failed_; }
    [[nodiscard]] std::size_t position() const { return pos_; }
    [[nodiscard]] std::size_t remaining() const { return failed_ ? 0 : data_.size() - pos_; }

    u8 u8le() { return static_cast<u8>(read_unsigned(1)); }
    u16 u16le() { return static_cast<u16>(read_unsigned(2)); }
    u32 u32le() { return static_cast<u32>(read_unsigned(4)); }
    i16 i16le() { return static_cast<i16>(u16le()); }

    /// Next n bytes as a view (empty and failed if fewer remain).
    std::span<const std::byte> take(std::size_t n) {
        if (!ensure(n)) {
            return {};
        }
        auto view = data_.subspan(pos_, n);
        pos_ += n;
        return view;
    }

    void skip(std::size_t n) { (void)take(n); }

    /// n bytes interpreted as ASCII, cut at the first NUL.
    std::string fixed_string(std::size_t n) {
        const auto bytes = take(n);
        std::string s;
        s.reserve(bytes.size());
        for (const std::byte b : bytes) {
            if (b == std::byte{0}) {
                break;
            }
            s.push_back(static_cast<char>(b));
        }
        return s;
    }

private:
    bool ensure(std::size_t n) {
        if (failed_ || n > data_.size() - pos_) {
            failed_ = true;
            return false;
        }
        return true;
    }

    u64 read_unsigned(std::size_t n) {
        if (!ensure(n)) {
            return 0;
        }
        u64 v = 0;
        for (std::size_t i = 0; i < n; ++i) {
            v |= static_cast<u64>(std::to_integer<u8>(data_[pos_ + i])) << (8 * i);
        }
        pos_ += n;
        return v;
    }

    std::span<const std::byte> data_;
    std::size_t pos_ = 0;
    bool failed_ = false;
};

} // namespace gta20
