#pragma once

#include <string>

namespace fc {

struct OsInfo {
    std::string name = "Linux";
    std::string prettyName = "Linux";
    std::string id = "linux";
    std::string version;
    std::string logo = "distributor-logo";
    std::string kernel;
    std::string architecture = "x86_64";
    std::string hostname = "localhost";
    std::string uptimeStr = "0m";
    long uptimeSeconds = 0;
};

class SystemInfo {
public:
    static OsInfo getOsInfo();
    static std::string formatUptime(long uptimeSeconds);
};

} // namespace fc
