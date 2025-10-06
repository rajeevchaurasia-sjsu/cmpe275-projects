#include "include/PopulationDataManagerMap.hpp"
#include "include/WorldBankCSVLoader.hpp"

void PopulationDataManagerMap::loadFromCSV(const std::string& filename) {
    
    WorldBankCSVLoader::loadFromCSV(filename, [this](const PopulationDTO& dto) {
        countriesMap[dto.getCountryCode()] = dto;
    });

}

void PopulationDataManagerMap::clear() {
    countriesMap.clear();
}

size_t PopulationDataManagerMap::getCountryCount() const {
    return countriesMap.size();
}

long PopulationDataManagerMap::getPopulation(const std::string& countryCode, int year) const {
    const PopulationDTO* country = getCountryData(countryCode);
    if (country == nullptr) {
        return -1; // Country not found
    }
    return country->getPopulationForYear(year);
}

const PopulationDTO* PopulationDataManagerMap::getCountryData(const std::string& countryCode) const {
    // O(log n) lookup using map
    auto it = countriesMap.find(countryCode);
    if (it != countriesMap.end()) {
        return &(it->second);
    }
    return nullptr; // Not found
}

std::vector<long> PopulationDataManagerMap::getTimeSeries(const std::string& countryCode, 
                                                          int startYear, int endYear) const {
    std::vector<long> result;
    const PopulationDTO* country = getCountryData(countryCode);
    
    if (country == nullptr) {
        return result; // Empty vector if country not found
    }
    
    for (int year = startYear; year <= endYear; year++) {
        result.push_back(country->getPopulationForYear(year));
    }
    
    return result;
}

const std::map<std::string, PopulationDTO>& PopulationDataManagerMap::getAllCountries() const {
    return countriesMap;
}