#include <chrono>
#include <string>
#include <iostream>

class BenchmarkTimer {
private:
    std::chrono::high_resolution_clock::time_point startTime;
    std::chrono::high_resolution_clock::time_point endTime;
    std::string operationName;
    bool running;
    bool autoPrint;
    
public:
    // Constructor - optionally auto-print on destruction
    BenchmarkTimer(const std::string &name = "", bool autoPrint = false);
    
    // Destructor - auto-prints if enabled
    ~BenchmarkTimer();
    
    // Start the timer
    void start();
    
    // Stop the timer
    void stop();
    
    // Get elapsed time in different units
    long long getMilliseconds() const;
    long long getMicroseconds() const;
    double getSeconds() const;
    
    // Print the timing result
    void print() const;
    
    // Static utility - measure a function execution time
    template<typename Func>
    static long long measureMilliseconds(Func func) {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    }
};