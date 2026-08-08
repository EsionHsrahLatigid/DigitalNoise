cmake_minimum_required(VERSION 3.31)

if(APPLE)
    project(DigitalNoise VERSION 0.1.0 LANGUAGES C CXX OBJC OBJCXX)
else()
    project(DigitalNoise VERSION 0.1.0 LANGUAGES C CXX)
endif()

option(DIGITALNOISE_BUILD_PLUGIN "Build the YUP Standalone/VST3/AU targets" ON)
option(DIGITALNOISE_BUILD_TESTS "Build deterministic DSP regression tests" ON)
option(DIGITALNOISE_ADHOC_SIGN "Ad-hoc sign local macOS plugin and app bundles after building" ON)
set(DIGITALNOISE_YUP_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../yup" CACHE PATH
    "Path to a local YUP checkout; fetched from upstream when unavailable")

add_library(digitalnoise_engine STATIC
    source/DigitalNoiseEngine.cpp)
target_include_directories(digitalnoise_engine PUBLIC
    "${CMAKE_CURRENT_LIST_DIR}/include")
target_compile_features(digitalnoise_engine PUBLIC cxx_std_20)
set_target_properties(digitalnoise_engine PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(digitalnoise_engine PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(DIGITALNOISE_BUILD_TESTS)
    include(CTest)

    function(enable_test_assertions target_name)
        if(MSVC)
            target_compile_options(${target_name} PRIVATE /UNDEBUG)
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
            target_compile_options(${target_name} PRIVATE -UNDEBUG)
        endif()
    endfunction()

    add_executable(digitalnoise_engine_tests
        tests/DigitalNoiseEngineTests.cpp)
    target_link_libraries(digitalnoise_engine_tests PRIVATE digitalnoise_engine)
    target_compile_features(digitalnoise_engine_tests PRIVATE cxx_std_20)
    enable_test_assertions(digitalnoise_engine_tests)
    add_test(NAME digitalnoise_engine_tests COMMAND digitalnoise_engine_tests)

endif()

if(DIGITALNOISE_BUILD_PLUGIN)
    set(YUP_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(YUP_BUILD_PLUGINS OFF CACHE BOOL "" FORCE)
    set(YUP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(YUP_ENABLE_PLUGINVAL OFF CACHE BOOL "" FORCE)
    set(YUP_ENABLE_VST3_VALIDATOR OFF CACHE BOOL "" FORCE)
    set(YUP_ENABLE_CLAP_VALIDATOR OFF CACHE BOOL "" FORCE)
    set(YUP_ENABLE_AUVAL_VALIDATOR OFF CACHE BOOL "" FORCE)

    if(EXISTS "${DIGITALNOISE_YUP_SOURCE_DIR}/CMakeLists.txt")
        add_subdirectory("${DIGITALNOISE_YUP_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/yup")
    else()
        include(FetchContent)
        FetchContent_Declare(yup
            GIT_REPOSITORY https://github.com/kunitoki/yup.git
            GIT_TAG 9a1c9bc699b6a714f6f52486462d98a140c8bf95)
        FetchContent_MakeAvailable(yup)
    endif()

    # YUP's iconutil-based helper rejects its generated iconset on current
    # macOS. Keep the compatibility fix project-local until upstream changes.
    if(APPLE)
        include(cmake/YupMacOSIconWorkaround.cmake)
    endif()

    yup_audio_plugin(
        TARGET_NAME digitalnoise
        TARGET_VERSION ${PROJECT_VERSION}
        TARGET_IDE_GROUP "DigitalNoise"
        TARGET_APP_ID "audio.2bit.digitalnoise"
        TARGET_APP_NAMESPACE "audio.2bit"
        TARGET_CXX_STANDARD 20
        PLUGIN_ID "audio.2bit.DigitalNoise"
        PLUGIN_NAME "DigitalNoise"
        PLUGIN_VENDOR "2bit"
        PLUGIN_EMAIL ""
        PLUGIN_VERSION "${PROJECT_VERSION}"
        PLUGIN_DESCRIPTION "Algorithmic digital noise instrument"
        PLUGIN_URL "https://github.com/2bbb/DigitalNoise"
        PLUGIN_IS_SYNTH ON
        PLUGIN_IS_MONO OFF
        PLUGIN_CREATE_CLAP OFF
        PLUGIN_CREATE_VST3 ON
        PLUGIN_CREATE_AU ON
        PLUGIN_AU_SUBTYPE "DgNz"
        PLUGIN_AU_MANUFACTURER "2Bit"
        PLUGIN_AU_SANDBOX_SAFE ON
        PLUGIN_CREATE_AUv3 OFF
        PLUGIN_CREATE_LV2 OFF
        PLUGIN_CREATE_AAX OFF
        PLUGIN_CREATE_STANDALONE ON
        PLUGIN_COPY_AFTER_BUILD OFF
        MODULES
            yup::yup_gui
            yup::yup_audio_processors)

    target_sources(digitalnoise_shared INTERFACE
        source/DigitalNoiseEditor.cpp
        source/DigitalNoiseEditor.h
        source/DigitalNoisePlugin.cpp
        source/DigitalNoisePlugin.h)
    target_link_libraries(digitalnoise_shared INTERFACE digitalnoise_engine)
    target_include_directories(digitalnoise_shared INTERFACE
        "${CMAKE_CURRENT_LIST_DIR}/include"
        "${CMAKE_CURRENT_LIST_DIR}/source")

    if(APPLE AND DIGITALNOISE_ADHOC_SIGN)
        find_program(DIGITALNOISE_CODESIGN_EXECUTABLE codesign REQUIRED)
        add_custom_command(TARGET digitalnoise_vst3_plugin POST_BUILD
            COMMAND "${DIGITALNOISE_CODESIGN_EXECUTABLE}" --force --deep --sign -
                    "$<TARGET_BUNDLE_DIR:digitalnoise_vst3_plugin>"
            COMMENT "Ad-hoc signing DigitalNoise VST3 for local loading"
            VERBATIM)
        add_custom_command(TARGET digitalnoise_standalone_plugin POST_BUILD
            COMMAND "${DIGITALNOISE_CODESIGN_EXECUTABLE}" --force --deep --sign -
                    "$<TARGET_BUNDLE_DIR:digitalnoise_standalone_plugin>"
            COMMENT "Ad-hoc signing DigitalNoise standalone app for local launch"
            VERBATIM)
    endif()
endif()
