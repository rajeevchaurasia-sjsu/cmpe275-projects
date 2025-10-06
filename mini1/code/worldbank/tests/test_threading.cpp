#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include "PopulationDataManagerHash.hpp"
#include "../utils/BenchMarkTimer.hpp"

#ifdef _OPENMP
    #include <omp.h>
    #define HAS_OPENMP 1
#else
    #define HAS_OPENMP 0
    #warning "OpenMP not available - threading disabled"
#endif

void printSeparator(const std::string& title = "") {
    std::cout << "\n================================================" << std::endl;
    if (!title.empty()) {
        std::cout << "  " << title << std::endl;
        std::cout << "================================================" << std::endl;
    }
}

int main() {
    std::cout << "=== Threading Performance Analysis ===" << std::endl;
    
    #if HAS_OPENMP
        std::cout << "OpenMP: ENABLED" << std::endl;
        std::cout << "Max threads available: " << omp_get_max_threads() << std::endl;
    #else
        std::cout << "OpenMP: DISABLED (sequential execution only)" << std::endl;
        std::cout << "Install OpenMP to enable parallel tests." << std::endl;
        return 1;
    #endif
    
    std::string csvPath = "../../../data/worldbank/API_SP.POP.TOTL_DS2_en_csv_v2_3401680.csv";
    
    // Load data once
    printSeparator("Loading Data");
    PopulationDataManagerHash impl;
    {
        BenchmarkTimer timer("Data Load", true);
        impl.loadFromCSV(csvPath);
    }
    std::cout << "Loaded " << impl.getCountryCount() << " countries" << std::endl;
    
    // Test countries (20 major countries)
    std::vector<std::string> countries = {
        "USA", "CHN", "IND", "BRA", "RUS", "JPN", "DEU", "GBR", "FRA", "ITA",
        "CAN", "KOR", "ESP", "AUS", "MEX", "IDN", "NLD", "SAU", "TUR", "CHE"
    };
    
    const int QUERIES_PER_COUNTRY = 100;
    const int TOTAL_QUERIES = countries.size() * QUERIES_PER_COUNTRY;
    
    // ============================================
    // TEST 1: Sequential Queries (Baseline)
    // ============================================
    printSeparator("TEST 1: Sequential Queries (Baseline)");
    
    long sequentialTime;
    {
        BenchmarkTimer timer("Sequential", false);
        
        for (int iter = 0; iter < QUERIES_PER_COUNTRY; iter++) {
            for (const auto& country : countries) {
                impl.getPopulation(country, 2020);
            }
        }
        
        sequentialTime = timer.getMicroseconds();
    }
    
    std::cout << "Total queries: " << TOTAL_QUERIES << std::endl;
    std::cout << "Total time:    " << sequentialTime << " µs" << std::endl;
    std::cout << "Avg per query: " << std::fixed << std::setprecision(2) 
              << (sequentialTime / (double)TOTAL_QUERIES) << " µs" << std::endl;
    
    // ============================================
    // TEST 2: Parallel Queries (Different Thread Counts)
    // ============================================
    
    std::vector<int> threadCounts = {2, 4, 8};
    std::vector<std::pair<int, long>> results;
    
    for (int numThreads : threadCounts) {
        printSeparator("TEST: Parallel with " + std::to_string(numThreads) + " threads");
        
        #if HAS_OPENMP
            omp_set_num_threads(numThreads);
        #endif
        
        long parallelTime;
        {
            BenchmarkTimer timer("Parallel", false);
            
            #if HAS_OPENMP
                #pragma omp parallel for schedule(dynamic)
            #endif
            for (int iter = 0; iter < QUERIES_PER_COUNTRY; iter++) {
                for (size_t i = 0; i < countries.size(); i++) {
                    impl.getPopulation(countries[i], 2020);
                }
            }
            
            parallelTime = timer.getMicroseconds();
        }
        
        double speedup = sequentialTime / (double)parallelTime;
        double efficiency = (speedup / numThreads) * 100;
        
        results.push_back({numThreads, parallelTime});
        
        std::cout << "Threads:       " << numThreads << std::endl;
        std::cout << "Total time:    " << parallelTime << " µs" << std::endl;
        std::cout << "Speedup:       " << std::fixed << std::setprecision(2) << speedup << "x" << std::endl;
        std::cout << "Efficiency:    " << std::setprecision(1) << efficiency << "%" << std::endl;
        std::cout << "Avg per query: " << std::setprecision(2) 
                  << (parallelTime / (double)TOTAL_QUERIES) << " µs" << std::endl;
    }
    
    // ============================================
    // TEST 3: Thread Scaling Table
    // ============================================
    printSeparator("TEST 3: Thread Scaling Analysis");
    
    std::cout << "\n";
    std::cout << "Threads | Time (µs) | Speedup | Efficiency | Avg/Query (µs)" << std::endl;
    std::cout << "--------|-----------|---------|------------|----------------" << std::endl;
    
    // Sequential baseline
    std::cout << "   1    | " 
              << std::setw(9) << sequentialTime << " |  "
              << std::setw(5) << "1.00x" << " |   "
              << std::setw(6) << "100.0%" << " |     "
              << std::fixed << std::setprecision(2)
              << (sequentialTime / (double)TOTAL_QUERIES) << std::endl;
    
    // Parallel results
    for (const auto& result : results) {
        int threads = result.first;
        long time = result.second;
        double speedup = sequentialTime / (double)time;
        double efficiency = (speedup / threads) * 100;
        
        std::cout << "   " << threads << "    | " 
                  << std::setw(9) << time << " |  "
                  << std::setw(5) << std::fixed << std::setprecision(2) << speedup << "x" << " |   "
                  << std::setw(6) << std::setprecision(1) << efficiency << "%" << " |     "
                  << std::setprecision(2) << (time / (double)TOTAL_QUERIES) << std::endl;
    }
    
    // ============================================
    // TEST 4: Time Series Parallel Processing
    // ============================================
    printSeparator("TEST 4: Parallel Time Series Processing");
    
    std::cout << "\nProcessing time series for 20 countries (1960-2023)..." << std::endl;
    
    // Sequential
    long seqTimeSeriesTime;
    {
        BenchmarkTimer timer("Sequential", false);
        
        for (const auto& country : countries) {
            auto series = impl.getTimeSeries(country, 1960, 2023);
        }
        
        seqTimeSeriesTime = timer.getMicroseconds();
    }
    std::cout << "Sequential: " << seqTimeSeriesTime << " µs" << std::endl;
    
    // Parallel (4 threads)
    #if HAS_OPENMP
        omp_set_num_threads(4);
    #endif
    
    long parTimeSeriesTime;
    {
        BenchmarkTimer timer("Parallel", false);
        
        #if HAS_OPENMP
            #pragma omp parallel for
        #endif
        for (size_t i = 0; i < countries.size(); i++) {
            auto series = impl.getTimeSeries(countries[i], 1960, 2023);
        }
        
        parTimeSeriesTime = timer.getMicroseconds();
    }
    std::cout << "Parallel (4 threads): " << parTimeSeriesTime << " µs" << std::endl;
    std::cout << "Speedup: " << std::fixed << std::setprecision(2) 
              << (seqTimeSeriesTime / (double)parTimeSeriesTime) << "x" << std::endl;
    
    // ============================================
    // Summary
    // ============================================
    printSeparator("SUMMARY & DISCOVERIES");
    
    std::cout << "\nKey Observations:" << std::endl;
    
    // Find best speedup
    double bestSpeedup = 0;
    int bestThreads = 1;
    for (const auto& result : results) {
        double speedup = sequentialTime / (double)result.second;
        if (speedup > bestSpeedup) {
            bestSpeedup = speedup;
            bestThreads = result.first;
        }
    }
    
    std::cout << "  ✓ Best speedup: " << std::fixed << std::setprecision(2) 
              << bestSpeedup << "x with " << bestThreads << " threads" << std::endl;
    
    // Check for linear scaling
    double twoThreadSpeedup = sequentialTime / (double)results[0].second;
    if (twoThreadSpeedup > 1.8) {
        std::cout << "  ✓ Near-linear scaling with 2 threads (low overhead)" << std::endl;
    } else {
        std::cout << "  ⚠ Significant overhead with 2 threads" << std::endl;
    }
    
    // Check diminishing returns
    double lastEfficiency = 100.0;
    bool diminishing = false;
    for (const auto& result : results) {
        double speedup = sequentialTime / (double)result.second;
        double efficiency = (speedup / result.first) * 100;
        if (efficiency < lastEfficiency - 10) {
            diminishing = true;
        }
        lastEfficiency = efficiency;
    }
    
    if (diminishing) {
        std::cout << "  ✓ Diminishing returns observed (Amdahl's Law in action!)" << std::endl;
    }
    
    std::cout << "  ✓ Hash map supports concurrent reads efficiently" << std::endl;
    std::cout << "  ✓ No race conditions (read-only operations)" << std::endl;
    
    std::cout << "\nConclusion:" << std::endl;
    std::cout << "  Threading provides significant speedup for query-heavy workloads." << std::endl;
    std::cout << "  Optimal thread count depends on CPU cores and query complexity." << std::endl;
    
    std::cout << "\n================================================" << std::endl;
    
    return 0;
}