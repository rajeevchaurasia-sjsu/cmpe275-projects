#ifndef POPULATION_DATA_MANAGER_MAP_HPP
#define POPULATION_DATA_MANAGER_MAP_HPP

#include <map>
#include <string>
#include "PopulationDTO.hpp"

class PopulationDataManagerMap {
private:
    // Storage: Map indexed by country code
    std::map<std::string, PopulationDTO> countriesMap;

public:
    // Constructor
    PopulationDataManagerMap() = default;
    
    // Load data from World Bank CSV file
    void loadFromCSV(const std::string& filename);
    
    // Clear all data
    void clear();
    
    // Query methods (same interface as vector version)
    size_t getCountryCount() const;
    
    // Point query: Get population for a specific country and year
    long getPopulation(const std::string& countryCode, int year) const;
    
    // Get all data for a country
    const PopulationDTO* getCountryData(const std::string& countryCode) const;
    
    // Range query: Get time series for a country
    std::vector<long> getTimeSeries(const std::string& countryCode, 
                                    int startYear, int endYear) const;
    
    // Get all countries (returns map reference)
    const std::map<std::string, PopulationDTO>& getAllCountries() const;
};

#endif // POPULATION_DATA_MANAGER_MAP_HPP