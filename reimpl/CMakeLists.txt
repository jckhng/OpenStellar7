cmake_minimum_required(VERSION 3.16)
project(stellar7_reimplementation LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

include(cmake/Runtime.cmake)

include(CTest)
if(BUILD_TESTING)
    include(cmake/Tests.cmake)
endif()

include(cmake/DeveloperChecks.cmake)
include(cmake/Packaging.cmake)
