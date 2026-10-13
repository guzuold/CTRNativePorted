// SPDX-License-Identifier: GPL-2.0-only
// Isolated host FIFO-grant control, not a guest semaphore/scheduler service.
// Public reference and hash: blocking_semaphore_domain_audit.json.
#pragma once
#include <condition_variable>
#include <list>
#include <memory>
#include <chrono>
#include <future>
namespace ctrport {
enum class QueueWaitResult {pending,granted,timed_out,canceled,deleted};
class SemaphoreQueueControl {
    struct Waiter {int need,priority;QueueWaitResult result=QueueWaitResult::pending;std::condition_variable wake;Waiter(int n,int p):need(n),priority(p){}};
    std::mutex lock;std::list<std::shared_ptr<Waiter>> waiters;int tokens=0,maximum;bool fifo,closed=false;
public:
    explicit SemaphoreQueueControl(int max,bool is_fifo=true):maximum(max),fifo(is_fifo){if(max<=0)throw std::runtime_error("Invalid maximum");}
    QueueWaitResult wait_result(int need,std::chrono::microseconds timeout,std::promise<void>& enqueued,int priority=700) {
        if(need<=0 || need>maximum)throw std::runtime_error("Invalid count");
        std::unique_lock<std::mutex> guard(lock);
        if(closed){enqueued.set_value();return QueueWaitResult::deleted;}
        if(tokens>=need){tokens-=need;enqueued.set_value();return QueueWaitResult::granted;}
        auto waiter=std::make_shared<Waiter>(need,priority);
        auto position=waiters.end();
        if(!fifo){position=waiters.begin();while(position!=waiters.end() && (*position)->priority<=priority)++position;}
        waiters.insert(position,waiter);enqueued.set_value();
        if(!waiter->wake.wait_for(guard,timeout,[&]{return waiter->result!=QueueWaitResult::pending;})) {
            waiters.remove(waiter);return QueueWaitResult::timed_out;
        }
        return waiter->result;
    }
    bool wait(int need,std::chrono::microseconds timeout,std::promise<void>& ready,int priority=700) {return wait_result(need,timeout,ready,priority)==QueueWaitResult::granted;}
    bool signal(int count) {
        std::lock_guard<std::mutex> guard(lock);
        if(closed || count<=0 || count>maximum-tokens)return false;
        tokens+=count;
        for(auto it=waiters.begin();it!=waiters.end();) {
            auto waiter=*it;if(waiter->need>tokens){++it;continue;}
            tokens-=waiter->need;waiter->result=QueueWaitResult::granted;it=waiters.erase(it);waiter->wake.notify_one();
        }
        return true;
    }
    std::size_t cancel(int reset) {
        std::lock_guard<std::mutex> guard(lock);
        if(closed || reset<0 || reset>maximum)throw std::runtime_error("Invalid cancel reset");
        const auto count=waiters.size();
        for(auto& waiter:waiters){waiter->result=QueueWaitResult::canceled;waiter->wake.notify_one();}
        waiters.clear();tokens=reset;return count;
    }
    bool close() {
        std::lock_guard<std::mutex> guard(lock);if(closed)return false;closed=true;
        for(auto& waiter:waiters){waiter->result=QueueWaitResult::deleted;waiter->wake.notify_one();}
        waiters.clear();return true;
    }
    std::size_t pending(){std::lock_guard<std::mutex> guard(lock);return waiters.size();}
    int remaining(){std::lock_guard<std::mutex> guard(lock);return tokens;}
};
inline int semaphore_microsecond_control() {
    using namespace std::chrono_literals;
    SemaphoreQueueControl queue(1);std::promise<void> ready;
    const auto start=std::chrono::steady_clock::now();
    const auto result=queue.wait_result(1,250us,ready);
    const auto elapsed=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();
    if(result!=QueueWaitResult::timed_out || elapsed<250 || queue.pending() || !queue.signal(1))return 1;
    std::promise<void> consume;
    if(queue.wait_result(1,0us,consume)!=QueueWaitResult::granted || queue.remaining())return 1;
    std::cout<<"{\"scope\":\"synthetic_semaphore_microseconds\",\"requested_us\":250,\"elapsed_us\":"<<elapsed<<",\"submillisecond_not_truncated\":true,\"available_before_zero_timeout\":true,\"guest_bound\":false}\n";return 0;
}
inline int semaphore_timeout_race_control() {
    using namespace std::chrono_literals;
    unsigned grants=0,timeouts=0;
    for(unsigned i=0;i<96;++i) {
        SemaphoreQueueControl queue(1);std::promise<void> ready;auto entered=ready.get_future();
        auto waiter=std::async(std::launch::async,[&]{return queue.wait_result(1,1ms,ready);});
        entered.get();
        if(i%3==0)std::this_thread::sleep_for(2ms);
        if(!queue.signal(1))return 1;
        const auto result=waiter.get();
        if(result==QueueWaitResult::granted){++grants;if(queue.remaining()!=0)return 1;}
        else if(result==QueueWaitResult::timed_out){++timeouts;if(queue.remaining()!=1)return 1;}
        else return 1;
        if(queue.pending())return 1;
    }
    if(grants+timeouts!=96 || !timeouts)return 1;
    std::cout<<"{\"scope\":\"synthetic_semaphore_timeout_signal_race\",\"iterations\":96,\"grants\":"<<grants<<",\"timeouts\":"<<timeouts<<",\"token_conservation\":true,\"pending_cleanup\":true,\"guest_bound\":false}\n";return 0;
}
inline int semaphore_cancel_control() {
    using namespace std::chrono_literals;
    for(bool deletion:{false,true}) {
        SemaphoreQueueControl queue(2);std::promise<void> one,two;
        auto ready_one=one.get_future(),ready_two=two.get_future();
        auto a=std::async(std::launch::async,[&]{return queue.wait_result(1,2000ms,one);});ready_one.get();
        auto b=std::async(std::launch::async,[&]{return queue.wait_result(2,2000ms,two);});ready_two.get();
        const bool woke=deletion?queue.close():queue.cancel(1)==2;
        const auto x=a.get(),y=b.get();const auto expected=deletion?QueueWaitResult::deleted:QueueWaitResult::canceled;
        if(!woke || x!=expected || y!=expected || queue.pending())return 1;
        if(deletion){if(queue.close() || queue.signal(1))return 1;std::promise<void> next;if(queue.wait_result(1,0ms,next)!=QueueWaitResult::deleted)return 1;}
        else {std::promise<void> next;if(queue.remaining()!=1 || !queue.wait(1,0ms,next))return 1;}
    }
    std::cout<<"{\"scope\":\"synthetic_semaphore_cancel_delete\",\"waiters_woken_with_error\":4,\"no_false_grants\":true,\"cancel_reset_consumable\":true,\"delete_terminal\":true,\"guest_bound\":false}\n";return 0;
}
inline int semaphore_priority_control() {
    using namespace std::chrono_literals;
    SemaphoreQueueControl queue(1,false);
    std::promise<void> low_ready,first_ready,second_ready;
    auto low_signal=low_ready.get_future(),first_signal=first_ready.get_future(),second_signal=second_ready.get_future();
    auto low=std::async(std::launch::async,[&]{return queue.wait(1,2000ms,low_ready,700);});low_signal.get();
    auto high_first=std::async(std::launch::async,[&]{return queue.wait(1,2000ms,first_ready,300);});first_signal.get();
    auto high_second=std::async(std::launch::async,[&]{return queue.wait(1,2000ms,second_ready,300);});second_signal.get();
    const bool first_grant=queue.pending()==3 && queue.signal(1) && high_first.get();
    const bool second_grant=queue.pending()==2 && queue.signal(1) && high_second.get();
    const bool final_grant=queue.pending()==1 && queue.signal(1) && low.get();
    if(!first_grant || !second_grant || !final_grant || queue.pending() || queue.remaining())return 1;
    std::cout<<"{\"scope\":\"synthetic_semaphore_priority_queue\",\"real_waiters\":3,\"priority_before_insertion_order\":true,\"equal_priority_fifo\":true,\"guest_scheduler_bound\":false}\n";return 0;
}
inline int semaphore_queue_control() {
    using namespace std::chrono_literals;
    SemaphoreQueueControl queue(3);std::promise<void> first_ready,second_ready;
    auto first_signal=first_ready.get_future(),second_signal=second_ready.get_future();
    auto first=std::async(std::launch::async,[&]{return queue.wait(2,2000ms,first_ready);});first_signal.get();
    auto second=std::async(std::launch::async,[&]{return queue.wait(1,2000ms,second_ready);});second_signal.get();
    // Public source scans FIFO list and skips a waiter needing more available tokens.
    const bool start=queue.pending()==2 && queue.signal(1);
    const bool single_granted=second.get();const bool retained=queue.pending()==1 && queue.remaining()==0;
    const bool release=queue.signal(2);const bool large=first.get();
    if(!start || !single_granted || !retained || !release || !large || queue.pending() || queue.remaining())return 1;
    std::promise<void> timeout_ready;auto timed=queue.wait(1,0ms,timeout_ready);
    if(timed || queue.pending() || !queue.signal(3) || queue.signal(1) || queue.remaining()!=3)return 1;
    std::promise<void> consume_ready;if(!queue.wait(3,0ms,consume_ready) || queue.remaining())return 1;
    std::cout<<"{\"scope\":\"synthetic_semaphore_queue\",\"real_blocking_workers\":2,\"atomic_multitoken_grants\":true,\"unsatisfied_waiter_skipped\":true,\"timeout_removed\":true,\"overflow_no_mutation\":true,\"guest_bound\":false}\n";return 0;
}
}
