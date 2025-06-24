#include "logger.hh"

#include <fstream>
#include <chrono>
#include <string>

std::ofstream output_file(OUTPUT_FILENAME, std::ios::app);

Logger logger;

static auto now_chrono = std::chrono::system_clock::now();

Logger::Logger() {

}

Logger::~Logger() {
    if (output_file.is_open()) 
        output_file.close();
}

void Logger::log(const std::string& message) const {
    output_file << ts() << message;
}

void Logger::operator << (const std::string& message) const {
    output_file << ts() << message;
}

std::string Logger::ts() const {
    std::time_t now = std::chrono::system_clock::to_time_t(now_chrono);
    tm time;
    mktime(&time);
    std::string timestamp = asctime(&time);
    return timestamp;
}
