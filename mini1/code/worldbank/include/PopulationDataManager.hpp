#ifndef POPULATION_DATA_MANAGER_HPP
#define POPULATION_DATA_MANAGER_HPP

#include <vector>
#include <map>
#include <string>
#include "PopulationDTO.hpp"

class PopulationDataManager {
private:
    // Storage: Vector of all country data (baseline approach)
    std::vector<PopulationDTO> countries;
    // std::map<std::string, int> countryCodeIndex; // Map country code to index in countries vector

public:
    // Constructor
    PopulationDataManager() = default;
    
    // Load data from World Bank CSV file
    void loadFromCSV(const std::string& filename);
    
    // Clear all data
    void clear();
    
    // Query methods
    size_t getCountryCount() const;
    
    // Point query: Get population for a specific country and year
    long getPopulation(const std::string& countryCode, int year) const;
    
    // Get all data for a country
    const PopulationDTO* getCountryData(const std::string& countryCode) const;
    
    // Range query: Get time series for a country
    std::vector<long> getTimeSeries(const std::string& countryCode, 
                                    int startYear, int endYear) const;
    
    // Get all countries (for iteration/analysis)
    const std::vector<PopulationDTO>& getAllCountries() const;
};

#endif // POPULATION_DATA_MANAGER_HPP