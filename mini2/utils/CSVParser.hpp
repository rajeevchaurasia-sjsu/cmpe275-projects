// From mini1
#include <string>
#include <vector>

class CSVParser {
public:
    // Parse a single CSV line into fields
    static std::vector<std::string> parseLine(const std::string &line);
    
    // Remove quotes from a string
    static std::string removeQuotes(const std::string &str);
    
    // Check if a line is empty or whitespace only
    static bool isEmpty(const std::string &line);
    
    // Trim whitespace from both ends
    static std::string trim(const std::string &str);
};