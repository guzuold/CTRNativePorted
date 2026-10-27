// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "video/pm4_decode.hpp"
#include <utility>
namespace ctrport {
// Lossless owned command cursor. Completion means the handler has completed the
// whole packet, including its effects. Pending/unsupported never advances it.
// This layer assigns no semantics to NOPs, barriers, DMA or GPU operations.
class GnmCommandCursor {
    std::vector<uint32_t> words_;
    std::vector<CtrPm4::Packet> packets_;
    size_t next_ = 0;
    uint64_t submission_ = 0;
    uint32_t buffer_ = 0, kind_ = 0;
    bool captured_ = false, running_ = false;

public:
    GnmCommandCursor() = default;
    GnmCommandCursor(const GnmCommandCursor&) = delete;
    GnmCommandCursor& operator=(const GnmCommandCursor&) = delete;
    int matches(uint64_t submission, uint32_t buffer, uint32_t kind, uint32_t word, const uint32_t* data,
                size_t count) const {
        if (!captured_ || submission != submission_ || buffer != buffer_ || kind != kind_ || !data) return 60;
        for (const auto& p : packets_)
            if (p.word == word) {
                if (p.words != count) return 60;
                return std::equal(words_.begin() + word, words_.begin() + word + count, data) ? 0 : 112;
            }
        return 60;
    }
    int capture(uint64_t submission, uint32_t buffer, uint32_t kind, const uint32_t* words, size_t count) {
        if (captured_ || !submission || kind > 1) return 60;
        std::vector<CtrPm4::Packet> packets;
        const int decoded = CtrPm4::DecodePackets(words, count, packets);
        if (decoded) return decoded;
        try {
            std::vector<uint32_t> copy(words, words + count);
            words_ = std::move(copy);
            packets_ = std::move(packets);
        } catch (...) {
            return 112;
        }
        submission_ = submission;
        buffer_ = buffer;
        kind_ = kind;
        captured_ = true;
        return 0;
    }
    template <class Handler> int resume(Handler&& handler) {
        if (!captured_) return 60;
        if (running_) return 91;
        running_ = true;
        struct Reset {
            bool& value;
            ~Reset() { value = false; }
        } reset{running_};
        while (next_ < packets_.size()) {
            const auto& packet = packets_[next_];
            int status = 112;
            try {
                status = handler(submission_, buffer_, kind_, packet, words_.data() + packet.word);
            } catch (...) {
                return 112;
            }
            if (status) return status;
            ++next_;
        }
        return 0;
    }
    const CtrPm4::Packet* pending() const { return captured_ && next_ < packets_.size() ? &packets_[next_] : nullptr; }
    size_t completed() const { return next_; }
    size_t count() const { return packets_.size(); }
    size_t words() const { return words_.size(); }
};
}
