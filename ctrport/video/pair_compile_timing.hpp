// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <windows.h>
#include <wincrypt.h>
#include <chrono>
#include <array>
#include <stdexcept>
#include <string>
#include <vector>
namespace CtrPm4 {
inline std::string ExactInputSha256(const void* data, size_t bytes) {
    if (!data || !bytes) throw std::runtime_error("Missing exact identity input");
    HCRYPTPROV provider = 0;
    HCRYPTHASH hash = 0;
    std::array<unsigned char, 32> digest{};
    DWORD size = static_cast<DWORD>(digest.size());
    if (bytes > MAXDWORD || !CryptAcquireContextW(&provider, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT) ||
        !CryptCreateHash(provider, CALG_SHA_256, 0, 0, &hash) ||
        !CryptHashData(hash, static_cast<const BYTE*>(data), static_cast<DWORD>(bytes), 0) ||
        !CryptGetHashParam(hash, HP_HASHVAL, digest.data(), &size, 0) || size != digest.size()) {
        if (hash) CryptDestroyHash(hash);
        if (provider) CryptReleaseContext(provider, 0);
        throw std::runtime_error("Exact SHA256 identity failed");
    }
    CryptDestroyHash(hash);
    CryptReleaseContext(provider, 0);
    static constexpr char hex[] = "0123456789abcdef";
    std::string output(64, '0');
    for (size_t i = 0; i < digest.size(); ++i) {
        output[i * 2] = hex[digest[i] >> 4];
        output[i * 2 + 1] = hex[digest[i] & 15];
    }
    return output;
}
struct PairInputIdentity {
    uint64_t address = 0;
    uint64_t bytes = 0;
    std::string sha256;
};
struct PairCaptureTiming {
    using Clock = std::chrono::steady_clock;
    double nestedMs = 0.0;
    unsigned nestedCalls = 0;
    std::vector<PairInputIdentity> nestedInputs;
};
inline thread_local PairCaptureTiming* activePairCaptureTiming = nullptr;
class ScopedPairCaptureTiming {
    PairCaptureTiming* previous;

public:
    explicit ScopedPairCaptureTiming(PairCaptureTiming& timing) : previous(activePairCaptureTiming) {
        activePairCaptureTiming = &timing;
    }
    ~ScopedPairCaptureTiming() { activePairCaptureTiming = previous; }
    ScopedPairCaptureTiming(const ScopedPairCaptureTiming&) = delete;
    ScopedPairCaptureTiming& operator=(const ScopedPairCaptureTiming&) = delete;
};
class ScopedNestedCaptureTimer {
    PairCaptureTiming* timing = activePairCaptureTiming;
    PairCaptureTiming::Clock::time_point started;

public:
    ScopedNestedCaptureTimer() {
        if (timing) started = PairCaptureTiming::Clock::now();
    }
    ~ScopedNestedCaptureTimer() {
        if (timing) {
            timing->nestedMs +=
                std::chrono::duration<double, std::milli>(PairCaptureTiming::Clock::now() - started).count();
            ++timing->nestedCalls;
        }
    }
};
}
