cmake_minimum_required(VERSION 3.15)
project(sip_rtp_gateway CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Dependencies

find_package(yaml-cpp REQUIRED)
if (NOT TARGET yaml-cpp::yaml-cpp AND TARGET yaml-cpp)
    add_library(yaml-cpp::yaml-cpp ALIAS yaml-cpp)
endif()
find_package(Threads REQUIRED)



# Source files
file(GLOB_RECURSE SRC_FILES
    "src/*.cpp"
    "src/*.h"
)

# Include directories
include_directories(
    src
)

# Executable
add_executable(sip_rtp_gateway
    ${SRC_FILES}
)

    target_link_libraries(sip_rtp_gateway
    PRIVATE
    yaml-cpp::yaml-cpp
    Threads::Threads
)

# Compile options
if (MSVC)
    target_compile_options(sip_rtp_gateway PRIVATE /W3 /wd4005 /wd4244 /wd4267)
else()
    target_compile_options(sip_rtp_gateway PRIVATE -Wall -Wextra -Wno-unused-parameter -pthread)
endif()
