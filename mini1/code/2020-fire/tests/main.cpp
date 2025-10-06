#include <iostream>
#include "AirQualityDataManager.hpp"
#include "BenchMarkTimer.hpp"

void printSeparator() {
    std::cout << "================================================" << std::endl;
}

int main() {
    std::cout << "=== Air Quality Data Manager Test ===" << std::endl;
    printSeparator();
    
    AirQualityDataManager manager;
    
    // ============================================================
    // TEST LEVEL 1: Single CSV file (Quick validation)
    // ============================================================
    std::cout << "\n[TEST 1] Loading a SINGLE CSV file..." << std::endl;
    std::string testFile = "../../data/2020-fire/data/20200818/20200818-09.csv";
    
    BenchmarkTimer timer1("Single file load");
    timer1.start();
    manager.loadFromCSV(testFile);
    timer1.stop();
    
    std::cout << "✓ Loaded " << manager.getReadingCount() << " readings in " 
              << timer1.getMilliseconds() << " ms" << std::endl;
    
    // Quick verification
    auto pollutants = manager.getAllPollutantTypes();
    std::cout << "✓ Found pollutant types: ";
    for (const auto &p : pollutants) {
        std::cout << p << " ";
    }
    std::cout << std::endl;
    
    printSeparator();
    
    
    // ============================================================
    // TEST LEVEL 2: Single date folder (Medium test)
    // ============================================================
    std::cout << "\n[TEST 2] Loading ONE DATE FOLDER..." << std::endl;
    manager.clear();  // Clear previous data
    
    std::string dateFolder = "../../data/2020-fire/data/20200818";
    
    BenchmarkTimer timer2("Date folder load");
    timer2.start();
    manager.loadFromDateFolder(dateFolder);
    timer2.stop();
    
    std::cout << "✓ Loaded " << manager.getReadingCount() << " readings in " 
              << timer2.getMilliseconds() << " ms" << std::endl;
    
    // Statistics
    double avgPM25 = manager.getAveragePollutantValue("PM2.5");
    double maxPM25 = manager.getMaxPollutantValue("PM2.5");
    int highAQI = manager.countReadingsAboveAQI(100);
    
    std::cout << "✓ Average PM2.5: " << avgPM25 << std::endl;
    std::cout << "✓ Maximum PM2.5: " << maxPM25 << std::endl;
    std::cout << "✓ High AQI readings (>100): " << highAQI << std::endl;
    
    printSeparator();
    
    
    // ============================================================
    // TEST LEVEL 3: ALL FOLDERS (Full dataset)
    // ============================================================
    std::cout << "\n[TEST 3] Loading ENTIRE DATASET (all folders)..." << std::endl;
    std::cout << "This may take a while..." << std::endl;
    manager.clear();  // Clear previous data
    
    std::string rootPath = "../../data/2020-fire/data";
    
    BenchmarkTimer timer3("Full dataset load");
    timer3.start();
    manager.loadFromDirectory(rootPath);
    timer3.stop();
    
    std::cout << "\n✓ COMPLETE! Loaded " << manager.getReadingCount() 
              << " total readings" << std::endl;
    std::cout << "✓ Loading time: " << timer3.getMilliseconds() << " ms ("
              << timer3.getSeconds() << " seconds)" << std::endl;
    
    // Detailed statistics
    auto dates = manager.getAllDates();
    auto allPollutants = manager.getAllPollutantTypes();
    
    std::cout << "\n--- Dataset Summary ---" << std::endl;
    std::cout << "Total readings: " << manager.getReadingCount() << std::endl;
    std::cout << "Unique dates: " << dates.size() << std::endl;
    std::cout << "Pollutant types: ";
    for (const auto &p : allPollutants) {
        auto readings = manager.getReadingsByPollutant(p);
        std::cout << p << " (" << readings.size() << ") ";
    }
    std::cout << std::endl;
    
    // Overall statistics
    std::cout << "\n--- Air Quality Statistics ---" << std::endl;
    for (const auto &pollutant : allPollutants) {
        double avg = manager.getAveragePollutantValue(pollutant);
        double max = manager.getMaxPollutantValue(pollutant);
        std::cout << pollutant << ": avg=" << avg << ", max=" << max << std::endl;
    }
    
    int hazardous = manager.countReadingsAboveAQI(150);
    int unhealthy = manager.countReadingsAboveAQI(100);
    std::cout << "\nReadings with AQI > 100 (Unhealthy): " << unhealthy << std::endl;
    std::cout << "Readings with AQI > 150 (Hazardous): " << hazardous << std::endl;
    
    printSeparator();
    
    
    // ============================================================
    // TEST LEVEL 4: Query performance tests
    // ============================================================
    std::cout << "\n[TEST 4] Query Performance Tests..." << std::endl;
    
    // Test indexed query (should be fast - O(log n))
    BenchmarkTimer timer4a;
    timer4a.start();
    auto pm25Results = manager.getReadingsByPollutant("PM2.5");
    timer4a.stop();
    std::cout << "✓ Indexed query (by pollutant): " << timer4a.getMicroseconds() 
              << " μs, found " << pm25Results.size() << " readings" << std::endl;
    
    // Test range query (requires scan - O(n))
    BenchmarkTimer timer4b;
    timer4b.start();
    auto aqiResults = manager.getReadingsByAQIRange(50, 100);
    timer4b.stop();
    std::cout << "✓ Range query (AQI 50-100): " << timer4b.getMicroseconds() 
              << " μs, found " << aqiResults.size() << " readings" << std::endl;
    
    // Test aggregation (requires scan and calculation - O(n))
    BenchmarkTimer timer4c;
    timer4c.start();
    double avgAll = manager.getAveragePollutantValue("PM2.5");
    timer4c.stop();
    std::cout << "✓ Aggregation (average=" << avgAll << "): " 
              << timer4c.getMicroseconds() << " μs" << std::endl;
    
    printSeparator();
    
    std::cout << "\n=== All tests completed successfully! ===" << std::endl;
    
    return 0;
}