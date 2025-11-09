
#ifndef AIR_QUALITY_READING_HPP
#define AIR_QUALITY_READING_HPP

#include <string>

class AirQualityReading
{
private:
    double latitude;
    double longitude;
    std::string datetime;
    std::string pollutantType;
    double value;
    std::string unit;
    double rawConcentration;
    int airQualityIndex;
    int category;
    std::string siteName;
    std::string agencyName;
    std::string siteId;
    std::string fullSiteId;

public:
    AirQualityReading(double lat, double lon, const std::string &dt, const std::string &pollutant,
                      double val, const std::string &u, double rawConcenteration, int aqi, int cat,
                      const std::string &site, const std::string &agency, const std::string &siteId, const std::string &fullSiteId)
    {

        this->latitude = lat;
        this->longitude = lon;
        this->datetime = dt;
        this->pollutantType = pollutant;
        this->value = val;
        this->unit = u;
        this->rawConcentration = rawConcenteration;
        this->airQualityIndex = aqi;
        this->category = cat;
        this->siteName = site;
        this->agencyName = agency;
        this->siteId = siteId;
        this->fullSiteId = fullSiteId;
    }

    // Getter methods
    double getLatitude() const { return latitude; }
    double getLongitude() const { return longitude; }
    std::string getDatetime() const { return datetime; }
    std::string getPollutantType() const { return pollutantType; }
    double getValue() const { return value; }
    std::string getUnit() const { return unit; }
    double getRawConcentration() const { return rawConcentration; }
    int getAirQualityIndex() const { return airQualityIndex; }
    int getCategory() const { return category; }
    std::string getSiteName() const { return siteName; }
    std::string getAgencyName() const { return agencyName; }
    std::string getSiteId() const { return siteId; }
    std::string getFullSiteId() const { return fullSiteId; }

};

#endif // AIR_QUALITY_READING_HPP