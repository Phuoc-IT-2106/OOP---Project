#include "agent/agent_loop.h"
#include "client/ollama_client.h"
#include "client/ollama_embedding_client.h"
#include "harness/benchmark_task.h"
#include "harness/functional_evaluator.h"
#include "harness/harness_runner.h"
#include "harness/keyword_evaluator.h"
#include "skills/skill_loader.h"
#include "tools/calculator_tool.h"
#include "tools/exec_tool.h"
#include "tools/file_tool.h"
#include "tools/list_directory_tool.h"
#include "tools/memory_tool.h"
#include "tools/text_stats_tool.h"
#include "tools/time_tool.h"
#include "tools/tool_registry.h"
#include "tools/web_search_tool.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

namespace fs = std::filesystem;

struct RunnerOptions {
#ifdef OOP_AGENT_SOURCE_DIR
    fs::path project_root{OOP_AGENT_SOURCE_DIR};
#else
    fs::path project_root{fs::current_path()};
#endif
    fs::path tasks_file;
    fs::path workspace;
    fs::path results_directory;
    std::string ollama_url{"http://localhost:11434"};
    std::string model{"qwen2.5:7b"};
    std::string embedding_model{"nomic-embed-text"};
    std::string searxng_url{"http://localhost:8080"};
};

void printUsage(const char *program) {
    std::cout
        << "Usage: " << program << " [options]\n"
        << "  --tasks PATH             Benchmark JSON (default: benchmark/tasks.json)\n"
        << "  --workspace PATH         Isolated task workspace\n"
        << "  --results PATH           Trajectory and summary directory\n"
        << "  --ollama-url URL         Ollama base URL\n"
        << "  --model NAME             Chat model name\n"
        << "  --embedding-model NAME   Embedding model name\n"
        << "  --searxng-url URL        SearXNG base URL\n"
        << "  --help                    Show this message\n";
}

std::string requireValue(int argc,
                         char **argv,
                         int &index,
                         const std::string &option) {
    if (index + 1 >= argc) {
        throw std::invalid_argument(option + " requires a value");
    }
    return argv[++index];
}

RunnerOptions parseOptions(int argc, char **argv) {
    RunnerOptions options;
    options.project_root = fs::absolute(options.project_root);
    options.tasks_file = options.project_root / "benchmark" / "tasks.json";
    options.workspace = options.project_root / "benchmark" / "runtime_workspace";
    options.results_directory = options.project_root / "benchmark" / "results";

    for (int index = 1; index < argc; ++index) {
        const std::string option = argv[index];
        if (option == "--help") {
            printUsage(argv[0]);
            std::exit(0);
        }
        const auto value = requireValue(argc, argv, index, option);
        if (option == "--tasks") {
            options.tasks_file = value;
        } else if (option == "--workspace") {
            options.workspace = value;
        } else if (option == "--results") {
            options.results_directory = value;
        } else if (option == "--ollama-url") {
            options.ollama_url = value;
        } else if (option == "--model") {
            options.model = value;
        } else if (option == "--embedding-model") {
            options.embedding_model = value;
        } else if (option == "--searxng-url") {
            options.searxng_url = value;
        } else {
            throw std::invalid_argument("unknown option: " + option);
        }
    }

    options.tasks_file = fs::absolute(options.tasks_file);
    options.workspace = fs::absolute(options.workspace);
    options.results_directory = fs::absolute(options.results_directory);
    return options;
}

void copyIfPresent(const fs::path &source, const fs::path &destination) {
    if (!fs::exists(source)) {
        return;
    }
    fs::create_directories(destination.parent_path());
    fs::copy_file(source,
                  destination,
                  fs::copy_options::overwrite_existing);
}

void prepareWorkspace(const RunnerOptions &options) {
    const auto normalized_workspace =
        options.workspace.lexically_normal();
    const auto normalized_project_root =
        options.project_root.lexically_normal();
    const bool workspace_contains_project =
        std::mismatch(normalized_workspace.begin(),
                      normalized_workspace.end(),
                      normalized_project_root.begin(),
                      normalized_project_root.end())
            .first == normalized_workspace.end();
    if (normalized_workspace == normalized_project_root ||
        workspace_contains_project ||
        normalized_workspace == normalized_workspace.root_path()) {
        throw std::invalid_argument(
            "benchmark workspace must not be the project or filesystem root");
    }

    // A fresh workspace prevents artifacts from a previous run from producing
    // false-positive FunctionalEvaluator results.
    std::error_code error;
    fs::remove_all(options.workspace, error);
    if (error) {
        throw std::runtime_error("cannot reset benchmark workspace: " +
                                 error.message());
    }

    fs::create_directories(options.workspace / "src");
    fs::create_directories(options.workspace / "skills");
    fs::create_directories(options.workspace / "benchmark");
    fs::create_directories(options.workspace / "output");
    fs::create_directories(options.workspace / "data");
    fs::create_directories(options.results_directory);

    copyIfPresent(options.project_root / "README.md",
                  options.workspace / "README.md");
    for (const auto &entry :
         fs::directory_iterator(options.project_root / "skills")) {
        if (entry.is_regular_file() &&
            entry.path().extension() == ".md") {
            copyIfPresent(entry.path(),
                          options.workspace / "skills" /
                              entry.path().filename());
        }
    }

    std::ofstream input_file(options.workspace / "input.txt");
    input_file << "first benchmark line\n"
                  "second benchmark line\n"
                  "third benchmark line\n";

    std::ofstream notes_file(options.workspace / "notes.txt");
    notes_file << "Persistent agents use tools to observe and act.\n"
                  "Harness evaluation records reproducible results.\n";
    if (!input_file.good() || !notes_file.good()) {
        throw std::runtime_error("cannot create benchmark fixture files");
    }
}

