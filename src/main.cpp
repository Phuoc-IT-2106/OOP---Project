#include "agent/agent_loop.h"
#include "client/ollama_client.h"
#include "client/ollama_embedding_client.h"
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

#include <concepts>
#include <cstdlib>
#include <expected>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

namespace fs = std::filesystem;

#if defined(__cpp_deleted_function) && __cpp_deleted_function >= 202403L
void legacyPositionalOnlyCli() = delete(
    "Use --task so the demo command is explicit and reproducible.");
#endif

template <typename T>
concept StringFragment = requires(const T &value) {
    std::string_view(value);
};

struct CliOptions {
#ifdef OOP_AGENT_SOURCE_DIR
    fs::path project_root{OOP_AGENT_SOURCE_DIR};
#else
    fs::path project_root{fs::current_path()};
#endif
    fs::path workspace{fs::current_path()};
    fs::path skills_directory;
    std::string task;
    std::string ollama_url{"http://localhost:11434"};
    std::string model{"qwen2.5:7b"};
    std::string embedding_model{"nomic-embed-text"};
    std::string searxng_url{"http://localhost:8080"};
    std::size_t max_steps{10};
    bool list_tools{false};
};

void printUsage(const char *program) {
    std::cout
        << "Usage: " << program << " --task \"your task\" [options]\n"
        << "Options:\n"
        << "  --task TEXT              Task for the agent to solve\n"
        << "  --workspace PATH         Root for file/list/memory tools\n"
        << "  --skills PATH            Directory containing .md skill files\n"
        << "  --ollama-url URL         Ollama base URL\n"
        << "  --model NAME             Ollama chat model\n"
        << "  --embedding-model NAME   Ollama embedding model\n"
        << "  --searxng-url URL        SearXNG base URL for web_search\n"
        << "  --max-steps N            ReAct step limit\n"
        << "  --list-tools             Print registered tools and exit\n"
        << "  --help                   Show this message\n";
}

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>

std::vector<std::string> getUtf8Args(int argc, char **argv) {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    int wide_argc = 0;
    LPWSTR *wide_argv = CommandLineToArgvW(GetCommandLineW(), &wide_argc);
    if (wide_argv == nullptr) {
        std::vector<std::string> fallback;
        for (int i = 0; i < argc; ++i) {
            fallback.push_back(argv[i]);
        }
        return fallback;
    }
    std::vector<std::string> utf8_args;
    utf8_args.reserve(wide_argc);
    for (int i = 0; i < wide_argc; ++i) {
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, wide_argv[i], -1, nullptr, 0, nullptr, nullptr);
        if (size_needed > 1) {
            std::string str(size_needed - 1, '\0');
            WideCharToMultiByte(CP_UTF8, 0, wide_argv[i], -1, &str[0], size_needed, nullptr, nullptr);
            utf8_args.push_back(std::move(str));
        } else {
            utf8_args.push_back({});
        }
    }
    LocalFree(wide_argv);
    return utf8_args;
}
#else
std::vector<std::string> getUtf8Args(int argc, char **argv) {
    std::vector<std::string> args;
    args.reserve(argc);
    for (int i = 0; i < argc; ++i) {
        args.push_back(argv[i]);
    }
    return args;
}
#endif

std::string requireValue(const std::vector<std::string> &args,
                         int &index,
                         const std::string &option) {
    if (index + 1 >= static_cast<int>(args.size())) {
        throw std::invalid_argument(option + " requires a value");
    }
    return args[++index];
}

std::expected<std::size_t, std::string> parsePositiveSize(
    const std::string &text,
    const std::string &option) {
    std::size_t consumed = 0;
    try {
        const auto value = std::stoull(text, &consumed);
        if (consumed != text.size() || value == 0) {
            return std::unexpected(option + " must be a positive integer");
        }
        return static_cast<std::size_t>(value);
    } catch (const std::exception &) {
        return std::unexpected(option + " must be a positive integer");
    }
}

std::optional<std::string> readNonEmptyEnvironment(const char *name) {
    const char *value = std::getenv(name);
    if (value == nullptr || std::string_view(value).empty()) {
        return std::nullopt;
    }
    return std::string(value);
}

template <StringFragment T>
std::string joinTaskParts(std::span<const T> parts) {
    std::string joined;
    for (std::size_t index = 0; index < parts.size(); ++index) {
        if (index != 0) {
            joined += ' ';
        }
        joined += std::string_view(parts[index]);
    }
    return joined;
}

