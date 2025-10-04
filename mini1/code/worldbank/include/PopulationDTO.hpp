
#ifndef POPULATIONDTO_HPP_
#define POPULATIONDTO_HPP_

#include <string>
#include <vector>

class PopulationDTO {
private:
    std::string countryName;
    std::string countryCode;
    std::vector<long> population;
    
public:
    static const int START_YEAR = 1960;
    static const int END_YEAR = 2023;
    static const int NUM_YEARS = END_YEAR - START_YEAR + 1;

    PopulationDTO() {
        countryName = "";
        countryCode = "";
        population.resize(NUM_YEARS, -1); // Initialize with -1 for missing data
    }

    PopulationDTO(const std::string& countryName, 
                const std::string& countryCode, 
                const std::vector<long>& population) {
        this->countryName = countryName;
        this->countryCode = countryCode;
        this->population = population;
    }

    std::string getCountryName() const {
        return countryName;
    }
    std::string getCountryCode() const {
        return countryCode;
    }

    long getPopulationForYear(int year) const {
        if (year < START_YEAR || year > END_YEAR) {
            return -1; // Invalid year
        }
        return population[year - START_YEAR];
    }

    const std::vector<long>& getPopulation() const {
        return population; // Return const reference to prevent modification
    }

    void setCountryName(const std::string& countryName) {
        this->countryName = countryName;
    }
    void setCountryCode(const std::string& countryCode) {
        this->countryCode = countryCode;
    }
    void setPopulationForYear(int year, long value) {
        if (year >= START_YEAR && year <= END_YEAR) {
            population[year - START_YEAR] = value;
        }
    }

};

#endif // POPULATIONDTO_HPP_