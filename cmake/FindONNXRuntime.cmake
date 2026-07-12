# =============================================================================
# LianCore - ONNX Runtime 下载与集成模块
# Beta阶段: 自动下载ONNX Runtime预编译库
# =============================================================================

# ONNX Runtime版本
set(ONNXRUNTIME_VERSION "1.18.1")

if(WIN32)
    set(ONNXRUNTIME_URL "https://github.com/microsoft/onnxruntime/releases/download/v${ONNXRUNTIME_VERSION}/onnxruntime-win-x64-${ONNXRUNTIME_VERSION}.zip")
    set(ONNXRUNTIME_ARCHIVE "onnxruntime-win-x64.zip")
elseif(APPLE)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
        set(ONNXRUNTIME_URL "https://github.com/microsoft/onnxruntime/releases/download/v${ONNXRUNTIME_VERSION}/onnxruntime-osx-arm64-${ONNXRUNTIME_VERSION}.tgz")
    else()
        set(ONNXRUNTIME_URL "https://github.com/microsoft/onnxruntime/releases/download/v${ONNXRUNTIME_VERSION}/onnxruntime-osx-x64-${ONNXRUNTIME_VERSION}.tgz")
    endif()
    set(ONNXRUNTIME_ARCHIVE "onnxruntime-osx.tgz")
else()
    set(ONNXRUNTIME_URL "https://github.com/microsoft/onnxruntime/releases/download/v${ONNXRUNTIME_VERSION}/onnxruntime-linux-x64-${ONNXRUNTIME_VERSION}.tgz")
    set(ONNXRUNTIME_ARCHIVE "onnxruntime-linux.tgz")
endif()

set(ONNXRUNTIME_DIR "${CMAKE_BINARY_DIR}/onnxruntime")

# 检查ONNX Runtime是否已下载
if(NOT EXISTS "${ONNXRUNTIME_DIR}/include/onnxruntime_cxx_api.h")
    message(STATUS "ONNX Runtime not found, downloading v${ONNXRUNTIME_VERSION}...")

    # 使用file(DOWNLOAD)下载
    set(ONNXRUNTIME_ARCHIVE_PATH "${CMAKE_BINARY_DIR}/${ONNXRUNTIME_ARCHIVE}")

    file(DOWNLOAD
        "${ONNXRUNTIME_URL}"
        "${ONNXRUNTIME_ARCHIVE_PATH}"
        STATUS ONNXRUNTIME_DOWNLOAD_STATUS
        SHOW_PROGRESS
    )

    list(GET ONNXRUNTIME_DOWNLOAD_STATUS 0 ONNXRUNTIME_DOWNLOAD_CODE)

    if(ONNXRUNTIME_DOWNLOAD_CODE EQUAL 0)
        message(STATUS "ONNX Runtime downloaded successfully, extracting...")

        # 解压
        file(MAKE_DIRECTORY "${ONNXRUNTIME_DIR}")

        execute_process(
            COMMAND ${CMAKE_COMMAND} -E tar xzf "${ONNXRUNTIME_ARCHIVE_PATH}"
            WORKING_DIRECTORY "${ONNXRUNTIME_DIR}"
            RESULT_VARIABLE ONNXRUNTIME_EXTRACT_RESULT
        )

        if(NOT ONNXRUNTIME_EXTRACT_RESULT EQUAL 0)
            # 尝试zip解压
            execute_process(
                COMMAND ${CMAKE_COMMAND} -E tar xf "${ONNXRUNTIME_ARCHIVE_PATH}"
                WORKING_DIRECTORY "${ONNXRUNTIME_DIR}"
            )
        endif()

        # 查找提取后的实际目录
        file(GLOB ONNXRUNTIME_SUBDIRS LIST_DIRECTORIES true "${ONNXRUNTIME_DIR}/*")
        foreach(SUBDIR ${ONNXRUNTIME_SUBDIRS})
            if(EXISTS "${SUBDIR}/include/onnxruntime_cxx_api.h")
                set(ONNXRUNTIME_EXTRACTED_DIR "${SUBDIR}")
                break()
            endif()
        endforeach()

        if(ONNXRUNTIME_EXTRACTED_DIR)
            message(STATUS "ONNX Runtime extracted to: ${ONNXRUNTIME_EXTRACTED_DIR}")
            set(ONNXRUNTIME_DIR "${ONNXRUNTIME_EXTRACTED_DIR}")
        endif()
    else()
        message(WARNING "ONNX Runtime download failed (code: ${ONNXRUNTIME_DOWNLOAD_CODE}). AI features will be limited.")
        set(ONNXRUNTIME_AVAILABLE FALSE)
    endif()
endif()

# 验证ONNX Runtime可用性
if(EXISTS "${ONNXRUNTIME_DIR}/include/onnxruntime_cxx_api.h")
    set(ONNXRUNTIME_AVAILABLE TRUE)
    message(STATUS "ONNX Runtime found at: ${ONNXRUNTIME_DIR}")

    # 创建导入库目标
    add_library(onnxruntime SHARED IMPORTED)

    if(WIN32)
        set_target_properties(onnxruntime PROPERTIES
            IMPORTED_LOCATION "${ONNXRUNTIME_DIR}/lib/onnxruntime.dll"
            IMPORTED_IMPLIB "${ONNXRUNTIME_DIR}/lib/onnxruntime.lib"
            INTERFACE_INCLUDE_DIRECTORIES "${ONNXRUNTIME_DIR}/include"
        )
    elseif(APPLE)
        set_target_properties(onnxruntime PROPERTIES
            IMPORTED_LOCATION "${ONNXRUNTIME_DIR}/lib/libonnxruntime.dylib"
            INTERFACE_INCLUDE_DIRECTORIES "${ONNXRUNTIME_DIR}/include"
        )
    else()
        set_target_properties(onnxruntime PROPERTIES
            IMPORTED_LOCATION "${ONNXRUNTIME_DIR}/lib/libonnxruntime.so"
            INTERFACE_INCLUDE_DIRECTORIES "${ONNXRUNTIME_DIR}/include"
        )
    endif()
else()
    set(ONNXRUNTIME_AVAILABLE FALSE)
    message(WARNING "ONNX Runtime not available. AI features will use rule-based fallback.")
endif()