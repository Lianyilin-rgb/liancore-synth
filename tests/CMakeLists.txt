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

# 排除依赖已废弃 JUCE 8 API 的旧测试
# P7修复: PresetManagerTests 已修复（使用ASCII工作目录路径替代createTempFile）
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*test_audio_graph\\.cpp$")
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*test_audio_node\\.cpp$")
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*test_modulation\\.cpp$")
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*test_wavetable\\.cpp$")
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*ParameterTreeTests\\.cpp$")
# 排除 MPE/微音程测试 (依赖 PluginProcessor GUI 模块, 测试构建不含 GUI)
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*MPEAndMicrotuningTests\\.cpp$")
# P7-2: ONNX测试已恢复 - 以下排除已移除
# list(FILTER TEST_SOURCES EXCLUDE REGEX ".*WavetableGeneratorTests\\.cpp$")
# list(FILTER TEST_SOURCES EXCLUDE REGEX ".*AudioTimbreAnalyzerTests\\.cpp$")
# list(FILTER TEST_SOURCES EXCLUDE REGEX ".*AIWavetableUITests\\.cpp$")
# list(FILTER TEST_SOURCES EXCLUDE REGEX ".*TimbreMatchTests\\.cpp$")
# list(FILTER TEST_SOURCES EXCLUDE REGEX ".*SpectralWarperTests\\.cpp$")
# list(FILTER TEST_SOURCES EXCLUDE REGEX ".*OversamplingProcessorTests\\.cpp$")

# 创建测试可执行文件
# 注意: PluginProcessor.cpp 依赖大量 JUCE GUI 模块, 测试单独编译不经济
add_executable(LianCoreTests
    ${TEST_SOURCES}

    # 核心音频节点基类 (效果器测试依赖)
    ${CMAKE_SOURCE_DIR}/src/core/AudioNode.cpp

    # AI推理层 (EmotionToParameterMapper 依赖)
    ${CMAKE_SOURCE_DIR}/src/ai/EmotionToParameterMapper.cpp
    ${CMAKE_SOURCE_DIR}/src/ai/OnnxModelExporter.cpp
    ${CMAKE_SOURCE_DIR}/src/ai/AIInferenceEngine.cpp
    ${CMAKE_SOURCE_DIR}/src/ai/TextTokenizer.cpp
    ${CMAKE_SOURCE_DIR}/src/ai/TransformerTextEncoder.cpp

    # MPE 处理器 (P0-2)
    ${CMAKE_SOURCE_DIR}/src/plugin/MPEProcessor.cpp

    # 微音程/调音支持 (P0-3)
    ${CMAKE_SOURCE_DIR}/src/tuning/ScalaFileLoader.cpp
    ${CMAKE_SOURCE_DIR}/src/tuning/MicrotuningManager.cpp

    # 效果器 (P1-2 - 新增6种)
    ${CMAKE_SOURCE_DIR}/src/synthesis/Chorus.cpp
    ${CMAKE_SOURCE_DIR}/src/synthesis/Flanger.cpp
    ${CMAKE_SOURCE_DIR}/src/synthesis/Phaser.cpp
    ${CMAKE_SOURCE_DIR}/src/synthesis/BitCrusher.cpp
    ${CMAKE_SOURCE_DIR}/src/synthesis/RingMod.cpp
    ${CMAKE_SOURCE_DIR}/src/synthesis/ConvolutionReverb.cpp

    # 混沌调制 (P1-3)
    ${CMAKE_SOURCE_DIR}/src/modulation/ChaoticLFO.cpp
    ${CMAKE_SOURCE_DIR}/src/modulation/ChaoticEnvelope.cpp
    ${CMAKE_SOURCE_DIR}/src/modulation/ModulationMatrix.cpp

    # AI文本到波表引擎 (Task 2)
    ${CMAKE_SOURCE_DIR}/src/ai/AITextToWavetable.cpp

    # 音色分析器 (Task 3)
    ${CMAKE_SOURCE_DIR}/src/ai/AudioTimbreAnalyzer.cpp

    # 预设管理系统 (Task 4)
    ${CMAKE_SOURCE_DIR}/src/params/PresetManager.cpp
    ${CMAKE_SOURCE_DIR}/src/params/PreparedStatement.cpp
    ${CMAKE_SOURCE_DIR}/src/params/sqlite3.c
    ${CMAKE_SOURCE_DIR}/src/security/AES256Encryptor.cpp

    # 波表引擎 (AI波表测试依赖)
    ${CMAKE_SOURCE_DIR}/src/synthesis/WavetableBank.cpp
    ${CMAKE_SOURCE_DIR}/src/synthesis/WavetableOscillator.cpp

    # P7-2: ONNX测试恢复 - 添加需要的源文件
    ${CMAKE_SOURCE_DIR}/src/ai/WavetableGenerator.cpp
    ${CMAKE_SOURCE_DIR}/src/synthesis/SpectralWarper.cpp
    ${CMAKE_SOURCE_DIR}/src/plugin/OversamplingProcessor.cpp
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
    ${CMAKE_SOURCE_DIR}/src/tuning
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
    juce_cryptography
    $<$<BOOL:${ONNXRUNTIME_AVAILABLE}>:onnxruntime>
)

# P7-2: ONNX Runtime 版本统一 - 启用 ONNX 头文件和编译定义
if(ONNXRUNTIME_AVAILABLE)
    target_include_directories(LianCoreTests PRIVATE ${ONNXRUNTIME_DIR}/include)
    target_compile_definitions(LianCoreTests PRIVATE LIANCORE_HAS_ONNX=1)

    # P7-2: 复制 ONNX 1.18.1 DLL 到测试输出目录 (避免 System32 1.17.1 冲突)
    add_custom_command(TARGET LianCoreTests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ONNXRUNTIME_DIR}/lib/onnxruntime.dll"
            "$<TARGET_FILE_DIR:LianCoreTests>/onnxruntime.dll"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ONNXRUNTIME_DIR}/lib/onnxruntime_providers_shared.dll"
            "$<TARGET_FILE_DIR:LianCoreTests>/onnxruntime_providers_shared.dll"
        COMMENT "P7-2: 复制 ONNX Runtime 1.18.1 DLL 到测试目录"
    )
endif()

# 复音数编译定义 (P1-1)
target_compile_definitions(LianCoreTests PRIVATE LIANCORE_MAX_POLYPHONY=32)

# UTF-8 编码支持 (MSVC)
if(MSVC)
    target_compile_options(LianCoreTests PRIVATE /utf-8)
endif()

# 注册测试
include(Catch)
catch_discover_tests(LianCoreTests)

