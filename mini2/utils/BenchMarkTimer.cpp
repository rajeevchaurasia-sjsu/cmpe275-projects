// From mini1
#include "BenchMarkTimer.hpp"

BenchmarkTimer::BenchmarkTimer(const std::string &name, bool autoPrint)
    : operationName(name), running(false), autoPrint(autoPrint) {
    start();
}

BenchmarkTimer::~BenchmarkTimer() {
    if (autoPrint && running) {
        stop();
        print();
    }
}

void BenchmarkTimer::start() {
    startTime = std::chrono::high_resolution_clock::now();
    running = true;
}

void BenchmarkTimer::stop() {
    if (running) {
        endTime = std::chrono::high_resolution_clock::now();
        running = false;
    }
}

long long BenchmarkTimer::getMilliseconds() const {
    if (running) {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();
    }
    return std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
}

long long BenchmarkTimer::getMicroseconds() const {
    if (running) {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(now - startTime).count();
    }
    return std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();
}

double BenchmarkTimer::getSeconds() const {
    return getMilliseconds() / 1000.0;
}

void BenchmarkTimer::print() const {
    if (!operationName.empty()) {
        std::cout << operationName << ": ";
    }
    std::cout << getMilliseconds() << " ms";
    if (getMilliseconds() > 1000) {
        std::cout << " (" << getSeconds() << " seconds)";
    }
    std::cout << std::endl;
}