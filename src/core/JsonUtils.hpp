#pragma once

#include "Types.hpp"
#include "SystemInfo.hpp"
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace fc {

class JsonUtils {
public:
    static std::string escape(const std::string& s) {
        std::ostringstream o;
        for (char c : s) {
            if (c == '"') o << "\\\"";
            else if (c == '\\') o << "\\\\";
            else if (c == '\b') o << "\\b";
            else if (c == '\f') o << "\\f";
            else if (c == '\n') o << "\\n";
            else if (c == '\r') o << "\\r";
            else if (c == '\t') o << "\\t";
            else if ('\x00' <= c && c <= '\x1f') {
                o << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
            } else {
                o << c;
            }
        }
        return o.str();
    }

    static std::string summaryToJson(const SystemSummary& s) {
        std::ostringstream oss;
        oss << "{"
            << "\"osName\":\"" << escape(s.os.name) << "\","
            << "\"osPrettyName\":\"" << escape(s.os.prettyName) << "\","
            << "\"osId\":\"" << escape(s.os.id) << "\","
            << "\"osLogo\":\"" << escape(s.os.logo) << "\","
            << "\"kernel\":\"" << escape(s.os.kernel) << "\","
            << "\"arch\":\"" << escape(s.os.architecture) << "\","
            << "\"hostname\":\"" << escape(s.os.hostname) << "\","
            << "\"uptime\":\"" << escape(s.os.uptimeStr) << "\","
            << "\"cpuModel\":\"" << escape(s.cpuModel) << "\","
            << "\"cpuCores\":" << s.cpuCores << ","
            << "\"cpuAvgFreqGhz\":" << s.cpuAvgFreqGhz << ","
            << "\"cpuTempC\":" << s.cpuTempC << ","
            << "\"cpuGovernor\":\"" << escape(s.cpuGovernor) << "\","
            << "\"gpuPresent\":" << (s.gpuPresent ? "true" : "false") << ","
            << "\"gpuName\":\"" << escape(s.gpuName) << "\","
            << "\"gpuDriver\":\"" << escape(s.gpuDriver) << "\","
            << "\"gpuTempC\":" << s.gpuTempC << ","
            << "\"gpuPowerW\":" << s.gpuPowerW << ","
            << "\"fansTotal\":" << s.fansTotal << ","
            << "\"fansActive\":" << s.fansActive << ","
            << "\"fansAvgRpm\":" << s.fansAvgRpm << ","
            << "\"activeProfile\":\"" << escape(s.activeProfileName) << "\","
            << "\"daemonRunning\":" << (s.daemonRunning ? "true" : "false")
            << "}";
        return oss.str();
    }

    static std::string curvePointsToJson(const std::vector<CurvePoint>& pts) {
        std::ostringstream oss;
        oss << "[";
        for (size_t i = 0; i < pts.size(); ++i) {
            oss << "{\"temp\":" << pts[i].tempC << ",\"speed\":" << pts[i].speedPercent << "}";
            if (i + 1 < pts.size()) oss << ",";
        }
        oss << "]";
        return oss.str();
    }

    static std::vector<CurvePoint> curvePointsFromJson(const std::string& json) {
        std::vector<CurvePoint> points;
        try {
            size_t pos = 0;
            while ((pos = json.find("temp", pos)) != std::string::npos) {
                size_t colon = json.find(':', pos);
                if (colon == std::string::npos) break;
                int temp = std::stoi(json.substr(colon + 1));

                size_t speedKey = json.find("speed", colon);
                if (speedKey == std::string::npos) break;
                size_t speedColon = json.find(':', speedKey);
                if (speedColon == std::string::npos) break;
                int speed = std::stoi(json.substr(speedColon + 1));

                points.push_back({temp, speed});
                pos = speedColon + 1;
            }
        } catch (...) {
            points.clear();
        }
        if (points.empty()) {
            points = {{30, 25}, {50, 45}, {70, 75}, {85, 100}};
        }
        return points;
    }

    static std::string fansToJson(const std::vector<FanInfo>& fans) {
        std::ostringstream oss;
        oss << "[";
        for (size_t i = 0; i < fans.size(); ++i) {
            const auto& f = fans[i];
            oss << "{"
                << "\"id\":\"" << escape(f.id) << "\","
                << "\"label\":\"" << escape(f.label) << "\","
                << "\"hwmonName\":\"" << escape(f.hwmonName) << "\","
                << "\"fanIndex\":" << f.fanIndex << ","
                << "\"type\":\"" << fanTypeToString(f.userType) << "\","
                << "\"reason\":\"" << escape(f.detectionReason) << "\","
                << "\"rpm\":" << f.rpm << ","
                << "\"hasPwm\":" << (f.hasPwm ? "true" : "false") << ","
                << "\"pwmPercent\":" << f.currentPwmPercent << ","
                << "\"mode\":\"" << fanControlModeToString(f.controlMode) << "\","
                << "\"curve\":" << curvePointsToJson(f.curvePoints)
                << "}";
            if (i + 1 < fans.size()) oss << ",";
        }
        oss << "]";
        return oss.str();
    }

    static std::string cpuInfoToJson(const CpuInfo& cpu) {
        std::ostringstream oss;
        oss << "{"
            << "\"model\":\"" << escape(cpu.modelName) << "\","
            << "\"coreCount\":" << cpu.coreCount << ","
            << "\"avgFreqGhz\":" << cpu.avgFreqGhz << ","
            << "\"minLimitGhz\":" << cpu.minLimitGhz << ","
            << "\"maxLimitGhz\":" << cpu.maxLimitGhz << ","
            << "\"tempC\":" << cpu.temperatureC << ","
            << "\"governor\":\"" << escape(cpu.currentGovernor) << "\","
            << "\"epp\":\"" << escape(cpu.energyPreference) << "\","
            << "\"turbo\":" << (cpu.turboBoost ? "true" : "false") << ","
            << "\"hasTurboControl\":" << (cpu.hasTurboControl ? "true" : "false") << ","
            << "\"availableGovernors\":[";
        for (size_t i = 0; i < cpu.availableGovernors.size(); ++i) {
            oss << "\"" << escape(cpu.availableGovernors[i]) << "\"";
            if (i + 1 < cpu.availableGovernors.size()) oss << ",";
        }
        oss << "],\"cores\":[";
        for (size_t i = 0; i < cpu.cores.size(); ++i) {
            const auto& c = cpu.cores[i];
            oss << "{\"id\":" << c.coreId << ",\"freq\":" << c.curFreqGhz << ",\"gov\":\"" << escape(c.governor) << "\"}";
            if (i + 1 < cpu.cores.size()) oss << ",";
        }
        oss << "]}";
        return oss.str();
    }

    static std::string gpuInfoToJson(const GpuInfo& gpu) {
        std::ostringstream oss;
        oss << "{"
            << "\"present\":" << (gpu.present ? "true" : "false") << ","
            << "\"name\":\"" << escape(gpu.name) << "\","
            << "\"driver\":\"" << escape(gpu.driver) << "\","
            << "\"pciAddress\":\"" << escape(gpu.pciAddress) << "\","
            << "\"tempC\":" << gpu.temperatureC << ","
            << "\"powerW\":" << gpu.powerW << ","
            << "\"sclkMhz\":" << gpu.curSclkMhz << ","
            << "\"mclkMhz\":" << gpu.curMclkMhz << ","
            << "\"perfLevel\":\"" << escape(gpu.perfLevel) << "\","
            << "\"activeSclk\":" << gpu.activeSclkIndex << ","
            << "\"activeMclk\":" << gpu.activeMclkIndex << ","
            << "\"activePowerProfile\":" << gpu.activePowerProfile
            << "}";
        return oss.str();
    }
};

} // namespace fc
