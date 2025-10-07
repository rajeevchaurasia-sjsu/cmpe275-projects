#include "include/AirQualityDataManager.hpp"
#include "../utils/CSVParser.hpp"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <limits>
#include <omp.h>
#include <mutex>

// Load data from a single CSV file
void AirQualityDataManager::loadFromCSV(const std::string &filename) {
    std::ifstream file(filename);
    
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << filename << std::endl;
        return;
    }
    
    std::string line;
    int lineNumber = 0;
    
    while (std::getline(file, line)) {
        lineNumber++;
        
        if (CSVParser::isEmpty(line)) continue;
        
        std::vector<std::string> fields = CSVParser::parseLine(line);
        
        // Check if we have all 13 fields
        if (fields.size() != 13) {
            std::cerr << "Warning: Invalid line " << lineNumber 
                      << " in " << filename 
                      << " (expected 13 fields, got " << fields.size() << ")" 
                      << std::endl;
            continue;
        }
        
        try {
            // Extract and convert fields
            double lat = std::stod(fields[0]);
            double lon = std::stod(fields[1]);
            std::string datetime = fields[2];
            std::string pollutant = fields[3];
            double value = std::stod(fields[4]);
            std::string unit = fields[5];
            double rawConc = std::stod(fields[6]);
            int aqi = std::stoi(fields[7]);
            int category = std::stoi(fields[8]);
            std::string siteName = fields[9];
            std::string agency = fields[10];
            std::string siteId = fields[11];
            std::string fullSiteId = fields[12];
            
            // Create the reading object
            AirQualityReading reading(lat, lon, datetime, pollutant, value, unit,
                                      rawConc, aqi, category, siteName, agency,
                                      siteId, fullSiteId);
            
            // Add to all three containers
            readings.push_back(reading);
            readingsByDate[datetime].push_back(reading);
            readingsByPollutant[pollutant].push_back(reading);
            
        } catch (const std::exception &e) {
            std::cerr << "Error parsing line " << lineNumber 
                      << " in " << filename << ": " << e.what() << std::endl;
            continue;
        }
    }
    
    file.close();
}

// Load all CSV files from a date folder
void AirQualityDataManager::loadFromDateFolder(const std::string &dateFolderPath) {
    try {
        for (const auto &entry : fs::directory_iterator(dateFolderPath)) {
            if (entry.path().extension() == ".csv") {
                loadFromCSV(entry.path().string());
            }
        }
    } catch (const fs::filesystem_error &e) {
        std::cerr << "Error reading date folder " << dateFolderPath 
                  << ": " << e.what() << std::endl;
    }
}

// Load all date folders from root directory
void AirQualityDataManager::loadFromDirectory(const std::string &rootPath) {
    try {
        for (const auto &entry : fs::directory_iterator(rootPath)) {
            if (entry.is_directory()) {
                std::cout << "Loading folder: " << entry.path().filename() << std::endl;
                loadFromDateFolder(entry.path().string());
            }
        }
    } catch (const fs::filesystem_error &e) {
        std::cerr << "Error reading root directory " << rootPath 
                  << ": " << e.what() << std::endl;
    }
}

// Clear all data
void AirQualityDataManager::clear() {
    readings.clear();
    readingsByDate.clear();
    readingsByPollutant.clear();
}

// Get all readings
std::vector<AirQualityReading> AirQualityDataManager::getAllReadings() const {
    return readings;
}

// Get count of readings
int AirQualityDataManager::getReadingCount() const {
    return readings.size();
}

// Get readings by date (uses index - fast!)
std::vector<AirQualityReading> AirQualityDataManager::getReadingsByDate(const std::string &date) const {
    auto it = readingsByDate.find(date);
    if (it != readingsByDate.end()) {
        return it->second;
    }
    return std::vector<AirQualityReading>();  // Empty vector if not found
}

// Get readings by pollutant type (uses index - fast!)
std::vector<AirQualityReading> AirQualityDataManager::getReadingsByPollutant(const std::string &pollutantType) const {
    auto it = readingsByPollutant.find(pollutantType);
    if (it != readingsByPollutant.end()) {
        return it->second;
    }
    return std::vector<AirQualityReading>();  // Empty vector if not found
}

// Get readings within an AQI range (needs to scan all, good for benchmarking)
std::vector<AirQualityReading> AirQualityDataManager::getReadingsByAQIRange(int minAQI, int maxAQI) const {
    std::vector<AirQualityReading> result;
    
    for (const auto &reading : readings) {
        int aqi = reading.getAirQualityIndex();
        if (aqi >= minAQI && aqi <= maxAQI) {
            result.push_back(reading);
        }
    }
    
    return result;
}

// Calculate average pollutant value (good for parallelization tests!)
double AirQualityDataManager::getAveragePollutantValue(const std::string &pollutantType) const {
    auto pollutantReadings = getReadingsByPollutant(pollutantType);
    
    if (pollutantReadings.empty()) {
        return 0.0;
    }
    
    double sum = 0.0;
    for (const auto &reading : pollutantReadings) {
        sum += reading.getValue();
    }
    
    return sum / pollutantReadings.size();
}

