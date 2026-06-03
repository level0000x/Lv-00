# LV-00 Sample Plugin CMakeLists.txt

cmake_minimum_required(VERSION 3.10)
project(sample_plugin VERSION 1.0.0 LANGUAGES C)

# 设置 C 标准
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 查找 LV-00 库
find_package(lv00 REQUIRED)

# 创建共享库
add_library(sample_plugin SHARED
    sample_plugin.c
)

# 设置包含目录
target_include_directories(sample_plugin PRIVATE
    ${CMAKE_SOURCE_DIR}/../../core/include
)

# 链接 LV-00 库
target_link_libraries(sample_plugin PRIVATE
    lv00
)

# 设置输出属性
set_target_properties(sample_plugin PROPERTIES
    PREFIX ""  # 不添加 lib 前缀
    OUTPUT_NAME "sample_plugin"
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins
)

# 安装规则
install(TARGETS sample_plugin
    LIBRARY DESTINATION plugins
)

# 安装配置文件
install(FILES sample_plugin.conf
    DESTINATION config/plugins
)
