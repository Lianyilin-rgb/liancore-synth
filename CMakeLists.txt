# =============================================================================
# LianCore - 商业端侧AI合成器软音源 V3
# 根CMake构建文件
# =============================================================================
cmake_minimum_required(VERSION 3.24)
# macOS 需要 Objective-C++ 支持（JUCE 内部使用 AppKit/Foundation）
if(APPLE)
    project(LianCore VERSION 3.0.0 LANGUAGES C CXX OBJCXX)
else()
    project(LianCore VERSION 3.0.0 LANGUAGES C CXX)
endif()

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
elseif(APPLE)
    add_compile_options(-Wall -Wextra -Wpedantic -ffast-math)
    # JUCE 内部使用 juce::String 通过可变参数函数，clang 会报 non-pod-varargs 警告
    add_compile_options(-Wno-error=non-pod-varargs)
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
    # 优先使用环境变量 JUCE_DIR，否则自动检测
    if(DEFINED ENV{JUCE_DIR})
        set(JUCE_DIR "$ENV{JUCE_DIR}")
    else()
        set(JUCE_DIR "${CMAKE_SOURCE_DIR}/juce-8.0.14-osx/JUCE")
    endif()
    # 如果嵌套路径不存在，尝试直接路径 (unzip 到根目录)
    if(NOT EXISTS "${JUCE_DIR}/CMakeLists.txt")
        set(JUCE_DIR "${CMAKE_SOURCE_DIR}/JUCE")
    endif()
else()
    message(FATAL_ERROR "Unsupported platform for JUCE local path")
endif()