// Get maximum pollutant value
double AirQualityDataManager::getMaxPollutantValue(const std::string &pollutantType) const {
    auto pollutantReadings = getReadingsByPollutant(pollutantType);
    
    if (pollutantReadings.empty()) {
        return 0.0;
    }
    
    double maxValue = std::numeric_limits<double>::lowest();
    for (const auto &reading : pollutantReadings) {
        maxValue = std::max(maxValue, reading.getValue());
    }
    
    return maxValue;
}

// Count readings above an AQI threshold
int AirQualityDataManager::countReadingsAboveAQI(int threshold) const {
    int count = 0;
    
    for (const auto &reading : readings) {
        if (reading.getAirQualityIndex() > threshold) {
            count++;
        }
    }
    
    return count;
}

// Get all unique dates
std::vector<std::string> AirQualityDataManager::getAllDates() const {
    std::vector<std::string> dates;
    
    for (const auto &pair : readingsByDate) {
        dates.push_back(pair.first);
    }
    
    return dates;
}

// Get all unique pollutant types
std::vector<std::string> AirQualityDataManager::getAllPollutantTypes() const {
    std::vector<std::string> types;
    
    for (const auto &pair : readingsByPollutant) {
        types.push_back(pair.first);
    }
    
    return types;
}

// Parallel loading of directory
void AirQualityDataManager::loadFromDirectoryParallel(const std::string &rootPath, int numThreads) {
    // Set number of threads
    omp_set_num_threads(numThreads);
    
    // Get all folder paths first
    std::vector<std::string> folderPaths;
    try {
        for (const auto &entry : fs::directory_iterator(rootPath)) {
            if (entry.is_directory()) {
                folderPaths.push_back(entry.path().string());
            }
        }
    } catch (const fs::filesystem_error &e) {
        std::cerr << "Error reading root directory " << rootPath 
                  << ": " << e.what() << std::endl;
        return;
    }
    
    // Mutex for thread-safe container access
    std::mutex dataMutex;
    
    // Parallel loading
    #pragma omp parallel for schedule(dynamic)
    for (size_t i = 0; i < folderPaths.size(); i++) {
        std::cout << "Thread " << omp_get_thread_num() 
                  << " loading: " << fs::path(folderPaths[i]).filename() << std::endl;
        
        // Create temporary manager for this thread
        AirQualityDataManager tempManager;
        tempManager.loadFromDateFolder(folderPaths[i]);
        
        // Merge into main manager (critical section)
        #pragma omp critical
        {
            auto tempReadings = tempManager.getAllReadings();
            for (const auto &reading : tempReadings) {
                readings.push_back(reading);
                readingsByDate[reading.getDatetime()].push_back(reading);
                readingsByPollutant[reading.getPollutantType()].push_back(reading);
            }
        }
    }
}

// Parallel range query
std::vector<AirQualityReading> AirQualityDataManager::getReadingsByAQIRangeParallel(int minAQI, int maxAQI) const {
    std::vector<AirQualityReading> result;
    std::mutex resultMutex;
    
    #pragma omp parallel
    {
        std::vector<AirQualityReading> localResult;
        
        #pragma omp for nowait
        for (size_t i = 0; i < readings.size(); i++) {
            int aqi = readings[i].getAirQualityIndex();
            if (aqi >= minAQI && aqi <= maxAQI) {
                localResult.push_back(readings[i]);
            }
        }
        
        // Merge local results
        #pragma omp critical
        {
            result.insert(result.end(), localResult.begin(), localResult.end());
        }
    }
    
    return result;
}

// Parallel average calculation
double AirQualityDataManager::getAveragePollutantValueParallel(const std::string &pollutantType) const {
    auto pollutantReadings = getReadingsByPollutant(pollutantType);
    
    if (pollutantReadings.empty()) {
        return 0.0;
    }
    
    double sum = 0.0;
    
    #pragma omp parallel for reduction(+:sum)
    for (size_t i = 0; i < pollutantReadings.size(); i++) {
        sum += pollutantReadings[i].getValue();
    }
    
    return sum / pollutantReadings.size();
}

// Parallel max calculation
double AirQualityDataManager::getMaxPollutantValueParallel(const std::string &pollutantType) const {
    auto pollutantReadings = getReadingsByPollutant(pollutantType);
    
    if (pollutantReadings.empty()) {
        return 0.0;
    }
    
    double maxValue = std::numeric_limits<double>::lowest();
    
    #pragma omp parallel for reduction(max:maxValue)
    for (size_t i = 0; i < pollutantReadings.size(); i++) {
        double val = pollutantReadings[i].getValue();
        if (val > maxValue) {
            maxValue = val;
        }
    }
    
    return maxValue;
}

// Parallel count
int AirQualityDataManager::countReadingsAboveAQIParallel(int threshold) const {
    int count = 0;
    
    #pragma omp parallel for reduction(+:count)
    for (size_t i = 0; i < readings.size(); i++) {
        if (readings[i].getAirQualityIndex() > threshold) {
            count++;
        }
    }
    
    return count;
}