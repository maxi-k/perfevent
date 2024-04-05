#pragma once
#include <functional>
#include <thread>
#include <utility>
#include <atomic>
#include <chrono>

#include "PerfEventViktor.hpp"

struct TimeslicedPerfBlock {
    PerfEvent& instance;
    std::chrono::milliseconds sleep_ms;
    size_t scale{1};
    std::atomic<bool> run{true};
    // callbacks for block start and end (e.g., to set additional params)
    // should obvisouly be vewry fast
    using callback_t = std::function<void(PerfEvent&)>;
    callback_t begin_cb, end_cb;
    // initialized last
    std::thread measurement_thread;

    static inline callback_t default_cb = [](PerfEvent& instance) {};
    TimeslicedPerfBlock(PerfEvent& instance, std::chrono::milliseconds sleep_ms, size_t scale = 1,
                        callback_t begin_cb = default_cb, callback_t end_cb = default_cb)
        : instance(instance)
        , sleep_ms(sleep_ms)
        , scale(scale)
        , begin_cb(std::move(begin_cb))
        , end_cb(std::move(end_cb))
        , measurement_thread([this]() { this->measure_loop(); }) {}

    ~TimeslicedPerfBlock() {
        run = false;
        measurement_thread.join();
    }

    void setScale(size_t scale) {
        this->scale = scale;
    }

private:
    void measure_loop() {
        while (run) {
            PerfEventBlock block(instance, scale);
            begin_cb(block.e);
            std::this_thread::sleep_for(sleep_ms);
            end_cb(block.e);
        }
    }
};
