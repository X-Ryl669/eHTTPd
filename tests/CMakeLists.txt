cmake_minimum_required(VERSION 3.10)

project(eHTTPdTests)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../ eHTTPd)

add_executable(RouteTesting
    RouteTesting.cpp)

add_executable(HeadersParsing
    HeadersParsing.cpp)

add_executable(PathNormalization
    PathNormalization.cpp)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

set_target_properties(RouteTesting PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

set_target_properties(HeadersParsing PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

set_target_properties(PathNormalization PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

target_compile_definitions(RouteTesting PUBLIC _DEBUG=$<CONFIG:Debug>)

target_compile_definitions(HeadersParsing PUBLIC _DEBUG=$<CONFIG:Debug>)

target_compile_definitions(PathNormalization PUBLIC _DEBUG=$<CONFIG:Debug>)


IF(CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
  IF (WIN32)
  ELSE()
    target_compile_options(RouteTesting PUBLIC -fno-rtti -ffunction-sections -fdata-sections)
    target_link_options(RouteTesting PUBLIC $<$<CXX_COMPILER_ID:GNU>:LINKER:--gc-sections,--as-needed>)
    target_link_options(RouteTesting PUBLIC $<$<CXX_COMPILER_ID:Clang>:LINKER:-dead_strip>)
    target_link_options(RouteTesting PUBLIC $<$<CXX_COMPILER_ID:AppleClang>:LINKER:-dead_strip>)
  ENDIF()
ENDIF()


target_link_libraries(RouteTesting LINK_PUBLIC eHTTPd ${CMAKE_DL_LIBS} Threads::Threads)
#install(TARGETS RouteTesting RUNTIME DESTINATION bin)
target_link_libraries(HeadersParsing LINK_PUBLIC eHTTPd ${CMAKE_DL_LIBS} Threads::Threads)
#install(TARGETS HeadersParsing RUNTIME DESTINATION bin)

target_link_libraries(PathNormalization LINK_PUBLIC eHTTPd ${CMAKE_DL_LIBS} Threads::Threads)


