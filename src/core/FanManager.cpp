#include "FanManager.hpp"
#include "SysfsUtils.hpp"
#include "I18n.hpp"

#include <filesystem>
#include <iostream>
#include <algorithm>
#include <cmath>

namespace fs = std::filesystem;

namespace fc {

FanManager::FanManager() {
    scanFans();
}

int FanManager::getAutoModeCode(const std::string& hwmonName) const {
    if (hwmonName.find("nct67") != std::string::npos) {
        return 5; // SmartFan IV mode
    }
    return 2; // Standard automatic mode for amdgpu and generic hwmon
}

FanType FanManager::detectFanType(const std::string& hwmonPath,
                                  const std::string& hwmonName,
                                  int fanIndex,
                                  const std::string& label,
                                  std::string& outReason) {
    // 1. Check parent PCI device class and driver (GPU detection)
    std::string devicePath;
    try {
        if (fs::exists(hwmonPath + "/device")) {
            devicePath = fs::canonical(hwmonPath + "/device").string();
        }
    } catch (...) {}

    // Check PCI class
    std::string pciClass = SysfsUtils::readFile(hwmonPath + "/device/class");
    if (!pciClass.empty()) {
        if (pciClass.find("0x03") == 0) {
            outReason = std::string(_("PCI Display / VGA Controller")) + " (" + hwmonName + ")";
            return FanType::GPU;
        }
    }

    if (devicePath.find("/drm/") != std::string::npos ||
        hwmonName == "amdgpu" ||
        hwmonName == "radeon" ||
        hwmonName == "nouveau" ||
        hwmonName.find("nvidia") != std::string::npos ||
        hwmonName == "i915" ||
        hwmonName == "xe") {
        outReason = std::string(_("GPU Controller (Driver: ")) + hwmonName + ")";
        return FanType::GPU;
    }

    // 2. Check explicit labels
    std::string lowerLabel = label;
    std::transform(lowerLabel.begin(), lowerLabel.end(), lowerLabel.begin(), ::tolower);
    if (!lowerLabel.empty()) {
        if (lowerLabel.find("gpu") != std::string::npos || lowerLabel.find("vga") != std::string::npos) {
            outReason = std::string(_("Sensor label: ")) + label;
            return FanType::GPU;
        }
        if (lowerLabel.find("cpu") != std::string::npos ||
            lowerLabel.find("processor") != std::string::npos ||
            lowerLabel.find("aio") != std::string::npos ||
            lowerLabel.find("pump") != std::string::npos) {
            outReason = std::string(_("Sensor label: ")) + label;
            return FanType::CPU;
        }
        if (lowerLabel.find("cha") != std::string::npos ||
            lowerLabel.find("chassis") != std::string::npos ||
            lowerLabel.find("case") != std::string::npos ||
            lowerLabel.find("sys") != std::string::npos ||
            lowerLabel.find("aux") != std::string::npos ||
            lowerLabel.find("front") != std::string::npos ||
            lowerLabel.find("rear") != std::string::npos ||
            lowerLabel.find("top") != std::string::npos ||
            lowerLabel.find("bottom") != std::string::npos) {
            outReason = std::string(_("Sensor label: ")) + label;
            return FanType::External;
        }
    }

    // 3. Check SuperIO PWM Temperature Selector
    std::string pwmTempSelPath = hwmonPath + "/pwm" + std::to_string(fanIndex) + "_temp_sel";
    if (fs::exists(pwmTempSelPath)) {
        std::string tempIdx = SysfsUtils::readFile(pwmTempSelPath);
        if (!tempIdx.empty()) {
            std::string tempLabel = SysfsUtils::readFile(hwmonPath + "/temp" + tempIdx + "_label");
            std::string lowerTemp = tempLabel;
            std::transform(lowerTemp.begin(), lowerTemp.end(), lowerTemp.begin(), ::tolower);

            if (lowerTemp.find("peci") != std::string::npos ||
                lowerTemp.find("cpu") != std::string::npos ||
                lowerTemp.find("core") != std::string::npos ||
                lowerTemp.find("package") != std::string::npos ||
                lowerTemp.find("tdie") != std::string::npos ||
                lowerTemp.find("tctl") != std::string::npos) {
                outReason = std::string(_("Bound to CPU thermal sensor: ")) + tempLabel;
                return FanType::CPU;
            }

            if (lowerTemp.find("aux") != std::string::npos ||
                lowerTemp.find("sys") != std::string::npos ||
                lowerTemp.find("ambient") != std::string::npos ||
                lowerTemp.find("pch") != std::string::npos ||
                lowerTemp.find("motherboard") != std::string::npos) {
                outReason = std::string(_("Bound to motherboard system sensor: ")) + tempLabel;
                return FanType::External;
            }
        }
    }

    // 4. Check external USB controllers
    if (hwmonName.find("corsair") != std::string::npos ||
        hwmonName.find("nzxt") != std::string::npos ||
        hwmonName.find("aquacomputer") != std::string::npos ||
        hwmonName.find("commander") != std::string::npos) {
        outReason = std::string(_("External USB cooling controller")) + " (" + hwmonName + ")";
        return FanType::External;
    }

    // 5. Laptop ACPI controllers
    if (hwmonName == "asus-nb-wmi" || hwmonName == "dell_smm" || hwmonName == "thinkpad_acpi") {
        if (fanIndex == 1) {
            outReason = _("Laptop primary cooling fan (CPU)");
            return FanType::CPU;
        } else {
            outReason = _("Laptop secondary cooling fan (GPU/Chassis)");
            return FanType::GPU;
        }
    }

    // 6. SuperIO motherboard pin heuristics
    if (hwmonName.find("nct") != std::string::npos ||
        hwmonName.find("it8") != std::string::npos ||
        hwmonName.find("w83") != std::string::npos) {
        if (fanIndex == 1 || fanIndex == 2) {
            outReason = std::string(_("Motherboard CPU_FAN/CPU_OPT header (SuperIO ")) + hwmonName + ")";
            return FanType::CPU;
        } else {
            outReason = _("Motherboard CHA_FAN header (Chassis/Case)");
            return FanType::External;
        }
    }

    outReason = std::string(_("hwmon device: ")) + hwmonName;
    return FanType::External;
}

void FanManager::scanFans() {
    m_fans.clear();

    const std::string hwmonBase = "/sys/class/hwmon";
    if (!fs::exists(hwmonBase)) return;

    for (const auto& entry : fs::directory_iterator(hwmonBase)) {
        std::string hwmonPath = entry.path().string();
        std::string hwmonName = SysfsUtils::readFile(hwmonPath + "/name");

        for (int i = 1; i <= 16; ++i) {
            std::string fanInputPath = hwmonPath + "/fan" + std::to_string(i) + "_input";
            if (!fs::exists(fanInputPath)) continue;

            FanInfo fan;
            fan.id = entry.path().filename().string() + "_fan" + std::to_string(i);
            fan.hwmonPath = hwmonPath;
            fan.hwmonName = hwmonName;
            fan.fanIndex = i;
            fan.label = SysfsUtils::readFile(hwmonPath + "/fan" + std::to_string(i) + "_label");
            fan.rpm = SysfsUtils::readLong(fanInputPath, 0);
            fan.minRpm = SysfsUtils::readLong(hwmonPath + "/fan" + std::to_string(i) + "_min", 0);
            fan.maxRpm = SysfsUtils::readLong(hwmonPath + "/fan" + std::to_string(i) + "_max", 0);

            fan.detectedType = detectFanType(hwmonPath, hwmonName, i, fan.label, fan.detectionReason);
            fan.userType = fan.detectedType;

            // Check PWM controls
            std::string pwmPath = hwmonPath + "/pwm" + std::to_string(i);
            if (fs::exists(pwmPath)) {
                fan.hasPwm = true;
                fan.pwmIndex = i;
                fan.currentPwm = SysfsUtils::readLong(pwmPath, 0);
                fan.currentPwmPercent = std::clamp(static_cast<int>(std::round(fan.currentPwm * 100.0 / 255.0)), 0, 100);

                std::string enablePath = hwmonPath + "/pwm" + std::to_string(i) + "_enable";
                if (fs::exists(enablePath)) {
                    fan.pwmEnableMode = SysfsUtils::readLong(enablePath, 2);
                    fan.isManual = (fan.pwmEnableMode == 1);
                } else {
                    fan.isManual = false;
                }

                fan.controlMode = fan.isManual ? FanControlMode::Simple : FanControlMode::Auto;
            } else {
                fan.hasPwm = false;
                fan.controlMode = FanControlMode::Auto;
            }

            m_fans.push_back(fan);
        }
    }
}

void FanManager::updateLiveMetrics(double cpuTemp, double gpuTemp, bool applyCurves) {
    for (auto& fan : m_fans) {
        std::string fanInputPath = fan.hwmonPath + "/fan" + std::to_string(fan.fanIndex) + "_input";
        fan.rpm = SysfsUtils::readLong(fanInputPath, 0);

        if (fan.hasPwm) {
            std::string pwmPath = fan.hwmonPath + "/pwm" + std::to_string(fan.pwmIndex);
            fan.currentPwm = SysfsUtils::readLong(pwmPath, fan.currentPwm);
            fan.currentPwmPercent = std::clamp(static_cast<int>(std::round(fan.currentPwm * 100.0 / 255.0)), 0, 100);

            std::string enablePath = fan.hwmonPath + "/pwm" + std::to_string(fan.pwmIndex) + "_enable";
            if (fs::exists(enablePath)) {
                fan.pwmEnableMode = SysfsUtils::readLong(enablePath, fan.pwmEnableMode);
                fan.isManual = (fan.pwmEnableMode == 1);
            }

            // ONLY apply curves if explicitly requested (e.g. by the root daemon)
            if (applyCurves && fan.controlMode == FanControlMode::Curve) {
                double targetTemp = cpuTemp;
                if (fan.userType == FanType::GPU && gpuTemp > 0.0) {
                    targetTemp = gpuTemp;
                } else if (fan.userType == FanType::External) {
                    targetTemp = std::max(cpuTemp, gpuTemp);
                }

                if (targetTemp > 0.0) {
                    int desiredPercent = fan.evaluateCurve(targetTemp);
                    // Apply if difference is at least 2% to avoid excessive sysfs writes
                    if (std::abs(desiredPercent - fan.currentPwmPercent) >= 2) {
                        setFanSpeedPercent(fan.id, desiredPercent);
                    }
                }
            }
        }
    }
}

FanInfo* FanManager::getFanById(const std::string& id) {
    for (auto& fan : m_fans) {
        if (fan.id == id) return &fan;
    }
    return nullptr;
}

bool FanManager::setFanSpeedPercent(const std::string& fanId, int percent) {
    FanInfo* fan = getFanById(fanId);
    if (!fan || !fan->hasPwm) return false;

    percent = std::clamp(percent, 0, 100);
    int pwmVal = std::clamp(static_cast<int>(std::round(percent * 255.0 / 100.0)), 0, 255);

    // 1. Set manual mode first
    std::string enablePath = fan->hwmonPath + "/pwm" + std::to_string(fan->pwmIndex) + "_enable";
    if (fs::exists(enablePath)) {
        SysfsUtils::writeFilePrivileged(enablePath, "1");
    }

    // 2. Set PWM value
    std::string pwmPath = fan->hwmonPath + "/pwm" + std::to_string(fan->pwmIndex);
    bool ok = SysfsUtils::writeFilePrivileged(pwmPath, std::to_string(pwmVal));
    if (ok) {
        fan->currentPwm = pwmVal;
        fan->currentPwmPercent = percent;
        fan->isManual = true;
        fan->pwmEnableMode = 1;
        std::cout << "[KAIZEN] Fan '" << fanId << "': speed set to " << percent << "% (PWM: " << pwmVal << ")" << std::endl;
    }
    return ok;
}

bool FanManager::setFanAutoMode(const std::string& fanId, bool autoMode) {
    FanInfo* fan = getFanById(fanId);
    if (!fan || !fan->hasPwm) return false;

    std::string enablePath = fan->hwmonPath + "/pwm" + std::to_string(fan->pwmIndex) + "_enable";
    if (!fs::exists(enablePath)) return false;

    int targetMode = autoMode ? getAutoModeCode(fan->hwmonName) : 1;
    bool ok = SysfsUtils::writeFilePrivileged(enablePath, std::to_string(targetMode));
    if (ok) {
        fan->pwmEnableMode = targetMode;
        fan->isManual = !autoMode;
        fan->controlMode = autoMode ? FanControlMode::Auto : FanControlMode::Simple;
        std::cout << "[KAIZEN] Fan '" << fanId << "': auto mode set to " << (autoMode ? "ENABLED" : "DISABLED") << std::endl;
    }
    return ok;
}

bool FanManager::setFanControlMode(const std::string& fanId, FanControlMode mode) {
    FanInfo* fan = getFanById(fanId);
    if (!fan || !fan->hasPwm) return false;

    fan->controlMode = mode;
    std::string modeStr = (mode == FanControlMode::Auto) ? "Auto" : ((mode == FanControlMode::Simple) ? "Simple (Manual)" : "Curve");
    std::cout << "[KAIZEN] Fan '" << fanId << "': control mode set to " << modeStr << std::endl;

    if (mode == FanControlMode::Auto) {
        return setFanAutoMode(fanId, true);
    } else {
        // Manual mode required for Simple slider or custom software Curve
        std::string enablePath = fan->hwmonPath + "/pwm" + std::to_string(fan->pwmIndex) + "_enable";
        if (fs::exists(enablePath)) {
            SysfsUtils::writeFilePrivileged(enablePath, "1");
        }
        fan->isManual = true;
        fan->pwmEnableMode = 1;
        return true;
    }
}

bool FanManager::setFanCurvePoints(const std::string& fanId, const std::vector<CurvePoint>& points) {
    FanInfo* fan = getFanById(fanId);
    if (!fan) return false;
    fan->curvePoints = points;
    // Sort points by temperature
    std::sort(fan->curvePoints.begin(), fan->curvePoints.end(), [](const CurvePoint& a, const CurvePoint& b) {
        return a.tempC < b.tempC;
    });
    std::cout << "[KAIZEN] Fan '" << fanId << "': curve updated with " << points.size() << " control points" << std::endl;
    return true;
}

bool FanManager::setUserFanType(const std::string& fanId, FanType type) {
    FanInfo* fan = getFanById(fanId);
    if (!fan) return false;
    fan->userType = type;
    std::string typeStr = (type == FanType::CPU) ? "CPU" : ((type == FanType::GPU) ? "GPU" : ((type == FanType::External) ? "Chassis" : "Unknown"));
    std::cout << "[KAIZEN] Fan '" << fanId << "': user type set to " << typeStr << std::endl;
    return true;
}

void FanManager::setAllFansPercent(int percent) {
    std::cout << "[KAIZEN] Setting all fans to " << percent << "% speed" << std::endl;
    for (auto& fan : m_fans) {
        if (fan.hasPwm) {
            fan.controlMode = FanControlMode::Simple;
            setFanSpeedPercent(fan.id, percent);
        }
    }
}

void FanManager::setAllFansAuto() {
    std::cout << "[KAIZEN] Restoring all fans to automatic hardware control" << std::endl;
    for (auto& fan : m_fans) {
        if (fan.hasPwm) {
            fan.controlMode = FanControlMode::Auto;
            setFanAutoMode(fan.id, true);
        }
    }
}

} // namespace fc
