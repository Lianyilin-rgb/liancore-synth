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
    add_compile_options(/W4 /MP /arch:AVX2 /fp:fast /utf-8)
    add_definitions(-DNOMINMAX -DWIN32_LEAN_AND_MEAN)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -march=native -ffast-math)
endif()

# LianCore 无 VST2 版本，禁止 VST2/VST3 参数ID混淆
add_definitions(-DJUCE_VST3_CAN_REPLACE_VST2=0)

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
    BUNDLE_ID "com.liancore.audio.liancore"

    # 插件格式
    FORMATS VST3 AAX AU
    VST3_CATEGORIES "Instrument|Synth"

    # 是否需要UI
    NEEDS_WEB_BROWSER FALSE

    # 禁用二进制资源编译器 (无BinaryData, 避免juceaide崩溃)
    NEEDS_BINARY_BUILDER FALSE

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
        src/ai/TransformerTextEncoder.cpp
        src/ai/ParameterSpaceMapper.cpp
        src/ai/AIModelTrainer.cpp
        src/ai/EmotionToParameterMapper.cpp
        src/ai/OnnxModelExporter.cpp
        src/ai/AudioTimbreAnalyzer.cpp
        src/ai/WavetableGenerator.cpp

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

# 注意: 所有源文件已经在上面的 juce_add_plugin(SOURCES) 中列出
# JUCE CMake 系统会自动处理编译顺序和依赖关系，无需重复添加

# ONNX Runtime 显式链接 (Generator Expression 在 juce_add_plugin 中可能不生效)
if(ONNXRUNTIME_AVAILABLE)
    target_link_libraries(LianCore PRIVATE onnxruntime)
endif()

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
# 生成 JuceHeader.h (手动调用 - JUCE 不会为 juce_add_plugin 自动生成)
# =============================================================================
juce_generate_juce_header(LianCore)

# =============================================================================
# 手动添加用户源文件到编译 (JUCE 8 juce_add_plugin 的 SOURCES 参数会被忽略)
# =============================================================================
target_sources(LianCore PRIVATE
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
    src/synthesis/Distortion.cpp
    src/synthesis/Delay.cpp
    src/synthesis/Reverb.cpp
    src/synthesis/Compressor.cpp
    src/synthesis/EQ.cpp
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
    src/params/sqlite3.c

    # 安全模块
    src/security/AES256Encryptor.cpp

    # AI推理层
    src/ai/AIInferenceEngine.cpp
    src/ai/TextTokenizer.cpp
    src/ai/TransformerTextEncoder.cpp
    src/ai/ParameterSpaceMapper.cpp
    src/ai/AIModelTrainer.cpp
    src/ai/EmotionToParameterMapper.cpp
    src/ai/OnnxModelExporter.cpp
    src/ai/AudioTimbreAnalyzer.cpp
    src/ai/WavetableGenerator.cpp

    # 插件入口
    src/plugin/PluginProcessor.cpp
    src/plugin/PluginEditor.cpp

    # JUCE 额外模块 (未包含在 juce_add_plugin 默认模块集中)
    ${JUCE_DIR}/modules/juce_audio_formats/juce_audio_formats.cpp
    ${JUCE_DIR}/modules/juce_dsp/juce_dsp.cpp
    ${JUCE_DIR}/modules/juce_cryptography/juce_cryptography.cpp
)

# 添加用户源文件头文件搜索路径
target_include_directories(LianCore PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/synthesis
    ${CMAKE_CURRENT_SOURCE_DIR}/src/modulation
    ${CMAKE_CURRENT_SOURCE_DIR}/src/params
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ai
    ${CMAKE_CURRENT_SOURCE_DIR}/src/plugin
    ${CMAKE_CURRENT_SOURCE_DIR}/src/security
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils
    # JUCE 模块头文件路径 (juce_cryptography/juce_dsp/juce_audio_formats 等额外模块)
    ${JUCE_DIR}/modules
)

# ONNX Runtime 条件编译定义
if(ONNXRUNTIME_AVAILABLE)
    target_compile_definitions(LianCore PRIVATE LIANCORE_HAS_ONNX=1)
    target_include_directories(LianCore PRIVATE ${ONNXRUNTIME_DIR}/include)
endif()

# =============================================================================
# 安装规则
# =============================================================================
install(TARGETS LianCore
    BUNDLE DESTINATION .
    RUNTIME DESTINATION "${CMAKE_INSTALL_PREFIX}/VST3"
)