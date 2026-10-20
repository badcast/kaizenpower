#include "CliApp.hpp"
#include "../tui/TuiApp.hpp"
#include "../dbus/DBusServer.hpp"
#include "../core/CpuManager.hpp"
#include "../core/GpuManager.hpp"
#include "../core/FanManager.hpp"
#include "../core/ProfileManager.hpp"
#include "../core/SystemInfo.hpp"
#include "../core/JsonUtils.hpp"
#include <iostream>
#include <unistd.h>
#include <libgen.h>
#include <cstring>

namespace fc {

CliApp::CliApp() {
    m_useColor = isatty(STDOUT_FILENO) != 0;
    m_client = std::make_shared<DBusClient>();
}

CliApp::~CliApp() = default;

std::string CliApp::colorize(const std::string& text, const char* ansiCode) {
    if (!m_useColor) return text;
    return std::string(ansiCode) + text + "\033[0m";
}

std::string CliApp::bold(const std::string& text) { return colorize(text, "\033[1m"); }
std::string CliApp::cyan(const std::string& text) { return colorize(text, "\033[36m"); }
std::string CliApp::green(const std::string& text) { return colorize(text, "\033[32m"); }
std::string CliApp::yellow(const std::string& text) { return colorize(text, "\033[33m"); }
std::string CliApp::red(const std::string& text) { return colorize(text, "\033[31m"); }
std::string CliApp::magenta(const std::string& text) { return colorize(text, "\033[35m"); }

void CliApp::printHelp(const char* progName) {
    std::cout << bold(cyan("Kaizen Power Tool")) << " — CLI & Daemon Control Utility\n\n"
              << bold("USAGE:\n")
              << "  " << progName << " [command] [options]\n\n"
              << bold("COMMANDS:\n")
              << "  " << bold("auto") << " [all|cpu|gpu|fan|<id>]          Restore hardware to factory automatic mode\n"
              << "  " << bold("get") << "  [summary|cpu|gpu|fan|all]         Query current hardware metrics\n"
              << "  " << bold("set") << "  cpu governor <name>               Set CPU scaling governor (performance, powersave, schedutil)\n"
              << "  " << bold("set") << "  cpu epp <preference>              Set Energy Performance Preference (performance, balance_power)\n"
              << "  " << bold("set") << "  cpu turbo <on|off>                Enable or disable Turbo Boost\n"
              << "  " << bold("set") << "  cpu min <GHz>                     Set minimum CPU frequency limit\n"
              << "  " << bold("set") << "  cpu max <GHz>                     Set maximum CPU frequency limit\n"
              << "  " << bold("set") << "  gpu level <auto|low|high|manual>  Set GPU DPM performance level\n"
              << "  " << bold("set") << "  gpu sclk <index>                  Fix GPU core clock state\n"
              << "  " << bold("set") << "  gpu mclk <index>                  Fix GPU memory clock state\n"
              << "  " << bold("set") << "  gpu profile <index>               Set AMD PowerPlay profile\n"
              << "  " << bold("set") << "  fan <id> speed <0-100>            Set manual fan PWM percentage\n"
              << "  " << bold("set") << "  fan <id> mode <auto|simple|curve> Switch fan control mode\n"
              << "  " << bold("set") << "  fan <id> curve <quiet|balanced|aggressive> Apply fan curve preset\n"
              << "  " << bold("set") << "  profile <silent|balanced|performance|cooling> Apply complete system profile\n\n"
              << bold("OPTIONS:\n")
              << "  " << bold("--tui") << "                            Launch interactive ncurses terminal dashboard\n"
              << "  " << bold("--color") << "                          Force colored terminal output (default)\n"
              << "  " << bold("--nocolor") << "                        Disable colored terminal output\n"
              << "  " << bold("-h, --help") << "                       Display this help message\n\n"
              << bold("NOTE:\n")
              << "  The dedicated D-Bus system daemon is " << cyan("kaizend") << " (managed by kaizend.service).\n"
              << "  If the binary is invoked as " << cyan("kaizentui") << ", it automatically enters interactive TUI mode.\n";
}

static std::string jsonGetString(const std::string& json, const std::string& key) {
    std::string needle = "\"" + key + "\":\"";
    size_t pos = json.find(needle);
    if (pos == std::string::npos) return "";
    size_t start = pos + needle.size();
    size_t end = json.find('"', start);
    if (end == std::string::npos) return "";
    return json.substr(start, end - start);
}

static std::string jsonGetRaw(const std::string& json, const std::string& key) {
    std::string needle = "\"" + key + "\":";
    size_t pos = json.find(needle);
    if (pos == std::string::npos) return "";
    size_t start = pos + needle.size();
    size_t end = json.find_first_of(",}]", start);
    if (end == std::string::npos) end = json.size();
    std::string v = json.substr(start, end - start);
    if (!v.empty() && v.front() == '"') v = v.substr(1);
    if (!v.empty() && v.back() == '"') v.pop_back();
    return v;
}

void CliApp::printSummary(const std::string& json) {
    std::cout << "\n" << bold(cyan("=== KAIZEN SYSTEM OVERVIEW ===")) << "\n";
    if (!json.empty()) {
        std::cout << bold("OS:           ") << jsonGetString(json, "osPrettyName") << " (" << jsonGetString(json, "arch") << ")\n"
                  << bold("Kernel:       ") << jsonGetString(json, "kernel") << "\n"
                  << bold("Uptime:       ") << jsonGetString(json, "uptime") << "\n"
                  << bold("Hostname:     ") << jsonGetString(json, "hostname") << "\n"
                  << bold("CPU:          ") << jsonGetString(json, "cpuModel") << " (" << jsonGetRaw(json, "cpuCores") << " threads)\n"
                  << bold("CPU Temp:     ") << green(jsonGetRaw(json, "cpuTempC") + "°C") << "\n"
                  << bold("CPU Gov:      ") << jsonGetString(json, "cpuGovernor") << "\n";

        if (jsonGetRaw(json, "gpuPresent") == "true") {
            std::cout << bold("GPU:          ") << jsonGetString(json, "gpuName") << " [" << jsonGetString(json, "gpuDriver") << "]\n"
                      << bold("GPU Temp:     ") << green(jsonGetRaw(json, "gpuTempC") + "°C") << "\n"
                      << bold("GPU Power:    ") << jsonGetRaw(json, "gpuPowerW") << " W\n";
        }

        std::cout << bold("Fans:         ") << jsonGetRaw(json, "fansTotal") << " detected ("
                  << jsonGetRaw(json, "fansActive") << " active, avg " << jsonGetRaw(json, "fansAvgRpm") << " RPM)\n"
                  << bold("Profile:      ") << cyan(jsonGetString(json, "activeProfile")) << "\n"
                  << bold("D-Bus Daemon: ") << green("Active") << "\n";
    } else {
        // Fallback reading directly
        OsInfo os = SystemInfo::getOsInfo();
        std::cout << bold("OS:           ") << os.prettyName << " (" << os.architecture << ")\n"
                  << bold("Kernel:       ") << os.kernel << "\n"
                  << bold("Uptime:       ") << os.uptimeStr << "\n"
                  << bold("Hostname:     ") << os.hostname << "\n";

        auto cpu = std::make_shared<CpuManager>();
        cpu->scanCpu();
        cpu->updateLiveMetrics();
        const auto& c = cpu->getCpuInfo();
        std::cout << bold("CPU:          ") << c.modelName << " (" << c.coreCount << " threads)\n"
                  << bold("CPU Temp:     ") << green(std::to_string(static_cast<int>(c.temperatureC)) + "°C") << "\n"
                  << bold("CPU Gov:      ") << c.currentGovernor << "\n";

        auto gpu = std::make_shared<GpuManager>();
        gpu->scanGpu();
        gpu->updateLiveMetrics();
        const auto& g = gpu->getGpuInfo();
        if (g.present) {
            std::cout << bold("GPU:          ") << g.name << " [" << g.driver << "]\n"
                      << bold("GPU Temp:     ") << green(std::to_string(static_cast<int>(g.temperatureC)) + "°C") << "\n"
                      << bold("GPU Power:    ") << g.powerW << " W\n";
        }

        auto fan = std::make_shared<FanManager>();
        fan->scanFans();
        fan->updateLiveMetrics(c.temperatureC, g.temperatureC);
        const auto& fans = fan->getFans();
        std::cout << bold("Fans:         ") << fans.size() << " detected\n";
        for (const auto& f : fans) {
            std::cout << "  • " << f.id << " (" << (f.label.empty() ? f.hwmonName : f.label) << "): "
                      << green(std::to_string(f.rpm) + " RPM") << " | "
                      << (f.hasPwm ? (std::to_string(f.currentPwmPercent) + "%") : "No PWM")
                      << " [" << fanControlModeToString(f.controlMode) << "]\n";
        }
    }
    std::cout << std::endl;
}

void CliApp::printCpu(const std::string& json) {
    std::cout << "\n" << bold(cyan("=== CPU STATUS ===")) << "\n";
    if (!json.empty()) {
        std::cout << bold("Model:        ") << jsonGetString(json, "model") << "\n"
                  << bold("Cores:        ") << jsonGetRaw(json, "coreCount") << "\n"
                  << bold("Temperature:  ") << green(jsonGetRaw(json, "tempC") + "°C") << "\n"
                  << bold("Average Freq: ") << jsonGetRaw(json, "avgFreqGhz") << " GHz\n"
                  << bold("Governor:     ") << jsonGetString(json, "governor") << "\n"
                  << bold("EPP:          ") << jsonGetString(json, "epp") << "\n"
                  << bold("Turbo Boost:  ") << (jsonGetRaw(json, "turbo") == "true" ? green("Enabled") : yellow("Disabled")) << "\n"
                  << bold("Freq Limits:  ") << jsonGetRaw(json, "minLimitGhz") << " GHz — " << jsonGetRaw(json, "maxLimitGhz") << " GHz\n";
    } else {
        auto cpu = std::make_shared<CpuManager>();
        cpu->scanCpu();
        cpu->updateLiveMetrics();
        const auto& c = cpu->getCpuInfo();
        std::cout << bold("Model:        ") << c.modelName << "\n"
                  << bold("Cores:        ") << c.coreCount << "\n"
                  << bold("Temperature:  ") << green(std::to_string(static_cast<int>(c.temperatureC)) + "°C") << "\n"
                  << bold("Average Freq: ") << c.avgFreqGhz << " GHz\n"
                  << bold("Governor:     ") << c.currentGovernor << "\n"
                  << bold("EPP:          ") << (c.energyPreference.empty() ? "N/A" : c.energyPreference) << "\n"
                  << bold("Turbo Boost:  ") << (c.turboBoost ? green("Enabled") : yellow("Disabled")) << "\n"
                  << bold("Freq Limits:  ") << c.minLimitGhz << " GHz — " << c.maxLimitGhz << " GHz\n";
    }
    std::cout << std::endl;
}

void CliApp::printGpu(const std::string& json) {
    std::cout << "\n" << bold(cyan("=== GPU STATUS ===")) << "\n";
    if (!json.empty()) {
        if (jsonGetRaw(json, "present") != "true") {
            std::cout << yellow("No supported dedicated GPU detected.") << "\n";
        } else {
            std::cout << bold("Model:        ") << jsonGetString(json, "name") << "\n"
                      << bold("Driver:       ") << jsonGetString(json, "driver") << " (" << jsonGetString(json, "pciAddress") << ")\n"
                      << bold("Temperature:  ") << green(jsonGetRaw(json, "tempC") + "°C") << "\n"
                      << bold("Power Draw:   ") << jsonGetRaw(json, "powerW") << " W\n"
                      << bold("Core Clock:   ") << jsonGetRaw(json, "sclkMhz") << " MHz\n"
                      << bold("Memory Clock: ") << jsonGetRaw(json, "mclkMhz") << " MHz\n"
                      << bold("Perf Level:   ") << jsonGetString(json, "perfLevel") << "\n";
        }
    } else {
        auto gpu = std::make_shared<GpuManager>();
        gpu->scanGpu();
        gpu->updateLiveMetrics();
        const auto& g = gpu->getGpuInfo();
        if (!g.present) {
            std::cout << yellow("No supported dedicated GPU detected.") << "\n";
        } else {
            std::cout << bold("Model:        ") << g.name << "\n"
                      << bold("Driver:       ") << g.driver << " (" << g.pciAddress << ")\n"
                      << bold("Temperature:  ") << green(std::to_string(static_cast<int>(g.temperatureC)) + "°C") << "\n"
                      << bold("Power Draw:   ") << g.powerW << " W\n"
                      << bold("Core Clock:   ") << g.curSclkMhz << " MHz\n"
                      << bold("Memory Clock: ") << g.curMclkMhz << " MHz\n"
                      << bold("Perf Level:   ") << g.perfLevel << "\n";
        }
    }
    std::cout << std::endl;
}

void CliApp::printFans(const std::string& json) {
    std::cout << "\n" << bold(cyan("=== COOLING FANS ===")) << "\n";
    if (!json.empty()) {
        size_t pos = 0;
        int count = 0;
        while ((pos = json.find("{\"id\":\"", pos)) != std::string::npos) {
            count++;
            size_t endObj = json.find('}', pos);
            if (endObj == std::string::npos) break;
            std::string item = json.substr(pos, endObj - pos + 1);

            std::string id = jsonGetString(item, "id");
            std::string lbl = jsonGetString(item, "label");
            std::string rpm = jsonGetRaw(item, "rpm");
            std::string hasPwm = jsonGetRaw(item, "hasPwm");
            std::string pwm = jsonGetRaw(item, "pwmPercent");
            std::string mode = jsonGetString(item, "mode");
            std::string type = jsonGetString(item, "type");

            std::cout << bold("• ") << bold(cyan(id)) << "\n"
                      << "  Label:    " << (lbl.empty() ? id : lbl) << "\n"
                      << "  Speed:    " << green(rpm + " RPM") << "\n"
                      << "  PWM:      " << (hasPwm == "true" ? (pwm + "%") : "None") << "\n"
                      << "  Mode:     " << mode << "\n"
                      << "  Type:     " << type << "\n";
            pos = endObj + 1;
        }
        std::cout << bold("Total fans: ") << count << "\n";
    } else {
        auto fan = std::make_shared<FanManager>();
        fan->scanFans();
        fan->updateLiveMetrics();
        const auto& fans = fan->getFans();
        std::cout << bold("Total fans: ") << fans.size() << "\n\n";
        for (const auto& f : fans) {
            std::cout << bold("• ") << bold(cyan(f.id)) << "\n"
                      << "  Label:    " << (f.label.empty() ? f.hwmonName : f.label) << "\n"
                      << "  Speed:    " << green(std::to_string(f.rpm) + " RPM") << "\n"
                      << "  PWM:      " << (f.hasPwm ? (std::to_string(f.currentPwmPercent) + "%") : "None") << "\n"
                      << "  Mode:     " << fanControlModeToString(f.controlMode) << "\n"
                      << "  Type:     " << fanTypeToString(f.userType) << "\n";
        }
    }
    std::cout << std::endl;
}

static bool parseDouble(const std::string& str, double& out) {
    if (str.empty()) return false;
    try {
        size_t idx = 0;
        out = std::stod(str, &idx);
        return idx == str.size();
    } catch (...) {
        return false;
    }
}

static bool parseInt(const std::string& str, int& out) {
    if (str.empty()) return false;
    try {
        size_t idx = 0;
        out = std::stoi(str, &idx);
        return idx == str.size();
    } catch (...) {
        return false;
    }
}

int CliApp::run(int argc, char** argv) {
    std::string progName = "kaizenctl";
    if (argc > 0 && argv[0]) {
        progName = basename(argv[0]);
    }

    // Check if called as kaizentui
    if (progName == "kaizentui") {
        TuiApp tui;
        return tui.run();
    }

    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--color") {
            m_useColor = true;
        } else if (a == "--nocolor") {
            m_useColor = false;
        } else {
            args.push_back(a);
        }
    }

    if (args.empty()) {
        printHelp(progName.c_str());
        return 0;
    }

    std::string cmd = args[0];

    // --tui
    if (cmd == "--tui") {
        TuiApp tui;
        return tui.run();
    }

    // -d / --daemon redirect
    if (cmd == "-d" || cmd == "--daemon") {
        std::cerr << yellow("Notice: 'kaizend' is the dedicated daemon binary.") << "\n"
                  << "Please run " << bold("kaizend") << " or enable " << bold("systemctl enable --now kaizend.service") << ".\n";
        return 1;
    }

    // -h / --help
    if (cmd == "-h" || cmd == "--help" || cmd == "help") {
        printHelp(progName.c_str());
        return 0;
    }

    // AUTO
    if (cmd == "auto") {
        std::string target = (args.size() > 1) ? args[1] : "all";
        std::string msg;
        bool ok = m_client->setAuto(target, msg);
        if (ok) {
            std::cout << green("✔ " + msg) << std::endl;
            return 0;
        } else {
            std::cerr << red("✘ " + msg) << std::endl;
            return 1;
        }
    }

    // GET
    if (cmd == "get") {
        std::string sub = (args.size() > 1) ? args[1] : "summary";
        if (sub == "summary" || sub == "all") {
            std::string json;
            m_client->getSummary(json);
            printSummary(json);
        } else if (sub == "cpu") {
            std::string json;
            m_client->getCpuInfo(json);
            printCpu(json);
        } else if (sub == "gpu") {
            std::string json;
            m_client->getGpuInfo(json);
            printGpu(json);
        } else if (sub == "fan" || sub == "fans") {
            std::string json;
            m_client->getFans(json);
            printFans(json);
        } else {
            std::cerr << red("Unknown get target: ") << sub << "\n";
            return 1;
        }
        return 0;
    }

    // SET
    if (cmd == "set") {
        if (args.size() < 3) {
            std::cerr << red("Missing set arguments. See --help for syntax.") << std::endl;
            return 1;
        }
        std::string category = args[1];
        std::string param = args[2];
        std::string val = (args.size() > 3) ? args[3] : "";

        std::string outMsg;
        bool ok = false;

        if (category == "cpu") {
            if (param == "governor") {
                if (val.empty()) {
                    std::cerr << red("Missing governor value.") << std::endl;
                    return 1;
                }
                ok = m_client->setCpuGovernor(val, outMsg);
            } else if (param == "epp") {
                if (val.empty()) {
                    std::cerr << red("Missing EPP preference value.") << std::endl;
                    return 1;
                }
                ok = m_client->setCpuEnergyPreference(val, outMsg);
            } else if (param == "turbo") {
                if (val.empty()) {
                    std::cerr << red("Missing turbo value (on/off).") << std::endl;
                    return 1;
                }
                bool enable = (val == "on" || val == "1" || val == "true" || val == "enable");
                ok = m_client->setCpuTurbo(enable, outMsg);
            } else if (param == "min") {
                double minG = 0.0;
                if (!parseDouble(val, minG) || minG <= 0.0) {
                    std::cerr << red("Invalid or missing numeric value for min GHz: ") << val << std::endl;
                    return 1;
                }
                ok = m_client->setCpuLimits(minG, 4.5, outMsg);
            } else if (param == "max") {
                double maxG = 0.0;
                if (!parseDouble(val, maxG) || maxG <= 0.0) {
                    std::cerr << red("Invalid or missing numeric value for max GHz: ") << val << std::endl;
                    return 1;
                }
                ok = m_client->setCpuLimits(0.8, maxG, outMsg);
            } else {
                std::cerr << red("Unknown CPU parameter: ") << param << std::endl;
                return 1;
            }
        } else if (category == "gpu") {
            if (param == "level") {
                if (val.empty()) {
                    std::cerr << red("Missing GPU performance level.") << std::endl;
                    return 1;
                }
                ok = m_client->setGpuPerfLevel(val, outMsg);
            } else if (param == "sclk") {
                int clk = 0;
                if (!parseInt(val, clk)) {
                    std::cerr << red("Invalid or missing numeric value for sclk index/MHz: ") << val << std::endl;
                    return 1;
                }
                ok = m_client->setGpuClock(clk, -1, outMsg);
            } else if (param == "mclk") {
                int clk = 0;
                if (!parseInt(val, clk)) {
                    std::cerr << red("Invalid or missing numeric value for mclk index/MHz: ") << val << std::endl;
                    return 1;
                }
                ok = m_client->setGpuClock(-1, clk, outMsg);
            } else if (param == "profile") {
                int prof = 0;
                if (!parseInt(val, prof)) {
                    std::cerr << red("Invalid or missing numeric value for GPU profile: ") << val << std::endl;
                    return 1;
                }
                ok = m_client->setGpuPowerProfile(prof, outMsg);
            } else {
                std::cerr << red("Unknown GPU parameter: ") << param << std::endl;
                return 1;
            }
        } else if (category == "fan") {
            std::string fanId = param;
            std::string fanAction = (args.size() > 3) ? args[3] : "";
            std::string fanVal = (args.size() > 4) ? args[4] : "";

            if (fanAction == "speed") {
                int spd = 0;
                if (!parseInt(fanVal, spd) || spd < 0 || spd > 100) {
                    std::cerr << red("Invalid or missing fan speed percentage (0-100): ") << fanVal << std::endl;
                    return 1;
                }
                ok = m_client->setFanSpeed(fanId, spd, outMsg);
            } else if (fanAction == "mode") {
                if (fanVal.empty()) {
                    std::cerr << red("Missing fan mode (auto/manual/curve).") << std::endl;
                    return 1;
                }
                ok = m_client->setFanMode(fanId, fanVal, outMsg);
            } else if (fanAction == "curve") {
                if (fanVal.empty()) {
                    std::cerr << red("Missing curve preset (quiet/balanced/aggressive).") << std::endl;
                    return 1;
                }
                std::vector<CurvePoint> pts;
                if (fanVal == "quiet") pts = {{25, 20}, {50, 35}, {70, 60}, {85, 100}};
                else if (fanVal == "aggressive") pts = {{25, 40}, {45, 65}, {65, 90}, {80, 100}};
                else pts = {{30, 25}, {50, 45}, {70, 75}, {85, 100}};
                ok = m_client->setFanCurve(fanId, JsonUtils::curvePointsToJson(pts), outMsg);
            } else {
                std::cerr << red("Unknown fan action: ") << fanAction << std::endl;
                return 1;
            }
        } else if (category == "profile") {
            ok = m_client->applyProfile(param, outMsg);
        } else {
            std::cerr << red("Unknown set category: ") << category << std::endl;
            return 1;
        }

        if (ok) {
            std::cout << green("✔ " + outMsg) << std::endl;
            return 0;
        } else {
            std::cerr << red("✘ " + outMsg) << std::endl;
            return 1;
        }
    }

    std::cerr << red("Unknown command: ") << cmd << "\nUse --help for usage.\n";
    return 1;
}

} // namespace fc
