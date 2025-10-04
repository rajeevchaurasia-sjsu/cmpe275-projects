#include "include/PopulationDataManager.hpp"
#include "../utils/CSVParser.hpp"
#include <fstream>
#include <iostream>
#include <sstream>

void PopulationDataManager::loadFromCSV(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << filename << std::endl;
        return;
    }
    
    std::string line;
    int lineCount = 0;
    while (lineCount < 5 && std::getline(file, line)) {
        lineCount++;
    }
    std::cout << "Skipped " << lineCount << " header lines, starting data load..." << std::endl;
    
    // Read data lines
    while (std::getline(file, line)) {
        if (CSVParser::isEmpty(line)) continue;
        
        std::vector<std::string> fields = CSVParser::parseLine(line);
        
        // World Bank CSV has: 4 metadata columns + 64 year columns = 68 total
        if (fields.size() < 68) {
            std::cerr << "Warning: Skipping malformed line (expected 68 fields, got " 
                      << fields.size() << ")" << std::endl;
            continue;
        }
        
        // Extract metadata
        std::string countryName = CSVParser::removeQuotes(fields[0]);
        std::string countryCode = CSVParser::removeQuotes(fields[1]);
        // fields[2] = Indicator Name (usually "Population, total")
        // fields[3] = Indicator Code (usually "SP.POP.TOTL")
        
        // Create DTO and populate year data
        PopulationDTO dto;
        dto.setCountryName(countryName);
        dto.setCountryCode(countryCode);
        
        // Parse population data for each year (1960-2023)
        for (int i = 0; i < PopulationDTO::NUM_YEARS; i++) {
            int year = PopulationDTO::START_YEAR + i;
            std::string valueStr = CSVParser::trim(fields[4 + i]); // Years start at column 4
            
            if (valueStr.empty() || valueStr == ".." || valueStr == "\"..\"") {
                // Missing data - already initialized to -1 in DTO
                continue;
            }
            
            try {
                long population = std::stol(valueStr);
                dto.setPopulationForYear(year, population);
            } catch (const std::exception& e) {
                std::cerr << "Warning: Could not parse population value '" 
                          << valueStr << "' for " << countryCode 
                          << " in year " << year << std::endl;
            }
        }
        
        countries.push_back(dto);
    }
    
    file.close();
    std::cout << "Loaded " << countries.size() << " countries from " << filename << std::endl;
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