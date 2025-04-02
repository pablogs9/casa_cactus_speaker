cmake_minimum_required(VERSION 3.10)

# Set the project name
project(cspot_ng)

# Find Avahi libraries
find_package(PkgConfig REQUIRED)
# Try to find just avahi-client, which often pulls in the necessary dependencies
pkg_check_modules(AVAHI avahi-client)

# If not found, try an alternative approach
if(NOT AVAHI_FOUND)
    message(STATUS "Could not find Avahi via pkg-config, checking for libraries directly")
    # Try to find the libraries directly
    find_library(AVAHI_CLIENT_LIBRARY NAMES avahi-client)
    find_library(AVAHI_COMMON_LIBRARY NAMES avahi-common)
    find_path(AVAHI_INCLUDE_DIR NAMES avahi-client/client.h)

    if(AVAHI_CLIENT_LIBRARY AND AVAHI_INCLUDE_DIR)
        set(AVAHI_LIBRARIES ${AVAHI_CLIENT_LIBRARY})
        if(AVAHI_COMMON_LIBRARY)
            list(APPEND AVAHI_LIBRARIES ${AVAHI_COMMON_LIBRARY})
        endif()
        set(AVAHI_INCLUDE_DIRS ${AVAHI_INCLUDE_DIR})
        set(AVAHI_FOUND TRUE)
    endif()
endif()

# Check if Avahi was found and provide helpful message if not
if(NOT AVAHI_FOUND)
    message(FATAL_ERROR "Could not find Avahi libraries. Please install Avahi development packages:
    - On Debian/Ubuntu: sudo apt-get install libavahi-client-dev
    - On Fedora/RHEL: sudo dnf install avahi-devel")
endif()

# Find OpenSSL
find_package(OpenSSL REQUIRED)
if(NOT OPENSSL_FOUND)
    message(FATAL_ERROR "OpenSSL not found. Please install OpenSSL development packages:
    - On Debian/Ubuntu: sudo apt-get install libssl-dev
    - On Fedora/RHEL: sudo dnf install openssl-devel")
endif()

# Find libcurl
find_package(CURL REQUIRED)
if(NOT CURL_FOUND)
    message(FATAL_ERROR "libcurl not found. Please install libcurl development packages:
    - On Debian/Ubuntu: sudo apt-get install libcurl4-openssl-dev
    - On Fedora/RHEL: sudo dnf install libcurl-devel")
endif()

# Find mbedTLS
pkg_check_modules(MBEDTLS mbedtls)

# If not found via pkg-config, try to find the libraries directly
if(NOT MBEDTLS_FOUND)
    message(STATUS "Could not find mbedTLS via pkg-config, checking for libraries directly")

    find_library(MBEDTLS_LIBRARY NAMES mbedtls)
    find_library(MBEDCRYPTO_LIBRARY NAMES mbedcrypto)
    find_library(MBEDX509_LIBRARY NAMES mbedx509)
    find_path(MBEDTLS_INCLUDE_DIR NAMES mbedtls/ssl.h)

    if(MBEDTLS_LIBRARY AND MBEDCRYPTO_LIBRARY AND MBEDX509_LIBRARY AND MBEDTLS_INCLUDE_DIR)
        set(MBEDTLS_LIBRARIES ${MBEDTLS_LIBRARY} ${MBEDCRYPTO_LIBRARY} ${MBEDX509_LIBRARY})
        set(MBEDTLS_INCLUDE_DIRS ${MBEDTLS_INCLUDE_DIR})
        set(MBEDTLS_FOUND TRUE)
    endif()
endif()

# Check if mbedTLS was found and provide helpful message if not
if(NOT MBEDTLS_FOUND)
    message(FATAL_ERROR "Could not find mbedTLS libraries. Please install mbedTLS development packages:
    - On Debian/Ubuntu: sudo apt-get install libmbedtls-dev
    - On Fedora/RHEL: sudo dnf install mbedtls-devel")
endif()

# Add the executable
add_executable(cspot_ng main.cpp)

# Search all the source files in the src directory
file(GLOB_RECURSE SOURCES
    src/*.cpp
    src/*.c
    src/*.h
    protobuf/*.pb.c
)

file(GLOB NANO_PB_SOURCES
    nanopb/*.c
)

# Add the source files to the executable
target_sources(cspot_ng PRIVATE ${SOURCES} ${NANO_PB_SOURCES})

# Add include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/nanopb
    ${AVAHI_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
    ${CURL_INCLUDE_DIRS}
    ${MBEDTLS_INCLUDE_DIRS}
)

# Link with libraries
target_link_libraries(cspot_ng PRIVATE
    ${AVAHI_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    ${CURL_LIBRARIES}
    ${MBEDTLS_LIBRARIES}
)