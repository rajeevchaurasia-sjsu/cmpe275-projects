#include <iostream>
#include <iomanip>
#include "AirQualityDataManager.hpp"
#include "utils/BenchMarkTimer.hpp"

void printSeparator() {
    std::cout << "================================================" << std::endl;
}

void compareLoadingPerformance(const std::string &dataRoot) {
    std::cout << "\n=== LOADING PERFORMANCE COMPARISON ===" << std::endl;
    printSeparator();
    
    // Serial loading
    AirQualityDataManager serialManager;
    BenchmarkTimer serialTimer;
    
    std::cout << "\n[SERIAL] Loading full dataset..." << std::endl;
    serialTimer.start();
    serialManager.loadFromDirectory(dataRoot);
    serialTimer.stop();
    
    int serialCount = serialManager.getReadingCount();
    long long serialTime = serialTimer.getMilliseconds();
    
    std::cout << "✓ Serial: " << serialCount << " readings in " 
              << serialTime << " ms" << std::endl;
    
    // Parallel loading with different thread counts
    std::vector<int> threadCounts = {2, 4, 8};
    
    for (int threads : threadCounts) {
        AirQualityDataManager parallelManager;
        BenchmarkTimer parallelTimer;
        
        std::cout << "\n[PARALLEL - " << threads << " threads] Loading full dataset..." << std::endl;
        parallelTimer.start();
        parallelManager.loadFromDirectoryParallel(dataRoot, threads);
        parallelTimer.stop();
        
        int parallelCount = parallelManager.getReadingCount();
        long long parallelTime = parallelTimer.getMilliseconds();
        double speedup = (double)serialTime / parallelTime;
        
        std::cout << "✓ Parallel (" << threads << "): " << parallelCount 
                  << " readings in " << parallelTime << " ms" << std::endl;
        std::cout << "  Speedup: " << std::fixed << std::setprecision(2) 
                  << speedup << "x" << std::endl;
    }
}

void compareQueryPerformance(AirQualityDataManager &manager) {
    std::cout << "\n=== QUERY PERFORMANCE COMPARISON ===" << std::endl;
    printSeparator();
    
    // Serial range query
    BenchmarkTimer serialTimer;
    serialTimer.start();
    auto serialResults = manager.getReadingsByAQIRange(50, 100);
    serialTimer.stop();
    
    std::cout << "\n[SERIAL] Range query (AQI 50-100):" << std::endl;
    std::cout << "  Time: " << serialTimer.getMicroseconds() << " μs" << std::endl;
    std::cout << "  Results: " << serialResults.size() << std::endl;
    
    // Parallel range query
    BenchmarkTimer parallelTimer;
    parallelTimer.start();
    auto parallelResults = manager.getReadingsByAQIRangeParallel(50, 100);
    parallelTimer.stop();
    
    std::cout << "\n[PARALLEL] Range query (AQI 50-100):" << std::endl;
    std::cout << "  Time: " << parallelTimer.getMicroseconds() << " μs" << std::endl;
    std::cout << "  Results: " << parallelResults.size() << std::endl;
    
    double speedup = (double)serialTimer.getMicroseconds() / parallelTimer.getMicroseconds();
    std::cout << "  Speedup: " << std::fixed << std::setprecision(2) 
              << speedup << "x" << std::endl;
}

void compareAggregationPerformance(AirQualityDataManager &manager) {
    std::cout << "\n=== AGGREGATION PERFORMANCE COMPARISON ===" << std::endl;
    printSeparator();
    
    std::string pollutant = "PM2.5";
    
    // Serial average
    BenchmarkTimer serialAvgTimer;
    serialAvgTimer.start();
    double serialAvg = manager.getAveragePollutantValue(pollutant);
    serialAvgTimer.stop();
    
    // Parallel average
    BenchmarkTimer parallelAvgTimer;
    parallelAvgTimer.start();
    double parallelAvg = manager.getAveragePollutantValueParallel(pollutant);
    parallelAvgTimer.stop();
    
    std::cout << "\n[AVERAGE " << pollutant << "]" << std::endl;
    std::cout << "  Serial: " << serialAvgTimer.getMicroseconds() 
              << " μs (result=" << serialAvg << ")" << std::endl;
    std::cout << "  Parallel: " << parallelAvgTimer.getMicroseconds() 
              << " μs (result=" << parallelAvg << ")" << std::endl;
    std::cout << "  Speedup: " << std::fixed << std::setprecision(2)
              << ((double)serialAvgTimer.getMicroseconds() / parallelAvgTimer.getMicroseconds()) 
              << "x" << std::endl;
    
    // Serial count
    BenchmarkTimer serialCountTimer;
    serialCountTimer.start();
    int serialCount = manager.countReadingsAboveAQI(100);
    serialCountTimer.stop();
    
    // Parallel count
    BenchmarkTimer parallelCountTimer;
    parallelCountTimer.start();
    int parallelCount = manager.countReadingsAboveAQIParallel(100);
    parallelCountTimer.stop();
    
    std::cout << "\n[COUNT AQI > 100]" << std::endl;
    std::cout << "  Serial: " << serialCountTimer.getMicroseconds() 
              << " μs (count=" << serialCount << ")" << std::endl;
    std::cout << "  Parallel: " << parallelCountTimer.getMicroseconds() 
              << " μs (count=" << parallelCount << ")" << std::endl;
    std::cout << "  Speedup: " << std::fixed << std::setprecision(2)
              << ((double)serialCountTimer.getMicroseconds() / parallelCountTimer.getMicroseconds()) 
              << "x" << std::endl;
}

int main() {
    std::cout << "\n";
    std::cout << "================================================" << std::endl;
    std::cout << "  SERIAL vs PARALLEL PERFORMANCE COMPARISON" << std::endl;
    std::cout << "================================================" << std::endl;
    
    std::string dataRoot = "../../data/2020-fire/data";
    
    // Test 1: Loading performance
    compareLoadingPerformance(dataRoot);
    
    // Load data once for query/aggregation tests
    std::cout << "\n\nLoading data for query/aggregation tests..." << std::endl;
    AirQualityDataManager manager;
    manager.loadFromDirectory(dataRoot);
    std::cout << "Loaded " << manager.getReadingCount() << " readings" << std::endl;
    
    // Test 2: Query performance
    compareQueryPerformance(manager);
    
    // Test 3: Aggregation performance
    compareAggregationPerformance(manager);
    
    std::cout << "\n";
    printSeparator();
    std::cout << "✓ All comparisons completed!" << std::endl;
    
    return 0;
}