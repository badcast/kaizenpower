#include "DBusServer.hpp"
#include "DBusCommon.hpp"
#include "../core/JsonUtils.hpp"
#include "../core/SystemInfo.hpp"
#include <glib-unix.h>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <unistd.h>

namespace fc {

DBusServer::DBusServer(std::shared_ptr<CpuManager> cpuMgr,
                       std::shared_ptr<GpuManager> gpuMgr,
                       std::shared_ptr<FanManager> fanMgr,
                       std::shared_ptr<ProfileManager> profileMgr)
    : m_cpuMgr(std::move(cpuMgr)),
      m_gpuMgr(std::move(gpuMgr)),
      m_fanMgr(std::move(fanMgr)),
      m_profileMgr(std::move(profileMgr)) {
    m_introspectionData = g_dbus_node_info_new_for_xml(KAIZEN_INTROSPECTION_XML, nullptr);
}

DBusServer::~DBusServer() {
    stop();
    if (m_introspectionData) {
        g_dbus_node_info_unref(m_introspectionData);
    }
}

bool DBusServer::init() {
    GBusType busType = G_BUS_TYPE_SYSTEM;

    if (geteuid() == 0) {
        busType = G_BUS_TYPE_SYSTEM;
        // If system policy file is missing, automatically register it in /etc/dbus-1/system.d/
        const std::string etcPolicy = "/etc/dbus-1/system.d/org.kaizen.PowerTool.conf";
        const std::string usrPolicy = "/usr/share/dbus-1/system.d/org.kaizen.PowerTool.conf";
        if (!std::filesystem::exists(etcPolicy) && !std::filesystem::exists(usrPolicy)) {
            std::cout << "Kaizen D-Bus: Registering system policy in " << etcPolicy << std::endl;
            std::ofstream ofs(etcPolicy);
            if (ofs.is_open()) {
                ofs << KAIZEN_DBUS_POLICY_XML;
                ofs.close();
                // Reload D-Bus system bus configuration
                int rc = system("dbus-send --system --type=method_call --dest=org.freedesktop.DBus /org/freedesktop/DBus org.freedesktop.DBus.ReloadConfig 2>/dev/null");
                (void)rc;
            }
        }
        std::cout << "Kaizen D-Bus: Operating on System Bus (Root daemon mode)" << std::endl;
    } else {
        busType = G_BUS_TYPE_SESSION;
        std::cout << "Kaizen D-Bus: Operating on Session Bus (testing / unprivileged mode)" << std::endl;
    }

    m_ownerId = g_bus_own_name(busType,
                               KAIZEN_DBUS_NAME,
                               G_BUS_NAME_OWNER_FLAGS_REPLACE,
                               onBusAcquired,
                               onNameAcquired,
                               onNameLost,
                               this,
                               nullptr);

    // Initial hardware scan
    if (m_fanMgr) m_fanMgr->scanFans();
    if (m_cpuMgr) m_cpuMgr->scanCpu();
    if (m_gpuMgr) m_gpuMgr->scanGpu();

    // 1-second live telemetry timer
    m_timerId = g_timeout_add(1000, onTimerTick, this);
    return true;
}

static gboolean onServerUnixSignal(gpointer userData) {
    DBusServer* self = static_cast<DBusServer*>(userData);
    if (self) {
        self->stop();
    }
    return G_SOURCE_CONTINUE;
}

void DBusServer::run() {
    m_loop = g_main_loop_new(nullptr, FALSE);
    guint sigInt = g_unix_signal_add(SIGINT, onServerUnixSignal, this);
    guint sigTerm = g_unix_signal_add(SIGTERM, onServerUnixSignal, this);
    g_main_loop_run(m_loop);
    if (sigInt > 0) g_source_remove(sigInt);
    if (sigTerm > 0) g_source_remove(sigTerm);
}

void DBusServer::stop() {
    if (m_timerId > 0) {
        g_source_remove(m_timerId);
        m_timerId = 0;
    }
    if (m_ownerId > 0) {
        g_bus_unown_name(m_ownerId);
        m_ownerId = 0;
    }
    if (m_loop) {
        g_main_loop_quit(m_loop);
        g_main_loop_unref(m_loop);
        m_loop = nullptr;
    }
}

SystemSummary DBusServer::buildSummary() {
    SystemSummary s;
    s.os = SystemInfo::getOsInfo();

    if (m_cpuMgr) {
        m_cpuMgr->updateLiveMetrics();
        const auto& cpu = m_cpuMgr->getCpuInfo();
        s.cpuModel = cpu.modelName;
        s.cpuCores = cpu.coreCount;
        s.cpuAvgFreqGhz = cpu.avgFreqGhz;
        s.cpuTempC = cpu.temperatureC;
        s.cpuGovernor = cpu.currentGovernor;
    }

    if (m_gpuMgr) {
        m_gpuMgr->updateLiveMetrics();
        const auto& gpu = m_gpuMgr->getGpuInfo();
        s.gpuPresent = gpu.present;
        s.gpuName = gpu.name;
        s.gpuDriver = gpu.driver;
        s.gpuTempC = gpu.temperatureC;
        s.gpuPowerW = gpu.powerW;
    }

    if (m_fanMgr) {
        m_fanMgr->updateLiveMetrics(s.cpuTempC, s.gpuTempC, true);
        const auto& fans = m_fanMgr->getFans();
        s.fansTotal = static_cast<int>(fans.size());
        s.fansActive = 0;
        int sumRpm = 0;
        for (const auto& f : fans) {
            if (f.rpm > 0) {
                s.fansActive++;
                sumRpm += f.rpm;
            }
        }
        s.fansAvgRpm = (s.fansActive > 0) ? (sumRpm / s.fansActive) : 0;
    }

    if (m_profileMgr) {
        s.activeProfile = m_profileMgr->getCurrentProfile();
        s.activeProfileName = m_profileMgr->getProfileName(s.activeProfile);
    }
    s.daemonRunning = true;
    return s;
}

gboolean DBusServer::onTimerTick(gpointer userData) {
    DBusServer* self = static_cast<DBusServer*>(userData);
    SystemSummary s = self->buildSummary();

    if (self->m_connection) {
        std::string json = JsonUtils::summaryToJson(s);
        g_dbus_connection_emit_signal(self->m_connection,
                                      nullptr,
                                      KAIZEN_DBUS_PATH,
                                      KAIZEN_DBUS_INTERFACE,
                                      "MetricsUpdated",
                                      g_variant_new("(s)", json.c_str()),
                                      nullptr);
    }
    return G_SOURCE_CONTINUE;
}

void DBusServer::onBusAcquired(GDBusConnection* connection, const gchar*, gpointer userData) {
    DBusServer* self = static_cast<DBusServer*>(userData);
    self->m_connection = connection;

    static const GDBusInterfaceVTable vtable = {
        handleMethodCall,
        nullptr,
        nullptr,
        { nullptr }
    };

    GError* error = nullptr;
    g_dbus_connection_register_object(connection,
                                      KAIZEN_DBUS_PATH,
                                      self->m_introspectionData->interfaces[0],
                                      &vtable,
                                      self,
                                      nullptr,
                                      &error);
    if (error) {
        std::cerr << "Kaizen D-Bus: Failed to register object: " << error->message << std::endl;
        g_error_free(error);
    }
}

void DBusServer::onNameAcquired(GDBusConnection*, const gchar* name, gpointer) {
    std::cout << "Kaizen Power Tool Daemon acquired D-Bus name: " << name << std::endl;
}

void DBusServer::onNameLost(GDBusConnection*, const gchar* name, gpointer) {
    std::cerr << "Kaizen Power Tool Daemon lost D-Bus name: " << name << std::endl;
}

void DBusServer::handleMethodCall(GDBusConnection*,
                                 const gchar*,
                                 const gchar*,
                                 const gchar*,
                                 const gchar* methodName,
                                 GVariant* parameters,
                                 GDBusMethodInvocation* invocation,
                                 gpointer userData) {
    DBusServer* self = static_cast<DBusServer*>(userData);
    std::string m(methodName);

    if (m == "GetSummary") {
        SystemSummary s = self->buildSummary();
        std::string json = JsonUtils::summaryToJson(s);
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(s)", json.c_str()));
    } else if (m == "GetCpuInfo") {
        if (self->m_cpuMgr) self->m_cpuMgr->updateLiveMetrics();
        std::string json = JsonUtils::cpuInfoToJson(self->m_cpuMgr ? self->m_cpuMgr->getCpuInfo() : CpuInfo{});
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(s)", json.c_str()));
    } else if (m == "SetCpuGovernor") {
        const gchar* gov = nullptr;
        g_variant_get(parameters, "(&s)", &gov);
        std::cout << "[KAIZEND] D-Bus request: SetCpuGovernor('" << (gov ? gov : "schedutil") << "')" << std::endl;
        bool ok = self->m_cpuMgr && self->m_cpuMgr->setGovernor(gov ? gov : "schedutil");
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(bs)", ok, ok ? "CPU Governor applied" : "Failed to set CPU governor"));
    } else if (m == "SetCpuLimits") {
        double minG = 0.0, maxG = 0.0;
        g_variant_get(parameters, "(dd)", &minG, &maxG);
        std::cout << "[KAIZEND] D-Bus request: SetCpuLimits(" << minG << " GHz, " << maxG << " GHz)" << std::endl;
        bool ok = self->m_cpuMgr && self->m_cpuMgr->setFreqLimits(minG, maxG);
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(bs)", ok, ok ? "CPU Limits applied" : "Failed to set CPU frequency limits"));
    } else if (m == "SetCpuTurbo") {
        gboolean turbo = TRUE;
        g_variant_get(parameters, "(b)", &turbo);
        std::cout << "[KAIZEND] D-Bus request: SetCpuTurbo(" << (turbo ? "ON" : "OFF") << ")" << std::endl;
        bool ok = self->m_cpuMgr && self->m_cpuMgr->setTurboBoost(turbo);
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(bs)", ok, ok ? "Turbo Boost updated" : "Failed to update Turbo Boost"));
    } else if (m == "SetCpuEnergyPreference") {
        const gchar* pref = nullptr;
        g_variant_get(parameters, "(&s)", &pref);
        std::cout << "[KAIZEND] D-Bus request: SetCpuEnergyPreference('" << (pref ? pref : "balance_performance") << "')" << std::endl;
        bool ok = self->m_cpuMgr && self->m_cpuMgr->setEnergyPreference(pref ? pref : "balance_performance");
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(bs)", ok, ok ? "EPP applied" : "Failed to set EPP"));
    } else if (m == "GetGpuInfo") {
        if (self->m_gpuMgr) self->m_gpuMgr->updateLiveMetrics();
        std::string json = JsonUtils::gpuInfoToJson(self->m_gpuMgr ? self->m_gpuMgr->getGpuInfo() : GpuInfo{});
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(s)", json.c_str()));
    } else if (m == "SetGpuPerfLevel") {
        const gchar* lvl = nullptr;
        g_variant_get(parameters, "(&s)", &lvl);
        std::cout << "[KAIZEND] D-Bus request: SetGpuPerfLevel('" << (lvl ? lvl : "auto") << "')" << std::endl;
        bool ok = self->m_gpuMgr && self->m_gpuMgr->setPerformanceLevel(lvl ? lvl : "auto");
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(bs)", ok, ok ? "GPU Performance Level set" : "Failed to set GPU performance level"));
    } else if (m == "SetGpuClock") {
        gint sclk = -1, mclk = -1;
        g_variant_get(parameters, "(ii)", &sclk, &mclk);
        std::cout << "[KAIZEND] D-Bus request: SetGpuClock(sclk=" << sclk << ", mclk=" << mclk << ")" << std::endl;
        bool ok = self->m_gpuMgr != nullptr;
        if (ok && sclk >= 0) ok = ok && self->m_gpuMgr->setSclkState(sclk);
        if (ok && mclk >= 0) ok = ok && self->m_gpuMgr->setMclkState(mclk);
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(bs)", ok, ok ? "GPU Clocks applied" : "Failed to set GPU clocks"));
    } else if (m == "SetGpuPowerProfile") {
        gint prof = -1;
        g_variant_get(parameters, "(i)", &prof);
        std::cout << "[KAIZEND] D-Bus request: SetGpuPowerProfile(" << prof << ")" << std::endl;
        bool ok = self->m_gpuMgr && self->m_gpuMgr->setPowerProfileMode(prof);
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(bs)", ok, ok ? "GPU Power Profile applied" : "Failed to set power profile"));
    } else if (m == "GetFans") {
        if (self->m_fanMgr) self->m_fanMgr->updateLiveMetrics();
        std::vector<FanInfo> fans = self->m_fanMgr ? self->m_fanMgr->getFans() : std::vector<FanInfo>{};
        std::string json = JsonUtils::fansToJson(fans);
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(s)", json.c_str()));
    } else if (m == "SetFanSpeed") {
        const gchar* fanId = nullptr;
        gint pct = 50;
        g_variant_get(parameters, "(&si)", &fanId, &pct);
        std::cout << "[KAIZEND] D-Bus request: SetFanSpeed('" << (fanId ? fanId : "") << "', " << pct << "%)" << std::endl;
        bool ok = self->m_fanMgr && self->m_fanMgr->setFanSpeedPercent(fanId ? fanId : "", pct);
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(bs)", ok, ok ? "Fan speed set" : "Failed to set fan speed"));
    } else if (m == "SetFanMode") {
        const gchar* fanId = nullptr;
        const gchar* modeStr = nullptr;
        g_variant_get(parameters, "(&s&s)", &fanId, &modeStr);
        std::cout << "[KAIZEND] D-Bus request: SetFanMode('" << (fanId ? fanId : "") << "', '" << (modeStr ? modeStr : "") << "')" << std::endl;
        FanControlMode mode = stringToFanControlMode(modeStr ? modeStr : "auto");
        bool ok = self->m_fanMgr && self->m_fanMgr->setFanControlMode(fanId ? fanId : "", mode);
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(bs)", ok, ok ? "Fan mode updated" : "Failed to update fan mode"));
    } else if (m == "SetFanCurve") {
        const gchar* fanId = nullptr;
        const gchar* curveJson = nullptr;
        g_variant_get(parameters, "(&s&s)", &fanId, &curveJson);
        std::cout << "[KAIZEND] D-Bus request: SetFanCurve('" << (fanId ? fanId : "") << "')" << std::endl;
        std::vector<CurvePoint> pts = JsonUtils::curvePointsFromJson(curveJson ? curveJson : "");
        bool ok = self->m_fanMgr && self->m_fanMgr->setFanCurvePoints(fanId ? fanId : "", pts);
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(bs)", ok, ok ? "Fan curve saved" : "Failed to save fan curve"));
    } else if (m == "SetAuto") {
        const gchar* comp = nullptr;
        g_variant_get(parameters, "(&s)", &comp);
        std::string c = comp ? comp : "all";
        std::cout << "[KAIZEND] D-Bus request: SetAuto('" << c << "')" << std::endl;
        bool ok = true;
        if (c == "all") {
            if (self->m_profileMgr) {
                self->m_profileMgr->restoreAuto();
            } else {
                if (self->m_cpuMgr) {
                    self->m_cpuMgr->setGovernor("schedutil");
                    self->m_cpuMgr->setTurboBoost(true);
                    self->m_cpuMgr->setEnergyPreference("balance_performance");
                }
                if (self->m_gpuMgr) {
                    self->m_gpuMgr->setPerformanceLevel("auto");
                }
                if (self->m_fanMgr) {
                    self->m_fanMgr->setAllFansAuto();
                }
            }
        } else if (c == "cpu") {
            if (self->m_cpuMgr) {
                self->m_cpuMgr->setGovernor("schedutil");
                self->m_cpuMgr->setTurboBoost(true);
                self->m_cpuMgr->setEnergyPreference("balance_performance");
            }
        } else if (c == "gpu") {
            if (self->m_gpuMgr) self->m_gpuMgr->setPerformanceLevel("auto");
        } else if (c == "fan" || c == "fans") {
            if (self->m_fanMgr) self->m_fanMgr->setAllFansAuto();
        } else {
            // Specific fan ID
            if (self->m_fanMgr) ok = self->m_fanMgr->setFanControlMode(c, FanControlMode::Auto);
        }
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(bs)", ok, ok ? "Hardware set to automatic factory mode" : "Failed to set auto"));
    } else if (m == "ApplyProfile") {
        const gchar* prof = nullptr;
        g_variant_get(parameters, "(&s)", &prof);
        std::string p = prof ? prof : "balanced";
        std::cout << "[KAIZEND] D-Bus request: ApplyProfile('" << p << "')" << std::endl;
        if (p == "auto" || p == "factory") {
            bool ok = self->m_profileMgr && self->m_profileMgr->restoreAuto();
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(bs)", ok, ok ? "Hardware set to automatic factory mode" : "Failed to set auto"));
            return;
        }

        SystemProfile sp = SystemProfile::Balanced;
        if (p == "silent" || p == "quiet" || p == "0") sp = SystemProfile::Silent;
        else if (p == "balanced" || p == "1") sp = SystemProfile::Balanced;
        else if (p == "performance" || p == "2") sp = SystemProfile::Performance;
        else if (p == "cooling" || p == "max_cooling" || p == "max" || p == "3") sp = SystemProfile::MaxCooling;

        bool ok = self->m_profileMgr && self->m_profileMgr->applyProfile(sp);
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(bs)", ok, ok ? "System Profile applied" : "Failed to apply profile"));
    } else {
        g_dbus_method_invocation_return_error(invocation,
                                              G_DBUS_ERROR,
                                              G_DBUS_ERROR_UNKNOWN_METHOD,
                                              "Unknown method %s",
                                              methodName);
    }
}

} // namespace fc
