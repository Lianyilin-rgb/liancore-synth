# =============================================================================
# LianCore - 商业端侧AI合成器软音源 V3
# 根CMake构建文件
# =============================================================================
cmake_minimum_required(VERSION 3.24)
project(LianCore VERSION 3.0.0 LANGUAGES CXX)

# C++20 标准要求
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# =============================================================================
# 全局编译选项
# =============================================================================
if(MSVC)
    add_compile_options(/W4 /MP /arch:AVX2 /fp:fast)
    add_definitions(-DNOMINMAX -DWIN32_LEAN_AND_MEAN)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -march=native -ffast-math)
endif()

# =============================================================================
# JUCE 框架配置 (本地预下载版本, 不再使用 FetchContent)
# =============================================================================
set(JUCE_VERSION "8.0.14")

if(WIN32)
    set(JUCE_DIR "${CMAKE_SOURCE_DIR}/juce-8.0.14-windows/JUCE")
elseif(APPLE)
    set(JUCE_DIR "${CMAKE_SOURCE_DIR}/juce-8.0.14-osx/JUCE")
else()
    message(FATAL_ERROR "Unsupported platform for JUCE local path")
endif()

if(NOT EXISTS "${JUCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "JUCE 8.0.14 not found at ${JUCE_DIR}. Please download it first.")
endif()

add_subdirectory(${JUCE_DIR})

# =============================================================================
# ONNX Runtime 集成 (Beta阶段)
# =============================================================================
include(cmake/FindONNXRuntime.cmake)

# =============================================================================
# 插件配置
# =============================================================================
juce_add_plugin(LianCore
    # 插件元数据
    COMPANY_NAME "LianCore Audio"
    PLUGIN_MANUFACTURER_CODE Lian
    PLUGIN_CODE Lcr1
    PRODUCT_NAME "LianCore"

    # 插件格式
    FORMATS VST3 AAX AU
    VST3_CATEGORIES "Instrument|Synth"

    # 是否需要UI
    NEEDS_WEB_BROWSER FALSE

    # 源文件
    SOURCES
        # 核心音频图引擎
        src/core/AudioNode.cpp
        src/core/AudioGraphEngine.cpp
        src/core/NodeFactory.cpp

        # 合成引擎
        src/synthesis/WavetableBank.cpp
        src/synthesis/WavetableOscillator.cpp
        src/synthesis/VirtualAnalogOscillator.cpp
        src/synthesis/NoiseGenerator.cpp
        src/synthesis/FilterProcessor.cpp
        # Beta Week 2 效果器链
        src/synthesis/Distortion.cpp
        src/synthesis/Delay.cpp
        src/synthesis/Reverb.cpp
        src/synthesis/Compressor.cpp
        src/synthesis/EQ.cpp
        # Beta 阶段新增合成引擎
        src/synthesis/SpectralOscillator.cpp
        src/synthesis/GranularPlayer.cpp
        src/synthesis/WaveguideResonator.cpp
        src/synthesis/MultiSampler.cpp
        src/synthesis/DrumSlicer.cpp
        src/synthesis/StepSequencer.cpp

        # 调制系统
        src/modulation/EnvelopeGenerator.cpp
        src/modulation/LFOGenerator.cpp
        src/modulation/ModulationMatrix.cpp

        # 参数系统
        src/params/ParameterTree.cpp
        src/params/PresetManager.cpp
        src/params/ParameterMapping.cpp
        src/params/PreparedStatement.cpp

        # 安全模块
        src/security/AES256Encryptor.cpp

        # AI推理层
        src/ai/AIInferenceEngine.cpp
        src/ai/TextTokenizer.cpp
        src/ai/ParameterSpaceMapper.cpp
        src/ai/AIModelTrainer.cpp
        src/ai/EmotionToParameterMapper.cpp

        # 插件入口
        src/plugin/PluginProcessor.cpp
        src/plugin/PluginEditor.cpp

    # 头文件搜索路径
    HEADER_SEARCH_PATHS
        src/core
        src/synthesis
        src/modulation
        src/params
        src/ai
        src/plugin
        src/security
        src/utils

    # 链接库
    LINK_LIBRARIES
        # ONNX Runtime (Beta阶段集成)
        $<$<BOOL:${ONNXRUNTIME_AVAILABLE}>:onnxruntime>

    # 编译器定义
    COMPILE_DEFINITIONS
        LIANCORE_VERSION=\"${PROJECT_VERSION}\"
        LIANCORE_BETA=1
        LIANCORE_MAX_WAVETABLE_FRAMES=256
        LIANCORE_WAVETABLE_FRAME_SIZE=2048
        LIANCORE_MAX_MODULATION_ROUTES=32
        LIANCORE_MAX_UNDO_STEPS=50
        LIANCORE_MAX_POLYPHONY=16
)

# =============================================================================
# 测试目标
# =============================================================================
option(LIANCORE_BUILD_TESTS "Build unit tests" ON)
if(LIANCORE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# =============================================================================
# CPack 打包配置 (NSIS安装包)
# =============================================================================
if(WIN32)
    set(CPACK_GENERATOR "NSIS")
    set(CPACK_NSIS_DISPLAY_NAME "LianCore VST3 Synthesizer")
    set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES64")
    set(CPACK_NSIS_CREATE_ICONS_EXTRA
        "CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\LianCore.lnk' '$INSTDIR\\\\LianCore.vst3'")
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "Common Files\\\\VST3")
    set(CPACK_NSIS_INSTALLED_ICON_NAME "LianCore.vst3")
    set(CPACK_NSIS_HELP_LINK "https://liancore.audio")
    set(CPACK_NSIS_URL_INFO_ABOUT "https://liancore.audio")
    set(CPACK_NSIS_CONTACT "support@liancore.audio")
    set(CPACK_NSIS_MENU_LINKS
        "https://liancore.audio" "LianCore Homepage"
    )
    include(CPack)
endif()

# =============================================================================
# 安装规则
# =============================================================================
install(TARGETS LianCore
    BUNDLE DESTINATION .
    RUNTIME DESTINATION "${CMAKE_INSTALL_PREFIX}/VST3"
)