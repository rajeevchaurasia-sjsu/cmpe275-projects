#include <iostream>
#include <vector>
#include "PopulationDTO.hpp"
#include "PopulationDataManager.hpp"
#include "../utils/BenchMarkTimer.hpp"

int main() {
    std::cout << "=== World Bank Population Data Manager Test ===" << std::endl;
    std::cout << "================================================" << std::endl;
    
    PopulationDataManager manager;
    
    std::string csvPath = "../../../data/worldbank/API_SP.POP.TOTL_DS2_en_csv_v2_3401680.csv";
    
    std::cout << "\n--- Loading CSV Data ---" << std::endl;
    {
        BenchmarkTimer timer("Load CSV", true);
        manager.loadFromCSV(csvPath);
    }
    
    std::cout << "Total countries loaded: " << manager.getCountryCount() << std::endl;
    
    // Test 1: Point Query
    std::cout << "\n--- Test 1: Point Query ---" << std::endl;
    long usaPop2020 = manager.getPopulation("USA", 2020);
    std::cout << "USA population in 2020: " << usaPop2020 << std::endl;
    
    long indiaPop2020 = manager.getPopulation("IND", 2020);
    std::cout << "India population in 2020: " << indiaPop2020 << std::endl;

    long chinaPop2020 = manager.getPopulation("CHN", 2020);
    std::cout << "China population in 2020: " << (chinaPop2020 != -1 ? std::to_string(chinaPop2020) : "Data not available") << std::endl;
    
    // Test 2: Get Country Data
    std::cout << "\n--- Test 2: Get Country Data ---" << std::endl;
    const PopulationDTO* usa = manager.getCountryData("USA");
    if (usa != nullptr) {
        std::cout << "Country: " << usa->getCountryName() << std::endl;
        std::cout << "Code: " << usa->getCountryCode() << std::endl;
        std::cout << "Population in 1960: " << usa->getPopulationForYear(1960) << std::endl;
        std::cout << "Population in 2023: " << usa->getPopulationForYear(2023) << std::endl;
    }
    
    // Test 3: Time Series Query
    std::cout << "\n--- Test 3: Time Series Query (India 2015-2023) ---" << std::endl;
    {
        BenchmarkTimer timer("Time Series Query", true);
        std::vector<long> indiaSeries = manager.getTimeSeries("IND", 2015, 2023);
        
        for (size_t i = 0; i < indiaSeries.size(); i++) {
            int year = 2015 + i;
            std::cout << "  " << year << ": " << indiaSeries[i] << std::endl;
        }
    }
    
    // Test 4: Query Performance Test
    std::cout << "\n--- Test 4: Query Performance (1000 random queries) ---" << std::endl;
    {
        BenchmarkTimer timer("1000 Point Queries", true);
        for (int i = 0; i < 1000; i++) {
            manager.getPopulation("USA", 2020);
        }
    }
    
    std::cout << "\n================================================" << std::endl;
    std::cout << "All tests completed successfully!" << std::endl;
    
    return 0;
}