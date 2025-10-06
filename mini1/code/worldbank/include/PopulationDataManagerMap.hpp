#ifndef POPULATION_DATA_MANAGER_MAP_HPP
#define POPULATION_DATA_MANAGER_MAP_HPP

#include <map>
#include <string>
#include "PopulationDTO.hpp"

class PopulationDataManagerMap {
private:

    std::map<std::string, PopulationDTO> countriesMap;

public:
    PopulationDataManagerMap() = default;
    
    void loadFromCSV(const std::string& filename);
    
    void clear();
    
    size_t getCountryCount() const;    

    long getPopulation(const std::string& countryCode, int year) const;
    
    const PopulationDTO* getCountryData(const std::string& countryCode) const;
    
    std::vector<long> getTimeSeries(const std::string& countryCode, 
                                    int startYear, int endYear) const;
    
    const std::map<std::string, PopulationDTO>& getAllCountries() const;
};

#endif // POPULATION_DATA_MANAGER_MAP_HPP