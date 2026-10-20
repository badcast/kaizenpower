#pragma once

#include "../core/CpuManager.hpp"
#include "../core/GpuManager.hpp"
#include "../core/FanManager.hpp"
#include "../core/ProfileManager.hpp"
#include <gio/gio.h>
#include <memory>
#include <string>

namespace fc {

class DBusServer {
public:
    DBusServer(std::shared_ptr<CpuManager> cpuMgr,
               std::shared_ptr<GpuManager> gpuMgr,
               std::shared_ptr<FanManager> fanMgr,
               std::shared_ptr<ProfileManager> profileMgr);
    ~DBusServer();

    bool init();
    void run();
    void stop();

    SystemSummary buildSummary();

private:
    std::shared_ptr<CpuManager> m_cpuMgr;
    std::shared_ptr<GpuManager> m_gpuMgr;
    std::shared_ptr<FanManager> m_fanMgr;
    std::shared_ptr<ProfileManager> m_profileMgr;

    GMainLoop* m_loop = nullptr;
    GDBusNodeInfo* m_introspectionData = nullptr;
    guint m_ownerId = 0;
    guint m_timerId = 0;
    GDBusConnection* m_connection = nullptr;

    static void onBusAcquired(GDBusConnection* connection, const gchar* name, gpointer userData);
    static void onNameAcquired(GDBusConnection* connection, const gchar* name, gpointer userData);
    static void onNameLost(GDBusConnection* connection, const gchar* name, gpointer userData);

    static void handleMethodCall(GDBusConnection* connection,
                                 const gchar* sender,
                                 const gchar* objectPath,
                                 const gchar* interfaceName,
                                 const gchar* methodName,
                                 GVariant* parameters,
                                 GDBusMethodInvocation* invocation,
                                 gpointer userData);

    static gboolean onTimerTick(gpointer userData);
};

} // namespace fc