CliOptions parseOptions(const std::vector<std::string> &args) {
    CliOptions options;
    options.project_root = fs::absolute(options.project_root);
    options.skills_directory = options.project_root / "skills";
    options.ollama_url =
        readNonEmptyEnvironment("OOP_AGENT_OLLAMA_URL").value_or(options.ollama_url);
    options.model =
        readNonEmptyEnvironment("OOP_AGENT_MODEL").value_or(options.model);
    options.embedding_model =
        readNonEmptyEnvironment("OOP_AGENT_EMBEDDING_MODEL")
            .value_or(options.embedding_model);
    options.searxng_url =
        readNonEmptyEnvironment("OOP_AGENT_SEARXNG_URL")
            .value_or(options.searxng_url);

    const auto env_max_steps =
        readNonEmptyEnvironment("OOP_AGENT_MAX_STEPS")
            .transform([](const std::string &value) {
                auto parsed = parsePositiveSize(value, "OOP_AGENT_MAX_STEPS");
                if (!parsed) {
                    throw std::invalid_argument(parsed.error());
                }
                return *parsed;
            });
    if (env_max_steps) {
        options.max_steps = *env_max_steps;
    }

    std::vector<std::string> positional_task;
    for (int index = 1; index < static_cast<int>(args.size()); ++index) {
        const std::string option = args[index];
        if (option == "--help") {
            printUsage(args[0].c_str());
            std::exit(0);
        }
        if (option == "--list-tools") {
            options.list_tools = true;
            continue;
        }

        if (option == "--task") {
            options.task = requireValue(args, index, option);
        } else if (option == "--workspace") {
            options.workspace = requireValue(args, index, option);
        } else if (option == "--skills") {
            options.skills_directory = requireValue(args, index, option);
        } else if (option == "--ollama-url") {
            options.ollama_url = requireValue(args, index, option);
        } else if (option == "--model") {
            options.model = requireValue(args, index, option);
        } else if (option == "--embedding-model") {
            options.embedding_model = requireValue(args, index, option);
        } else if (option == "--searxng-url") {
            options.searxng_url = requireValue(args, index, option);
        } else if (option == "--max-steps") {
            auto parsed =
                parsePositiveSize(requireValue(args, index, option),
                                  option);
            if (!parsed) {
                throw std::invalid_argument(parsed.error());
            }
            options.max_steps = *parsed;
        } else if (!option.empty() && option.front() == '-') {
            throw std::invalid_argument("unknown option: " + option);
        } else {
            positional_task.push_back(option);
        }
    }

    if (options.task.empty() && !positional_task.empty()) {
        options.task = joinTaskParts<std::string>(positional_task);
    }

    options.workspace = fs::absolute(options.workspace);
    options.skills_directory = fs::absolute(options.skills_directory);
    return options;
}

void registerDefaultTools(
    oop_agent::tools::ToolRegistry &registry,
    const CliOptions &options,
    const std::shared_ptr<oop_agent::tools::MemoryStore> &memory_store,
    const std::shared_ptr<oop_agent::client::EmbeddingClient> &embedder) {
    using namespace oop_agent::tools;

    FileToolConfig file_config;
    file_config.root_directory = options.workspace;

    ListDirectoryConfig list_config;
    list_config.root_directory = options.workspace;

    WebSearchConfig web_config;
    web_config.base_url = options.searxng_url;

    MemoryToolConfig memory_config;
    memory_config.database_path = options.workspace / "data" / "memory.sqlite3";

    registry.registerFactory("exec", [] {
        return std::make_unique<ExecTool>();
    });
    registry.registerFactory("read_file", [file_config] {
        return std::make_unique<ReadFileTool>(file_config);
    });
    registry.registerFactory("write_file", [file_config] {
        return std::make_unique<WriteFileTool>(file_config);
    });
    registry.registerFactory("web_search", [web_config] {
        return std::make_unique<WebSearchTool>(web_config);
    });
    registry.registerFactory("memory_save", [memory_store, memory_config, embedder] {
        return std::make_unique<MemorySaveTool>(
            memory_store, memory_config, embedder);
    });
    registry.registerFactory("memory_search", [memory_store, memory_config, embedder] {
        return std::make_unique<MemorySearchTool>(
            memory_store, memory_config, embedder);
    });
    registry.registerFactory("calculator", [] {
        return std::make_unique<CalculatorTool>();
    });
    registry.registerFactory("time", [] {
        return std::make_unique<TimeTool>();
    });
    registry.registerFactory("list_directory", [list_config] {
        return std::make_unique<ListDirectoryTool>(list_config);
    });
    registry.registerFactory("text_stats", [] {
        return std::make_unique<TextStatsTool>();
    });
}

void printTools(const oop_agent::tools::ToolRegistry &registry) {
    for (const auto &tool : registry.availableTools()) {
        std::cout << "- " << tool.name << ": " << tool.description << '\n';
    }
}

} // namespace

int main(int argc, char **argv) {
    try {
        const auto args = getUtf8Args(argc, argv);
        const CliOptions options = parseOptions(args);
        fs::create_directories(options.workspace / "data");

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
        memory_config.database_path = options.workspace / "data" / "memory.sqlite3";
        auto memory_store = oop_agent::tools::makeSqliteMemoryStore(memory_config);

        oop_agent::tools::ToolRegistry registry;
        registerDefaultTools(registry, options, memory_store, embedder);

        if (options.list_tools) {
            printTools(registry);
            return 0;
        }

        if (options.task.empty()) {
            printUsage(argv[0]);
            return 1;
        }

        oop_agent::skills::SkillLoader skill_loader(options.skills_directory);
        oop_agent::agent::AgentLoopConfig agent_config;
        agent_config.max_steps = options.max_steps;
        oop_agent::agent::AgentLoop agent(
            llm_client, registry, skill_loader, agent_config);

        // ExecTool uses the process working directory, so align it with the
        // same workspace enforced by the file and directory tools.
        fs::current_path(options.workspace);
        const auto result = agent.run(options.task);

        std::cout << "Model: " << options.model << '\n'
                  << "Steps: " << result.steps_taken << '\n'
                  << "Tokens: " << result.total_tokens << '\n';

        if (!result.selected_skills.empty()) {
            std::cout << "Skills:";
            for (const auto &skill : result.selected_skills) {
                std::cout << ' ' << skill;
            }
            std::cout << '\n';
        }

        if (!result.success) {
            std::cerr << "Agent failed: " << result.error_message << '\n';
            return 2;
        }

        std::cout << "\nFinal answer:\n" << result.final_answer << '\n';
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "oop_agent error: " << error.what() << '\n';
        return 2;
    }
}
