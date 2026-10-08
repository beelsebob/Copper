cmake_minimum_required(VERSION 3.25)

project(Copper LANGUAGES C CXX)

option(COPPER_ENABLE_METAL "Build Copper's Metal backend when available" ${APPLE})

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)

if(APPLE AND EXISTS "/opt/homebrew")
    list(PREPEND CMAKE_PREFIX_PATH "/opt/homebrew")
endif()

find_package(nlohmann_json REQUIRED)
find_package(GEOS REQUIRED)
find_package(HDF5 REQUIRED COMPONENTS C HL)
find_package(PkgConfig REQUIRED)
pkg_check_modules(BLOSC2 REQUIRED IMPORTED_TARGET blosc2)

file(GLOB COPPER_UTILS_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/CopperUtils/*.cpp")
add_library(CopperUtils STATIC ${COPPER_UTILS_SOURCES})
target_include_directories(CopperUtils PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/CopperUtils")
target_link_libraries(CopperUtils PUBLIC nlohmann_json::nlohmann_json GEOS::geos_c)

# CSXCAD's upstream CMake expects installed fparser and TinyXML.  Copper vendors both, so make
# those targets available under the variables its build file consumes.
add_library(CopperFParser STATIC
    submodules/fparser/fparser.cc
    submodules/fparser/fpoptimizer.cc)
target_include_directories(CopperFParser PUBLIC submodules/fparser)
target_compile_options(CopperFParser PRIVATE -w)
add_library(CopperTinyXML STATIC
    submodules/tinyxml/tinystr.cpp
    submodules/tinyxml/tinyxml.cpp
    submodules/tinyxml/tinyxmlerror.cpp
    submodules/tinyxml/tinyxmlparser.cpp)
target_include_directories(CopperTinyXML PUBLIC submodules/tinyxml)
target_compile_definitions(CopperTinyXML PRIVATE TIXML_USE_STL)
target_compile_options(CopperTinyXML PRIVATE -w)
# These static libraries are linked into CSXCAD, which is a shared library on
# Linux.  Explicit PIC is required on architectures such as AArch64.
set_target_properties(CopperUtils CopperFParser CopperTinyXML
    PROPERTIES POSITION_INDEPENDENT_CODE ON)
set(fparser_LIBRARIES CopperFParser CACHE FILEPATH "" FORCE)
set(fparser_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/submodules/fparser" CACHE PATH "" FORCE)
set(TinyXML_LIBRARY CopperTinyXML CACHE FILEPATH "" FORCE)
set(TinyXML_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/submodules/tinyxml" CACHE PATH "" FORCE)
set(CSXCAD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(submodules/CSXCAD)
set_target_properties(CSXCAD PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED YES)
target_compile_options(CSXCAD PRIVATE -w)
target_link_libraries(CSXCAD CopperUtils)

file(GLOB_RECURSE COPPER_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Copper/*.cpp")
if(APPLE AND COPPER_ENABLE_METAL)
    enable_language(OBJCXX)
    list(APPEND COPPER_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/Copper/Internal/CopperEngine.mm")
endif()

add_library(Copper SHARED ${COPPER_SOURCES})
add_library(Copper::Copper ALIAS Copper)
target_include_directories(Copper
    PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Copper>"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/submodules/CSXCAD/src"
        ${HDF5_INCLUDE_DIRS})
target_link_libraries(Copper
    PUBLIC CSXCAD CopperUtils nlohmann_json::nlohmann_json ${HDF5_LIBRARIES} ${HDF5_HL_LIBRARIES} PkgConfig::BLOSC2)

if(APPLE)
    find_library(ACCELERATE_FRAMEWORK Accelerate REQUIRED)
    target_link_libraries(Copper PRIVATE ${ACCELERATE_FRAMEWORK})
endif()

if(APPLE AND COPPER_ENABLE_METAL)
    find_library(FOUNDATION_FRAMEWORK Foundation REQUIRED)
    find_library(METAL_FRAMEWORK Metal REQUIRED)
    target_link_libraries(Copper PRIVATE ${FOUNDATION_FRAMEWORK} ${METAL_FRAMEWORK})
    set_target_properties(Copper PROPERTIES FRAMEWORK TRUE FRAMEWORK_VERSION A)
    # The Objective-C++ engine is written for ARC, as the Xcode project builds it.
    target_compile_options(Copper PRIVATE $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>)
    set(COPPER_METAL_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/metal")
    # newDefaultLibraryWithBundle: only looks for default.metallib in the framework's Resources.
    set(COPPER_METALLIB "${COPPER_METAL_DIRECTORY}/default.metallib")
    add_custom_command(OUTPUT "${COPPER_METALLIB}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${COPPER_METAL_DIRECTORY}"
        COMMAND xcrun -sdk macosx metal -c "${CMAKE_CURRENT_SOURCE_DIR}/Copper/Shaders/CopperFDTD.metal"
                -o "${COPPER_METAL_DIRECTORY}/CopperFDTD.air"
        COMMAND xcrun -sdk macosx metallib "${COPPER_METAL_DIRECTORY}/CopperFDTD.air" -o "${COPPER_METALLIB}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Copper/Shaders/CopperFDTD.metal"
                "${CMAKE_CURRENT_SOURCE_DIR}/Copper/Shaders/CopperShaderTypes.h")
    target_sources(Copper PRIVATE "${COPPER_METALLIB}")
    set_source_files_properties("${COPPER_METALLIB}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
else()
    target_compile_definitions(Copper PRIVATE COPPER_CPU_ONLY=1)
endif()

set_target_properties(Copper PROPERTIES OUTPUT_NAME Copper)

include(GNUInstallDirs)
install(TARGETS Copper CopperUtils CSXCAD
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    FRAMEWORK DESTINATION "${CMAKE_INSTALL_LIBDIR}")