void validateTasks(const std::vector<oop_agent::harness::BenchmarkTask> &tasks) {
    if (tasks.size() != 10) {
        throw std::runtime_error("benchmark must contain exactly 10 tasks; found " +
                                 std::to_string(tasks.size()));
    }
    std::unordered_set<std::string> task_ids;
    for (const auto &task : tasks) {
        if (task.id.empty() || task.description.empty() ||
            task.instruction.empty() || task.eval_type.empty() ||
            task.max_steps == 0) {
            throw std::runtime_error(
                "benchmark task has invalid required fields: " + task.id);
        }
        if (task.eval_type == "functional" && task.eval_script.empty()) {
            throw std::runtime_error(
                "functional task is missing eval_script: " + task.id);
        }
        if (task.eval_type == "keyword" &&
            task.expected_keywords.empty()) {
            throw std::runtime_error(
                "keyword task is missing expected_keywords: " + task.id);
        }
        if (task.eval_type != "functional" &&
            task.eval_type != "keyword") {
            throw std::runtime_error(
                "unsupported eval_type in task: " + task.id);
        }
        if (!task_ids.insert(task.id).second) {
            throw std::runtime_error(
                "duplicate benchmark task id: " + task.id);
        }
    }
}

void registerTools(
    oop_agent::tools::ToolRegistry &registry,
    const RunnerOptions &options,
    const std::shared_ptr<oop_agent::tools::MemoryStore> &memory_store,
    const std::shared_ptr<oop_agent::client::EmbeddingClient> &embedder) {
    using namespace oop_agent::tools;

    FileToolConfig files;
    files.root_directory = options.workspace;
    ListDirectoryConfig directories;
    directories.root_directory = options.workspace;
    WebSearchConfig web;
    web.base_url = options.searxng_url;
    MemoryToolConfig memory;
    memory.database_path = options.workspace / "data" / "memory.sqlite3";

    registry.registerFactory(
        "exec", [] { return std::make_unique<ExecTool>(); });
    registry.registerFactory(
        "read_file",
        [files] { return std::make_unique<ReadFileTool>(files); });
    registry.registerFactory(
        "write_file",
        [files] { return std::make_unique<WriteFileTool>(files); });
    registry.registerFactory(
        "calculator",
        [] { return std::make_unique<CalculatorTool>(); });
    registry.registerFactory(
        "web_search",
        [web] { return std::make_unique<WebSearchTool>(web); });
    registry.registerFactory(
        "memory_save",
        [memory_store, memory, embedder] {
            return std::make_unique<MemorySaveTool>(
                memory_store, memory, embedder);
        });
    registry.registerFactory(
        "memory_search",
        [memory_store, memory, embedder] {
            return std::make_unique<MemorySearchTool>(
                memory_store, memory, embedder);
        });
    registry.registerFactory(
        "time", [] { return std::make_unique<TimeTool>(); });
    registry.registerFactory(
        "list_directory",
        [directories] {
            return std::make_unique<ListDirectoryTool>(directories);
        });
    registry.registerFactory(
        "text_stats",
        [] { return std::make_unique<TextStatsTool>(); });
}

} // namespace

int main(int argc, char **argv) {
    try {
        const RunnerOptions options = parseOptions(argc, argv);
        const auto tasks =
            oop_agent::harness::loadBenchmarkTasks(options.tasks_file);
        validateTasks(tasks);
        prepareWorkspace(options);

        oop_agent::client::OllamaConfig llm_config;
        llm_config.base_url = options.ollama_url;
        llm_config.model_name = options.model;
        oop_agent::client::OllamaClient llm_client(llm_config);

        oop_agent::client::OllamaEmbeddingConfig embedding_config;
        embedding_config.base_url = options.ollama_url;
        embedding_config.model_name = options.embedding_model;
        auto embedder =
            std::make_shared<oop_agent::client::OllamaEmbeddingClient>(
                embedding_config);

        oop_agent::tools::MemoryToolConfig memory_config;
        memory_config.database_path =
            options.workspace / "data" / "memory.sqlite3";
        auto memory_store =
            oop_agent::tools::makeSqliteMemoryStore(memory_config);

        oop_agent::tools::ToolRegistry registry;
        registerTools(
            registry, options, memory_store, embedder);

        oop_agent::skills::SkillLoader skill_loader(
            options.project_root / "skills");
        oop_agent::agent::AgentLoopConfig agent_config;
        agent_config.max_steps = 10;
        oop_agent::agent::AgentLoop agent(
            llm_client, registry, skill_loader, agent_config);

        oop_agent::harness::NativeEnvironment environment(
            options.workspace,
            {"result.txt", "summary.txt", "output"});
        oop_agent::harness::HarnessRunner harness(
            agent,
            environment,
            options.results_directory,
            options.model);
        harness.registerEvaluator(
            std::make_unique<oop_agent::harness::KeywordEvaluator>());
        harness.registerEvaluator(
            std::make_unique<oop_agent::harness::FunctionalEvaluator>());

        // Shell-based exec/evaluation must resolve relative paths inside the
        // same isolated workspace used by the file tools.
        fs::current_path(options.workspace);
        const auto result = harness.runBatch(tasks);

        std::cout << "\nBenchmark complete\n"
                  << "Passed: " << result.passedTasks() << "/"
                  << result.totalTasks() << '\n'
                  << "Success rate: " << result.successRate() << "%\n"
                  << "Summary: "
                  << (options.results_directory / "benchmark_summary.json")
                  << '\n';
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "Benchmark runner error: " << error.what() << '\n';
        return 2;
    }
}
