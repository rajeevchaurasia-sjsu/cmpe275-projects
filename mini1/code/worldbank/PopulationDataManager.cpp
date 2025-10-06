#include "include/PopulationDataManager.hpp"
#include "include/WorldBankCSVLoader.hpp"

void PopulationDataManager::loadFromCSV(const std::string& filename) {
    
    // Use common CSV loader with callback to store data
    WorldBankCSVLoader::loadFromCSV(filename, [this](const PopulationDTO& dto) {
        countries.push_back(dto);
    });
    
}

void PopulationDataManager::clear() {
    countries.clear();
}

size_t PopulationDataManager::getCountryCount() const {
    return countries.size();
}

long PopulationDataManager::getPopulation(const std::string& countryCode, int year) const {
    const PopulationDTO* country = getCountryData(countryCode);
    if (country == nullptr) {
        return -1; // Country not found
    }
    return country->getPopulationForYear(year);
}

const PopulationDTO* PopulationDataManager::getCountryData(const std::string& countryCode) const {
    // Linear search (baseline - can be optimized later)
    for (const auto& country : countries) {
        if (country.getCountryCode() == countryCode) {
            return &country;
        }
    }
    return nullptr; // Not found
}

std::vector<long> PopulationDataManager::getTimeSeries(const std::string& countryCode, 
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

const std::vector<PopulationDTO>& PopulationDataManager::getAllCountries() const {
    return countries;
}