cmake_minimum_required(VERSION 3.16)

if (POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif()

project(LocalFileShare
    VERSION 0.1.0
    DESCRIPTION "A lightweight C++ local network file sharing tool"
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

set(ONNXRUNTIME_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/onnxruntime/onnxruntime-win-x64-gpu-1.23.2" CACHE PATH "ONNX Runtime GPU SDK root")
set(LFS_ENABLE_ONNXRUNTIME OFF)
if (EXISTS "${ONNXRUNTIME_ROOT}/include/onnxruntime_cxx_api.h" AND EXISTS "${ONNXRUNTIME_ROOT}/lib/onnxruntime.lib")
    set(LFS_ENABLE_ONNXRUNTIME ON)
endif()

FetchContent_Declare(
    sqlite3
    URL https://sqlite.org/2026/sqlite-amalgamation-3530100.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_GetProperties(sqlite3)
if (NOT sqlite3_POPULATED)
    FetchContent_Populate(sqlite3)
    add_library(sqlite3 STATIC ${sqlite3_SOURCE_DIR}/sqlite3.c)
    target_include_directories(sqlite3 PUBLIC ${sqlite3_SOURCE_DIR})
endif()

add_executable(LocalFileShare
    src/main.cpp
    src/AppOptions.cpp
    src/FileManager.cpp
    src/HtmlRenderer.cpp
    src/PerceptualHash.cpp
    src/PhotoImage.cpp
    src/PhotoQualityScorer.cpp
    src/PhotoTagger.cpp
    src/PhotoService.cpp
    src/Server.cpp
    src/SQLitePhotoRepository.cpp
    src/TidyEngine.cpp
    third_party/qrcodegen.cpp
)

target_include_directories(LocalFileShare PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

target_link_libraries(LocalFileShare PRIVATE sqlite3 gdiplus)

if (LFS_ENABLE_ONNXRUNTIME)
    target_compile_definitions(LocalFileShare PRIVATE LFS_ENABLE_ONNXRUNTIME)
    target_include_directories(LocalFileShare PRIVATE "${ONNXRUNTIME_ROOT}/include")
    target_link_directories(LocalFileShare PRIVATE "${ONNXRUNTIME_ROOT}/lib")
    target_link_libraries(LocalFileShare PRIVATE
        onnxruntime
        onnxruntime_providers_cuda
        onnxruntime_providers_shared
        gdiplus
    )
    add_custom_command(TARGET LocalFileShare POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ONNXRUNTIME_ROOT}/lib/onnxruntime.dll"
            "${ONNXRUNTIME_ROOT}/lib/onnxruntime_providers_cuda.dll"
            "${ONNXRUNTIME_ROOT}/lib/onnxruntime_providers_shared.dll"
            "$<TARGET_FILE_DIR:LocalFileShare>"
    )
endif()

enable_testing()

add_executable(PhotoRepositoryTests
    tests/PhotoRepositoryTests.cpp
    src/SQLitePhotoRepository.cpp
)

target_include_directories(PhotoRepositoryTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(PhotoRepositoryTests PRIVATE sqlite3)

add_test(NAME PhotoRepositoryTests COMMAND PhotoRepositoryTests)

add_executable(PhotoServiceTests
    tests/PhotoServiceTests.cpp
    src/PhotoService.cpp
    src/PhotoTagger.cpp
    src/SQLitePhotoRepository.cpp
    src/FileManager.cpp
    src/HtmlRenderer.cpp
    src/PerceptualHash.cpp
    src/PhotoImage.cpp
    src/PhotoQualityScorer.cpp
    third_party/qrcodegen.cpp
)

target_include_directories(PhotoServiceTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

target_link_libraries(PhotoServiceTests PRIVATE sqlite3 gdiplus)

add_test(NAME PhotoServiceTests COMMAND PhotoServiceTests)

add_executable(TidyEngineTests
    tests/TidyEngineTests.cpp
    src/TidyEngine.cpp
    src/SQLitePhotoRepository.cpp
    src/PerceptualHash.cpp
    src/PhotoImage.cpp
    src/PhotoQualityScorer.cpp
)

target_include_directories(TidyEngineTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(TidyEngineTests PRIVATE sqlite3 gdiplus)

add_test(NAME TidyEngineTests COMMAND TidyEngineTests)

add_executable(AppOptionsTests
    tests/AppOptionsTests.cpp
    src/AppOptions.cpp
    src/FileManager.cpp
    src/HtmlRenderer.cpp
    third_party/qrcodegen.cpp
)

target_include_directories(AppOptionsTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

add_test(NAME AppOptionsTests COMMAND AppOptionsTests)

if (WIN32)
    target_link_libraries(LocalFileShare PRIVATE ws2_32 iphlpapi bcrypt)
endif()

if (MSVC)
    target_compile_options(LocalFileShare PRIVATE /utf-8)
endif()
