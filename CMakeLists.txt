cmake_minimum_required(VERSION 3.12)

project(NovelOscUGens VERSION 0.2.0 LANGUAGES CXX)

option(SCSYNTH "Build the scsynth plugin target" ON)
option(SUPERNOVA "Build the supernova plugin target" OFF)
option(STRICT "Enable strict compiler warnings" OFF)
option(NOVEL_OSC_SANITIZE
    "Enable address and undefined-behavior sanitizers for tests" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/stage"
        CACHE PATH "Install prefix" FORCE)
endif()

if(NOT SC_PATH)
    message(FATAL_ERROR
        "SC_PATH must point to a matching SuperCollider source tree")
endif()

get_filename_component(SC_PATH_FOR_MODULES "${SC_PATH}" ABSOLUTE
    BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
list(APPEND CMAKE_MODULE_PATH
    "${SC_PATH_FOR_MODULES}/tools/cmake_gen")

include(SuperColliderServerPlugin)
sc_do_initial_compiler_config()
sc_check_sc_path("${SC_PATH}")

set(novel_osc_cpp_files
    Source/NovelOscUGens.cpp
    Source/HarmonicStrideOsc.cpp
    Source/TableMorph2D.cpp
    Source/TZSplitOsc.cpp
    Source/ScaleSpreadOsc.cpp
    Source/SpectralBasisOsc.cpp
    Source/PartialClusterOsc.cpp
    Source/SyncRingOsc.cpp
    Source/PMNetworkOsc.cpp
    Source/ClusterPMOsc.cpp
    Source/ScannedNetworkOsc.cpp
    Source/VectorPhaseOsc.cpp
    Source/SpectralFrameOsc.cpp
    Source/RippleFormantOsc.cpp
    Source/ShiftLogicOsc.cpp
    Source/QuadratureFeedbackOsc.cpp
    Source/SyncModeOsc.cpp
    Source/TableMorph3D.cpp
    Source/PitchRegister.cpp
)

set(novel_osc_sc_files
    Classes/NovelOscUGens.sc
    Classes/AdditionalNovelOscUGens.sc
)

file(GLOB novel_osc_schelp_files CONFIGURE_DEPENDS
    HelpSource/Classes/*.schelp)

sc_add_server_plugin(
    "NovelOscUGens"
    "NovelOscUGens"
    "${novel_osc_cpp_files}"
    "${novel_osc_sc_files}"
    "${novel_osc_schelp_files}"
)

foreach(target NovelOscUGens_scsynth NovelOscUGens_supernova)
    if(TARGET ${target})
        target_include_directories(${target} SYSTEM PRIVATE
            "${SC_PATH_FOR_MODULES}/include/plugin_interface"
            "${SC_PATH_FOR_MODULES}/include/common"
            "${SC_PATH_FOR_MODULES}/common")
        target_include_directories(${target} PRIVATE Source Source/DSP)
        if(STRICT)
            if(MSVC)
                target_compile_options(${target} PRIVATE /W4)
            else()
                target_compile_options(${target} PRIVATE
                    -Wall -Wextra -Wpedantic
                    -Wno-unused-parameter)
            endif()
        endif()
    endif()
endforeach()

if(TARGET NovelOscUGens_scsynth)
    set_target_properties(NovelOscUGens_scsynth
        PROPERTIES OUTPUT_NAME "NovelOscUGens")
    if(MINGW)
        target_link_options(NovelOscUGens_scsynth PRIVATE
            -static-libgcc -static-libstdc++)
    endif()
endif()

if(TARGET NovelOscUGens_supernova)
    set_target_properties(NovelOscUGens_supernova
        PROPERTIES OUTPUT_NAME "NovelOscUGens_supernova")
endif()

install(FILES
    README.md
    CHANGELOG.md
    LICENSE
    ORIGINS.md
    THIRD_PARTY_NOTICES.md
    DESTINATION NovelOscUGens)
install(DIRECTORY docs/ DESTINATION NovelOscUGens/docs)
install(DIRECTORY tests/benchmarks/
    DESTINATION NovelOscUGens/benchmarks)
install(FILES HelpSource/Guides/NovelOscillators.schelp
    DESTINATION NovelOscUGens/HelpSource/Guides)

include(CTest)
if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
    add_executable(novel_osc_reference_tests
        tests/reference/NovelOscReferenceTests.cpp)
    target_include_directories(novel_osc_reference_tests PRIVATE
        Source Source/DSP)

    if(STRICT)
        if(MSVC)
            target_compile_options(novel_osc_reference_tests PRIVATE /W4)
        else()
            target_compile_options(novel_osc_reference_tests PRIVATE
                -Wall -Wextra -Wpedantic)
        endif()
    endif()

    if(NOVEL_OSC_SANITIZE)
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
            target_compile_options(novel_osc_reference_tests PRIVATE
                -fsanitize=address,undefined
                -fno-omit-frame-pointer)
            set_property(TARGET novel_osc_reference_tests
                APPEND_STRING PROPERTY LINK_FLAGS
                " -fsanitize=address,undefined")
        else()
            message(FATAL_ERROR
                "NOVEL_OSC_SANITIZE requires Clang or GCC")
        endif()
    endif()

    add_test(NAME novel_osc_reference_tests
        COMMAND novel_osc_reference_tests)
endif()
