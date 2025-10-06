#ifndef WORLDBANK_CSV_LOADER_HPP
#define WORLDBANK_CSV_LOADER_HPP

#include <string>
#include <vector>
#include <functional>
#include "PopulationDTO.hpp"

/**
 * WorldBankCSVLoader - Common CSV parsing logic for World Bank data
 * 
 * Extracts the duplicate CSV parsing code and provides a callback mechanism
 * for different storage implementations to handle the parsed data.
 */
class WorldBankCSVLoader {
public:
    /**
     * Parse World Bank CSV file and invoke callback for each country
     * 
     * @param filename Path to CSV file
     * @param callback Function to call for each parsed PopulationDTO
     * @return Number of countries successfully loaded
     */
    static int loadFromCSV(
        const std::string& filename,
        std::function<void(const PopulationDTO&)> callback
    );

private:
    /**
     * Skip metadata lines and return true if successful
     */
    static bool skipMetadataLines(std::ifstream& file);
    
    /**
     * Parse a single data line into PopulationDTO
     * Returns nullptr if line is invalid
     */
    static PopulationDTO* parseDataLine(const std::string& line);
};

#endif // WORLDBANK_CSV_LOADER_HPP