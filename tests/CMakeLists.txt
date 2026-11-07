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
