include(ECMAddTests)

ecm_add_test(tst_core.cpp TEST_NAME tst_core LINK_LIBRARIES slideshowcore Qt6::Test)
ecm_add_test(bench_indexer.cpp TEST_NAME bench_indexer LINK_LIBRARIES slideshowcore Qt6::Test)

# The playback controller with its settings class, outside the QML module.
add_executable(tst_playback tst_playback.cpp ../src/playbackcontroller.cpp ../src/playlist.cpp)
kconfig_add_kcfg_files(tst_playback ../src/profilesettings.kcfgc GENERATE_MOC)
target_include_directories(tst_playback PRIVATE ../src)
target_link_libraries(tst_playback PRIVATE slideshowcore Qt6::Test Qt6::Qml KF6::ConfigGui)
add_test(NAME tst_playback COMMAND tst_playback)

add_executable(tst_settingsio tst_settingsio.cpp ../src/settingsio.cpp)
kconfig_add_kcfg_files(tst_settingsio ../src/profilesettings.kcfgc GENERATE_MOC)
target_include_directories(tst_settingsio PRIVATE ../src)
target_link_libraries(tst_settingsio PRIVATE Qt6::Test Qt6::Gui Qt6::Qml KF6::ConfigGui)
add_test(NAME tst_settingsio COMMAND tst_settingsio)
