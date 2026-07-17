# =============================================================================
# LianCore V3 - AAX SDK 查找模块
# 支持 AAX SDK 2.9.0+ (源码编译) 和旧版 (预编译库)
# AAX SDK 需 Avid 开发者账号 (https://developer.avid.com)
# 设置环境变量 AAX_SDK_DIR 指向 SDK 根目录
# =============================================================================

set(AAX_SDK_DIR "" CACHE PATH "AAX SDK installation directory")

# 自动检测 AAX SDK 路径
if(NOT AAX_SDK_DIR)
    if(DEFINED ENV{AAX_SDK_DIR})
        set(AAX_SDK_DIR "$ENV{AAX_SDK_DIR}")
    elseif(WIN32)
        if(EXISTS "${CMAKE_SOURCE_DIR}/aax-sdk-2-9-0")
            set(AAX_SDK_DIR "${CMAKE_SOURCE_DIR}/aax-sdk-2-9-0")
        elseif(EXISTS "F:/aax-sdk-2-9-0")
            set(AAX_SDK_DIR "F:/aax-sdk-2-9-0")
        elseif(EXISTS "C:/aax-sdk-2-9-0")
            set(AAX_SDK_DIR "C:/aax-sdk-2-9-0")
        endif()
    elseif(APPLE)
        if(EXISTS "${CMAKE_SOURCE_DIR}/aax-sdk-2-9-0")
            set(AAX_SDK_DIR "${CMAKE_SOURCE_DIR}/aax-sdk-2-9-0")
        endif()
    endif()
endif()

if(AAX_SDK_DIR AND EXISTS "${AAX_SDK_DIR}/Interfaces/AAX.h")
    message(STATUS "AAX SDK found at: ${AAX_SDK_DIR}")

    # 检测 SDK 版本（2.9.0+ 无预编译库，需要源码编译）
    if(EXISTS "${AAX_SDK_DIR}/Interfaces/AAX_Exports.cpp")
        set(AAX_SDK_VERSION "2.9.0+")
        set(AAX_SDK_SOURCE_BUILD TRUE)
        message(STATUS "AAX SDK version: ${AAX_SDK_VERSION} (source build)")
    else()
        set(AAX_SDK_VERSION "legacy")
        set(AAX_SDK_SOURCE_BUILD FALSE)
        message(STATUS "AAX SDK version: legacy (prebuilt library)")
    endif()

    # 头文件路径（包含 ACF 子目录，AAX_Init.h 需要 acfbasetypes.h）
    set(AAX_INCLUDE_DIRS
        "${AAX_SDK_DIR}/Interfaces"
        "${AAX_SDK_DIR}/Interfaces/ACF"
    )

    # 库文件配置
    if(AAX_SDK_SOURCE_BUILD)
        # SDK 2.9.0+: 源码编译 AAXExports
        set(AAX_EXPORTS_SOURCE "${AAX_SDK_DIR}/Interfaces/AAX_Exports.cpp")
        set(AAX_LIBRARIES "")  # 无预编译库，使用源码
        message(STATUS "AAX SDK will use source-built AAX_Exports.cpp")
    else()
        # 旧版 SDK: 使用预编译库
        if(WIN32)
            if(CMAKE_SIZEOF_VOID_P EQUAL 8)
                set(AAX_LIBRARY_DIR "${AAX_SDK_DIR}/Libraries/AAXLibrary/x64/Release")
            else()
                set(AAX_LIBRARY_DIR "${AAX_SDK_DIR}/Libraries/AAXLibrary/Win32/Release")
            endif()
            set(AAX_LIBRARIES "${AAX_LIBRARY_DIR}/AAXLibrary.lib")
        elseif(APPLE)
            set(AAX_LIBRARY_DIR "${AAX_SDK_DIR}/Libraries/AAXLibrary/MOS")
            set(AAX_LIBRARIES "${AAX_LIBRARY_DIR}/libAAXLibrary.a")
        endif()
    endif()

    # AAX 编译定义
    add_definitions(-DLIANCORE_HAS_AAX=1)
    add_definitions(-DAAX_PLUGIN_MANUFACTURER_CODE=Lian)
    add_definitions(-DAAX_PLUGIN_PRODUCT_CODE=Lcr1)
    set(LIANCORE_AAX_AVAILABLE TRUE)
    message(STATUS "AAX SDK configured successfully")
else()
    message(STATUS "AAX SDK not found. AAX build will be skipped.")
    message(STATUS "  Download: https://developer.avid.com")
    message(STATUS "  Set AAX_SDK_DIR env var or place in aax-sdk-2-9-0/")
    set(LIANCORE_AAX_AVAILABLE FALSE)
    set(AAX_INCLUDE_DIRS "")
    set(AAX_LIBRARIES "")
    set(AAX_EXPORTS_SOURCE "")
endif()