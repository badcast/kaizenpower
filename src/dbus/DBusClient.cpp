#include "DBusClient.hpp"
#include "DBusCommon.hpp"
#include <iostream>

namespace fc {

DBusClient::DBusClient() = default;

DBusClient::~DBusClient() {
    if (m_proxy) {
        g_object_unref(m_proxy);
        m_proxy = nullptr;
    }
}

bool DBusClient::isConnected() const {
    if (!m_proxy) return false;
    gchar* owner = g_dbus_proxy_get_name_owner(m_proxy);
    if (owner) {
        g_free(owner);
        return true;
    }
    return false;
}

bool DBusClient::connect() {
    if (isConnected()) return true;

    if (m_proxy) {
        g_object_unref(m_proxy);
        m_proxy = nullptr;
    }

    GError* error = nullptr;

    // 1. Check System Bus first (production mode)
    m_proxy = g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM,
                                           G_DBUS_PROXY_FLAGS_NONE,
                                           nullptr,
                                           KAIZEN_DBUS_NAME,
                                           KAIZEN_DBUS_PATH,
                                           KAIZEN_DBUS_INTERFACE,
                                           nullptr,
                                           &error);
    if (m_proxy) {
        gchar* owner = g_dbus_proxy_get_name_owner(m_proxy);
        if (owner) {
            g_free(owner);
            if (error) g_error_free(error);
            return true;
        }
        g_object_unref(m_proxy);
        m_proxy = nullptr;
    }
    if (error) {
        g_error_free(error);
        error = nullptr;
    }

    // 2. Fallback to Session Bus (testing or development mode)
    m_proxy = g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SESSION,
                                           G_DBUS_PROXY_FLAGS_NONE,
                                           nullptr,
                                           KAIZEN_DBUS_NAME,
                                           KAIZEN_DBUS_PATH,
                                           KAIZEN_DBUS_INTERFACE,
                                           nullptr,
                                           &error);
    if (m_proxy) {
        gchar* owner = g_dbus_proxy_get_name_owner(m_proxy);
        if (owner) {
            g_free(owner);
            if (error) g_error_free(error);
            return true;
        }
        g_object_unref(m_proxy);
        m_proxy = nullptr;
    }
    if (error) {
        g_error_free(error);
    }

    return false;
}

bool DBusClient::getSummary(std::string& outJson) {
    if (!connect()) return false;
    GError* error = nullptr;
    GVariant* res = g_dbus_proxy_call_sync(m_proxy, "GetSummary", nullptr, G_DBUS_CALL_FLAGS_NONE, 1500, nullptr, &error);
    if (!res) {
        if (error) g_error_free(error);
        return false;
    }
    const gchar* json = nullptr;
    g_variant_get(res, "(&s)", &json);
    if (json) outJson = json;
    g_variant_unref(res);
    return true;
}

bool DBusClient::getCpuInfo(std::string& outJson) {
    if (!connect()) return false;
    GError* error = nullptr;
    GVariant* res = g_dbus_proxy_call_sync(m_proxy, "GetCpuInfo", nullptr, G_DBUS_CALL_FLAGS_NONE, 1500, nullptr, &error);
    if (!res) {
        if (error) g_error_free(error);
        return false;
    }
    const gchar* json = nullptr;
    g_variant_get(res, "(&s)", &json);
    if (json) outJson = json;
    g_variant_unref(res);
    return true;
}

bool DBusClient::getGpuInfo(std::string& outJson) {
    if (!connect()) return false;
    GError* error = nullptr;
    GVariant* res = g_dbus_proxy_call_sync(m_proxy, "GetGpuInfo", nullptr, G_DBUS_CALL_FLAGS_NONE, 1500, nullptr, &error);
    if (!res) {
        if (error) g_error_free(error);
        return false;
    }
    const gchar* json = nullptr;
    g_variant_get(res, "(&s)", &json);
    if (json) outJson = json;
    g_variant_unref(res);
    return true;
}

