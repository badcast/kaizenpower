#include "CpuManager.hpp"
#include "SysfsUtils.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cmath>

namespace fs = std::filesystem;

namespace fc {

CpuManager::CpuManager() {
    scanCpu();
}

void CpuManager::findTempSensor() {
    m_tempSensorPath.clear();
    const std::string hwmonBase = "/sys/class/hwmon";
    if (!fs::exists(hwmonBase)) return;

    // Look for coretemp, k10temp, zenpower
    for (const auto& entry : fs::directory_iterator(hwmonBase)) {
        std::string p = entry.path().string();
        std::string name = SysfsUtils::readFile(p + "/name");
        if (name == "coretemp" || name == "k10temp" || name == "zenpower") {
            // temp1_input is usually package / Tctl
            if (fs::exists(p + "/temp1_input")) {
                m_tempSensorPath = p + "/temp1_input";
                return;
            }
        }
    }

    // Fallback: acpitz or thermal_zone0
    if (fs::exists("/sys/class/thermal/thermal_zone0/temp")) {
        m_tempSensorPath = "/sys/class/thermal/thermal_zone0/temp";
    }
}

void CpuManager::findTurboControl() {
    m_turboPath.clear();
    m_isNoTurboFile = false;

    if (fs::exists("/sys/devices/system/cpu/intel_pstate/no_turbo")) {
        m_turboPath = "/sys/devices/system/cpu/intel_pstate/no_turbo";
        m_isNoTurboFile = true;
        m_info.hasTurboControl = true;
    } else if (fs::exists("/sys/devices/system/cpu/cpufreq/boost")) {
        m_turboPath = "/sys/devices/system/cpu/cpufreq/boost";
        m_isNoTurboFile = false;
        m_info.hasTurboControl = true;
    } else {
        m_info.hasTurboControl = false;
    }
}

void CpuManager::scanCpu() {
    m_info = CpuInfo();
    findTempSensor();
    findTurboControl();

    // 1. Model Name
    std::ifstream cpuinfo("/proc/cpuinfo");
    if (cpuinfo.is_open()) {
        std::string line;
        while (std::getline(cpuinfo, line)) {
            if (line.find("model name") != std::string::npos) {
                auto pos = line.find(':');
                if (pos != std::string::npos) {
                    m_info.modelName = SysfsUtils::trim(line.substr(pos + 1));
                    break;
                }
            }
        }
    }
    if (m_info.modelName.empty()) {
        m_info.modelName = "Generic x86_64 Processor";
    }

    // 2. Scan CPU policies
    const std::string cpufreqBase = "/sys/devices/system/cpu/cpufreq";
    std::vector<std::string> policyPaths;

    if (fs::exists(cpufreqBase)) {
        for (const auto& entry : fs::directory_iterator(cpufreqBase)) {
            std::string filename = entry.path().filename().string();
            if (filename.find("policy") == 0) {
                policyPaths.push_back(entry.path().string());
            }
        }
    }

    // Sort policies numerically
    std::sort(policyPaths.begin(), policyPaths.end(), [](const std::string& a, const std::string& b) {
        int ia = 0, ib = 0;
        try {
            ia = std::stoi(a.substr(a.find("policy") + 6));
            ib = std::stoi(b.substr(b.find("policy") + 6));
        } catch (...) {}
        return ia < ib;
    });

    m_info.coreCount = static_cast<int>(policyPaths.size());

    if (!policyPaths.empty()) {
        std::string p0 = policyPaths[0];

        // Available governors
        std::string govStr = SysfsUtils::readFile(p0 + "/scaling_available_governors");
        std::stringstream ss(govStr);
        std::string g;
        while (ss >> g) {
            m_info.availableGovernors.push_back(g);
        }

        m_info.currentGovernor = SysfsUtils::readFile(p0 + "/scaling_governor");

        // Energy performance preferences
        std::string eppStr = SysfsUtils::readFile(p0 + "/energy_performance_available_preferences");
        if (!eppStr.empty()) {
            std::stringstream ssEpp(eppStr);
            std::string pref;
            while (ssEpp >> pref) {
                m_info.availableEnergyPreferences.push_back(pref);
            }
            m_info.energyPreference = SysfsUtils::readFile(p0 + "/energy_performance_preference");
        }

        // Hardware Frequency limits
        long minKhz = SysfsUtils::readLong(p0 + "/cpuinfo_min_freq", 800000);
        long maxKhz = SysfsUtils::readLong(p0 + "/cpuinfo_max_freq", 4200000);
        m_info.minLimitGhz = minKhz / 1000000.0;
        m_info.maxLimitGhz = maxKhz / 1000000.0;

        // Current limits
        long curMinKhz = SysfsUtils::readLong(p0 + "/scaling_min_freq", minKhz);
        long curMaxKhz = SysfsUtils::readLong(p0 + "/scaling_max_freq", maxKhz);

        // Core details
        m_info.cores.clear();
        for (size_t i = 0; i < policyPaths.size(); ++i) {
            CpuCoreInfo core;
            core.coreId = static_cast<int>(i);
            core.minFreqGhz = curMinKhz / 1000000.0;
            core.maxFreqGhz = curMaxKhz / 1000000.0;
            core.curFreqGhz = SysfsUtils::readLong(policyPaths[i] + "/scaling_cur_freq", 0) / 1000000.0;
            core.governor = SysfsUtils::readFile(policyPaths[i] + "/scaling_governor");
            m_info.cores.push_back(core);
        }
    }

    // Turbo Boost
    if (m_info.hasTurboControl) {
        long val = SysfsUtils::readLong(m_turboPath, 0);
        if (m_isNoTurboFile) {
            m_info.turboBoost = (val == 0); // 0 = enabled
        } else {
            m_info.turboBoost = (val == 1); // 1 = enabled
        }
    }

    updateLiveMetrics();
}

void CpuManager::updateLiveMetrics() {
    double totalGhz = 0.0;
    int count = 0;

    for (auto& core : m_info.cores) {
        std::string p = "/sys/devices/system/cpu/cpufreq/policy" + std::to_string(core.coreId);
        long curKhz = SysfsUtils::readLong(p + "/scaling_cur_freq", 0);
        core.curFreqGhz = curKhz / 1000000.0;
        core.governor = SysfsUtils::readFile(p + "/scaling_governor");
        totalGhz += core.curFreqGhz;
        count++;
    }

    if (count > 0) {
        m_info.avgFreqGhz = totalGhz / count;
    }

    // Temperature
    if (!m_tempSensorPath.empty()) {
        long tempMilli = SysfsUtils::readLong(m_tempSensorPath, 0);
        m_info.temperatureC = tempMilli / 1000.0;
    }
}

bool CpuManager::setGovernor(const std::string& governor) {
    bool allOk = true;
    for (int i = 0; i < m_info.coreCount; ++i) {
        std::string p = "/sys/devices/system/cpu/cpufreq/policy" + std::to_string(i) + "/scaling_governor";
        if (!SysfsUtils::writeFilePrivileged(p, governor)) {
            allOk = false;
        }
    }
    if (allOk) {
        m_info.currentGovernor = governor;
        std::cout << "[KAIZEN] CPU: Scaling governor set to '" << governor << "'" << std::endl;
    }
    return allOk;
}

bool CpuManager::setEnergyPreference(const std::string& preference) {
    bool allOk = true;
    for (int i = 0; i < m_info.coreCount; ++i) {
        std::string p = "/sys/devices/system/cpu/cpufreq/policy" + std::to_string(i) + "/energy_performance_preference";
        if (fs::exists(p)) {
            if (!SysfsUtils::writeFilePrivileged(p, preference)) {
                allOk = false;
            }
        }
    }
    if (allOk) {
        m_info.energyPreference = preference;
        std::cout << "[KAIZEN] CPU: Energy performance preference set to '" << preference << "'" << std::endl;
    }
    return allOk;
}

bool CpuManager::setFreqLimits(double minGhz, double maxGhz) {
    long minKhz = static_cast<long>(std::round(minGhz * 1000000.0));
    long maxKhz = static_cast<long>(std::round(maxGhz * 1000000.0));

    bool allOk = true;
    for (int i = 0; i < m_info.coreCount; ++i) {
        std::string pMin = "/sys/devices/system/cpu/cpufreq/policy" + std::to_string(i) + "/scaling_min_freq";
        std::string pMax = "/sys/devices/system/cpu/cpufreq/policy" + std::to_string(i) + "/scaling_max_freq";

        long curMaxKhz = SysfsUtils::readLong(pMax, 0);
        if (minKhz > curMaxKhz) {
            bool okMax = SysfsUtils::writeFilePrivileged(pMax, std::to_string(maxKhz));
            bool okMin = SysfsUtils::writeFilePrivileged(pMin, std::to_string(minKhz));
            if (!okMax || !okMin) allOk = false;
        } else {
            bool okMin = SysfsUtils::writeFilePrivileged(pMin, std::to_string(minKhz));
            bool okMax = SysfsUtils::writeFilePrivileged(pMax, std::to_string(maxKhz));
            if (!okMin || !okMax) allOk = false;
        }
    }
    if (allOk) {
        m_info.minLimitGhz = minGhz;
        m_info.maxLimitGhz = maxGhz;
        std::cout << "[KAIZEN] CPU: Frequency limits set to " << minGhz << " GHz (min) - " << maxGhz << " GHz (max)" << std::endl;
    }
    return allOk;
}

bool CpuManager::setTurboBoost(bool enable) {
    if (!m_info.hasTurboControl || m_turboPath.empty()) return false;

    std::string valStr;
    if (m_isNoTurboFile) {
        valStr = enable ? "0" : "1";
    } else {
        valStr = enable ? "1" : "0";
    }

    bool ok = SysfsUtils::writeFilePrivileged(m_turboPath, valStr);
    if (ok) {
        m_info.turboBoost = enable;
        std::cout << "[KAIZEN] CPU: Turbo Boost " << (enable ? "ENABLED" : "DISABLED") << std::endl;
    }
    return ok;
}

} // namespace fc
