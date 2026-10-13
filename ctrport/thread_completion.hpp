// SPDX-License-Identifier: GPL-2.0-only
// Independent lifecycle control; public contract audit: thread_exit_join_lifecycle_audit.json.
// No guest pthread binding or scheduler implementation.
#pragma once
#include <mutex>
#include <stdexcept>
#include <cstdint>
#include <future>
#include <atomic>
namespace ctrport {
class ThreadCompletion {
    std::mutex lock;
    bool published=false,completed=false,claimed=false,detached=false,collected=false;
    std::uint64_t result=0,joiner=0;
public:
    void publish() {
        std::lock_guard<std::mutex> guard(lock);
        if(published)throw std::runtime_error("Thread already published");
        published=true;
    }
    bool abort_creation() {
        std::lock_guard<std::mutex> guard(lock);
        if(!published || completed || claimed || detached || collected)return false;
        collected=true;return true;
    }
    bool claim_join(std::uint64_t owner,bool self=false) {
        std::lock_guard<std::mutex> guard(lock);
        if(!owner || !published || self || claimed || detached || collected)return false;
        claimed=true;joiner=owner;return true;
    }
    void complete(std::uint64_t value) {
        std::lock_guard<std::mutex> guard(lock);
        if(!published || completed || collected)throw std::runtime_error("Invalid thread completion");
        result=value;completed=true;if(detached)collected=true;
    }
    bool consume_join(std::uint64_t owner,std::uint64_t& value) {
        std::lock_guard<std::mutex> guard(lock);
        if(!owner || owner!=joiner || !claimed || !completed || collected || detached)return false;
        value=result;collected=true;return true;
    }
    bool backout_join(std::uint64_t owner) {
        std::lock_guard<std::mutex> guard(lock);
        if(!owner || owner!=joiner || !claimed || collected)return false;
        claimed=false;joiner=0;return true;
    }
    bool detach() {
        std::lock_guard<std::mutex> guard(lock);
        if(!published || claimed || detached || collected)return false;
        detached=true;if(completed)collected=true;return true;
    }
    bool has_joiner() {std::lock_guard<std::mutex> guard(lock);return claimed;}
    bool is_collected() {std::lock_guard<std::mutex> guard(lock);return collected;}
};
inline int thread_creation_rollback_control() {
    ThreadCompletion state;std::uint64_t output_cell=0;
    std::promise<bool> start;auto signal=start.get_future();
    std::atomic<bool> body_entered{false};
    auto worker=std::async(std::launch::async,[&] {
        if(!signal.get())return;
        body_entered.store(true);state.complete(7);
    });
    // Explicit injected failure after publication, before execution release.
    state.publish();output_cell=17;
    const bool aborted=state.abort_creation();output_cell=0;
    start.set_value(false);worker.get();
    std::uint64_t result=99;
    if(!aborted || output_cell || body_entered.load() || !state.is_collected()
       || state.claim_join(1) || state.consume_join(1,result) || result!=99
       || state.abort_creation())return 1;
    bool rejected=false;try{state.complete(9);}catch(const std::runtime_error&){rejected=true;}
    if(!rejected)return 1;
    ThreadCompletion active;active.publish();if(!active.claim_join(1) || active.abort_creation())return 1;
    active.complete(8);if(active.abort_creation() || !active.consume_join(1,result) || result!=8)return 1;
    std::cout<<"{\"scope\":\"synthetic_creation_rollback\",\"injected_failure\":true,\"body_not_entered\":true,\"output_cleared\":true,\"terminal_record\":true,\"active_abort_rejected\":true,\"guest_bound\":false}\n";
    return 0;
}
inline int thread_publication_control() {
    ThreadCompletion state;
    std::promise<void> ready,start;
    auto ready_signal=ready.get_future();auto start_signal=start.get_future();
    std::atomic<bool> entered{false};std::uint64_t output_cell=0;
    auto worker=std::async(std::launch::async,[&] {
        ready.set_value();start_signal.get();
        entered.store(true);
        if(output_cell!=17)throw std::runtime_error("Identity publication not visible");
        if(!state.claim_join(23))throw std::runtime_error("Published record unavailable");
        state.complete(91);
    });
    ready_signal.get();
    const bool blocked=!entered.load() && output_cell==0;
    // Creator publishes record and output cell before release of start signal.
    state.publish();output_cell=17;start.set_value();worker.get();
    std::uint64_t result=0;
    if(!blocked || !entered.load() || !state.consume_join(23,result) || result!=91)return 1;
    std::cout<<"{\"scope\":\"synthetic_thread_publication\",\"blocked_before_publication\":true,\"output_visible_before_body\":true,\"result_collected\":true,\"guest_bound\":false}\n";
    return 0;
}
inline int thread_completion_control() {
    ThreadCompletion state;std::uint64_t out=99;
    if(state.claim_join(1) || state.consume_join(1,out) || out!=99)return 1;
    bool rejected=false;try{state.complete(5);}catch(const std::runtime_error&){rejected=true;}
    if(!rejected)return 1;
    state.publish();if(state.claim_join(1,true))return 1;
    auto a=std::async(std::launch::async,[&]{return state.claim_join(1);});
    auto b=std::async(std::launch::async,[&]{return state.claim_join(2);});
    const bool first=a.get(),second=b.get();if(first==second || state.detach())return 1;
    const std::uint64_t owner=first?1:2,foreign=first?2:1;
    if(state.consume_join(foreign,out) || state.backout_join(foreign) || state.consume_join(owner,out) || out!=99)return 1;
    if(!state.backout_join(owner) || state.backout_join(owner) || !state.claim_join(3))return 1;
    auto worker=std::async(std::launch::async,[&]{state.complete(42);});worker.get();
    if(state.is_collected() || !state.consume_join(3,out) || out!=42 || !state.is_collected() || state.consume_join(3,out))return 1;
    for(bool early:{false,true}) {
        ThreadCompletion detached;detached.publish();
        if(early && !detached.detach())return 1;
        detached.complete(7);
        if(!early && (detached.is_collected() || !detached.detach()))return 1;
        if(!detached.is_collected() || detached.detach() || detached.claim_join(1))return 1;
    }
    std::cout<<"{\"scope\":\"synthetic_thread_completion\",\"single_joiner\":true,\"owner_checked\":true,\"backout_reclaim\":true,\"retained_until_join\":true,\"detach_orders_checked\":2,\"guest_bound\":false}\n";
    return 0;
}
}
