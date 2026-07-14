# =============================================================================
# LianCore - 测试构建配置
# =============================================================================

# Catch2 测试框架 (本地预下载版本, 不再使用 FetchContent)
set(CATCH2_DIR "${CMAKE_SOURCE_DIR}/catch2-3.5.0")
if(NOT EXISTS "${CATCH2_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "Catch2 3.5.0 not found at ${CATCH2_DIR}. Please download it first.")
endif()
add_subdirectory(${CATCH2_DIR} ${CMAKE_BINARY_DIR}/catch2-build)
list(APPEND CMAKE_MODULE_PATH "${CATCH2_DIR}/extras")

# 收集所有测试源文件 (递归搜索子目录)
file(GLOB_RECURSE TEST_SOURCES
    ${CMAKE_SOURCE_DIR}/tests/*.cpp
)

# 排除依赖 PresetManager (JUCE 8 移除 juce_sqlite) 的测试
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*PresetManagerTests\.cpp$")
# 排除依赖已废弃 JUCE 8 API 的旧测试
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*test_audio_graph\.cpp$")
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*test_audio_node\.cpp$")
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*test_modulation\.cpp$")
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*test_wavetable\.cpp$")
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*ParameterTreeTests\.cpp$")
# 排除 MPE/微音程测试 (P0-2/P0-3 尚未实现, 测试引用已移除的 API)
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*MPEAndMicrotuningTests\.cpp$")

# 创建测试可执行文件
# 注意: PluginProcessor.cpp 依赖大量 JUCE GUI 模块, 测试单独编译不经济
# 当 MPE/微音程实现后, 再添加 PluginProcessor 依赖
add_executable(LianCoreTests
    ${TEST_SOURCES}

    # AI推理层 (EmotionToParameterMapper 依赖)
    ${CMAKE_SOURCE_DIR}/src/ai/EmotionToParameterMapper.cpp
    ${CMAKE_SOURCE_DIR}/src/ai/OnnxModelExporter.cpp
    ${CMAKE_SOURCE_DIR}/src/ai/AIInferenceEngine.cpp
    ${CMAKE_SOURCE_DIR}/src/ai/TextTokenizer.cpp
    ${CMAKE_SOURCE_DIR}/src/ai/TransformerTextEncoder.cpp
    ${CMAKE_SOURCE_DIR}/src/ai/AudioTimbreAnalyzer.cpp
    ${CMAKE_SOURCE_DIR}/src/ai/WavetableGenerator.cpp
)

# 配置测试目标
target_include_directories(LianCoreTests PRIVATE
    ${CMAKE_SOURCE_DIR}/tests/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/synthesis
    ${CMAKE_SOURCE_DIR}/src/modulation
    ${CMAKE_SOURCE_DIR}/src/params
    ${CMAKE_SOURCE_DIR}/src/ai
    ${CMAKE_SOURCE_DIR}/src/utils
    ${CMAKE_SOURCE_DIR}/src/security
    ${CMAKE_SOURCE_DIR}/src/plugin
    ${JUCE_DIR}/modules
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(LianCoreTests PRIVATE
    Catch2::Catch2WithMain
    juce_audio_basics
    juce_audio_processors
    juce_core
    juce_data_structures
    juce_dsp
    $<$<BOOL:${ONNXRUNTIME_AVAILABLE}>:onnxruntime>
)

if(ONNXRUNTIME_AVAILABLE)
    target_include_directories(LianCoreTests PRIVATE ${ONNXRUNTIME_DIR}/include)
    target_compile_definitions(LianCoreTests PRIVATE LIANCORE_HAS_ONNX=1)
endif()

# UTF-8 编码支持 (MSVC)
if(MSVC)
    target_compile_options(LianCoreTests PRIVATE /utf-8)
endif()

# 注册测试
include(Catch)
catch_discover_tests(LianCoreTests)