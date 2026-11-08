# Wallpaper Slideshow

A slideshow of a folder of images, animations and videos drawn over the KDE Plasma
wallpaper, without changing it. Requires Plasma 6.7+ on Wayland.

It draws on Wayland layer-shell surfaces on the bottom layer, above the desktop and
below all windows. The real wallpaper and desktop icons come back when it exits or is
hidden from the tray.

## Build

With the Fedora packages below (other distributions need their equivalents):

```sh
sudo dnf install gcc-c++ cmake ninja-build extra-cmake-modules \
  qt6-qtbase-devel qt6-qtdeclarative-devel layer-shell-qt-devel mpvqt-devel \
  kf6-kconfig-devel kf6-kcoreaddons-devel kf6-kdbusaddons-devel kf6-kglobalaccel-devel \
  kf6-knotifications-devel kf6-kstatusnotifieritem-devel kf6-kirigami-devel \
  qt6-qtimageformats
cmake -S . -B build -G Ninja -DCMAKE_INSTALL_PREFIX=~/.local
cmake --build build
ctest --test-dir build
cmake --install build
```

## Use

Run `kde-wallpaper-slideshow` and add one or more folders. Their files are pooled into
one shuffled playlist; overlapping folders are fine. Closing the settings window keeps
the app in the tray. Left-click the tray icon to show the window again. The tray menu
has Play/Pause, Hide/Show and Exit.

The General tab holds app-wide options. By default one "All screens" tab configures
every monitor and they all show the same item. With "Use separate settings for each
screen", every monitor gets its own tab with its own folders, timing, scaling,
playlist and controls, and plays independently. A monitor's settings start as a copy of
the "All screens" ones.

The Power section of the General tab can keep the computer from sleeping, and the screen
from turning off or locking, while a slideshow plays. Sleep you start yourself, or closing
the lid, still works. It can also pause or hide the slideshow when the screen locks or the
computer goes to sleep.

The Filename section of each tab can show the current file's name in the top right
corner, all the time or only when the pointer comes near it, with a choice of margins,
font, text size, fill and border colors and a drop shadow. In the hover mode, that corner takes
clicks instead of the desktop.

The playlist follows the system color scheme, or can be set to Breeze Light or Dark in
its tab, for example to match a dark panel while the rest of the desktop is light.

Typing in the playlist's search box limits playback, prev/next included, to the
matching files until the search is cleared. The `.*` button next to it makes the search a
case-insensitive regular expression; an invalid one is shown in red and ignored.

Settings can be saved to and loaded from a JSON file in the Settings File section.
Export All on the General tab saves every setting, including the tabs of screens that
aren't connected, and Import there restores them. Each screen's tab has its own Export
and Import: importing a file there applies it to that screen only, using the profile in
the file for that screen, else its only profile, else its "All screens" one. Imported
settings are saved like ones changed in the window. Entries that are unknown or of the
wrong type are left out, and numbers are clamped to their allowed range. Enum settings
(`scaleMode`, `playlistCorner`, `playlistTheme`) are stored as the index of the choice.

- Settings are in `~/.config/kde-wallpaper-slideshowrc` (`[General]` and one
  `[Profile <name>]` group per tab).
- Runtime state (current item, pause, hidden) is in
  `~/.local/state/kde-wallpaper-slideshowstaterc`.
- The file index of each tab is cached in `~/.cache/kde-wallpaper-slideshow/`.

Global shortcuts (next, previous, play/pause, hide/show) are registered without keys.
Assign them in System Settings › Shortcuts.

Remote control over D-Bus:

```sh
busctl --user call io.github.kde-wallpaper-slideshow /Slideshow \
  io.github.kdewallpaperslideshow.Slideshow next
```

Methods: `next`, `previous`, `first`, `last`, `togglePause`, `toggleHidden`,
`jumpTo(n)`, `showSettings`, `quit`.

## AI Disclosure

> [!WARNING]
> This project was implemented by Claude Code; I provided only the functional and
> performance requirements. You are responsible for any code or binaries you run, so
> don't run code you don't trust.
