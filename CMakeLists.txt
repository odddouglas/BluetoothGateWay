# 确保 CMake 版本至少为 3.5
cmake_minimum_required(VERSION 3.16)

# 包含 ESP-IDF 项目的 CMake 配置文件
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# 定义项目名称
project(esp32gateway)

# 使用 list(APPEND ...) 方式添加组件目录
list(APPEND EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/bsp/LED")
list(APPEND EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/bsp/KEY")
list(APPEND EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/bsp/WS2812")
list(APPEND EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/bsp/MQTT")

idf_component_get_property(mqtt mqtt COMPONENT_LIB)
target_sources(${mqtt} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/bsp/MQTT/custom_outbox.cpp)
# First we get our dependency
idf_component_get_property(pthread pthread COMPONENT_LIB)
# And them we link the components
target_link_libraries(${mqtt} ${pthread})