if(NOT EXISTS "${JUCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "JUCE 8.0.14 not found at ${JUCE_DIR}. Please download it first.")
endif()

add_subdirectory(${JUCE_DIR})

# =============================================================================
# ONNX Runtime 集成 (Beta阶段, 仅在 Windows 启用)
# =============================================================================
if(NOT APPLE)
    include(cmake/FindONNXRuntime.cmake)
else()
    set(ONNXRUNTIME_AVAILABLE FALSE)
endif()

# =============================================================================
# AAX SDK 集成 (Avid Pro Tools)
# =============================================================================
include(cmake/FindAAX.cmake)

# =============================================================================
# 插件配置
# =============================================================================
# 插件格式 (macOS CI 无 AAX SDK，排除 AAX；Windows 保留 AAX)
if(APPLE)
    set(LIANCORE_PLUGIN_FORMATS "VST3 AU")
else()
    set(LIANCORE_PLUGIN_FORMATS "VST3 AAX AU")
endif()

juce_add_plugin(LianCore
    # 插件元数据
    COMPANY_NAME "连毅霖"
    PLUGIN_MANUFACTURER_CODE Lian
    PLUGIN_CODE Lcr1
    PRODUCT_NAME "LianCore"
    BUNDLE_ID "com.liancore.audio.liancore"

    # 插件格式
    FORMATS ${LIANCORE_PLUGIN_FORMATS}
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
        # P1-2 新增效果器
        src/synthesis/Chorus.cpp
        src/synthesis/Flanger.cpp
        src/synthesis/Phaser.cpp
        src/synthesis/BitCrusher.cpp
        src/synthesis/RingMod.cpp
        src/synthesis/ConvolutionReverb.cpp
        # Beta 阶段新增合成引擎
        src/synthesis/SpectralOscillator.cpp
        src/synthesis/SpectralWarper.cpp
        src/synthesis/GranularPlayer.cpp
        src/synthesis/WaveguideResonator.cpp
        src/synthesis/MultiSampler.cpp
        src/synthesis/DrumSlicer.cpp
        src/synthesis/StepSequencer.cpp

        # 调制系统
        src/modulation/LFOGenerator.cpp
        src/modulation/EnvelopeGenerator.cpp
        src/modulation/ChaoticLFO.cpp
        src/modulation/ChaoticEnvelope.cpp
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
        src/ai/AITextToWavetable.cpp

        # UI 组件
        src/ui/WavetableEditor.cpp
        src/ui/MPERecordingUI.cpp
        src/ui/GranularEngineUI.cpp
        src/ui/GrainEnvelopeVisualizer.cpp
        src/ui/EffectsChainUI.cpp

        # 插件入口
        src/plugin/PluginProcessor.cpp
        src/plugin/PluginEditor.cpp
        src/plugin/MPEProcessor.cpp
        src/plugin/MPERecorder.cpp
        src/plugin/OversamplingProcessor.cpp

        # 微音程/调音支持 (P0-3)
        src/tuning/ScalaFileLoader.cpp
        src/tuning/MicrotuningManager.cpp

    # 头文件搜索路径
    HEADER_SEARCH_PATHS
        src/core
        src/synthesis
        src/modulation
        src/params
        src/ai
        src/plugin
        src/ui
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
        LIANCORE_MAX_POLYPHONY=32
)

# 注意: 所有源文件已经在上面的 juce_add_plugin(SOURCES) 中列出
# JUCE CMake 系统会自动处理编译顺序和依赖关系，无需重复添加

# ONNX Runtime 显式链接 (Generator Expression 在 juce_add_plugin 中可能不生效)
if(ONNXRUNTIME_AVAILABLE)
    target_link_libraries(LianCore PRIVATE onnxruntime)
endif()

# =============================================================================
# macOS: JUCE Plugin Target Diagnostics & Build Fix
# =============================================================================
if(APPLE)
    message(STATUS "=== JUCE Plugin Target Diagnostics ===")
    if(TARGET LianCore)
        message(STATUS "  LianCore: EXISTS")
    else()
        message(STATUS "  LianCore: MISSING")
    endif()
    if(TARGET LianCore_All)
        message(STATUS "  LianCore_All: EXISTS")
    else()
        message(STATUS "  LianCore_All: MISSING")
    endif()
    if(TARGET LianCore_VST3)
        message(STATUS "  LianCore_VST3: EXISTS")
    else()
        message(STATUS "  LianCore_VST3: MISSING")
    endif()
    if(TARGET LianCore_A)
        message(STATUS "  LianCore_A: EXISTS")
    else()
        message(STATUS "  LianCore_A: MISSING")
    endif()
    message(STATUS "=== End Plugin Target Diagnostics ===")

    # 强制 ALL_BUILD 依赖 LianCore_All 以确保所有插件格式被构建
    if(TARGET ALL_BUILD AND TARGET LianCore_All)
        add_dependencies(ALL_BUILD LianCore_All)
    endif()
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
    src/synthesis/Chorus.cpp
    src/synthesis/Flanger.cpp
    src/synthesis/Phaser.cpp
    src/synthesis/BitCrusher.cpp
    src/synthesis/RingMod.cpp
    src/synthesis/ConvolutionReverb.cpp
    src/synthesis/SpectralOscillator.cpp
    src/synthesis/SpectralWarper.cpp
    src/synthesis/GranularPlayer.cpp
    src/synthesis/WaveguideResonator.cpp
    src/synthesis/MultiSampler.cpp
    src/synthesis/DrumSlicer.cpp
    src/synthesis/StepSequencer.cpp

    # 调制系统
    src/modulation/LFOGenerator.cpp
    src/modulation/EnvelopeGenerator.cpp
    src/modulation/ChaoticLFO.cpp
    src/modulation/ChaoticEnvelope.cpp
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
    src/ai/AITextToWavetable.cpp

    # UI 组件
    src/ui/WavetableEditor.cpp
    src/ui/MPERecordingUI.cpp
    src/ui/GranularEngineUI.cpp
    src/ui/GrainEnvelopeVisualizer.cpp
    src/ui/EffectsChainUI.cpp

    # 插件入口
    src/plugin/PluginProcessor.cpp
    src/plugin/PluginEditor.cpp
    src/plugin/MPEProcessor.cpp
    src/plugin/MPERecorder.cpp
    src/plugin/OversamplingProcessor.cpp

    # 微音程/调音支持 (P0-3)
    src/tuning/ScalaFileLoader.cpp
    src/tuning/MicrotuningManager.cpp

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
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/plugin
    ${CMAKE_CURRENT_SOURCE_DIR}/src/tuning
    ${CMAKE_CURRENT_SOURCE_DIR}/src/security
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils
    # JUCE 模块头文件路径 (juce_cryptography/juce_dsp/juce_audio_formats 等额外模块)
    ${JUCE_DIR}/modules
)

