cmake_minimum_required (VERSION 3.27)
project(NetComm CXX)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")

add_library(netcomm SHARED server.cpp client.cpp)

add_executable(server_app server_main.cpp)
add_executable(client_app client_main.cpp)

target_link_libraries(server_app netcomm)
target_link_libraries(client_app netcomm)

file(COPY ${CMAKE_SOURCE_DIR}/common.h ${CMAKE_SOURCE_DIR}/client.h ${CMAKE_SOURCE_DIR}/server.h DESTINATION ${PROJECT_BINARY_DIR}/include)