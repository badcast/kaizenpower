#include "SysfsUtils.hpp"
#include "PrivilegeManager.hpp"

#include <unistd.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <array>
#include <memory>
#include <algorithm>

namespace fc {

bool SysfsUtils::isRoot() {
    return PrivilegeManager::instance().isAuthorized();
}

bool SysfsUtils::canWrite(const std::string& path) {
    return access(path.c_str(), W_OK) == 0;
}

std::string SysfsUtils::trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

std::string SysfsUtils::readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return "";
    std::string line;
    if (std::getline(file, line)) {
        return trim(line);
    }
    return "";
}

long SysfsUtils::readLong(const std::string& path, long defaultVal) {
    std::string s = readFile(path);
    if (s.empty()) return defaultVal;
    try {
        return std::stol(s);
    } catch (...) {
        return defaultVal;
    }
}

double SysfsUtils::readDouble(const std::string& path, double defaultVal) {
    std::string s = readFile(path);
    if (s.empty()) return defaultVal;
    try {
        return std::stod(s);
    } catch (...) {
        return defaultVal;
    }
}

std::vector<std::string> SysfsUtils::readLines(const std::string& path) {
    std::vector<std::string> lines;
    std::ifstream file(path);
    if (!file.is_open()) return lines;
    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(trim(line));
    }
    return lines;
}

bool SysfsUtils::writeFile(const std::string& path, const std::string& content) {
    if (canWrite(path)) {
        std::ofstream file(path);
        if (file.is_open()) {
            file << content;
            return file.good();
        }
    }
    return false;
}

bool SysfsUtils::writeFilePrivileged(const std::string& path, const std::string& content) {
    if (canWrite(path)) {
        return writeFile(path, content);
    }
    return PrivilegeManager::instance().writePrivileged(path, content);
}

std::string SysfsUtils::executeCommand(const std::string& cmd) {
    struct PipeDeleter {
        void operator()(FILE* f) const {
            if (f) pclose(f);
        }
    };
    std::array<char, 256> buffer;
    std::string result;
    std::unique_ptr<FILE, PipeDeleter> pipe(popen(cmd.c_str(), "r"));
    if (!pipe) return "";
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return trim(result);
}

bool SysfsUtils::relaunchAsRoot() {
    return PrivilegeManager::instance().requestAuthorization();
}

} // namespace fc