# macOS 需要将 JUCE 额外模块编译为 Objective-C++ (Foundation/Security 框架依赖)
if(APPLE)
    set_source_files_properties(
        ${JUCE_DIR}/modules/juce_audio_formats/juce_audio_formats.cpp
        ${JUCE_DIR}/modules/juce_cryptography/juce_cryptography.cpp
        ${JUCE_DIR}/modules/juce_dsp/juce_dsp.cpp
        PROPERTIES LANGUAGE OBJCXX
    )

    # 添加 VST3/AU 插件包装器源文件到 LianCore MODULE 库
    # JUCE 8.0.14 的 juce_add_plugin 在 macOS 下不自动创建插件格式目标
    # 手动将包装器源文件加入 target_sources 以确保 MODULE 库包含入口点
    set(JUCE_PLUGIN_CLIENT_DIR ${JUCE_DIR}/modules/juce_audio_plugin_client)

    # 定义插件格式宏 (juce_CheckSettingMacros.h 需要)
    # 这些宏通常由 JUCE 的 CMake 函数自动定义，但 direct target_sources 需要手动设置
    target_compile_definitions(LianCore PRIVATE
        JUCE_PLUGINBUILD=1
        JucePlugin_Build_VST=1
        JucePlugin_Build_VST3=1
        JucePlugin_Build_AU=1
        JucePlugin_Build_AUv3=0
        JucePlugin_Build_AAX=0
        JucePlugin_Build_Standalone=0
        JucePlugin_Build_Unity=0
        JucePlugin_Enable_IAA=0
    )

    target_sources(LianCore PRIVATE
        ${JUCE_PLUGIN_CLIENT_DIR}/juce_audio_plugin_client_VST3.cpp
        ${JUCE_PLUGIN_CLIENT_DIR}/juce_audio_plugin_client_VST3.mm
        ${JUCE_PLUGIN_CLIENT_DIR}/juce_audio_plugin_client_AU_1.mm
        ${JUCE_PLUGIN_CLIENT_DIR}/juce_audio_plugin_client_AU_2.mm
    )

    # VST3 .cpp 文件包含 Objective-C 头文件，需要作为 OBJCXX 编译
    set_source_files_properties(
        ${JUCE_PLUGIN_CLIENT_DIR}/juce_audio_plugin_client_VST3.cpp
        ${JUCE_PLUGIN_CLIENT_DIR}/juce_audio_plugin_client_VST3.mm
        ${JUCE_PLUGIN_CLIENT_DIR}/juce_audio_plugin_client_AU_1.mm
        ${JUCE_PLUGIN_CLIENT_DIR}/juce_audio_plugin_client_AU_2.mm
        PROPERTIES LANGUAGE OBJCXX
    )

    # 添加插件包装器需要的头文件搜索路径
    # AU 包装器需要 AU/AudioUnitSDK 路径 (include 为 "AudioUnitSDK/xxx.h")
    # VST3 包装器需要 VST3 路径
    target_include_directories(LianCore PRIVATE
        ${JUCE_PLUGIN_CLIENT_DIR}
        ${JUCE_PLUGIN_CLIENT_DIR}/AU
        ${JUCE_PLUGIN_CLIENT_DIR}/VST3
    )

    message(STATUS "  [LianCore] Added VST3/AU wrapper sources + compile definitions for macOS plugin bundle")
endif()

# ONNX Runtime 条件编译定义
if(ONNXRUNTIME_AVAILABLE)
    target_compile_definitions(LianCore PRIVATE LIANCORE_HAS_ONNX=1)
    target_include_directories(LianCore PRIVATE ${ONNXRUNTIME_DIR}/include)
endif()

# AAX SDK 条件编译
if(LIANCORE_AAX_AVAILABLE)
    target_include_directories(LianCore PRIVATE ${AAX_INCLUDE_DIRS})
    target_compile_definitions(LianCore PRIVATE LIANCORE_HAS_AAX=1)
    # AAX SDK 2.9.0+: 源码编译 AAX_Exports.cpp
    if(AAX_EXPORTS_SOURCE)
        target_sources(LianCore PRIVATE "${AAX_EXPORTS_SOURCE}")
    endif()
    # 旧版 SDK: 链接预编译库
    if(AAX_LIBRARIES)
        target_link_libraries(LianCore PRIVATE ${AAX_LIBRARIES})
    endif()
    message(STATUS "AAX build enabled")
endif()

# =============================================================================
# 安装规则
# =============================================================================
install(TARGETS LianCore
    BUNDLE DESTINATION .
    RUNTIME DESTINATION "${CMAKE_INSTALL_PREFIX}/VST3"
)

# =============================================================================
# Windows: POST_BUILD 复制 Web UI 资源到 VST3 Bundle
# 将 ui/ 目录下的所有文件复制到 VST3 bundle 的 Contents/Resources/ui/
# 确保插件在 DAW 中加载时能访问 Web UI 界面资源
# 使用 if(WIN32) 隔离，不影响 macOS 构建
# =============================================================================
if(WIN32)
    add_custom_command(TARGET LianCore POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:LianCore>/../Resources/ui"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/ui"
            "$<TARGET_FILE_DIR:LianCore>/../Resources/ui"
        COMMENT "Copying Web UI resources to VST3 bundle"
    )
endif()