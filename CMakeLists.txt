cmake_minimum_required(VERSION 3.20)
project(OopAgentFramework LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
    add_compile_options(/utf-8)
endif()

find_package(CURL REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(Threads REQUIRED)

option(OOP_AGENT_ENABLE_GUI "Build Linux GUI agent tools when libxdo is available" ON)

find_path(XDO_INCLUDE_DIR NAMES xdo.h)
find_library(XDO_LIBRARY NAMES xdo)

set(OOP_AGENT_GUI_AVAILABLE OFF)
if(OOP_AGENT_ENABLE_GUI AND XDO_INCLUDE_DIR AND XDO_LIBRARY)
    set(OOP_AGENT_GUI_AVAILABLE ON)
elseif(OOP_AGENT_ENABLE_GUI)
    message(WARNING "libxdo was not found; GUI action support will be skipped")
endif()

add_library(oop_agent_client
    src/client/ollama_client.cpp
    src/client/ollama_embedding_client.cpp
    src/client/base64.cpp
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
    src/tools/time_tool.cpp
    src/tools/list_directory_tool.cpp
    src/tools/text_stats_tool.cpp
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

if(OOP_AGENT_GUI_AVAILABLE)
    add_library(oop_agent_gui
        src/gui/action_executor.cpp
    )

    target_include_directories(oop_agent_gui
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${XDO_INCLUDE_DIR}
    )

    target_link_libraries(oop_agent_gui
        PUBLIC
            ${XDO_LIBRARY}
    )

    target_sources(oop_agent_tools
        PRIVATE
            src/tools/gui_action_tool.cpp
    )

    target_link_libraries(oop_agent_tools
        PUBLIC
            oop_agent_gui
    )
endif()

add_library(oop_agent_core
    src/agent/agent_loop.cpp
    src/agent/loop_detector.cpp
    src/agent/multi_agent_coordinator.cpp
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
        Threads::Threads
)

add_library(oop_agent_harness
    src/harness/keyword_evaluator.cpp
    src/harness/functional_evaluator.cpp
    src/harness/vlm_evaluator.cpp
    src/harness/native_environment.cpp
    src/harness/sandbox_environment.cpp
    src/harness/step_hook.cpp
    src/harness/trajectory.cpp
    src/harness/trajectory_recorder.cpp
    src/harness/harness_runner.cpp
)

target_include_directories(oop_agent_harness
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(oop_agent_harness
    PUBLIC
        oop_agent_core
        nlohmann_json::nlohmann_json
)

add_executable(oop_agent
    src/main.cpp
)

target_compile_definitions(oop_agent
    PRIVATE
        OOP_AGENT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(oop_agent
    PRIVATE
        oop_agent_client
        oop_agent_core
)

add_executable(oop_agent_benchmark
    benchmark/run_eval.cpp
)

target_compile_definitions(oop_agent_benchmark
    PRIVATE
        OOP_AGENT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(oop_agent_benchmark
    PRIVATE
        oop_agent_client
        oop_agent_harness
)

include(CTest)
if(BUILD_TESTING)
    add_executable(oop_agent_client_tests
        tests/client_tests.cpp
    )
    target_link_libraries(oop_agent_client_tests PRIVATE oop_agent_client)
    add_test(NAME oop_agent_client_tests COMMAND oop_agent_client_tests)

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

    add_executable(oop_agent_harness_tests
        tests/harness_tests.cpp
    )
    target_link_libraries(oop_agent_harness_tests PRIVATE oop_agent_harness)
    add_test(NAME oop_agent_harness_tests COMMAND oop_agent_harness_tests)

    if(OOP_AGENT_GUI_AVAILABLE)
        add_executable(oop_agent_gui_tests
            tests/gui_agent_tests.cpp
        )
        target_link_libraries(
            oop_agent_gui_tests
            PRIVATE
                oop_agent_tools
                oop_agent_gui
        )
        add_test(NAME oop_agent_gui_tests COMMAND oop_agent_gui_tests)
    endif()
endif()
