#!/usr/bin/env bash
set -euo pipefail
export PATH=/opt/tools/bin:/opt/tools/libexec:/opt/rust/cargo/bin:$PATH
export RUSTUP_HOME=/opt/rust/rustup CARGO_HOME=/opt/rust/cargo WINE_SDK_PATH=/usr/include/wine/wine/windows LANG=en_US.UTF-8
cd /opt/vlc-redesign/win64
make -j2 -C src libvlccore.la
x86_64-w64-mingw32-strip --strip-unneeded src/.libs/libvlccore.dll -o /opt/vlc-redesign-package/libvlccore.dll
make -j2 -C modules/gui/qt
x86_64-w64-mingw32-strip --strip-unneeded modules/gui/qt/.libs/libqt_plugin.dll -o /opt/vlc-redesign-package/plugins/gui/libqt_plugin.dll
echo 'FULL_VIDEO_BUILD_SUCCESS'
