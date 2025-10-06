
#ifndef AIR_QUALITY_DATA_MANAGER_HPP
#define AIR_QUALITY_DATA_MANAGER_HPP

#include <vector>
#include <map>
#include <string>
#include <filesystem>
#include "AirQualityReading.hpp"

namespace fs = std::filesystem;

class AirQualityDataManager {
private:
    std::vector<AirQualityReading> readings;
    std::map<std::string, std::vector<AirQualityReading>> readingsByDate;
    std::map<std::string, std::vector<AirQualityReading>> readingsByPollutant;

public:
    // Loading methods (Serial)
    void loadFromCSV(const std::string &filename);
    void loadFromDateFolder(const std::string &dateFolderPath);
    void loadFromDirectory(const std::string &rootPath);
    
    // NEW: Parallel loading methods
    void loadFromDirectoryParallel(const std::string &rootPath, int numThreads = 4);
    
    // Clear all data
    void clear();
    
    // Basic query methods
    std::vector<AirQualityReading> getAllReadings() const;
    int getReadingCount() const;
    
    // Indexed query methods
    std::vector<AirQualityReading> getReadingsByDate(const std::string &date) const;
    std::vector<AirQualityReading> getReadingsByPollutant(const std::string &pollutantType) const;
    
    // Range query methods
    std::vector<AirQualityReading> getReadingsByAQIRange(int minAQI, int maxAQI) const;
    
    // NEW: Parallel range query
    std::vector<AirQualityReading> getReadingsByAQIRangeParallel(int minAQI, int maxAQI) const;
    
    // Aggregation methods (Serial)
    double getAveragePollutantValue(const std::string &pollutantType) const;
    double getMaxPollutantValue(const std::string &pollutantType) const;
    int countReadingsAboveAQI(int threshold) const;
    
    // NEW: Parallel aggregation methods
    double getAveragePollutantValueParallel(const std::string &pollutantType) const;
    double getMaxPollutantValueParallel(const std::string &pollutantType) const;
    int countReadingsAboveAQIParallel(int threshold) const;
    
    // Metadata methods
    std::vector<std::string> getAllDates() const;
    std::vector<std::string> getAllPollutantTypes() const;
};

#endif // AIR_QUALITY_DATA_MANAGER_HPP