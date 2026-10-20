#pragma once

#include <string>
#include <vector>
#include <filesystem>

namespace fc {

class SysfsUtils {
public:
    static bool isRoot();
    static bool canWrite(const std::string& path);
    
    static std::string readFile(const std::string& path);
    static long readLong(const std::string& path, long defaultVal = 0);
    static double readDouble(const std::string& path, double defaultVal = 0.0);
    static std::vector<std::string> readLines(const std::string& path);
    
    static bool writeFile(const std::string& path, const std::string& content);
    static bool writeFilePrivileged(const std::string& path, const std::string& content);
    
    static std::string executeCommand(const std::string& cmd);
    static bool relaunchAsRoot();
    
    static std::string trim(const std::string& str);
};

} // namespace fc
