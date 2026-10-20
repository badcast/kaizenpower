#pragma once

#include "I18n.hpp"
#include "SystemInfo.hpp"
#include <string>
#include <vector>
#include <utility>
#include <algorithm>
#include <cmath>

namespace fc {

enum class FanType {
    CPU,
    GPU,
    External, // Chassis / Case / External
    Unknown
};

enum class FanControlMode {
    Auto,   // Hardware / BIOS / Driver control
    Simple, // Fixed manual slider speed
    Curve   // Dynamic temperature-based curve
};

inline std::string fanControlModeToString(FanControlMode mode) {
    switch (mode) {
        case FanControlMode::Auto: return "auto";
        case FanControlMode::Simple: return "simple";
        case FanControlMode::Curve: return "curve";
        default: return "auto";
    }
}

inline FanControlMode stringToFanControlMode(const std::string& str) {
    if (str == "simple") return FanControlMode::Simple;
    if (str == "curve") return FanControlMode::Curve;
    return FanControlMode::Auto;
}

inline std::string fanTypeToString(FanType type) {
    switch (type) {
        case FanType::CPU: return _("CPU Fan");
        case FanType::GPU: return _("GPU Fan");
        case FanType::External: return _("Chassis / External Fan");
        case FanType::Unknown: default: return _("Unknown Fan");
    }
}

inline std::string fanTypeBadgeText(FanType type) {
    switch (type) {
        case FanType::CPU: return "CPU Fan";
        case FanType::GPU: return "GPU Fan";
        case FanType::External: return _("Chassis / External");
        case FanType::Unknown: default: return _("Unknown");
    }
}

inline std::string fanTypeIcon(FanType type) {
    switch (type) {
        case FanType::CPU: return "computer-symbolic";
        case FanType::GPU: return "show-gpu-effects-symbolic";
        case FanType::External: return "applications-system-symbolic";
        case FanType::Unknown: default: return "system-run-symbolic";
    }
}

inline std::string fanTypeCssClass(FanType type) {
    switch (type) {
        case FanType::CPU: return "fan-badge-cpu";
        case FanType::GPU: return "fan-badge-gpu";
        case FanType::External: return "fan-badge-case";
        case FanType::Unknown: default: return "fan-badge-unknown";
    }
}

struct CurvePoint {
    int tempC = 30;        // 0 - 100 °C
    int speedPercent = 30; // 0 - 100 %
};

struct FanInfo {
    std::string id;              // e.g. "hwmon4_fan2"
    std::string hwmonPath;       // e.g. "/sys/class/hwmon/hwmon4"
    std::string hwmonName;       // e.g. "nct6791"
    int fanIndex = 1;            // e.g. 2
    std::string label;           // e.g. "CPU Fan" or custom
    FanType detectedType = FanType::Unknown;
    FanType userType = FanType::Unknown;
    std::string detectionReason; // e.g. "Bound to thermal sensor: PECI Agent 0"
    
    int rpm = 0;
    int minRpm = 0;
    int maxRpm = 0;
    
    // PWM Controls
    bool hasPwm = false;
    int pwmIndex = 1;
    int currentPwm = 0;          // 0-255
    int currentPwmPercent = 0;   // 0-100
    int pwmEnableMode = 2;       // 1 = manual, 2 or 5 = auto, 0 = disabled/full
    bool isManual = false;

    // Control Modes: Auto (BIOS), Simple (Slider), Curve
    FanControlMode controlMode = FanControlMode::Auto;

    // Thermal Curve Points
    std::vector<CurvePoint> curvePoints = {
        {30, 25},
        {50, 45},
        {70, 75},
        {85, 100}
    };

    int evaluateCurve(double tempC) const {
        if (curvePoints.empty()) return currentPwmPercent;
        if (tempC <= curvePoints.front().tempC) return curvePoints.front().speedPercent;
        if (tempC >= curvePoints.back().tempC) return curvePoints.back().speedPercent;

        for (size_t i = 0; i < curvePoints.size() - 1; ++i) {
            if (tempC >= curvePoints[i].tempC && tempC <= curvePoints[i + 1].tempC) {
                double t0 = curvePoints[i].tempC;
                double t1 = curvePoints[i + 1].tempC;
                double s0 = curvePoints[i].speedPercent;
                double s1 = curvePoints[i + 1].speedPercent;
                if (std::abs(t1 - t0) < 0.001) return static_cast<int>(s1);
                double ratio = (tempC - t0) / (t1 - t0);
                return std::clamp(static_cast<int>(std::round(s0 + ratio * (s1 - s0))), 0, 100);
            }
        }
        return curvePoints.back().speedPercent;
    }
};

struct CpuCoreInfo {
    int coreId = 0;
    double curFreqGhz = 0.0;
    double minFreqGhz = 0.0;
    double maxFreqGhz = 0.0;
    std::string governor;
};

struct CpuInfo {
    std::string modelName;
    int coreCount = 0;
    double avgFreqGhz = 0.0;
    double minLimitGhz = 0.8;
    double maxLimitGhz = 4.2;
    double temperatureC = 0.0;
    
    std::string currentGovernor;
    std::vector<std::string> availableGovernors;
    
    std::string energyPreference;
    std::vector<std::string> availableEnergyPreferences;
    
    bool turboBoost = true;
    bool hasTurboControl = false;
    
    std::vector<CpuCoreInfo> cores;
};

struct GpuClockState {
    int index = 0;
    int mhz = 0;
    bool isActive = false;
};

struct GpuInfo {
    bool present = false;
    std::string name;
    std::string driver;          // e.g. "amdgpu", "nvidia", "i915"
    std::string pciAddress;      // e.g. "0000:01:00.0"
    std::string cardPath;        // e.g. "/sys/class/drm/card1"
    
    double temperatureC = 0.0;
    double powerW = 0.0;
    double powerCapW = 0.0;
    
    int curSclkMhz = 0;          // GPU core clock
    int curMclkMhz = 0;          // GPU memory clock
    
    std::string perfLevel;       // "auto", "low", "high", "manual"
    std::vector<std::string> availablePerfLevels;
    
    std::vector<GpuClockState> sclkStates;
    std::vector<GpuClockState> mclkStates;
    int activeSclkIndex = -1;
    int activeMclkIndex = -1;
    
    int activePowerProfile = -1;
    std::vector<std::pair<int, std::string>> powerProfiles;
};

enum class SystemProfile {
    Silent,
    Balanced,
    Performance,
    MaxCooling
};

struct SystemSummary {
    OsInfo os;
    std::string cpuModel;
    int cpuCores = 0;
    double cpuAvgFreqGhz = 0.0;
    double cpuTempC = 0.0;
    std::string cpuGovernor;
    bool gpuPresent = false;
    std::string gpuName;
    std::string gpuDriver;
    double gpuTempC = 0.0;
    double gpuPowerW = 0.0;
    int fansTotal = 0;
    int fansActive = 0;
    int fansAvgRpm = 0;
    SystemProfile activeProfile = SystemProfile::Balanced;
    std::string activeProfileName = "Balanced";
    bool daemonRunning = false;
};

} // namespace fc
