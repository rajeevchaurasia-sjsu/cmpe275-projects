#include "include/PopulationDataManagerHash.hpp"
#include "include/WorldBankCSVLoader.hpp"

void PopulationDataManagerHash::loadFromCSV(const std::string& filename) {
    // Use common CSV loader with callback to store data
    WorldBankCSVLoader::loadFromCSV(filename, [this](const PopulationDTO& dto) {
        countriesHash[dto.getCountryCode()] = dto;
    });
}

void PopulationDataManagerHash::clear() {
    countriesHash.clear();
}

size_t PopulationDataManagerHash::getCountryCount() const {
    return countriesHash.size();
}

long PopulationDataManagerHash::getPopulation(const std::string& countryCode, int year) const {
    const PopulationDTO* country = getCountryData(countryCode);
    if (country == nullptr) {
        return -1; // Country not found
    }
    return country->getPopulationForYear(year);
}

const PopulationDTO* PopulationDataManagerHash::getCountryData(const std::string& countryCode) const {
    // O(1) average lookup using hash map
    auto it = countriesHash.find(countryCode);
    if (it != countriesHash.end()) {
        return &(it->second);
    }
    return nullptr; // Not found
}

std::vector<long> PopulationDataManagerHash::getTimeSeries(const std::string& countryCode, 
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

const std::unordered_map<std::string, PopulationDTO>& PopulationDataManagerHash::getAllCountries() const {
    return countriesHash;
}