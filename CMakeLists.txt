cmake_minimum_required(VERSION 3.20)
project(OopAgentFramework LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(CURL REQUIRED)
find_package(nlohmann_json REQUIRED)

add_library(oop_agent_client
    src/client/ollama_client.cpp
)

target_include_directories(oop_agent_client
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(oop_agent_client
    PUBLIC
        CURL::libcurl
        nlohmann_json::nlohmann_json
)

add_library(oop_agent_tools
    src/tools/tool_registry.cpp
    src/tools/exec_tool.cpp
    src/tools/file_tool.cpp
)

target_include_directories(oop_agent_tools
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

include(CTest)
if(BUILD_TESTING)
    add_executable(oop_agent_tool_tests
        tests/tool_tests.cpp
    )
    target_link_libraries(oop_agent_tool_tests PRIVATE oop_agent_tools)
    add_test(NAME oop_agent_tool_tests COMMAND oop_agent_tool_tests)
endif()
