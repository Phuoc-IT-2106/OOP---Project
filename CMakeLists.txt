cmake_minimum_required(VERSION 3.20)
project(OopAgentFramework LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(CURL REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(SQLite3 REQUIRED)

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
    src/tools/calculator_tool.cpp
    src/tools/exec_tool.cpp
    src/tools/file_tool.cpp
    src/tools/web_search_tool.cpp
    src/tools/searxng_transport.cpp
    src/tools/memory_tool.cpp
    src/tools/sqlite_memory_store.cpp
)

target_include_directories(oop_agent_tools
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(oop_agent_tools
    PUBLIC
        CURL::libcurl
        nlohmann_json::nlohmann_json
        SQLite::SQLite3
)

add_library(oop_agent_core
    src/agent/agent_loop.cpp
    src/skills/skill_loader.cpp
)

target_include_directories(oop_agent_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(oop_agent_core
    PUBLIC
        oop_agent_tools
        nlohmann_json::nlohmann_json
)

include(CTest)
if(BUILD_TESTING)
    add_executable(oop_agent_tool_tests
        tests/tool_tests.cpp
    )
    target_link_libraries(oop_agent_tool_tests PRIVATE oop_agent_tools)
    add_test(NAME oop_agent_tool_tests COMMAND oop_agent_tool_tests)

    add_executable(oop_agent_core_tests
        tests/agent_tests.cpp
    )
    target_link_libraries(oop_agent_core_tests PRIVATE oop_agent_core)
    add_test(NAME oop_agent_core_tests COMMAND oop_agent_core_tests)
endif()
