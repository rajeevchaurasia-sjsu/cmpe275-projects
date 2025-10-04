#include <vector>
#include <map>
#include <string>
#include <filesystem>
#include "AirQualityReading.h"

namespace fs = std::filesystem;

class AirQualityDataManager {
private:
    std::vector<AirQualityReading> readings;
    std::map<std::string, std::vector<AirQualityReading>> readingsByDate;
    std::map<std::string, std::vector<AirQualityReading>> readingsByPollutant;

public:
    // Loading methods
    void loadFromCSV(const std::string &filename);
    void loadFromDateFolder(const std::string &dateFolderPath);
    void loadFromDirectory(const std::string &rootPath);
    
    // Clear all data
    void clear();
    
    // Basic query methods
    std::vector<AirQualityReading> getAllReadings() const;
    int getReadingCount() const;
    
    // Indexed query methods
    std::vector<AirQualityReading> getReadingsByDate(const std::string &date) const;
    std::vector<AirQualityReading> getReadingsByPollutant(const std::string &pollutantType) const;
    
    // Range query methods (useful for benchmarking)
    std::vector<AirQualityReading> getReadingsByAQIRange(int minAQI, int maxAQI) const;
    
    // Aggregation methods (good for parallel processing tests)
    double getAveragePollutantValue(const std::string &pollutantType) const;
    double getMaxPollutantValue(const std::string &pollutantType) const;
    int countReadingsAboveAQI(int threshold) const;
    
    // Metadata methods
    std::vector<std::string> getAllDates() const;
    std::vector<std::string> getAllPollutantTypes() const;
};