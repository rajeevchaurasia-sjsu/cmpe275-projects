#include "WorldBankCSVLoader.hpp"
#include "CSVParser.hpp"
#include <fstream>
#include <iostream>
#include <sstream>

int WorldBankCSVLoader::loadFromCSV(
    const std::string& filename,
    std::function<void(const PopulationDTO&)> callback
) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << filename << std::endl;
        return 0;
    }
    
    if (!skipMetadataLines(file)) {
        std::cerr << "Error: Failed to skip metadata lines" << std::endl;
        return 0;
    }
    
    std::cout << "Skipped metadata lines, starting data load..." << std::endl;
    
    std::string line;
    int countriesLoaded = 0;
    
    while (std::getline(file, line)) {
        if (CSVParser::isEmpty(line)) continue;
        
        std::vector<std::string> fields = CSVParser::parseLine(line);
        
        // World Bank CSV: 4 metadata + 64 years = 68 fields
        if (fields.size() < 68) {
            std::cerr << "Warning: Skipping malformed line (expected 68 fields, got " 
                      << fields.size() << ")" << std::endl;
            continue;
        }
        
        std::string countryName = CSVParser::removeQuotes(fields[0]);
        std::string countryCode = CSVParser::removeQuotes(fields[1]);
        
        PopulationDTO dto;
        dto.setCountryName(countryName);
        dto.setCountryCode(countryCode);
        
        // Parse population data for years 1960-2023 (columns 4-67)
        for (int i = 0; i < PopulationDTO::NUM_YEARS; i++) {
            std::string valueStr = CSVParser::trim(fields[4 + i]);
            
            // Handle missing data
            if (valueStr.empty() || valueStr == ".." || valueStr == "\"..\"") {
                continue; // Leave as -1
            }
            
            try {
                valueStr = CSVParser::removeQuotes(valueStr);
                long population = std::stol(valueStr);
                dto.setPopulationForYear(PopulationDTO::START_YEAR + i, population);
            } catch (const std::exception&) {
                // Skip invalid values silently
            }
        }
        
        callback(dto);
        countriesLoaded++;
    }
    
    file.close();
    std::cout << "Successfully loaded " << countriesLoaded << " countries" << std::endl;
    
    return countriesLoaded;
}

bool WorldBankCSVLoader::skipMetadataLines(std::ifstream& file) {
    std::string line;
    int lineCount = 0;
    
    // Skip first 5 lines (metadata + header)
    while (lineCount < 5 && std::getline(file, line)) {
        lineCount++;
    }
    
    return lineCount == 5;
}