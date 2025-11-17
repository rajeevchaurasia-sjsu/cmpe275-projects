// From mini1
#include "CSVParser.hpp"
#include <algorithm>
#include <cctype>

// Remove leading and trailing quotes
std::string CSVParser::removeQuotes(const std::string &str) {
    if (str.length() >= 2 && str.front() == '"' && str.back() == '"') {
        return str.substr(1, str.length() - 2);
    }
    return str;
}

// Parse a CSV line with proper quote handling
std::vector<std::string> CSVParser::parseLine(const std::string &line) {
    std::vector<std::string> fields;
    std::string field;
    bool inQuotes = false;
    
    for (size_t i = 0; i < line.length(); i++) {
        char c = line[i];
        
        if (c == '"') {
            inQuotes = !inQuotes;  // Toggle quote state
        } else if (c == ',' && !inQuotes) {
            // End of field
            fields.push_back(removeQuotes(field));
            field.clear();
        } else {
            field += c;
        }
    }
    
    // Don't forget the last field
    fields.push_back(removeQuotes(field));
    
    return fields;
}

// Check if line is empty or whitespace
bool CSVParser::isEmpty(const std::string &line) {
    return line.empty() || 
           std::all_of(line.begin(), line.end(), 
                      [](unsigned char c) { return std::isspace(c); });
}

// Trim whitespace from both ends
std::string CSVParser::trim(const std::string &str) {
    auto start = std::find_if_not(str.begin(), str.end(),
                                  [](unsigned char c) { return std::isspace(c); });
    auto end = std::find_if_not(str.rbegin(), str.rend(),
                                [](unsigned char c) { return std::isspace(c); }).base();
    
    return (start < end) ? std::string(start, end) : std::string();
}