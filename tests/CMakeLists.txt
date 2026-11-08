function(cutee_add_test name)
    qt_add_executable(${name} ${ARGN})
    set_target_properties(${name} PROPERTIES WIN32_EXECUTABLE OFF)
    target_link_libraries(${name} PRIVATE cutee_core cutee_models Qt6::Test)
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;CUTEE_DATA_DIR=${CMAKE_CURRENT_BINARY_DIR}/testdata-${name}")
endfunction()

cutee_add_test(tst_database tst_database.cpp)
cutee_add_test(tst_scores tst_scores.cpp)
cutee_add_test(tst_summary tst_summary.cpp)
cutee_add_test(tst_score_model tst_score_model.cpp)
cutee_add_test(tst_school_stage tst_school_stage.cpp)
cutee_add_test(tst_data_maintenance tst_data_maintenance.cpp)

# The verification window is a self-contained widget; compile it directly so
# its keyword gating is covered without opening the whole settings page.
cutee_add_test(tst_danger_dialog
    tst_danger_dialog.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/DangerConfirmDialog.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/DangerConfirmDialog.h
    ${CMAKE_SOURCE_DIR}/src/ui/UiKit.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/UiKit.h)
target_link_libraries(tst_danger_dialog PRIVATE Qt6::Widgets)

# The profile page lives in the executable target, so the test compiles its
# sources directly instead of linking them from a library.
cutee_add_test(tst_profile_page
    tst_profile_page.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/ProfilePage.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/ProfilePage.h
    ${CMAKE_SOURCE_DIR}/src/ui/UiKit.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/UiKit.h)
target_link_libraries(tst_profile_page PRIVATE Qt6::Widgets)

# Same approach for the settings page action contract.
cutee_add_test(tst_settings_page
    tst_settings_page.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/SettingsPage.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/SettingsPage.h
    ${CMAKE_SOURCE_DIR}/src/ui/DangerConfirmDialog.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/DangerConfirmDialog.h
    ${CMAKE_SOURCE_DIR}/src/ui/UiKit.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/UiKit.h)
target_link_libraries(tst_settings_page PRIVATE Qt6::Widgets)

# The trend chart is a self-contained widget; compile it directly so the chart
# data contract stays covered without starting the whole window.
cutee_add_test(tst_trend_chart
    tst_trend_chart.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/TrendChart.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/TrendChart.h
    ${CMAKE_SOURCE_DIR}/src/ui/UiKit.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/UiKit.h)
target_link_libraries(tst_trend_chart PRIVATE Qt6::Widgets Qt6::Charts)
