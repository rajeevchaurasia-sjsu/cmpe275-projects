#!/usr/bin/env python3
"""
Generate realistic air quality data for Mini 2 project
Creates CSV files with air quality readings similar to the 2020 California wildfires
"""

import csv
import random
import os
from datetime import datetime, timedelta

# California locations affected by 2020 wildfires
LOCATIONS = [
    ("San Francisco", 37.7749, -122.4194),
    ("Oakland", 37.8044, -122.2711),
    ("Berkeley", 37.8715, -122.2730),
    ("San Jose", 37.3382, -121.8863),
    ("Palo Alto", 37.4419, -122.1430),
    ("Mountain View", 37.3860, -122.0840),
    ("Sunnyvale", 37.3688, -122.0363),
    ("Santa Clara", 37.3541, -121.9552),
    ("Fremont", 37.5483, -121.9886),
    ("Hayward", 37.6688, -122.0808),
    ("Livermore", 37.6819, -121.7680),
    ("Pleasanton", 37.6624, -121.8747),
    ("Walnut Creek", 37.9101, -122.0652),
    ("Concord", 37.9775, -122.0311),
    ("Antioch", 38.0049, -121.8058),
    ("Pittsburg", 38.0280, -121.8847),
    ("Brentwood", 37.9319, -121.6958),
    ("Oakley", 37.9974, -121.7124),
    ("Discovery Bay", 37.9085, -121.6002),
    ("Byron", 37.8671, -121.6380)
]

POLLUTANTS = [
    ("PM2.5", "µg/m³", 0, 500),
    ("PM10", "µg/m³", 0, 600),
    ("O3", "ppm", 0, 0.2),
    ("NO2", "ppm", 0, 0.2),
    ("SO2", "ppm", 0, 0.1),
    ("CO", "ppm", 0, 50)
]

def get_aqi_category(aqi):
    """Convert AQI value to category"""
    if aqi <= 50:
        return 1, "Good"
    elif aqi <= 100:
        return 2, "Moderate"
    elif aqi <= 150:
        return 3, "Unhealthy for Sensitive Groups"
    elif aqi <= 200:
        return 4, "Unhealthy"
    elif aqi <= 300:
        return 5, "Very Unhealthy"
    else:
        return 6, "Hazardous"

def generate_reading(base_date, hour, location, pollutant_info):
    """Generate a single air quality reading"""
    pollutant, unit, min_val, max_val = pollutant_info
    location_name, lat, lon = location

    # Add some randomness to the timestamp
    reading_time = base_date + timedelta(hours=hour, minutes=random.randint(0, 59))

    # Generate realistic values with wildfire influence
    # Higher values during wildfire periods (Aug-Oct 2020)
    month = reading_time.month
    wildfire_multiplier = 1.0
    if month in [8, 9, 10]:  # Peak wildfire season
        wildfire_multiplier = random.uniform(2.0, 5.0)

    # Base value with seasonal variation
    base_value = random.uniform(min_val, min(max_val * 0.3, max_val * 0.1))

    # Apply wildfire effect
    value = base_value * wildfire_multiplier

    # Ensure within bounds
    value = min(value, max_val)

    # Calculate AQI (simplified - using PM2.5 equivalent)
    if pollutant == "PM2.5":
        aqi = min(500, value * 2)  # Rough AQI calculation
    elif pollutant == "PM10":
        aqi = min(500, value * 1.5)
    else:
        aqi = min(500, value * 100)  # For gases

    category_num, category_name = get_aqi_category(int(aqi))

    # Generate realistic raw concentration (similar to value but with more variation)
    raw_concentration = value * random.uniform(0.8, 1.2)

    return {
        'latitude': lat,
        'longitude': lon,
        'datetime': reading_time.strftime('%Y-%m-%dT%H:%M:%SZ'),
        'pollutant': pollutant,
        'value': round(value, 2),
        'unit': unit,
        'raw_concentration': round(raw_concentration, 2),
        'aqi': int(aqi),
        'category': category_num,
        'site_name': location_name,
        'agency': 'California Air Resources Board',
        'site_id': f'CA_{location_name.replace(" ", "_")}_{random.randint(1000, 9999)}',
        'full_site_id': f'CA_{location_name.replace(" ", "_")}_{random.randint(10000, 99999)}'
    }

def generate_day_data(date, output_dir):
    """Generate data for a full day"""
    print(f"Generating data for {date.strftime('%Y-%m-%d')}")

    # Create date directory
    date_dir = os.path.join(output_dir, date.strftime('%Y%m%d'))
    os.makedirs(date_dir, exist_ok=True)

    # Generate hourly data files
    for hour in range(24):
        filename = os.path.join(date_dir, f"{date.strftime('%Y%m%d')}-{hour:02d}.csv")

        with open(filename, 'w', newline='') as csvfile:
            fieldnames = ['latitude', 'longitude', 'datetime', 'pollutant', 'value', 'unit',
                         'raw_concentration', 'aqi', 'category', 'site_name', 'agency',
                         'site_id', 'full_site_id']
            writer = csv.DictWriter(csvfile, fieldnames=fieldnames)

            # Generate readings for all locations and pollutants
            for location in LOCATIONS:
                for pollutant in POLLUTANTS:
                    reading = generate_reading(date, hour, location, pollutant)
                    writer.writerow(reading)

def main():
    """Main function to generate air quality data"""
    output_dir = "data/air_quality"

    # Generate data for August and September 2020 (wildfire season)
    start_date = datetime(2020, 8, 1)
    end_date = datetime(2020, 9, 30)

    current_date = start_date
    while current_date <= end_date:
        generate_day_data(current_date, output_dir)
        current_date += timedelta(days=1)

    print("Data generation complete!")
    print(f"Generated data in: {output_dir}")

if __name__ == "__main__":
    main()