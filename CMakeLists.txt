cmake_minimum_required(VERSION 3.21)

project(CUTEE VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/utf-8 /permissive-)
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Sql Concurrent Test)

qt_standard_project_setup(REQUIRES 6.5)

set(CUTEE_CORE_SOURCES
    src/core/AppPaths.h
    src/core/AppPaths.cpp
    src/core/Logging.h
    src/core/Logging.cpp
    src/core/Models.h
    src/core/Models.cpp
    src/core/Result.h
    src/core/CsvExport.h
    src/core/CsvExport.cpp
    src/core/ScoreSummary.h
    src/core/ScoreSummary.cpp
    src/core/ScoreValue.h
    src/core/ScoreValue.cpp
    src/core/TimeUtils.h
    src/core/TimeUtils.cpp
    src/storage/Database.h
    src/storage/Database.cpp
    src/storage/DbUtils.h
    src/storage/DbUtils.cpp
    src/storage/Migrations.h
    src/storage/Migrations.cpp
    src/storage/SettingsRepository.h
    src/storage/SettingsRepository.cpp
    src/storage/ExamRecordRepository.h
    src/storage/ExamRecordRepository.cpp
    src/storage/StudentProfileRepository.h
    src/storage/StudentProfileRepository.cpp
    src/storage/SubjectRepository.h
    src/storage/SubjectRepository.cpp
    src/service/ScoreService.h
    src/service/ScoreService.cpp
    src/service/LegacyDataService.h
    src/service/LegacyDataService.cpp
)

add_library(cutee_core STATIC ${CUTEE_CORE_SOURCES})
target_include_directories(cutee_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(cutee_core PUBLIC Qt6::Core Qt6::Sql)
target_compile_definitions(cutee_core PUBLIC CUTEE_VERSION="${PROJECT_VERSION}")

if(MSVC)
    target_compile_options(cutee_core PRIVATE /W4)
else()
    target_compile_options(cutee_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Presentation models shared by the pages and the unit tests. They need Qt Gui
# only for role colours, so they stay out of the executable target.
add_library(cutee_models STATIC
    src/ui/ScoreTableModel.h
    src/ui/ScoreTableModel.cpp
)
target_link_libraries(cutee_models PUBLIC cutee_core Qt6::Gui)

if(MSVC)
    target_compile_options(cutee_models PRIVATE /W4)
else()
    target_compile_options(cutee_models PRIVATE -Wall -Wextra -Wpedantic)
endif()

qt_add_executable(CUTEE
    resources/cutee.qrc
    src/app/main.cpp
    src/app/MainWindow.h
    src/app/MainWindow.cpp
    src/app/Theme.h
    src/app/Theme.cpp
    src/ui/UiKit.h
    src/ui/UiKit.cpp
    src/ui/LogPage.h
    src/ui/LogPage.cpp
    src/ui/ProfilePage.h
    src/ui/ProfilePage.cpp
    src/ui/ScoreDialog.h
    src/ui/ScoreDialog.cpp
    src/ui/ScorePage.h
    src/ui/ScorePage.cpp
    src/ui/SummaryPage.h
    src/ui/SummaryPage.cpp
    src/ui/SettingsPage.h
    src/ui/SettingsPage.cpp
    src/ui/SubjectDialog.h
    src/ui/SubjectDialog.cpp
    src/ui/SubjectPage.h
    src/ui/SubjectPage.cpp
)
set_target_properties(CUTEE PROPERTIES
    WIN32_EXECUTABLE "$<NOT:$<CONFIG:Debug>>"
    AUTORCC ON)
target_link_libraries(CUTEE PRIVATE cutee_core cutee_models Qt6::Widgets Qt6::Concurrent)

option(CUTEE_BUILD_TESTS "Build the Qt Test suites" ON)
if(CUTEE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
