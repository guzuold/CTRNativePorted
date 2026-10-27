// SPDX-License-Identifier: GPL-2.0-only
// Isolated host cancellation/cleanup integration, never a guest cancellation API.
#pragma once
#include "cancellation_control.hpp"
#include "tls_storage.hpp"
#include "thread_completion.hpp"
#include "thread_identity.hpp"
namespace ctrport {
inline int cancellation_lifecycle_control() {
    TlsStorage storage({{{7}, 16, 8}});
    *static_cast<unsigned char*>(storage.address(1, 0)) = 91;
    DeferredCancellationControl cancellation;
    ThreadCompletion completion;
    std::promise<void> ready, proceed;
    auto published = ready.get_future(), resume = proceed.get_future();
    std::uint64_t identity = 0;
    completion.publish();
    if (!completion.claim_join(123)) return 1;
    auto worker = std::async(std::launch::async, [&] {
        identity = thread_self();
        bool delivered = false;
        {
            TlsThreadLifetime lifetime(storage);
            TlsContextScope context(storage);
            auto* own = static_cast<unsigned char*>(storage.address(1, 0));
            if (*own != 7) return false;
            *own = 72;
            ready.set_value();
            resume.get();
            delivered = cancellation.cancellation_point();
        }
        if (!delivered || current_tls_storage || storage.release_current_thread() != 0 || !release_thread_identity())
            return false;
        cancellation.finish();
        completion.complete(17);
        return true;
    });
    published.get();
    const bool requested = cancellation.request();
    proceed.set_value();
    if (!worker.get() || !requested || cancellation.request() || cancellation.cancellation_point()) return 1;
    {
        std::lock_guard<std::mutex> guard(thread_identity_lock);
        if (thread_identities.count(identity)) return 1;
    }
    std::uint64_t result = 0;
    if (!completion.consume_join(123, result) || result != 17 || !completion.is_collected()) return 1;
    if (*static_cast<unsigned char*>(storage.address(1, 0)) != 91) return 1;
    storage.release_current_thread();
    std::cout
        << "{\"scope\":\"isolated_cancellation_lifecycle\",\"tls_and_identity_cleaned_before_completion\":true,\"parent_tls_unchanged\":true,\"completion_retained_and_collected\":true,\"post_exit_request_rejected\":true,\"result_marker_is_synthetic\":true,\"guest_bound\":false}\n";
    return 0;
}
}
