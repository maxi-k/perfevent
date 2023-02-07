#pragma once

#include <atomic>
#include <vector>
#include <thread>
#include <chrono>
#include <time.h>
#include <ostream>
#include <stdexcept>
#include <tbb/enumerable_thread_specific.h>

#include "PerfEvent.hpp"

struct BackgroundTracker;
BackgroundTracker* GLOBAL_TRACKER = nullptr;

#ifndef PERF_NO_BACKGROUND_TRACKING
struct BackgroundTracker {
    using event = PerfEvent::event;
    using clock_t = std::chrono::steady_clock;
    using counter_t = uint64_t;
    struct Record {
        unsigned type;
        clock_t::time_point time;
        counter_t value;
    };
    PerfEventBlock perf;
    std::vector<std::string>& names;
    std::vector<event*> tracked_events;
    tbb::enumerable_thread_specific<std::vector<Record>> thread_events;
    std::atomic<int> barrier{0};
    std::ostream& output;
    std::thread tracker;

    BackgroundTracker(std::vector<std::string>& names, uint64_t scale = 1,
                      BenchmarkParameters params = {}, bool printHeader = true,
                      unsigned freq_us = 100, std::ostream& output = std::cerr)
        : perf(scale, params, printHeader)
        , names(initialize_names(names))
        , tracked_events(initialize_tracked_events(perf.e))
        , thread_events([&, scale]() {
            std::vector<Record> res;
            res.reserve(scale / std::thread::hardware_concurrency());
            return res;
        })
        , output(output)
        , tracker(tracker_task, std::ref(*this), freq_us) {
        if (GLOBAL_TRACKER) { throw std::logic_error("BackgroundTracker already exists"); }
        GLOBAL_TRACKER = this;
        barrier.store(1);
    };

    ~BackgroundTracker() {
        barrier.store(2);
        tracker.join();
        perf.e->stopCounters();
        perf.stopped = true;
        write_events_csv();
        GLOBAL_TRACKER = nullptr;
    }

    static void tracker_task(BackgroundTracker& tracker, unsigned freq_us) {
        while (!tracker.barrier) { usleep(freq_us); }
        auto& list = tracker.thread_events.local();
        auto tracked_id = tracker.names.size() - 1;

        while (tracker.barrier.load() == 1) {
            auto event_id = tracked_id;
            for (auto& event : tracker.tracked_events) {
                if (read(event->fd, &event->data, sizeof(uint64_t) * 3) != sizeof(uint64_t) * 3)
                    std::cerr << "Error reading counter " << tracker.names[event_id] << std::endl;
                list.emplace_back(event_id, clock_t::now(), event->readCounterCheap());
                --event_id;
            }
            usleep(freq_us);
        }
    }

    inline void push_event(unsigned event_id, counter_t value) {
        auto& list = thread_events.local();
        list.emplace_back(event_id, clock_t::now(), value);
    }

    inline void push_event(std::vector<Record>& list, unsigned event_id, counter_t value) {
        list.emplace_back(event_id, clock_t::now(), value);
    }

    inline void push_event(const std::string& event_name, counter_t value) {
        auto& list = thread_events.local();
        list.emplace_back(id_for_name(event_name), clock_t::now(), value);
    }

    inline void push_event(std::vector<Record>& list, const std::string& event_name, counter_t value) {
        list.emplace_back(id_for_name(event_name), clock_t::now(), value);
    }

    inline unsigned id_for_name(const std::string& ev_name) const {
        for (auto i = 0u; i != names.size(); ++i) {
            if (names[i] == ev_name) { return i; }
        }
        return -1;
    }

private:
    static std::vector<std::string>& initialize_names(std::vector<std::string>& names) {
        names.push_back("LLC-misses");
        return names;
    }

    static std::vector<event*> initialize_tracked_events(PerfRef& perf) {
        return std::vector<event*>({perf->getEvent("LLC-misses")});
    }

    inline static constexpr uint64_t to_us(const Record& record) {
        return std::chrono::duration_cast<std::chrono::microseconds>(record.time.time_since_epoch())
            .count();
    }

    void write_events_csv() const {
        auto max_name_length = 0ul;
        for (auto& name : names) { max_name_length = std::max(max_name_length, name.length()); }
        max_name_length = std::max(sizeof("event, ") - 1, max_name_length);

        auto time_length = std::to_string(to_us(thread_events.begin()->front())).length();
        time_length = std::max(sizeof("time, ") - 1, time_length);

        auto value_length = std::to_string(thread_events.begin()->back().value).length();
        time_length = std::max(sizeof("time, ") - 1, time_length);

        if (perf.printHeader) {
            output << std::setw(max_name_length + 2) << "event, " << std::setw(time_length + 2)
                   << "time, " << std::setw(value_length) << "value" << std::endl;
        }
        for (auto& thread_list : thread_events) {
            for (auto& record : thread_list) {
                output << std::setw(max_name_length) << names[record.type] << ", "
                       << std::setw(time_length) << to_us(record) << ", "
                       << std::setw(value_length) << record.value
                       << std::endl;
            }
        }
    }
};  // struct BackgroundTracker
#else
// NO-OP implementation with same public API
// same behavior as PerfEventBlock
struct BackgroundTracker {
    using event = PerfEvent::event;
    using clock_t = std::chrono::steady_clock;
    using counter_t = uint64_t;

    PerfEventBlock perf;

    struct Record {
        unsigned type;
        clock_t::time_point time;
        counter_t value;
    };

    BackgroundTracker(std::vector<std::string>& names, uint64_t scale = 1,
                      BenchmarkParameters params = {}, bool printHeader = true,
                      unsigned freq_us = 10, std::ostream& output = std::cerr)
        : perf(scale, params, printHeader) {
        if (GLOBAL_TRACKER) { throw std::logic_error("BackgroundTracker already exists"); }
        GLOBAL_TRACKER = this;
    };

    ~BackgroundTracker() { GLOBAL_TRACKER = nullptr; }

    inline void push_event(unsigned event_id, counter_t value) {}
    inline void push_event(std::vector<Record>& list, unsigned event_id, counter_t value) {}
    inline void push_event(const std::string& event_name, counter_t value) {}
    inline void push_event(std::vector<Record>& list, const std::string& event_name, counter_t value) {}
    inline unsigned id_for_name(const std::string& ev_name) const { return -1; }
};  // struct BackgroundTracker
#endif
