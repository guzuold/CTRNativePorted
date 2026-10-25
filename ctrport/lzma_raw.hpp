// SPDX-License-Identifier: GPL-2.0-only
// Raw LZMA1 decoder shared by the launcher (xbox_prepare.hpp) and the runtime
// (settings/menu_overlay.hpp). C++17.
#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>
namespace ctrport::lzma_raw {
using std::uint8_t; using std::uint16_t; using std::uint32_t; using std::size_t;
[[noreturn]] inline void lzma_fail(const std::string& what) { throw std::runtime_error("archive: " + what); }
// Raw LZMA1 decoder (no header, no end marker required), written from the public
// LZMA specification. The output size of each 32 KiB archive block is known.
class Decoder {
public:
    Decoder(const uint8_t* in, size_t in_size, uint8_t props, uint32_t) : in_(in), end_(in + in_size) {
        if (props >= 9 * 5 * 5) lzma_fail("invalid LZMA properties");
        lc_ = props % 9; lp_ = (props / 9) % 5; pb_ = props / 45;
        literal_.assign(size_t(0x300) << (lc_ + lp_), 1024);
        for (auto* p : {&match_, &rep0_long_}) p->fill(1024);
        for (auto* p : {&rep_, &rep_g0_, &rep_g1_, &rep_g2_}) p->fill(1024);
        for (auto& s : slot_) s.fill(1024);
        special_.fill(1024); align_.fill(1024);
        if (next() != 0) lzma_fail("corrupt LZMA stream");
        for (int i = 0; i < 4; ++i) code_ = (code_ << 8) | next();
    }
    void decode(uint8_t* out, size_t size) {
        size_t pos = 0;
        uint32_t state = 0, rep[4]{};
        const uint32_t pb_mask = (1u << pb_) - 1, lp_mask = (1u << lp_) - 1;
        while (pos < size) {
            const uint32_t pos_state = uint32_t(pos) & pb_mask;
            if (!bit(match_[(state << 4) + pos_state])) {
                const uint8_t prev = pos ? out[pos - 1] : 0;
                uint16_t* probs = &literal_[0x300 * (((uint32_t(pos) & lp_mask) << lc_) + (prev >> (8 - lc_)))];
                uint32_t symbol = 1;
                if (state >= 7) {
                    if (rep[0] >= pos) lzma_fail("LZMA distance outside block");
                    uint32_t match_byte = out[pos - rep[0] - 1];
                    while (symbol < 0x100) {
                        const uint32_t match_bit = (match_byte >> 7) & 1; match_byte <<= 1;
                        const uint32_t b = bit(probs[((1 + match_bit) << 8) + symbol]);
                        symbol = (symbol << 1) | b;
                        if (match_bit != b) break;
                    }
                }
                while (symbol < 0x100) symbol = (symbol << 1) | bit(probs[symbol]);
                out[pos++] = uint8_t(symbol);
                state = state < 4 ? 0 : state < 10 ? state - 3 : state - 6;
                continue;
            }
            uint32_t length;
            if (bit(rep_[state])) {
                if (!bit(rep_g0_[state])) {
                    if (!bit(rep0_long_[(state << 4) + pos_state])) {
                        if (rep[0] >= pos) lzma_fail("LZMA distance outside block");
                        state = state < 7 ? 9 : 11;
                        out[pos] = out[pos - rep[0] - 1]; ++pos;
                        continue;
                    }
                } else {
                    uint32_t distance;
                    if (!bit(rep_g1_[state])) distance = rep[1];
                    else {
                        if (!bit(rep_g2_[state])) distance = rep[2];
                        else { distance = rep[3]; rep[3] = rep[2]; }
                        rep[2] = rep[1];
                    }
                    rep[1] = rep[0]; rep[0] = distance;
                }
                length = rep_length_.decode(*this, pos_state);
                state = state < 7 ? 8 : 11;
            } else {
                rep[3] = rep[2]; rep[2] = rep[1]; rep[1] = rep[0];
                length = length_.decode(*this, pos_state);
                state = state < 7 ? 7 : 10;
                rep[0] = distance(length);
                if (rep[0] == 0xFFFFFFFFu) break;  // end marker
            }
            length += 2;
            if (rep[0] >= pos) lzma_fail("LZMA distance outside block");
            for (; length && pos < size; --length, ++pos) out[pos] = out[pos - rep[0] - 1];
        }
        if (pos != size) lzma_fail("short LZMA block");
    }

private:
    struct Length {
        uint16_t choice = 1024, choice2 = 1024;
        std::array<std::array<uint16_t, 8>, 16> low{}, mid{};
        std::array<uint16_t, 256> high{};
        Length() { for (auto& a : low) a.fill(1024); for (auto& a : mid) a.fill(1024); high.fill(1024); }
        uint32_t decode(Decoder& d, uint32_t pos_state) {
            if (!d.bit(choice)) return d.tree(low[pos_state].data(), 3);
            if (!d.bit(choice2)) return 8 + d.tree(mid[pos_state].data(), 3);
            return 16 + d.tree(high.data(), 8);
        }
    };
    uint8_t next() { return in_ < end_ ? *in_++ : 0; }
    void normalize() { if (range_ < (1u << 24)) { range_ <<= 8; code_ = (code_ << 8) | next(); } }
    uint32_t bit(uint16_t& prob) {
        const uint32_t bound = (range_ >> 11) * prob;
        uint32_t result;
        if (code_ < bound) { range_ = bound; prob = uint16_t(prob + ((2048 - prob) >> 5)); result = 0; }
        else { range_ -= bound; code_ -= bound; prob = uint16_t(prob - (prob >> 5)); result = 1; }
        normalize();
        return result;
    }
    uint32_t tree(uint16_t* probs, int bits) {
        uint32_t m = 1;
        for (int i = 0; i < bits; ++i) m = (m << 1) + bit(probs[m]);
        return m - (1u << bits);
    }
    uint32_t reverse(uint16_t* probs, int bits) {
        uint32_t m = 1, symbol = 0;
        for (int i = 0; i < bits; ++i) { const uint32_t b = bit(probs[m]); m = (m << 1) + b; symbol |= b << i; }
        return symbol;
    }
    uint32_t direct(int bits) {
        uint32_t result = 0;
        for (; bits > 0; --bits) {
            range_ >>= 1; code_ -= range_;
            const uint32_t t = 0u - (code_ >> 31);
            code_ += range_ & t;
            result = (result << 1) + (t + 1);
            normalize();
        }
        return result;
    }
    uint32_t distance(uint32_t length) {
        const uint32_t slot = tree(slot_[(std::min<uint32_t>)(length, 3)].data(), 6);
        if (slot < 4) return slot;
        const int bits = int(slot >> 1) - 1;
        uint32_t dist = (2 | (slot & 1)) << bits;
        if (slot < 14) return dist + reverse(&special_[dist - slot], bits);
        dist += direct(bits - 4) << 4;
        return dist + reverse(align_.data(), 4);
    }
    const uint8_t* in_; const uint8_t* end_;
    uint32_t range_ = 0xFFFFFFFFu, code_ = 0, lc_ = 0, lp_ = 0, pb_ = 0;
    std::vector<uint16_t> literal_;
    std::array<uint16_t, 12 * 16> match_{}, rep0_long_{};
    std::array<uint16_t, 12> rep_{}, rep_g0_{}, rep_g1_{}, rep_g2_{};
    std::array<std::array<uint16_t, 64>, 4> slot_{};
    std::array<uint16_t, 1 + 128 - 14> special_{};
    std::array<uint16_t, 16> align_{};
    Length length_, rep_length_;
};
}  // namespace ctrport::lzma_raw
