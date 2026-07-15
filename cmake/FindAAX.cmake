# =============================================================================
# LianCore V3 - AAX SDK 查找模块
# AAX SDK 需要 Avid 开发者账号 (https://developer.avid.com)
# 下载后将 AAX SDK 解压到项目根目录或设置 AAX_SDK_DIR 环境变量
# =============================================================================

set(AAX_SDK_DIR "" CACHE PATH "AAX SDK installation directory")

if(NOT AAX_SDK_DIR)
    if(DEFINED ENV{AAX_SDK_DIR})
        set(AAX_SDK_DIR "$ENV{AAX_SDK_DIR}")
    elseif(WIN32)
        if(EXISTS "${CMAKE_SOURCE_DIR}/aax-sdk")
            set(AAX_SDK_DIR "${CMAKE_SOURCE_DIR}/aax-sdk")
        elseif(EXISTS "C:/SDK/AAX")
            set(AAX_SDK_DIR "C:/SDK/AAX")
        endif()
    elseif(APPLE)
        if(EXISTS "${CMAKE_SOURCE_DIR}/aax-sdk")
            set(AAX_SDK_DIR "${CMAKE_SOURCE_DIR}/aax-sdk")
        elseif(EXISTS "/Library/Developer/AAX")
            set(AAX_SDK_DIR "/Library/Developer/AAX")
        endif()
    endif()
endif()

if(AAX_SDK_DIR AND EXISTS "${AAX_SDK_DIR}")
    message(STATUS "AAX SDK found at: ${AAX_SDK_DIR}")
    set(AAX_INCLUDE_DIRS
        "${AAX_SDK_DIR}/Interfaces"
        "${AAX_SDK_DIR}/Libraries/AAXLibrary"
    )
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
    add_definitions(-DLIANCORE_HAS_AAX=1)
    add_definitions(-AAX_PLUGIN_MANUFACTURER_CODE=Lian)
    add_definitions(-AAX_PLUGIN_PRODUCT_CODE=Lcr1)
    set(LIANCORE_AAX_AVAILABLE TRUE)
    message(STATUS "AAX SDK configured successfully")
else()
    message(STATUS "AAX SDK not found. AAX build will be skipped.")
    message(STATUS "  Download: https://developer.avid.com")
    message(STATUS "  Place in aax-sdk/ or set AAX_SDK_DIR env var")
    set(LIANCORE_AAX_AVAILABLE FALSE)
endif()