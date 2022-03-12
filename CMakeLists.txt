cmake_minimum_required(VERSION 3.19 FATAL_ERROR)

set(CMAKE_C_STANDARD 99)


project(durin LANGUAGES C)

# Since we want the fortran building to be optional, check if it exists
include(CheckLanguage)
check_language(Fortran)

###############################################################################
# Building HDF5 in-tree
set(FETCHCONTENT_QUIET OFF CACHE BOOL "Quiet Fetching" FORCE)
include(FetchContent)
FetchContent_Declare(
    hdf5
    URL https://github.com/HDFGroup/hdf5/archive/refs/tags/hdf5-1_12_1.tar.gz
    URL_HASH SHA256=e6dde173c2d243551922d23a0387a79961205b018502e6a742acb30b61bc2d5f
)
# To get what we want, this includes overriding settings 
set(HDF5_EXTERNALLY_CONFIGURED ON)
set(ALLOW_UNSUPPORTED ON CACHE BOOL "Allow HL and threadsafe" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "Build HDF5 Unit Testing")
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build Shared Libraries")
set(HDF5_BUILD_CPP_LIB OFF CACHE BOOL "Build HDF5 C++ Library")
set(HDF5_BUILD_HL_LIB ON CACHE BOOL "Build HIGH Level HDF5 Library")
set(HDF5_BUILD_TOOLS OFF CACHE BOOL "Build HDF5 Tools")
set(HDF5_BUILD_EXAMPLES OFF CACHE BOOL "Build HDF5 Library Examples")
set(HDF5_DISABLE_COMPILER_WARNINGS ON CACHE BOOL "Disable HDF5 Warnings")
set(HDF5_GENERATE_HEADERS OFF CACHE BOOL "Don't regenerate HDF5 headers" FORCE)
set(HDF5_ENABLE_THREADSAFE ON CACHE BOOL "Enable thread-safety")
set(HDF5_ENABLE_DEPRECATED_SYMBOLS ON CACHE BOOL "Enable Deprecated Symbols")

FetchContent_MakeAvailable(hdf5)

###############################################################################
# Bitshuffle
#
add_library(bslz4
    STATIC
    bslz4/src/lz4.c
    bslz4/src/bitshuffle.c
    bslz4/src/bitshuffle_core.c
    bslz4/src/iochain.c
)
target_include_directories(bslz4 PUBLIC bslz4/src)
set_property(TARGET bslz4 PROPERTY POSITION_INDEPENDENT_CODE ON)

###############################################################################
# Objects shared between targeta
#
# Only build the objects shared between durin-plugin and example, once.
#
add_library(durin-objects OBJECT src/file.c src/err.c src/filters.c )
target_link_libraries(durin-objects PUBLIC hdf5-static hdf5_hl-static )
target_link_libraries(durin-objects PRIVATE bslz4 )
set_property(TARGET durin-objects PROPERTY POSITION_INDEPENDENT_CODE ON)
# Even if using HDF5 1.12, we want the 1.10 API
target_compile_definitions(durin-objects PUBLIC H5_USE_110_API)

###############################################################################
# durin-plugin
#
add_library(durin-plugin MODULE src/plugin.c )
target_link_libraries(durin-plugin PRIVATE bslz4 durin-objects)
set_property(TARGET durin-plugin PROPERTY PREFIX "")

###############################################################################
# example
#
add_executable(example src/test.c )
target_link_libraries(example durin-objects )
set_property(TARGET example PROPERTY EXCLUDE_FROM_ALL YES)
###############################################################################
# The fortran plugin tester is optional
#
if(CMAKE_Fortran_COMPILER)
    enable_language(Fortran)
    find_package(OpenMP COMPONENTS Fortran)
    if (OpenMP_FOUND) 
        add_executable(test_plugin test/generic_data_plugin.f90 test/test_generic_host.f90)
        target_link_libraries(test_plugin PRIVATE dl OpenMP::OpenMP_Fortran)
        set_property(TARGET test_plugin PROPERTY EXCLUDE_FROM_ALL YES)
    endif()
endif()
