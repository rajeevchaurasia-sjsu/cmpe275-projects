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

public:
    PopulationDataManager() = default;
    
    void loadFromCSV(const std::string& filename);
    
    void clear();
    
    size_t getCountryCount() const;
    
    long getPopulation(const std::string& countryCode, int year) const;
    
    const PopulationDTO* getCountryData(const std::string& countryCode) const;
    
    std::vector<long> getTimeSeries(const std::string& countryCode, 
                                    int startYear, int endYear) const;
    
    const std::vector<PopulationDTO>& getAllCountries() const;
};

#endif // POPULATION_DATA_MANAGER_HPP