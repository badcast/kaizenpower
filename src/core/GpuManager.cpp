#include "GpuManager.hpp"
#include "SysfsUtils.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <regex>

namespace fs = std::filesystem;

namespace fc {

GpuManager::GpuManager() {
    scanGpu();
}

void GpuManager::scanGpu() {
    m_info = GpuInfo();
    m_isAmd = false;
    m_isNvidia = false;
    m_isIntel = false;
    m_hwmonPath.clear();

    // Check DRM cards first
    const std::string drmBase = "/sys/class/drm";
    if (fs::exists(drmBase)) {
        for (const auto& entry : fs::directory_iterator(drmBase)) {
            std::string filename = entry.path().filename().string();
            // Look for card0, card1, etc. but not card0-DP-1
            if (filename.find("card") == 0 && filename.find('-') == std::string::npos) {
                std::string cardPath = entry.path().string();
                std::string driverPath = cardPath + "/device/driver";
                std::string driverName;
                try {
                    if (fs::exists(driverPath)) {
                        driverName = fs::canonical(driverPath).filename().string();
                    }
                } catch (...) {}

                if (driverName == "amdgpu" || driverName == "radeon") {
                    scanAmdGpu(cardPath);
                    if (m_info.present) return;
                } else if (driverName == "i915" || driverName == "xe") {
                    scanIntelGpu(cardPath);
                    if (m_info.present) return;
                }
            }
        }
    }

    // Check NVIDIA via nvidia-smi if no AMD/Intel DRM primary
    scanNvidiaGpu();
}

void GpuManager::scanAmdGpu(const std::string& cardPath) {
    m_isAmd = true;
    m_info.present = true;
    m_info.driver = "amdgpu";
    m_info.cardPath = cardPath;

    // GPU Model name from lspci
    std::string lspciOut = SysfsUtils::executeCommand("lspci | grep -E 'VGA|3D' | grep -i amd | head -n 1");
    if (!lspciOut.empty()) {
        auto pos = lspciOut.find(": ");
        if (pos != std::string::npos) {
            m_info.name = SysfsUtils::trim(lspciOut.substr(pos + 2));
        } else {
            m_info.name = lspciOut;
        }
    } else {
        m_info.name = "AMD Radeon Graphics";
    }

    std::string devPath = cardPath + "/device";

    // Find hwmon directory for this GPU
    if (fs::exists(devPath + "/hwmon")) {
        for (const auto& entry : fs::directory_iterator(devPath + "/hwmon")) {
            m_hwmonPath = entry.path().string();
            break;
        }
    }

    // Performance levels
    m_info.availablePerfLevels = {"auto", "low", "high", "manual"};
    m_info.perfLevel = SysfsUtils::readFile(devPath + "/power_dpm_force_performance_level");
    if (m_info.perfLevel.empty()) m_info.perfLevel = "auto";

    // Parse SCLK states
    std::string sclkContent = SysfsUtils::readFile(devPath + "/pp_dpm_sclk");
    // Actually pp_dpm_sclk has multiple lines, so use readLines
    auto sclkLines = SysfsUtils::readLines(devPath + "/pp_dpm_sclk");
    m_info.sclkStates.clear();
    for (const auto& line : sclkLines) {
        // e.g. "0: 300Mhz *" or "1: 600Mhz"
        std::regex re(R"((\d+):\s*(\d+)Mhz(\s*\*?)?)");
        std::smatch match;
        if (std::regex_search(line, match, re)) {
            GpuClockState state;
            state.index = std::stoi(match[1].str());
            state.mhz = std::stoi(match[2].str());
            state.isActive = (line.find('*') != std::string::npos);
            if (state.isActive) {
                m_info.activeSclkIndex = state.index;
                m_info.curSclkMhz = state.mhz;
            }
            m_info.sclkStates.push_back(state);
        }
    }

    // Parse MCLK states
    auto mclkLines = SysfsUtils::readLines(devPath + "/pp_dpm_mclk");
    m_info.mclkStates.clear();
    for (const auto& line : mclkLines) {
        std::regex re(R"((\d+):\s*(\d+)Mhz(\s*\*?)?)");
        std::smatch match;
        if (std::regex_search(line, match, re)) {
            GpuClockState state;
            state.index = std::stoi(match[1].str());
            state.mhz = std::stoi(match[2].str());
            state.isActive = (line.find('*') != std::string::npos);
            if (state.isActive) {
                m_info.activeMclkIndex = state.index;
                m_info.curMclkMhz = state.mhz;
            }
            m_info.mclkStates.push_back(state);
        }
    }

    // Parse Power Profiles
    auto profileLines = SysfsUtils::readLines(devPath + "/pp_power_profile_mode");
    m_info.powerProfiles.clear();
    for (const auto& line : profileLines) {
        // e.g. "  1 3D_FULL_SCREEN *:        0              100"
        std::regex re(R"(\s*(\d+)\s+([A-Z0-9_]+)(\s*\*?)?:)");
        std::smatch match;
        if (std::regex_search(line, match, re)) {
            int idx = std::stoi(match[1].str());
            std::string name = match[2].str();
            bool active = (match[3].matched && match[3].str().find('*') != std::string::npos);
            if (active) {
                m_info.activePowerProfile = idx;
            }
            m_info.powerProfiles.push_back({idx, name});
        }
    }

    updateLiveMetrics();
}

void GpuManager::scanIntelGpu(const std::string& cardPath) {
    m_isIntel = true;
    m_info.present = true;
    m_info.driver = "i915";
    m_info.cardPath = cardPath;
    m_info.name = "Intel Integrated Graphics";
    m_info.availablePerfLevels = {"auto"};
    m_info.perfLevel = "auto";
    updateLiveMetrics();
}

void GpuManager::scanNvidiaGpu() {
    std::string testSmi = SysfsUtils::executeCommand("which nvidia-smi 2>/dev/null");
    if (testSmi.empty()) return;

    std::string out = SysfsUtils::executeCommand("nvidia-smi --query-gpu=name,driver_version,temperature.gpu,power.draw,power.limit,clocks.gr,clocks.mem --format=csv,noheader,nounits 2>/dev/null");
    if (out.empty()) return;

    m_isNvidia = true;
    m_info.present = true;
    m_info.driver = "nvidia";

    std::stringstream ss(out);
    std::string field;
    std::vector<std::string> fields;
    while (std::getline(ss, field, ',')) {
        fields.push_back(SysfsUtils::trim(field));
    }

    if (fields.size() >= 7) {
        m_info.name = fields[0];
        try {
            m_info.temperatureC = std::stod(fields[2]);
            m_info.powerW = std::stod(fields[3]);
            m_info.powerCapW = std::stod(fields[4]);
            m_info.curSclkMhz = std::stoi(fields[5]);
            m_info.curMclkMhz = std::stoi(fields[6]);
        } catch (...) {}
    }
}

void GpuManager::updateLiveMetrics() {
    if (!m_info.present) return;

    if (m_isAmd) {
        std::string devPath = m_info.cardPath + "/device";

        // Update active SCLK & MCLK
        auto sclkLines = SysfsUtils::readLines(devPath + "/pp_dpm_sclk");
        for (const auto& line : sclkLines) {
            if (line.find('*') != std::string::npos) {
                std::regex re(R"((\d+):\s*(\d+)Mhz)");
                std::smatch match;
                if (std::regex_search(line, match, re)) {
                    m_info.activeSclkIndex = std::stoi(match[1].str());
                    m_info.curSclkMhz = std::stoi(match[2].str());
                }
            }
        }

        auto mclkLines = SysfsUtils::readLines(devPath + "/pp_dpm_mclk");
        for (const auto& line : mclkLines) {
            if (line.find('*') != std::string::npos) {
                std::regex re(R"((\d+):\s*(\d+)Mhz)");
                std::smatch match;
                if (std::regex_search(line, match, re)) {
                    m_info.activeMclkIndex = std::stoi(match[1].str());
                    m_info.curMclkMhz = std::stoi(match[2].str());
                }
            }
        }

        // Hwmon temp & power
        if (!m_hwmonPath.empty()) {
            long tempMilli = SysfsUtils::readLong(m_hwmonPath + "/temp1_input", 0);
            m_info.temperatureC = tempMilli / 1000.0;

            // Power
            long powerMicro = SysfsUtils::readLong(m_hwmonPath + "/power1_average", 0);
            if (powerMicro == 0) {
                powerMicro = SysfsUtils::readLong(m_hwmonPath + "/power1_input", 0);
            }
            m_info.powerW = powerMicro / 1000000.0;

            long capMicro = SysfsUtils::readLong(m_hwmonPath + "/power1_cap", 0);
            m_info.powerCapW = capMicro / 1000000.0;
        }
    } else if (m_isNvidia) {
        std::string out = SysfsUtils::executeCommand("nvidia-smi --query-gpu=temperature.gpu,power.draw,clocks.gr,clocks.mem --format=csv,noheader,nounits 2>/dev/null");
        if (!out.empty()) {
            std::stringstream ss(out);
            std::string f;
            std::vector<std::string> fields;
            while (std::getline(ss, f, ',')) {
                fields.push_back(SysfsUtils::trim(f));
            }
            if (fields.size() >= 4) {
                try {
                    m_info.temperatureC = std::stod(fields[0]);
                    m_info.powerW = std::stod(fields[1]);
                    m_info.curSclkMhz = std::stoi(fields[2]);
                    m_info.curMclkMhz = std::stoi(fields[3]);
                } catch (...) {}
            }
        }
    } else if (m_isIntel) {
        m_info.curSclkMhz = SysfsUtils::readLong(m_info.cardPath + "/gt_cur_freq_mhz", 0);
    }
}

bool GpuManager::setPerformanceLevel(const std::string& level) {
    if (!m_isAmd) return false;
    std::string path = m_info.cardPath + "/device/power_dpm_force_performance_level";
    bool ok = SysfsUtils::writeFilePrivileged(path, level);
    if (ok) {
        m_info.perfLevel = level;
        std::cout << "[KAIZEN] GPU: Performance level set to '" << level << "'" << std::endl;
    }
    return ok;
}

bool GpuManager::setSclkState(int stateIndex) {
    if (!m_isAmd) return false;
    // Set to manual performance level first if not manual
    if (m_info.perfLevel != "manual") {
        setPerformanceLevel("manual");
    }
    std::string path = m_info.cardPath + "/device/pp_dpm_sclk";
    bool ok = SysfsUtils::writeFilePrivileged(path, std::to_string(stateIndex));
    if (ok) {
        m_info.activeSclkIndex = stateIndex;
        std::cout << "[KAIZEN] GPU: Core clock state (sclk) set to " << stateIndex << std::endl;
    }
    return ok;
}

bool GpuManager::setMclkState(int stateIndex) {
    if (!m_isAmd) return false;
    if (m_info.perfLevel != "manual") {
        setPerformanceLevel("manual");
    }
    std::string path = m_info.cardPath + "/device/pp_dpm_mclk";
    bool ok = SysfsUtils::writeFilePrivileged(path, std::to_string(stateIndex));
    if (ok) {
        m_info.activeMclkIndex = stateIndex;
        std::cout << "[KAIZEN] GPU: Memory clock state (mclk) set to " << stateIndex << std::endl;
    }
    return ok;
}

bool GpuManager::setPowerProfileMode(int modeIndex) {
    if (!m_isAmd) return false;
    std::string path = m_info.cardPath + "/device/pp_power_profile_mode";
    bool ok = SysfsUtils::writeFilePrivileged(path, std::to_string(modeIndex));
    if (ok) {
        m_info.activePowerProfile = modeIndex;
        std::cout << "[KAIZEN] GPU: PowerPlay profile mode set to " << modeIndex << std::endl;
    }
    return ok;
}

} // namespace fc
