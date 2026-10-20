#include "SystemInfo.hpp"
#include <fstream>
#include <sstream>
#include <sys/utsname.h>
#include <sys/sysinfo.h>
#include <unistd.h>

namespace fc {

static std::string trimQuotes(const std::string& str) {
    if (str.size() >= 2 && ((str.front() == '"' && str.back() == '"') ||
                            (str.front() == '\'' && str.back() == '\''))) {
        return str.substr(1, str.size() - 2);
    }
    return str;
}

std::string SystemInfo::formatUptime(long uptimeSeconds) {
    long days = uptimeSeconds / 86400;
    long hours = (uptimeSeconds % 86400) / 3600;
    long minutes = (uptimeSeconds % 3600) / 60;

    std::ostringstream oss;
    if (days > 0) {
        oss << days << "d " << hours << "h " << minutes << "m";
    } else if (hours > 0) {
        oss << hours << "h " << minutes << "m";
    } else {
        oss << minutes << "m";
    }
    return oss.str();
}

OsInfo SystemInfo::getOsInfo() {
    OsInfo info;

    // 1. Read /etc/os-release
    std::ifstream file("/etc/os-release");
    if (!file.is_open()) {
        file.open("/usr/lib/os-release");
    }

    if (file.is_open()) {
        std::string line;
        while (std::getline(file, line)) {
            if (line.rfind("PRETTY_NAME=", 0) == 0) {
                info.prettyName = trimQuotes(line.substr(12));
            } else if (line.rfind("NAME=", 0) == 0) {
                info.name = trimQuotes(line.substr(5));
            } else if (line.rfind("ID=", 0) == 0) {
                info.id = trimQuotes(line.substr(3));
            } else if (line.rfind("VERSION_ID=", 0) == 0) {
                info.version = trimQuotes(line.substr(11));
            } else if (line.rfind("LOGO=", 0) == 0) {
                info.logo = trimQuotes(line.substr(5));
            }
        }
        file.close();
    }

    if (info.prettyName.empty()) {
        info.prettyName = info.name.empty() ? "Linux" : info.name;
    }
    if (info.logo.empty()) {
        info.logo = "distributor-logo";
    }

    // 2. Kernel and machine details via uname
    struct utsname uts;
    if (uname(&uts) == 0) {
        info.kernel = uts.release;
        info.architecture = uts.machine;
        info.hostname = uts.nodename;
    }

    // 3. System uptime via sysinfo
    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        info.uptimeSeconds = si.uptime;
        info.uptimeStr = formatUptime(si.uptime);
    }

    return info;
}

} // namespace fc
