#pragma once
#include "enhanced_runtime.hpp"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <string>
#include <thread>

namespace stellar7 {
namespace enhancedHostDetail {
inline std::atomic<const char*> operation{"idle"};
inline std::atomic<std::uint64_t> completed{0};
inline std::atomic<unsigned> entered{0};
inline constexpr unsigned startupTraceOperations=32;
}
class EnhancedHostOperation {
public:
    explicit EnhancedHostOperation(const char* name):enabled_(enhancedHostDiagnosticsEnabled()),name_(name) {
        if(!enabled_) {return;}
        previous_=enhancedHostDetail::operation.exchange(name);
        startup_=enhancedHostDetail::entered.fetch_add(1)<enhancedHostDetail::startupTraceOperations;
        if(startup_) {enhancedHostTrace((std::string("enter ")+name_).c_str());}
    }
    ~EnhancedHostOperation() {
        if(!enabled_) {return;}
        if(startup_) {enhancedHostTrace((std::string("leave ")+name_).c_str());}
        enhancedHostDetail::operation=previous_;++enhancedHostDetail::completed;
    }
    EnhancedHostOperation(const EnhancedHostOperation&)=delete;
    EnhancedHostOperation& operator=(const EnhancedHostOperation&)=delete;
private:
    bool enabled_;
    bool startup_=false;
    const char* name_;
    const char* previous_=nullptr;
};
template<class Call> decltype(auto) enhancedHostCall(const char* operation,Call&& call) {
    const EnhancedHostOperation scope(operation);return call();
}
class EnhancedHostDiagnostics {
public:
    EnhancedHostDiagnostics() {
        if(!enhancedHostDiagnosticsEnabled()) {return;}
        worker_=std::thread([this] {
            const auto start=std::chrono::steady_clock::now();
            std::unique_lock<std::mutex> lock(mutex_);
            while(!changed_.wait_for(lock,std::chrono::seconds(1),[this] {return stop_;})) {
                lock.unlock();
                const auto milliseconds=std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now()-start).count();
                const auto line="heartbeat ms="+std::to_string(milliseconds)+" operation="
                    +enhancedHostDetail::operation.load()+" completed="+std::to_string(enhancedHostDetail::completed.load());
                enhancedHostTrace(line.c_str());lock.lock();
            }
        });
    }
    ~EnhancedHostDiagnostics() {
        {const std::lock_guard<std::mutex> lock(mutex_);stop_=true;}
        changed_.notify_one();if(worker_.joinable()) {worker_.join();}
    }
    EnhancedHostDiagnostics(const EnhancedHostDiagnostics&)=delete;
    EnhancedHostDiagnostics& operator=(const EnhancedHostDiagnostics&)=delete;
private:
    std::mutex mutex_;
    std::condition_variable changed_;
    std::thread worker_;
    bool stop_=false;
};
}
