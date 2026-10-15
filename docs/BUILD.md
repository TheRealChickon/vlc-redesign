# Build information

Upstream VLC revision: `392bc1d0abdb7396b6c97fef206efa1d23868e19`. The `vlc/` tree is a complete source snapshot at this revision plus the redesign changes. Original build scripts, dependency recipes and patches are retained. The source manifest hashes every included VLC file.

Tested target: Windows x86_64, VLC 4.0.0-dev, Qt 6.8.3, MinGW-w64 cross compilation on isolated Debian/WSL. The build environment came from VideoLAN's public CI image `registry.videolan.org/vlc-debian-win64-posix:20260611225331`.

The build used VideoLAN's pinned prebuilt contrib archive:

`https://artifacts.videolan.org/vlc/win64/vlc-contrib-x86_64-w64-mingw32-a5538c557d276378d5ba0f8f8010a8b87eaa6f78.tar.zst`

Its exact compiled dependencies are not replaced by the source recipes alone. For public binary distribution collect the corresponding upstream dependency sources and exact patches, compare versions with this archive, and retain their notices. This archive is a build input, not a corresponding-source archive.

The upstream build entry point is `vlc/extras/package/win32/build.sh`. Its cross-build toolchain, prebuilt selection, package path and explicit environment must be set before use; consult the preserved script's help. The tested tree was at `/opt/vlc-redesign`, with build directory `win64` and stage `/opt/vlc-redesign-package`. GUI-only rebuilds use `scripts/rebuild-toolbar-fit.sh` after a complete configured build. It requires the existing toolchain; it is not a one-command fresh-machine installer.

Native checks are explicitly opt-in through `VLC_REDESIGN_QA_DIR` and mode flags in `maininterface/redesign_qa.hpp`. The Windows runner creates a private desktop, starts only the QA package there and never switches the user's desktop. Use a task-local approved test path and synthetic local fixtures. The test runner is included for reproducibility; fixtures and portable library state are not included.

The original VLC engine, library and 409 plugins are retained. New executables at new paths can trigger Windows Firewall discovery prompts; this build does not request administrator elevation. Do not enable network discovery merely to play local files.

The full video update rebuilds the modified core and Qt GUI with scripts/rebuild-full-video.sh in the configured toolchain. Automake and Meson resource lists include the new folder/media panels. Public distribution still needs exact dependency sources.
