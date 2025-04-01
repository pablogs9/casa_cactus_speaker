cmake_minimum_required(VERSION 3.10)

# Set the project name
project(cspot_ng)

# Add the executable
add_executable(cspot_ng main.cpp)

# Add include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/nanopb
)