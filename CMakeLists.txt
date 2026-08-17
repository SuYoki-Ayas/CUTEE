cmake_minimum_required(VERSION 3.21)

project(CUTEE VERSION 0.1.0 LANGUAGES CXX)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 查找 Qt6
find_package(Qt6 6.2 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Sql
)

# 启用 Qt 代码自动处理
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# 添加可执行文件
add_executable(${PROJECT_NAME} WIN32
    src/main.cpp
    src/CUTEE.cpp
    src/CUTEE.h
    src/CUTEE.ui
)

# 设置包含目录
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# 链接 Qt 库
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Sql
)

# 在文件末尾添加
if(WIN32)
    # 创建部署目录
    set(DEPLOY_DIR "${CMAKE_BINARY_DIR}/deploy")
    
    # 获取 Qt 的 bin 目录
    get_target_property(_qmake_executable Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(_qt_bin_dir "${_qmake_executable}" DIRECTORY)
    
    # 添加自定义命令来部署
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        # 创建部署目录
        COMMAND ${CMAKE_COMMAND} -E make_directory "${DEPLOY_DIR}"
        
        # 复制可执行文件到部署目录
        COMMAND ${CMAKE_COMMAND} -E copy_if_different 
                "$<TARGET_FILE:${PROJECT_NAME}>" 
                "${DEPLOY_DIR}/"
        
        # 运行 windeployqt 在部署目录
        COMMAND "${_qt_bin_dir}/windeployqt.exe"
                --release
                --compiler-runtime
                "${DEPLOY_DIR}/${PROJECT_NAME}.exe"
        
        COMMENT "Deploying Qt dependencies to ${DEPLOY_DIR}..."
        VERBATIM
    )
    
    # 打印部署目录位置
    message(STATUS "Deployment directory: ${DEPLOY_DIR}")
endif()