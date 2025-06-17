#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <algorithm>
#include <cctype>

class PGNParser {
private:
    std::map<std::string, std::string> headers;
    
    std::string categorizeOpening(const std::string& eco) {
        if (eco.empty()) {
            return "Unknown category";
        }
        
        // Trim whitespace and get first character
        std::string trimmedEco = trim(eco);
        if (trimmedEco.empty()) {
            return "Unknown category";
        }
        
        char first = std::toupper(trimmedEco[0]); // Index 0, nicht 1!
        switch (first) {
            case 'A': return "Flankeneröffnung";
            case 'B': return "Halboffene Eröffnung";
            case 'C': return "Offene Eröffnung";
            case 'D': return "Geschlossene Eröffnung";
            case 'E': return "Indische Verteidigung";
            default: return "Unknown category";
        }
    }
    
    std::string trim(const std::string& str) {
        size_t start = str.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) return "";
        size_t end = str.find_last_not_of(" \t\r\n");
        return str.substr(start, end - start + 1);
    }
    
    std::string getHeaderValue(const std::string& key) {
        auto it = headers.find(key);
        return (it != headers.end()) ? it->second : "";
    }
    
public:
    std::string escapeCSV(const std::string& field) {
        // Nur escapen wenn das Feld Sonderzeichen enthält
        if (field.find(',') != std::string::npos || 
            field.find('"') != std::string::npos || 
            field.find('\n') != std::string::npos) {
            std::string escaped = "\"";
            for (char c : field) {
                if (c == '"') escaped += "\"\"";
                else escaped += c;
            }
            escaped += "\"";
            return escaped;
        }
        return field;
    }
    
    std::string formatNumber(const std::string& value) {
        // Entferne Anführungszeichen und gib nur die Zahl zurück
        std::string result = value;
        if (!result.empty() && result[0] == '"' && result.back() == '"') {
            result = result.substr(1, result.length() - 2);
        }
        // Prüfe ob es eine gültige Zahl ist
        if (result.empty() || !std::all_of(result.begin(), result.end(), ::isdigit)) {
            return "";
        }
        return result;
    }
    struct GameData {
        int gameNumber;
        std::string whiteElo;
        std::string blackElo;
        std::string eco;
        std::string opening;
        std::string event;
        std::string result;
        std::string openingCategory;
    };
    
    bool parseGame(const std::string& gameText, GameData& data) {
        headers.clear();
        std::istringstream iss(gameText);
        std::string line;
        
        // Parse headers
        while (std::getline(iss, line)) {
            line = trim(line);
            if (line.empty()) break;
            
            if (line[0] == '[' && line.back() == ']') {
                // Parse header line like [Event "Tournament Name"]
                size_t spacePos = line.find(' ');
                if (spacePos != std::string::npos) {
                    std::string key = line.substr(1, spacePos - 1);
                    std::string value = line.substr(spacePos + 1);
                    
                    // Remove closing bracket first
                    if (!value.empty() && value.back() == ']') {
                        value = value.substr(0, value.length() - 1);
                    }
                    
                    // Remove quotes
                    if (value.length() >= 2 && value[0] == '"' && value.back() == '"') {
                        value = value.substr(1, value.length() - 2);
                    }
                    
                    headers[key] = value;
                }
            }
        }
        
        // Extract data
        data.whiteElo = getHeaderValue("WhiteElo");
        data.blackElo = getHeaderValue("BlackElo");
        data.eco = getHeaderValue("ECO");
        data.opening = getHeaderValue("Opening");
        data.event = getHeaderValue("Event");
        data.result = getHeaderValue("Result");
        data.openingCategory = categorizeOpening(data.eco);
        
        return !headers.empty();
    }
};

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <input_pgn_file> <output_csv_file>" << std::endl;
        return 1;
    }
    
    std::string inputFile = argv[1];
    std::string outputFile = argv[2];
    
    std::ifstream inFile(inputFile);
    if (!inFile.is_open()) {
        std::cerr << "Error: Could not open input file: " << inputFile << std::endl;
        return 1;
    }
    
    std::ofstream outFile(outputFile);
    if (!outFile.is_open()) {
        std::cerr << "Error: Could not create output file: " << outputFile << std::endl;
        return 1;
    }
    
    // Write CSV header
    outFile << "GameNumber,WhiteElo,BlackElo,ECO,Opening,Event,Result,OpeningCategory\n";
    
    PGNParser parser;
    std::string line;
    std::string currentGame;
    int gameNumber = 0;
    bool inGame = false;
    
    while (std::getline(inFile, line)) {
        // Trim line
        std::string trimmedLine = line;
        size_t start = trimmedLine.find_first_not_of(" \t\r\n");
        if (start != std::string::npos) {
            size_t end = trimmedLine.find_last_not_of(" \t\r\n");
            trimmedLine = trimmedLine.substr(start, end - start + 1);
        } else {
            trimmedLine = "";
        }
        
        if (trimmedLine.empty()) {
            // Empty line - if we were in a game, process it
            if (inGame && !currentGame.empty()) {
                PGNParser::GameData data;
                data.gameNumber = gameNumber;
                
                if (parser.parseGame(currentGame, data)) {
                    outFile << data.gameNumber << ","
                           << parser.formatNumber(data.whiteElo) << ","
                           << parser.formatNumber(data.blackElo) << ","
                           << data.eco << ","
                           << parser.escapeCSV(data.opening) << ","
                           << parser.escapeCSV(data.event) << ","
                           << data.result << ","
                           << data.openingCategory << "\n";
                    gameNumber++;
                    
                    // Progress output every 100,000 games
                    if (gameNumber % 100000 == 0) {
                        std::cout << "Processed " << gameNumber << " games..." << std::endl;
                    }
                }
                
                currentGame.clear();
                inGame = false;
            }
        } else {
            // Non-empty line
            currentGame += line + "\n";
            inGame = true;
        }
    }
    
    // Process last game if file doesn't end with empty line
    if (inGame && !currentGame.empty()) {
        PGNParser::GameData data;
        data.gameNumber = gameNumber;
        
        if (parser.parseGame(currentGame, data)) {
            outFile << data.gameNumber << ","
                   << parser.formatNumber(data.whiteElo) << ","
                   << parser.formatNumber(data.blackElo) << ","
                   << data.eco << ","
                   << parser.escapeCSV(data.opening) << ","
                   << parser.escapeCSV(data.event) << ","
                   << data.result << ","
                   << data.openingCategory << "\n";
                   
            // Final progress check
            if ((gameNumber + 1) % 100000 == 0) {
                std::cout << "Processed " << (gameNumber + 1) << " games..." << std::endl;
            }
        }
    }
    
    inFile.close();
    outFile.close();
    
    std::cout << "Successfully converted " << gameNumber << " games from " 
              << inputFile << " to " << outputFile << std::endl;
    
    return 0;
}