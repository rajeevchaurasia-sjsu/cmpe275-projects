#include <iostream>
#include <iomanip>
#include "PopulationDTO.hpp"
#include "PopulationDataManager.hpp"
#include "PopulationDataManagerMap.hpp"
#include "PopulationDataManagerHash.hpp"  // ⬅️ Add this
#include "../utils/BenchMarkTimer.hpp"

void printSeparator(const std::string& title = "") {
    std::cout << "\n================================================" << std::endl;
    if (!title.empty()) {
        std::cout << "  " << title << std::endl;
        std::cout << "================================================" << std::endl;
    }
}

int main() {
    std::cout << "=== Vector vs Map vs Hash Implementation Comparison ===" << std::endl;
    
    std::string csvPath = "../../../data/worldbank/API_SP.POP.TOTL_DS2_en_csv_v2_3401680.csv";
    
    // ============================================
    // TEST 1: Loading Performance
    // ============================================
    printSeparator("TEST 1: CSV Loading Performance");
    
    PopulationDataManager vectorImpl;
    PopulationDataManagerMap mapImpl;
    PopulationDataManagerHash hashImpl;  // ⬅️ Add this
    
    long vectorLoadTime, mapLoadTime, hashLoadTime;  // ⬅️ Add hashLoadTime
    
    {
        BenchmarkTimer timer("Vector Load", false);
        vectorImpl.loadFromCSV(csvPath);
        vectorLoadTime = timer.getMilliseconds();
        std::cout << "[Vector] Load time: " << vectorLoadTime << " ms" << std::endl;
    }
    
    {
        BenchmarkTimer timer("Map Load", false);
        mapImpl.loadFromCSV(csvPath);
        mapLoadTime = timer.getMilliseconds();
        std::cout << "[Map] Load time: " << mapLoadTime << " ms" << std::endl;
    }
    
    // ⬅️ Add hash loading test
    {
        BenchmarkTimer timer("Hash Load", false);
        hashImpl.loadFromCSV(csvPath);
        hashLoadTime = timer.getMilliseconds();
        std::cout << "[Hash] Load time: " << hashLoadTime << " ms" << std::endl;
    }
    
    std::cout << "\nCountries loaded:" << std::endl;
    std::cout << "  Vector: " << vectorImpl.getCountryCount() << std::endl;
    std::cout << "  Map: " << mapImpl.getCountryCount() << std::endl;
    std::cout << "  Hash: " << hashImpl.getCountryCount() << std::endl;  // ⬅️ Add this
    
    // ============================================
    // TEST 2: Single Point Query Performance
    // ============================================
    printSeparator("TEST 2: Single Point Query (USA, 2020)");
    
    long vectorQueryTime, mapQueryTime, hashQueryTime;  // ⬅️ Add hashQueryTime
    long vectorResult, mapResult, hashResult;  // ⬅️ Add hashResult
    
    {
        BenchmarkTimer timer("Vector Query", false);
        vectorResult = vectorImpl.getPopulation("USA", 2020);
        vectorQueryTime = timer.getMicroseconds();
        std::cout << "[Vector] Query time: " << vectorQueryTime << " µs, Result: " << vectorResult << std::endl;
    }
    
    {
        BenchmarkTimer timer("Map Query", false);
        mapResult = mapImpl.getPopulation("USA", 2020);
        mapQueryTime = timer.getMicroseconds();
        std::cout << "[Map] Query time: " << mapQueryTime << " µs, Result: " << mapResult << std::endl;
    }
    
    // ⬅️ Add hash query test
    {
        BenchmarkTimer timer("Hash Query", false);
        hashResult = hashImpl.getPopulation("USA", 2020);
        hashQueryTime = timer.getMicroseconds();
        std::cout << "[Hash] Query time: " << hashQueryTime << " µs, Result: " << hashResult << std::endl;
    }
    
    if (vectorResult == mapResult && mapResult == hashResult) {
        std::cout << "✓ All results match!" << std::endl;
    } else {
        std::cout << "✗ WARNING: Results differ!" << std::endl;
    }
    
    // ============================================
    // TEST 3: Multiple Point Queries (1000x)
    // ============================================
    printSeparator("TEST 3: 1000 Point Queries (Random Countries)");
    
    std::vector<std::string> testCountries = {"USA", "IND", "CHN", "BRA", "DEU", "JPN", "GBR", "FRA", "ITA", "CAN"};
    const int numQueries = 1000;
    
    long vectorTotalTime, mapTotalTime, hashTotalTime;  // ⬅️ Add hashTotalTime
    
    {
        BenchmarkTimer timer("Vector 1000 Queries", false);
        for (int i = 0; i < numQueries; i++) {
            vectorImpl.getPopulation(testCountries[i % testCountries.size()], 2020);
        }
        vectorTotalTime = timer.getMicroseconds();
        std::cout << "[Vector] Total: " << vectorTotalTime << " µs, Avg: " 
                  << (vectorTotalTime / numQueries) << " µs per query" << std::endl;
    }
    
    {
        BenchmarkTimer timer("Map 1000 Queries", false);
        for (int i = 0; i < numQueries; i++) {
            mapImpl.getPopulation(testCountries[i % testCountries.size()], 2020);
        }
        mapTotalTime = timer.getMicroseconds();
        std::cout << "[Map] Total: " << mapTotalTime << " µs, Avg: " 
                  << (mapTotalTime / numQueries) << " µs per query" << std::endl;
    }
    
    // ⬅️ Add hash bulk query test
    {
        BenchmarkTimer timer("Hash 1000 Queries", false);
        for (int i = 0; i < numQueries; i++) {
            hashImpl.getPopulation(testCountries[i % testCountries.size()], 2020);
        }
        hashTotalTime = timer.getMicroseconds();
        std::cout << "[Hash] Total: " << hashTotalTime << " µs, Avg: " 
                  << (hashTotalTime / numQueries) << " µs per query" << std::endl;
    }
    
    // ============================================
    // TEST 4: Time Series Query
    // ============================================
    printSeparator("TEST 4: Time Series Query (India 1960-2023)");
    
    {
        BenchmarkTimer timer("Vector Time Series", false);
        auto series = vectorImpl.getTimeSeries("IND", 1960, 2023);
        long time = timer.getMicroseconds();
        std::cout << "[Vector] Time: " << time << " µs, Data points: " << series.size() << std::endl;
    }
    
    {
        BenchmarkTimer timer("Map Time Series", false);
        auto series = mapImpl.getTimeSeries("IND", 1960, 2023);
        long time = timer.getMicroseconds();
        std::cout << "[Map] Time: " << time << " µs, Data points: " << series.size() << std::endl;
    }
    
    // ⬅️ Add hash time series test
    {
        BenchmarkTimer timer("Hash Time Series", false);
        auto series = hashImpl.getTimeSeries("IND", 1960, 2023);
        long time = timer.getMicroseconds();
        std::cout << "[Hash] Time: " << time << " µs, Data points: " << series.size() << std::endl;
    }
    
    // ============================================
    // Summary
    // ============================================
    printSeparator("SUMMARY");
    
    std::cout << std::fixed << std::setprecision(2);
    
    std::cout << "\nLoad Performance:" << std::endl;
    std::cout << "  Vector: " << vectorLoadTime << " ms (baseline)" << std::endl;
    std::cout << "  Map:    " << mapLoadTime << " ms (" 
              << (vectorLoadTime / (double)mapLoadTime) << "x)" << std::endl;
    std::cout << "  Hash:   " << hashLoadTime << " ms (" 
              << (vectorLoadTime / (double)hashLoadTime) << "x)" << std::endl;
    
    std::cout << "\nSingle Query Performance:" << std::endl;
    std::cout << "  Vector: " << vectorQueryTime << " µs (baseline)" << std::endl;
    std::cout << "  Map:    " << mapQueryTime << " µs (" 
              << (vectorQueryTime / (double)mapQueryTime) << "x faster)" << std::endl;
    std::cout << "  Hash:   " << hashQueryTime << " µs (" 
              << (vectorQueryTime / (double)hashQueryTime) << "x faster)" << std::endl;
    
    std::cout << "\nBulk Query Performance (1000 queries):" << std::endl;
    std::cout << "  Vector: " << vectorTotalTime << " µs (baseline)" << std::endl;
    std::cout << "  Map:    " << mapTotalTime << " µs (" 
              << (vectorTotalTime / (double)mapTotalTime) << "x faster)" << std::endl;
    std::cout << "  Hash:   " << hashTotalTime << " µs (" 
              << (vectorTotalTime / (double)hashTotalTime) << "x faster)" << std::endl;
    
    std::cout << "\n================================================" << std::endl;
    std::cout << "Comparison completed successfully!" << std::endl;
    
    return 0;
}