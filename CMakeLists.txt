cmake_minimum_required(VERSION 3.16)
project(4kscaler LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SIMPLENES_DIR ${CMAKE_SOURCE_DIR}/third_party/SimpleNES)
set(NIS_DIR ${CMAKE_SOURCE_DIR}/third_party/NIS/NIS)

file(GLOB SIMPLENES_SOURCES
    ${SIMPLENES_DIR}/src/*.cpp
    ${SIMPLENES_DIR}/src/APU/*.cpp
)
# Host owns window/input; skip SFML-era entrypoints.
list(FILTER SIMPLENES_SOURCES EXCLUDE REGEX ".*/Emulator\\.cpp$")
list(FILTER SIMPLENES_SOURCES EXCLUDE REGEX ".*/KeybindingsParser\\.cpp$")

add_library(simplenes STATIC
    ${SIMPLENES_SOURCES}
    ${SIMPLENES_DIR}/vendor/miniaudio/miniaudio.c
)
target_include_directories(simplenes PUBLIC
    ${SIMPLENES_DIR}/include
    ${SIMPLENES_DIR}/vendor/miniaudio
)
target_compile_definitions(simplenes PRIVATE MA_NO_DECODING MA_NO_ENCODING)
if(MSVC)
    target_compile_options(simplenes PRIVATE /W3)
else()
    target_compile_options(simplenes PRIVATE -Wall -Wno-unused-parameter)
    # miniaudio is C
    set_source_files_properties(${SIMPLENES_DIR}/vendor/miniaudio/miniaudio.c PROPERTIES LANGUAGE C)
endif()

add_executable(4kscaler
    src/main.cpp
    src/nes_host.cpp
    src/d3d11_renderer.cpp
    src/nis_upscaler.cpp
    src/tiny_sr_upscaler.cpp
    src/input.cpp
    src/prefs.cpp
    src/launcher.cpp
)
target_include_directories(4kscaler PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${NIS_DIR}
)
target_link_libraries(4kscaler PRIVATE simplenes d3d11 dxgi d3dcompiler ole32 shell32 uuid winmm comdlg32 msimg32)
if(MINGW)
    target_link_options(4kscaler PRIVATE -static-libgcc -static-libstdc++)
endif()

# Copy shaders next to the binary for runtime compile.
add_custom_command(TARGET 4kscaler POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:4kscaler>/NIS
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:4kscaler>/shaders
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${NIS_DIR}/NIS_Main.hlsl
        ${NIS_DIR}/NIS_Scaler.h
        ${NIS_DIR}/NIS_Config.h
        $<TARGET_FILE_DIR:4kscaler>/NIS
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/shaders/tiny_sr.hlsl
        $<TARGET_FILE_DIR:4kscaler>/shaders/tiny_sr.hlsl
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/shaders/tinysr_weights.bin
        $<TARGET_FILE_DIR:4kscaler>/shaders/tinysr_weights.bin
)

install(TARGETS 4kscaler RUNTIME DESTINATION bin)
