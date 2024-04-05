#pragma once
#include <thread>
#include <utility>
#include <chrono>

#include "PerfEvent.hpp"

struct TimeslicedPerfBlock {
    PerfEvent& instance;
    BenchmarkParameters params;
    std::chrono::milliseconds sleep_ms;
    size_t scale{1};
    bool run{true}, header{true};
    // initialized last
    std::thread measurement_thread;

    TimeslicedPerfBlock(PerfEvent& instance, BenchmarkParameters params, std::chrono::milliseconds sleep_ms, size_t scale = 1)
        : instance(instance)
        , params(std::move(params))
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

    BenchmarkParameters& getParams() { return params; }

private:
    void measure_loop() {
        while (run) {
            PerfEventBlock block(instance, scale, params, std::exchange(header, false));
            std::this_thread::sleep_for(sleep_ms);
        }
    }
};
