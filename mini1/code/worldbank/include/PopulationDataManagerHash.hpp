#ifndef POPULATION_DATA_MANAGER_HASH_HPP
#define POPULATION_DATA_MANAGER_HASH_HPP

#include <unordered_map>
#include <string>
#include <vector>
#include "PopulationDTO.hpp"

class PopulationDataManagerHash {
private:

    std::unordered_map<std::string, PopulationDTO> countriesHash;

public:
    PopulationDataManagerHash() = default;
    
    void loadFromCSV(const std::string& filename);
    
    void clear();
    
    size_t getCountryCount() const;
    
    long getPopulation(const std::string& countryCode, int year) const;
    
    const PopulationDTO* getCountryData(const std::string& countryCode) const;
    
    std::vector<long> getTimeSeries(const std::string& countryCode, 
                                    int startYear, int endYear) const;
    
    const std::unordered_map<std::string, PopulationDTO>& getAllCountries() const;
};

#endif // POPULATION_DATA_MANAGER_HASH_HPP