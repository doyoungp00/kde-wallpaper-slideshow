cmake_minimum_required(VERSION 3.22)
project(kde-wallpaper-slideshow VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ECM 6.0 REQUIRED NO_MODULE)
list(APPEND CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
include(KDEInstallDirs)
include(KDECMakeSettings)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Concurrent DBus Gui Qml Quick QuickControls2 Widgets)
find_package(KF6 6.0 REQUIRED COMPONENTS Config CoreAddons DBusAddons GlobalAccel Kirigami Notifications StatusNotifierItem)
find_package(LayerShellQt 6.0 REQUIRED)
find_package(MpvQt 1.0 REQUIRED)
qt_standard_project_setup(REQUIRES 6.8)

add_subdirectory(src)
if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_subdirectory(tests)
endif()

install(FILES io.github.kde-wallpaper-slideshow.desktop DESTINATION ${KDE_INSTALL_APPDIR})
