# WebAssembly build for Lv-00
# This CMake file is used with Emscripten to build for the web

cmake_minimum_required(VERSION 3.15)
project(lv00-web VERSION 3.2.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Emscripten specific settings
if(NOT EMSCRIPTEN)
    message(FATAL_ERROR "This CMake file must be used with Emscripten. Use emcmake cmake ...")
endif()

# Emscripten 版本检查：建议使用 3.1.0 以上版本以获得完整的 WebAssembly 特性支持
if(EMSCRIPTEN_VERSION VERSION_LESS "3.2.0")
    message(WARNING "Emscripten ${EMSCRIPTEN_VERSION} 版本较旧。"
                    "建议升级到 3.1.0+ 以获得 -s EXPORT_ES6 等特性的完整支持。")
endif()

# 显式列出源文件（与主 CMakeLists 保持一致），避免 file(GLOB_RECURSE) 的隐患
set(LV00_HEADERS
    ../include/lv00/lv00.h
    ../include/lv00/error_codes.h
    ../include/lv00/symbolic_coord.h
    ../include/lv00/constraint_graph.h
    ../include/lv00/graph_hash.h
    ../include/lv00/normalization.h
    ../include/lv00/solver.h
    ../include/lv00/rewrite.h
    ../include/lv00/unify.h
    ../include/lv00/axiom_pkg.h
    ../include/lv00/module.h
    ../include/lv00/engine.h
    ../include/lv00/debug.h
    ../include/lv00/mpz_poly.h
    ../include/lv00/func_block.h
    ../include/lv00/type_system.h
    ../include/lv00/proof.h
    ../include/lv00/recursion.h
    ../include/lv00/lv00_utils.h
    ../include/lv00/high_dim.h
    ../include/lv00/interop.h
    ../include/lv00/formula_parser.h
    ../include/lv00/formula_renderer.h
    ../include/lv00/formula_converter.h
    ../include/lv00/prop_verifier.h
    ../include/lv00/lexer_shared.h
    ../include/lv00/stream.h
    ../include/lv00/func_block_utils.h
    ../include/lv00/stream_context_util.h
    ../include/lv00/func_block_registry.h
    ../include/lv00/func_block_preset_ops.h
    # 模块化预设函数块系统
    ../include/lv00/preset_blocks.h
    ../include/lv00/preset_basic_geometry.h
    ../include/lv00/preset_transformations.h
    ../include/lv00/preset_measurements.h
    ../include/lv00/preset_polygons.h
    ../include/lv00/preset_algebraic.h
)

# 排除 debug.c 和 engine.c 的原因：
#   - debug.c：浏览器环境没有文件系统，日志输出方式不同，不适用
#   - engine.c：引擎主循环依赖系统级线程/信号，在 Web Worker 模型中行为不同
set(LV00_SOURCES
    ../src/core/error_codes.c
    ../src/core/symbolic_coord.c
    ../src/core/constraint_graph.c
    ../src/core/normalization.c
    ../src/core/solver.c
    ../src/core/mpz_poly_resultant.c
    ../src/core/rewrite.c
    ../src/core/unify.c
    ../src/axiom/axiom_pkg.c
    ../src/core/module.c
    ../src/core/stream.c
    ../src/func_block/func_block.c
    ../src/func_block/func_block_determinism.c
    ../src/func_block/func_block_instantiate.c
    ../src/func_block/func_block_serialize.c
    ../src/func_block/func_block_utils.c
    ../src/func_block/func_block_selector.c
    ../src/func_block/func_block_compose.c
    ../src/func_block/func_block_registry.c
    # 模块化预设函数块系统
    ../src/func_block/preset_blocks.c
    ../src/func_block/func_block_preset_ops.c
    ../src/preset/preset_basic_geometry.c
    ../src/preset/preset_transformations.c
    ../src/preset/preset_measurements.c
    ../src/preset/preset_polygons.c
    ../src/preset/preset_algebraic.c
    ../src/core/type_system.c
    ../src/core/proof.c
    ../src/core/proof_multi_strategy.c
    ../src/core/recursion.c
    ../src/utils/lv00_utils.c
    ../src/core/lv00.c
    ../src/core/high_dim.c
    ../src/interop/interop.c
    ../src/parser/formula_parser.c
    ../src/parser/formula_renderer.c
    ../src/parser/formula_converter.c
    ../src/core/prop_verifier.c
    ../src/parser/lexer_shared.c
    ../src/parser/parser_safety.c
)

# Create WebAssembly module
add_executable(lv00_web ${LV00_SOURCES} ${LV00_HEADERS} lv00_web_bindings.c)

# 使用 target_include_directories 替代全局 include_directories，避免污染所有目标
target_include_directories(lv00_web PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

# Emscripten link options
set_target_properties(lv00_web PROPERTIES
    LINK_FLAGS "-O3 \
        -s WASM=1 \
        -s EXPORT_ES6=1 \
        -s MODULARIZE=1 \
        -s EXPORT_NAME='Lv00Module' \
        -s EXPORTED_FUNCTIONS='[\"_malloc\", \"_free\"]' \
        -s EXPORTED_RUNTIME_METHODS='[\"ccall\", \"cwrap\", \"getValue\", \"setValue\", \"UTF8ToString\", \"stringToUTF8\"]' \
        -s ALLOW_MEMORY_GROWTH=1 \
        -s INITIAL_MEMORY=64MB \
        -s MAXIMUM_MEMORY=256MB \
        -s STACK_SIZE=1MB \
        --bind"
)

# 将构建产物复制到构建目录下的 dist/，而非污染源码树
add_custom_command(TARGET lv00_web POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/dist
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_BINARY_DIR}/lv00_web.js
        ${CMAKE_BINARY_DIR}/dist/lv00_web.js
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_BINARY_DIR}/lv00_web.wasm
        ${CMAKE_BINARY_DIR}/dist/lv00_web.wasm
    COMMENT "Copying WebAssembly files to ${CMAKE_BINARY_DIR}/dist/"
)
