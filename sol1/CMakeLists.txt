cmake_minimum_required(VERSION 3.27)
project(sol1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

include(FetchContent)
FetchContent_Declare(
        unordered_dense
        GIT_REPOSITORY https://github.com/martinus/unordered_dense.git
        GIT_TAG v4.5.0
        GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(unordered_dense)

find_package(OpenMP REQUIRED)

add_executable(sol1 main.cpp Data.h ProjectAllocator.h SimulationState.h
        SkillToContributorsMap.h)

target_link_libraries(sol1 PRIVATE
        unordered_dense::unordered_dense
        OpenMP::OpenMP_CXX
)
target_link_options(sol1 PRIVATE -fopenmp)

target_compile_options(sol1 PRIVATE
        $<$<CONFIG:Release>:-march=native -fno-plt -fno-semantic-interposition>
)

include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_ok)
if(ipo_ok)
    set_property(TARGET sol1 PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
endif()

set(PGO "" CACHE STRING "GEN or USE")
set(PGO_DIR "${CMAKE_BINARY_DIR}/pgo")
if(PGO STREQUAL "GEN")
    target_compile_options(sol1 PRIVATE -fprofile-generate=${PGO_DIR})
    target_link_options(sol1 PRIVATE -fprofile-generate=${PGO_DIR})
elseif(PGO STREQUAL "USE")
    target_compile_options(sol1 PRIVATE -fprofile-use=${PGO_DIR} -fprofile-correction)
    target_link_options(sol1 PRIVATE -fprofile-use=${PGO_DIR})
endif()