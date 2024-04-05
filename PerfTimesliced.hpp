#pragma once
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
    // initialized last
    std::thread measurement_thread;

    TimeslicedPerfBlock(PerfEvent& instance, std::chrono::milliseconds sleep_ms, size_t scale = 1)
        : instance(instance)
        , sleep_ms(sleep_ms)
        , scale(scale)
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
            std::this_thread::sleep_for(sleep_ms);
        }
    }
};
