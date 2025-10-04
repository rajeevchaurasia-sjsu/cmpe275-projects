#ifndef POPULATION_DATA_MANAGER_HASH_HPP
#define POPULATION_DATA_MANAGER_HASH_HPP

#include <unordered_map>
#include <string>
#include <vector>
#include "PopulationDTO.hpp"

class PopulationDataManagerHash {
private:
    // Storage: Hash map indexed by country code
    std::unordered_map<std::string, PopulationDTO> countriesHash;

public:
    // Constructor
    PopulationDataManagerHash() = default;
    
    // Load data from World Bank CSV file
    void loadFromCSV(const std::string& filename);
    
    // Clear all data
    void clear();
    
    // Query methods (same interface as other implementations)
    size_t getCountryCount() const;
    
    // Point query: Get population for a specific country and year
    long getPopulation(const std::string& countryCode, int year) const;
    
    // Get all data for a country
    const PopulationDTO* getCountryData(const std::string& countryCode) const;
    
    // Range query: Get time series for a country
    std::vector<long> getTimeSeries(const std::string& countryCode, 
                                    int startYear, int endYear) const;
    
    // Get all countries (returns unordered_map reference)
    const std::unordered_map<std::string, PopulationDTO>& getAllCountries() const;
};

#endif // POPULATION_DATA_MANAGER_HASH_HPP