bool DBusClient::getFans(std::string& outJson) {
    if (!connect()) return false;
    GError* error = nullptr;
    GVariant* res = g_dbus_proxy_call_sync(m_proxy, "GetFans", nullptr, G_DBUS_CALL_FLAGS_NONE, 1500, nullptr, &error);
    if (!res) {
        if (error) g_error_free(error);
        return false;
    }
    const gchar* json = nullptr;
    g_variant_get(res, "(&s)", &json);
    if (json) outJson = json;
    g_variant_unref(res);
    return true;
}

bool DBusClient::callBoolStringMethod(const char* methodName, GVariant* parameters, std::string& outMsg) {
    if (!connect()) {
        outMsg = "Failed to connect to Kaizen D-Bus daemon. Is kaizend running?";
        return false;
    }

    GError* error = nullptr;
    GVariant* res = g_dbus_proxy_call_sync(m_proxy, methodName, parameters, G_DBUS_CALL_FLAGS_NONE, 3000, nullptr, &error);
    if (!res) {
        if (error) {
            outMsg = std::string("D-Bus Error: ") + error->message;
            g_error_free(error);
        } else {
            outMsg = "D-Bus call failed with unknown error";
        }
        return false;
    }

    gboolean ok = FALSE;
    const gchar* msg = nullptr;
    g_variant_get(res, "(b&s)", &ok, &msg);
    if (msg) outMsg = msg;
    g_variant_unref(res);
    return (ok == TRUE);
}

bool DBusClient::setCpuGovernor(const std::string& gov, std::string& outMsg) {
    return callBoolStringMethod("SetCpuGovernor", g_variant_new("(s)", gov.c_str()), outMsg);
}

bool DBusClient::setCpuLimits(double minGhz, double maxGhz, std::string& outMsg) {
    return callBoolStringMethod("SetCpuLimits", g_variant_new("(dd)", minGhz, maxGhz), outMsg);
}

bool DBusClient::setCpuTurbo(bool enable, std::string& outMsg) {
    return callBoolStringMethod("SetCpuTurbo", g_variant_new("(b)", enable ? TRUE : FALSE), outMsg);
}

bool DBusClient::setCpuEnergyPreference(const std::string& pref, std::string& outMsg) {
    return callBoolStringMethod("SetCpuEnergyPreference", g_variant_new("(s)", pref.c_str()), outMsg);
}

bool DBusClient::setGpuPerfLevel(const std::string& level, std::string& outMsg) {
    return callBoolStringMethod("SetGpuPerfLevel", g_variant_new("(s)", level.c_str()), outMsg);
}

bool DBusClient::setGpuClock(int sclkIdx, int mclkIdx, std::string& outMsg) {
    return callBoolStringMethod("SetGpuClock", g_variant_new("(ii)", sclkIdx, mclkIdx), outMsg);
}

bool DBusClient::setGpuPowerProfile(int profileIdx, std::string& outMsg) {
    return callBoolStringMethod("SetGpuPowerProfile", g_variant_new("(i)", profileIdx), outMsg);
}

bool DBusClient::setFanSpeed(const std::string& fanId, int percent, std::string& outMsg) {
    return callBoolStringMethod("SetFanSpeed", g_variant_new("(si)", fanId.c_str(), percent), outMsg);
}

bool DBusClient::setFanMode(const std::string& fanId, const std::string& mode, std::string& outMsg) {
    return callBoolStringMethod("SetFanMode", g_variant_new("(ss)", fanId.c_str(), mode.c_str()), outMsg);
}

bool DBusClient::setFanCurve(const std::string& fanId, const std::string& curveJson, std::string& outMsg) {
    return callBoolStringMethod("SetFanCurve", g_variant_new("(ss)", fanId.c_str(), curveJson.c_str()), outMsg);
}

bool DBusClient::setAuto(const std::string& component, std::string& outMsg) {
    return callBoolStringMethod("SetAuto", g_variant_new("(s)", component.c_str()), outMsg);
}

bool DBusClient::applyProfile(const std::string& profileName, std::string& outMsg) {
    return callBoolStringMethod("ApplyProfile", g_variant_new("(s)", profileName.c_str()), outMsg);
}

} // namespace fc
