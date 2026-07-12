# =============================================================================
# LianCore - 测试构建配置
# =============================================================================

# Catch2 测试框架
include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.5.0
)
FetchContent_MakeAvailable(Catch2)

# 收集所有测试源文件
file(GLOB TEST_SOURCES
    ${CMAKE_SOURCE_DIR}/tests/*.cpp
)

# 创建测试可执行文件
add_executable(LianCoreTests
    ${TEST_SOURCES}

    # 核心模块
    ${CMAKE_SOURCE_DIR}/src/core/AudioNode.cpp
    ${CMAKE_SOURCE_DIR}/src/core/AudioGraphEngine.cpp
    ${CMAKE_SOURCE_DIR}/src/core/NodeFactory.cpp

    # 合成引擎
    ${CMAKE_SOURCE_DIR}/src/synthesis/WavetableBank.cpp
    ${CMAKE_SOURCE_DIR}/src/synthesis/WavetableOscillator.cpp
    ${CMAKE_SOURCE_DIR}/src/synthesis/VirtualAnalogOscillator.cpp
    ${CMAKE_SOURCE_DIR}/src/synthesis/NoiseGenerator.cpp
    ${CMAKE_SOURCE_DIR}/src/synthesis/FilterProcessor.cpp

    # 调制系统
    ${CMAKE_SOURCE_DIR}/src/modulation/EnvelopeGenerator.cpp
    ${CMAKE_SOURCE_DIR}/src/modulation/LFOGenerator.cpp
    ${CMAKE_SOURCE_DIR}/src/modulation/ModulationMatrix.cpp

    # 参数系统
    ${CMAKE_SOURCE_DIR}/src/params/ParameterTree.cpp
    ${CMAKE_SOURCE_DIR}/src/params/PresetManager.cpp

    # AI推理层
    ${CMAKE_SOURCE_DIR}/src/ai/AIInferenceEngine.cpp
    ${CMAKE_SOURCE_DIR}/src/ai/TextTokenizer.cpp
    ${CMAKE_SOURCE_DIR}/src/ai/ParameterSpaceMapper.cpp
)

# 配置测试目标
target_include_directories(LianCoreTests PRIVATE
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/synthesis
    ${CMAKE_SOURCE_DIR}/src/modulation
    ${CMAKE_SOURCE_DIR}/src/params
    ${CMAKE_SOURCE_DIR}/src/ai
    ${CMAKE_SOURCE_DIR}/src/utils
)

target_link_libraries(LianCoreTests PRIVATE
    Catch2::Catch2WithMain
    juce_audio_basics
    juce_audio_formats
    juce_core
    juce_data_structures
    juce_dsp
    juce_events
)

# 注册测试
include(Catch)
catch_discover_tests(LianCoreTests)