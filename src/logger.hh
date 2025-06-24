#pragma once

#include <string>

const std::string OUTPUT_FILENAME = "logs.txt";

extern std::ofstream output_file;

class Logger {
public:
    Logger();
    ~Logger();
    void log(const std::string& message) const;
    void log_error(const std::string& message) const;
    void operator << (const std::string& message) const;
private:
    std::string ts() const; // get timestamp
};

extern Logger logger;
