# UI-free library, indexing and probing; shared with the tests.
add_library(slideshowcore STATIC
    core/library.cpp
    core/libraryindexer.cpp
    core/mediaprobe.cpp
)
target_include_directories(slideshowcore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(slideshowcore PUBLIC Qt6::Core Qt6::Concurrent)

qt_add_executable(kde-wallpaper-slideshow
    main.cpp
    app.cpp
    dbusinterface.cpp
    imageprovider.cpp
    mpvvideo.cpp
    playbackcontroller.cpp
    playlist.cpp
    profile.cpp
    settingsio.cpp
    statestore.cpp
    traycontroller.cpp
)
kconfig_add_kcfg_files(kde-wallpaper-slideshow settings.kcfgc profilesettings.kcfgc GENERATE_MOC)
target_compile_definitions(kde-wallpaper-slideshow PRIVATE PROJECT_VERSION="${PROJECT_VERSION}")

qt_add_qml_module(kde-wallpaper-slideshow
    URI Slideshow
    VERSION 1.0
    # Qt is shared, so imported QML plugins load at run time and need no linking.
    NO_IMPORT_SCAN
    QML_FILES
        qml/CurrentTitle.qml
        qml/EdgeBar.qml
        qml/MediaSlot.qml
        qml/OverlayUI.qml
        qml/PlaylistPanel.qml
        qml/ProfilePage.qml
        qml/ScreenRenderer.qml
        qml/SettingsFileButtons.qml
        qml/SettingsWindow.qml
)

target_link_libraries(kde-wallpaper-slideshow PRIVATE
    slideshowcore
    Qt6::DBus
    Qt6::Gui
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Widgets
    KF6::ConfigGui
    KF6::CoreAddons
    KF6::DBusAddons
    KF6::GlobalAccel
    KF6::Notifications
    KF6::StatusNotifierItem
    LayerShellQt::Interface
    MpvQt::MpvQt
)

install(TARGETS kde-wallpaper-slideshow ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
