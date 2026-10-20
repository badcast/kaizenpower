cmake_minimum_required(VERSION 3.20)
project(KaizenPowerTool VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(ADWAITA REQUIRED IMPORTED_TARGET libadwaita-1 gtk4)
pkg_check_modules(GIO REQUIRED IMPORTED_TARGET gio-2.0 gio-unix-2.0)
pkg_check_modules(NCURSES REQUIRED IMPORTED_TARGET ncursesw)

# Core Backend Library
set(CORE_SOURCES
    src/core/I18n.cpp
    src/core/SystemInfo.cpp
    src/core/SysfsUtils.cpp
    src/core/FanManager.cpp
    src/core/CpuManager.cpp
    src/core/GpuManager.cpp
    src/core/ProfileManager.cpp
    src/core/PrivilegeManager.cpp
    src/dbus/DBusClient.cpp
    src/dbus/DBusServer.cpp
)

set(CORE_HEADERS
    src/core/I18n.hpp
    src/core/SystemInfo.hpp
    src/core/Types.hpp
    src/core/JsonUtils.hpp
    src/core/SysfsUtils.hpp
    src/core/FanManager.hpp
    src/core/CpuManager.hpp
    src/core/GpuManager.hpp
    src/core/ProfileManager.hpp
    src/core/PrivilegeManager.hpp
    src/dbus/DBusCommon.hpp
    src/dbus/DBusClient.hpp
    src/dbus/DBusServer.hpp
)

add_library(kaizen_core STATIC ${CORE_SOURCES} ${CORE_HEADERS})
target_link_libraries(kaizen_core PUBLIC PkgConfig::GIO)
target_include_directories(kaizen_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

# 1. Daemon executable: kaizend
add_executable(kaizend src/daemon_main.cpp)
target_link_libraries(kaizend PRIVATE kaizen_core)

# 2. CLI and TUI executable: kaizenctl
set(CLI_SOURCES
    src/cli_main.cpp
    src/cli/CliApp.cpp
    src/tui/TuiApp.cpp
)
set(CLI_HEADERS
    src/cli/CliApp.hpp
    src/tui/TuiApp.hpp
)
add_executable(kaizenctl ${CLI_SOURCES} ${CLI_HEADERS})
target_link_libraries(kaizenctl PRIVATE kaizen_core PkgConfig::NCURSES)

# 3. GUI executable: kaizengui
set(GUI_SOURCES
    src/main.cpp
    src/ui/UIUtils.cpp
    src/ui/FanCurveWidget.cpp
    src/ui/FanCard.cpp
    src/ui/CpuView.cpp
    src/ui/GpuView.cpp
    src/ui/ProfilesView.cpp
    src/ui/SummaryView.cpp
    src/app/MainWindow.cpp
    src/app/Application.cpp
)
set(GUI_HEADERS
    src/ui/UIUtils.hpp
    src/ui/FanCurveWidget.hpp
    src/ui/FanCard.hpp
    src/ui/CpuView.hpp
    src/ui/GpuView.hpp
    src/ui/ProfilesView.hpp
    src/ui/SummaryView.hpp
    src/app/MainWindow.hpp
    src/app/Application.hpp
)
add_executable(kaizengui ${GUI_SOURCES} ${GUI_HEADERS})
target_link_libraries(kaizengui PRIVATE kaizen_core PkgConfig::ADWAITA)

# Symlink kaizentui -> kaizenctl in build directory
add_custom_target(kaizentui ALL
    COMMAND ${CMAKE_COMMAND} -E create_symlink kaizenctl ${CMAKE_BINARY_DIR}/kaizentui
    DEPENDS kaizenctl
)

# Gettext translations compilation
set(LANGUAGES ru kk ky tr uz)
set(MO_FILES)

foreach(LANG ${LANGUAGES})
    set(PO_FILE "${CMAKE_CURRENT_SOURCE_DIR}/po/${LANG}.po")
    set(MO_DIR "${CMAKE_BINARY_DIR}/locale/${LANG}/LC_MESSAGES")
    set(MO_FILE "${MO_DIR}/fancontroller.mo")
    
    add_custom_command(
        OUTPUT "${MO_FILE}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${MO_DIR}"
        COMMAND msgfmt -c "${PO_FILE}" -o "${MO_FILE}"
        DEPENDS "${PO_FILE}"
        COMMENT "Compiling ${LANG} gettext translation"
    )
    list(APPEND MO_FILES "${MO_FILE}")
    install(FILES "${MO_FILE}" DESTINATION "share/locale/${LANG}/LC_MESSAGES")
endforeach()

add_custom_target(translations ALL DEPENDS ${MO_FILES})
add_dependencies(kaizengui translations)
add_dependencies(kaizenctl translations)
add_dependencies(kaizend translations)

# Install rules
install(TARGETS kaizend kaizenctl kaizengui DESTINATION bin)
install(DIRECTORY resources DESTINATION share/kaizen)
install(FILES resources/logo.png DESTINATION share/pixmaps RENAME io.github.kaizen.PowerTool.png)
install(FILES resources/icons/hicolor/512x512/apps/io.github.kaizen.PowerTool.png DESTINATION share/icons/hicolor/512x512/apps)
install(FILES resources/icons/hicolor/scalable/apps/io.github.kaizen.PowerTool.png DESTINATION share/icons/hicolor/scalable/apps)
install(FILES data/org.kaizen.PowerTool.conf DESTINATION share/dbus-1/system.d)
install(FILES data/kaizend.service DESTINATION lib/systemd/system)
install(FILES data/kaizenctl-sysusers.conf DESTINATION lib/sysusers.d RENAME kaizenctl.conf)
install(FILES data/io.github.kaizen.PowerTool.desktop DESTINATION share/applications)
