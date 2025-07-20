cmake_minimum_required(VERSION 3.10)

if (POLICY CMP0025)
  cmake_policy(SET CMP0025 NEW)
endif ()


# Not using project properties here since it doesn't work in 3.7 version here
project(eHTTPd)

option(REDUCED_FOOTPRINT "Whether to enable reduced footprint for the client code" ON)
option(ENABLE_TLS "Whether to enable TLS/SSL code (you'll need MBedTLS available)" OFF)


add_library(eHTTPd
            src/Normalization.cpp
)

set_property(TARGET eHTTPd
             PROPERTY POSITION_INDEPENDENT_CODE ON
)

set_target_properties(eHTTPd PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

target_compile_definitions(eHTTPd PUBLIC _DEBUG=$<CONFIG:Debug>
                                        MaxSupport=$<STREQUAL:${REDUCED_FOOTPRINT},ON>
                                        UseTLS=$<STREQUAL:${ENABLE_TLS},ON>
					)

IF (WIN32)
ELSE()
  target_compile_options(eHTTPd PUBLIC -fno-rtti -ffunction-sections -fdata-sections)
ENDIF()

if (${ENABLE_TLS} STREQUAL "ON")
  # MbedTLS doesn't provide any variable when the package is found, so it's not possible to extract the required include and lib dir automatically
  find_package(MbedTLS REQUIRED)
  IF (${MbedTLS_FOUND})
    # However, it does provide a pkgconfig files
    set(ENV{PKG_CONFIG_PATH} ${MbedTLS_DIR}/../../pkgconfig)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(mbedtls REQUIRED IMPORTED_TARGET mbedtls)

    target_link_libraries(eHTTPd PRIVATE mbedx509 mbedtls mbedcrypto)
    target_include_directories(eHTTPd PRIVATE ${mbedtls_INCLUDE_DIRS})
    target_link_directories(eHTTPd PUBLIC ${mbedtls_LIBRARY_DIRS})
  ELSE()
     message(ERROR "Couldn't find MbedTLS pkgconfig path")
  ENDIF()
endif ()


add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../eCommon eCommon)
target_link_libraries(eHTTPd PUBLIC eCommon)

target_include_directories(eHTTPd PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

install(TARGETS eHTTPd
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib)
#        PUBLIC_HEADER DESTINATION include/eHTTPd)


install(DIRECTORY include/ DESTINATION include/eHTTPd FILES_MATCHING PATTERN "*.hpp")

install(FILES ${CMAKE_BINARY_DIR}/eHTTPd.pc DESTINATION lib/pkgconfig)
