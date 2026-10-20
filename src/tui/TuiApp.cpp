#include "TuiApp.hpp"
#include "../core/SystemInfo.hpp"
#include <curses.h>
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace fc {

TuiApp::TuiApp() {
    m_dbusClient = std::make_shared<DBusClient>();
    m_useDirect = !m_dbusClient->connect();

    m_cpuMgr = std::make_shared<CpuManager>();
    m_gpuMgr = std::make_shared<GpuManager>();
    m_fanMgr = std::make_shared<FanManager>();
    m_profileMgr = std::make_shared<ProfileManager>(m_fanMgr, m_cpuMgr, m_gpuMgr);

    m_cpuMgr->scanCpu();
    m_gpuMgr->scanGpu();
    m_fanMgr->scanFans();
}

TuiApp::~TuiApp() {
    if (!isendwin()) {
        endwin();
    }
}

void TuiApp::initColors() {
    if (has_colors()) {
        start_color();
        use_default_colors();
        init_pair(1, COLOR_GREEN, -1);
        init_pair(2, COLOR_CYAN, -1);
        init_pair(3, COLOR_YELLOW, -1);
        init_pair(4, COLOR_RED, -1);
        init_pair(5, COLOR_WHITE, COLOR_BLUE);
        init_pair(6, COLOR_MAGENTA, -1);
        init_pair(7, COLOR_BLACK, COLOR_CYAN);
    }
}

void TuiApp::drawProgressBar(int y, int x, int width, double fraction, int colorPair) {
    fraction = std::clamp(fraction, 0.0, 1.0);
    int filled = static_cast<int>(fraction * width);
    mvaddch(y, x, '[');
    wattron(stdscr, COLOR_PAIR(colorPair) | A_BOLD);
    for (int i = 0; i < filled; ++i) {
        addch('|');
    }
    wattroff(stdscr, COLOR_PAIR(colorPair) | A_BOLD);
    for (int i = filled; i < width; ++i) {
        addch('.');
    }
    addch(']');
}

void TuiApp::renderDashboard() {
    erase();
    int maxRows = LINES;
    int maxCols = COLS;

    if (maxRows < 12 || maxCols < 50) {
        mvprintw(0, 0, "Terminal too small (%dx%d). Minimum: 50x12", maxCols, maxRows);
        refresh();
        return;
    }

    // 1. Header Banner
    attron(COLOR_PAIR(5) | A_BOLD);
    mvhline(0, 0, ' ', maxCols);
    mvprintw(0, 2, "KAIZEN POWER TOOL — Hardware & Fan Control Dashboard");
    mvprintw(0, maxCols - 24, "[Mode: %s]", m_useDirect ? "Standalone" : "D-Bus Daemon");
    attroff(COLOR_PAIR(5) | A_BOLD);

    // 2. OS & System Summary Line
    OsInfo os = SystemInfo::getOsInfo();
    attron(COLOR_PAIR(2) | A_BOLD);
    mvprintw(1, 2, "OS: %s (%s)  |  Kernel: %s  |  Arch: %s  |  Uptime: %s",
             os.prettyName.c_str(), os.hostname.c_str(), os.kernel.c_str(),
             os.architecture.c_str(), os.uptimeStr.c_str());
    attroff(COLOR_PAIR(2) | A_BOLD);
    mvhline(2, 0, ACS_HLINE, maxCols);

    // Refresh metrics
    double cpuTemp = 0.0, gpuTemp = 0.0;
    std::string cpuModel = "CPU", cpuGov = "schedutil", gpuName = "GPU", gpuDriver = "N/A";
    double cpuAvgFreq = 0.0, gpuPower = 0.0;
    int cpuCores = 0, sclk = 0, mclk = 0;
    bool turbo = false;

    if (m_cpuMgr) {
        m_cpuMgr->updateLiveMetrics();
        const auto& c = m_cpuMgr->getCpuInfo();
        cpuModel = c.modelName;
        cpuTemp = c.temperatureC;
        cpuGov = c.currentGovernor;
        cpuAvgFreq = c.avgFreqGhz;
        cpuCores = c.coreCount;
        turbo = c.turboBoost;
    }
    if (m_gpuMgr) {
        m_gpuMgr->updateLiveMetrics();
        const auto& g = m_gpuMgr->getGpuInfo();
        gpuName = g.name.empty() ? "Integrated/Generic GPU" : g.name;
        gpuDriver = g.driver;
        gpuTemp = g.temperatureC;
        gpuPower = g.powerW;
        sclk = g.curSclkMhz;
        mclk = g.curMclkMhz;
    }
    if (m_fanMgr) {
        m_fanMgr->updateLiveMetrics(cpuTemp, gpuTemp);
    }

    // 3. CPU Box (Left Column)
    int colWidth = (maxCols - 6) / 2;
    int cpuX = 2;
    int gpuX = cpuX + colWidth + 2;

    attron(COLOR_PAIR(2) | A_BOLD);
    mvprintw(3, cpuX, "┌─ CPU Telemetry & Power ─────────────────────────┐");
    attroff(COLOR_PAIR(2) | A_BOLD);

    mvprintw(4, cpuX + 2, "Model:    %.38s", cpuModel.c_str());
    mvprintw(5, cpuX + 2, "Cores:    %d Threads | Avg Freq: %.2f GHz", cpuCores, cpuAvgFreq);
    mvprintw(6, cpuX + 2, "Governor: %-12s | Turbo: %s", cpuGov.c_str(), turbo ? "ON" : "OFF");

    int tempColor = (cpuTemp < 60.0) ? 1 : ((cpuTemp < 80.0) ? 3 : 4);
    mvprintw(7, cpuX + 2, "Temp:     ");
    drawProgressBar(7, cpuX + 12, 16, cpuTemp / 100.0, tempColor);
    wattron(stdscr, COLOR_PAIR(tempColor) | A_BOLD);
    printw(" %.1f°C", cpuTemp);
    wattroff(stdscr, COLOR_PAIR(tempColor) | A_BOLD);

    // 4. GPU Box (Right Column)
    attron(COLOR_PAIR(2) | A_BOLD);
    mvprintw(3, gpuX, "┌─ GPU Telemetry & Power ─────────────────────────┐");
    attroff(COLOR_PAIR(2) | A_BOLD);

    mvprintw(4, gpuX + 2, "Model:    %.38s", gpuName.c_str());
    mvprintw(5, gpuX + 2, "Driver:   %-12s | Power: %.1f W", gpuDriver.c_str(), gpuPower);
    mvprintw(6, gpuX + 2, "Clocks:   Core: %d MHz | VRAM: %d MHz", sclk, mclk);

    int gpuTempColor = (gpuTemp < 60.0) ? 1 : ((gpuTemp < 80.0) ? 3 : 4);
    mvprintw(7, gpuX + 2, "Temp:     ");
    drawProgressBar(7, gpuX + 12, 16, gpuTemp / 100.0, gpuTempColor);
    wattron(stdscr, COLOR_PAIR(gpuTempColor) | A_BOLD);
    printw(" %.1f°C", gpuTemp);
    wattroff(stdscr, COLOR_PAIR(gpuTempColor) | A_BOLD);

    // 5. Fans Table (Bottom Box)
    mvhline(9, 0, ACS_HLINE, maxCols);
    attron(COLOR_PAIR(2) | A_BOLD);
    mvprintw(10, 2, "COOLING FANS (Speed & Curves)");
    attroff(COLOR_PAIR(2) | A_BOLD);

    attron(A_UNDERLINE | A_BOLD);
    mvprintw(11, 2, "%-16s %-18s %-10s %-10s %-8s %-10s",
             "DEVICE ID", "LABEL", "TYPE", "RPM", "PWM %", "MODE");
    attroff(A_UNDERLINE | A_BOLD);

    int fanRow = 12;
    if (m_fanMgr) {
        const auto& fans = m_fanMgr->getFans();
        for (const auto& fan : fans) {
            if (fanRow >= maxRows - 4) break;
            std::string typeStr = (fan.userType == FanType::CPU) ? "CPU" :
                                  ((fan.userType == FanType::GPU) ? "GPU" : "Chassis");
            std::string modeStr = fanControlModeToString(fan.controlMode);

            int rpmColor = (fan.rpm > 0) ? 1 : 3;
            mvprintw(fanRow, 2, "%-16.16s %-18.18s %-10s ",
                     fan.id.c_str(), fan.label.empty() ? fan.hwmonName.c_str() : fan.label.c_str(), typeStr.c_str());

            wattron(stdscr, COLOR_PAIR(rpmColor) | A_BOLD);
            printw("%-10d ", fan.rpm);
            wattroff(stdscr, COLOR_PAIR(rpmColor) | A_BOLD);

            printw("%-8s %-10s",
                   fan.hasPwm ? (std::to_string(fan.currentPwmPercent) + "%").c_str() : "N/A",
                   modeStr.c_str());
            fanRow++;
        }
    }

    // 6. Status and Keybindings Bar
    int bottomRow = maxRows - 2;
    mvhline(bottomRow - 1, 0, ACS_HLINE, maxCols);

    attron(COLOR_PAIR(6) | A_BOLD);
    mvprintw(bottomRow, 2, "Status: %s", m_statusMessage.c_str());
    attroff(COLOR_PAIR(6) | A_BOLD);

    attron(COLOR_PAIR(7) | A_BOLD);
    mvhline(bottomRow + 1, 0, ' ', maxCols);
    mvprintw(bottomRow + 1, 2, "[A] Auto All   [S] Silent   [B] Balanced   [P] Performance   [M] MaxCool   [R] Refresh   [Q] Quit");
    attroff(COLOR_PAIR(7) | A_BOLD);

    refresh();
}

int TuiApp::run() {
    initscr();
    raw();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    timeout(1000); // 1-second non-blocking tick for live refresh
    initColors();

    bool running = true;
    while (running) {
        renderDashboard();
        int ch = getch();
        if (ch == ERR) continue;

        switch (ch) {
            case 'q':
            case 'Q':
            case 27: // ESC
                running = false;
                break;
            case 'a':
            case 'A': {
                std::string msg;
                if (!m_useDirect) {
                    m_dbusClient->setAuto("all", msg);
                } else {
                    if (m_cpuMgr) {
                        m_cpuMgr->setGovernor("schedutil");
                        m_cpuMgr->setTurboBoost(true);
                        m_cpuMgr->setEnergyPreference("balance_performance");
                    }
                    if (m_gpuMgr) m_gpuMgr->setPerformanceLevel("auto");
                    if (m_fanMgr) m_fanMgr->setAllFansAuto();
                    msg = "Hardware restored to factory automatic mode";
                }
                m_statusMessage = msg;
                break;
            }
            case 's':
            case 'S': {
                std::string msg;
                if (!m_useDirect) m_dbusClient->applyProfile("silent", msg);
                else {
                    m_profileMgr->applyProfile(SystemProfile::Silent);
                    msg = "Profile applied: Silent";
                }
                m_statusMessage = msg;
                break;
            }
            case 'b':
            case 'B': {
                std::string msg;
                if (!m_useDirect) m_dbusClient->applyProfile("balanced", msg);
                else {
                    m_profileMgr->applyProfile(SystemProfile::Balanced);
                    msg = "Profile applied: Balanced";
                }
                m_statusMessage = msg;
                break;
            }
            case 'p':
            case 'P': {
                std::string msg;
                if (!m_useDirect) m_dbusClient->applyProfile("performance", msg);
                else {
                    m_profileMgr->applyProfile(SystemProfile::Performance);
                    msg = "Profile applied: Performance";
                }
                m_statusMessage = msg;
                break;
            }
            case 'm':
            case 'M': {
                std::string msg;
                if (!m_useDirect) m_dbusClient->applyProfile("cooling", msg);
                else {
                    m_profileMgr->applyProfile(SystemProfile::MaxCooling);
                    msg = "Profile applied: Extreme Cooling";
                }
                m_statusMessage = msg;
                break;
            }
            case 'r':
            case 'R':
                m_statusMessage = "Refreshed metrics";
                break;
            default:
                break;
        }
    }

    endwin();
    return 0;
}

} // namespace fc
