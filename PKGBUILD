# Maintainer: Kaizen Team <kaizen@archlinux.org>
pkgname=kaizen-power-tool
pkgver=1.0.0
pkgrel=1
pkgdesc="Modern hardware power and fan control suite for CPU, GPU, and cooling fans with D-Bus daemon, CLI, TUI, and GTK4 GUI"
arch=('x86_64' 'aarch64')
url="https://github.com/kaizen/kaizen-power-tool"
license=('GPL-3.0-or-later')
depends=(
    'gtk4'
    'libadwaita'
    'ncurses'
    'glib2'
    'systemd-libs'
    'polkit'
)
makedepends=(
    'cmake'
    'gcc'
    'gettext'
    'pkgconf'
)
provides=('kaizen-power-tool' 'kaizenctl' 'kaizend' 'kaizengui' 'kaizentui')
conflicts=('kaizen-power-tool-git')
source=()

build() {
    cmake -B build -S . \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr
    cmake --build build
}

package() {
    DESTDIR="${pkgdir}" cmake --install build

    # System integration files
    install -Dm644 data/kaizend.service "${pkgdir}/usr/lib/systemd/system/kaizend.service"
    install -Dm644 data/org.kaizen.PowerTool.conf "${pkgdir}/usr/share/dbus-1/system.d/org.kaizen.PowerTool.conf"
    install -Dm644 data/kaizenctl-sysusers.conf "${pkgdir}/usr/lib/sysusers.d/kaizenctl.conf"
    install -Dm644 data/io.github.kaizen.PowerTool.desktop "${pkgdir}/usr/share/applications/io.github.kaizen.PowerTool.desktop"
    install -Dm644 resources/logo.png "${pkgdir}/usr/share/pixmaps/io.github.kaizen.PowerTool.png"
    install -Dm644 resources/icons/hicolor/512x512/apps/io.github.kaizen.PowerTool.png "${pkgdir}/usr/share/icons/hicolor/512x512/apps/io.github.kaizen.PowerTool.png"
    install -Dm644 resources/icons/hicolor/scalable/apps/io.github.kaizen.PowerTool.png "${pkgdir}/usr/share/icons/hicolor/scalable/apps/io.github.kaizen.PowerTool.png"

    # Create convenient symlink for TUI
    ln -sf kaizenctl "${pkgdir}/usr/bin/kaizentui"
}
