// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "gnm_submission_snapshot.hpp"
#include "video_session_owner.hpp"
#include "video/pm4_init_session.hpp"
namespace ctrport {
// Initialization packets retain the original provider. General owned submissions
// use the shared ordered executor. Unknown counts/CCB/packets fail explicitly.
using OrderedGnmSubmit = int (*)(uint32_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
inline OrderedGnmSubmit ordered_gnm_submit = nullptr;
inline int runtime_gnm_submit(uint32_t count, uint64_t da, uint64_t ds, uint64_t ca, uint64_t cs, uint64_t caller = 0) {
    if (count != 1) throw std::runtime_error("GnmSubmit only observed count1 implemented");
    GnmSubmissionSnapshot captured;
    if (!capture_gnm_submission(count, da, ds, ca, cs, captured))
        throw std::runtime_error("GnmSubmit owned snapshot rejected");
    if (captured.draw[0].words.empty() || !captured.constant[0].words.empty())
        throw std::runtime_error("GnmSubmit empty DCB/nonempty CCB unsupported");
    std::shared_ptr<RuntimeVideoSession> session;
    {
        std::lock_guard<std::mutex> lock(runtime_video_mutex);
        session = runtime_video_session;
    }
    if (!session) throw std::runtime_error("GnmSubmit needs live owner Vulkan session");
    auto backing = direct_memory_backing;
    if (!backing) throw std::runtime_error("GnmSubmit direct owner unavailable");
    const auto& words = captured.draw[0].words;
    std::vector<CtrPm4::InitOperation> preflight_plan;
    const int preflight = CtrPm4::PrepareInit(words.data(), words.size(), preflight_plan);
    if (preflight && ordered_gnm_submit) {
        std::printf(
            "{\"scope\":\"original_GnmSubmit_ordered_route\",\"caller\":%llu,\"words\":%zu,\"initialization_preflight\":%d,\"flip_requested\":false}\n",
            (unsigned long long)caller, words.size(), preflight);
        std::fflush(stdout);
        return ordered_gnm_submit(count, da, ds, ca, cs, caller);
    }
    const int status = session->invoke([&](const CtrVideoSessionApiV1* api) {
        return ctr_video_submit_init_v1(api->context, &backing, words.data(), words.size());
    });
    if (status) {
        // Diagnose the immutable snapshot without executing any prefix or persisting payloads.
        std::vector<CtrPm4::Packet> packets;
        const int decoded = CtrPm4::DecodePackets(words.data(), words.size(), packets);
        std::printf(
            "{\"scope\":\"original_GnmSubmit_init_rejection\",\"host_status\":%d,\"decode_status\":%d,\"words\":%zu,\"packets\":%zu,\"preflight_status\":%d,\"prefix_not_executed_proved\":%s}\n",
            status, decoded, words.size(), packets.size(), preflight, preflight ? "true" : "false");
        if (!decoded)
            for (const auto& packet : packets) {
                std::vector<CtrPm4::InitOperation> prepared;
                const int admitted = CtrPm4::PrepareInit(words.data() + packet.word, packet.words, prepared);
                if (admitted) {
                    std::printf(
                        "{\"scope\":\"original_GnmSubmit_init_first_unsupported\",\"word\":%zu,\"opcode\":%u,\"packet_words\":%u,\"header\":%u,\"status\":%d,\"GPU_execution\":false}\n",
                        packet.word, packet.opcode, packet.words, words[packet.word], admitted);
                    if (packet.opcode == 0x3c && packet.words == 7) {
                        const auto* wait = words.data() + packet.word;
                        const uint64_t address = uint64_t(wait[2]) | (uint64_t(wait[3]) << 32);
                        uint32_t value = 0;
                        const auto memory = make_owned_guest_copy_api();
                        const int read =
                            (wait[1] & 16u) ? memory.read(memory.context, address, &value, sizeof(value)) : 88;
                        std::printf(
                            "{\"scope\":\"original_GnmSubmit_wait_observation\",\"word\":%zu,\"control\":%u,\"memory_space\":%s,\"address\":%llu,\"reference\":%u,\"mask\":%u,\"interval\":%u,\"read_status\":%d,\"observed\":%u,\"effects\":false}\n",
                            packet.word, wait[1], (wait[1] & 16u) ? "true" : "false",
                            static_cast<unsigned long long>(address), wait[4], wait[5], wait[6], read, value);
                    }
                    break;
                }
            }
        std::fflush(stdout);
        throw std::runtime_error("GnmSubmit whole initialization execution rejected host_status=" +
                                 std::to_string(status));
    }
    size_t waits = 0;
    for (const auto& operation : preflight_plan)
        if (operation.opcode == 0x3c) ++waits;
    std::printf(
        "{\"scope\":\"original_GnmSubmit_plan_complete\",\"words\":%zu,\"operations\":%zu,\"waits\":%zu,\"provider_status\":%d,\"waits_not_skipped\":true}\n",
        words.size(), preflight_plan.size(), waits, status);
    std::fflush(stdout);
    return 0;
}
extern "C" int ctr_gnm_submit(uint32_t count, uint64_t da, uint64_t ds, uint64_t ca, uint64_t cs, uint64_t caller) {
    try {
        const int result = runtime_gnm_submit(count, da, ds, ca, cs, caller);
        std::printf(
            "{\"scope\":\"original_GnmSubmit_return\",\"caller\":%llu,\"count\":%u,\"result\":%d,\"owned_command_execution\":true,\"presentation\":false}\n",
            static_cast<unsigned long long>(caller), count, result);
        std::fflush(stdout);
        return result;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "NID=zwY0YV91TTI library=libSceGnmDriver caller=0x%llx unsupported: %s\n",
                     static_cast<unsigned long long>(caller), error.what());
        std::fflush(stderr);
        ExitProcess(4);
    }
}
extern "C" void ctr_sysv_gnm_submit();
extern "C" int ctr_gnm_submit_bridge_fixture(const uint64_t*);
}
