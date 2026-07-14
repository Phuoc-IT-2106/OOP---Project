# OOP AI Agent Framework

Do an mon Lap trinh Huong doi tuong 2026: xay dung mot AI Agent framework bang C++17+ theo kien truc phan lop.

## Muc tieu

- Thiet ke framework agent khong chi goi API va in ket qua.
- Ket noi Ollama API hoac API tuong thich OpenAI thong qua lop `LLMClient`.
- Tach ro cac tang: client, tools, skills, agent loop, loop detector, harness, evaluator, recorder.
- The hien OOP: abstract class, inheritance, composition, polymorphism, separation of concerns.
- Ap dung cac design pattern bat buoc: Strategy, Template Method, Registry / Factory, Observer / Hook.

## Cau truc repo

```text
src/                 Header va implementation theo tung module
skills/              Markdown instruction files cho SkillLoader
benchmark/           Task benchmark va runner danh gia
docs/                UML, sequence diagram, component diagram va bao cao
```

## Trang thai hien tai

Repo dang o giai doan trien khai cac module cot loi. `OllamaClient` da co logic HTTP POST text-only den Ollama `/api/chat`. Cac interface `Tool`, `LLMClient`, `Evaluator`, `ToolRegistry`, `ExecTool`, `ReadFileTool`, `WriteFileTool`, `WebSearchTool`, `MemorySaveTool` va `MemorySearchTool` da san sang de tich hop vao `AgentLoop`.

## Module chinh

- `src/client`: `LLMClient`, `OllamaClient`.
- `src/tools`: `Tool`, `ToolRegistry`, 5 tool bat buoc va 3 tool mo rong.
- `src/skills`: `SkillLoader`.
- `src/agent`: `AgentLoop`, `LoopDetector`.
- `src/harness`: `HarnessRunner`, `Trajectory`, `Evaluator`, `Environment`, hook va recorder.

## Dang ky tool dong

`ToolRegistry` luu factory theo ten, tao tool khi can chay va ap dung allow/deny policy. Vi du composition root cua chuong trinh co the dang ky cac tool ma khong sua registry:

```cpp
oop_agent::tools::ToolRegistry registry;
oop_agent::tools::FileToolConfig files;
files.root_directory = "workspace";

registry.registerFactory("exec", [] {
    return std::make_unique<oop_agent::tools::ExecTool>();
});
registry.registerFactory("read_file", [files] {
    return std::make_unique<oop_agent::tools::ReadFileTool>(files);
});
registry.registerFactory("write_file", [files] {
    return std::make_unique<oop_agent::tools::WriteFileTool>(files);
});

oop_agent::tools::WebSearchConfig search;
search.base_url = "http://localhost:8080"; // SearXNG instance
registry.registerFactory("web_search", [search] {
    return std::make_unique<oop_agent::tools::WebSearchTool>(search);
});

oop_agent::tools::MemoryToolConfig memory_config;
memory_config.database_path = "data/memory.sqlite3";
auto memory_store = oop_agent::tools::makeSqliteMemoryStore(memory_config);
registry.registerFactory("memory_save", [memory_store, memory_config] {
    return std::make_unique<oop_agent::tools::MemorySaveTool>(memory_store, memory_config);
});
registry.registerFactory("memory_search", [memory_store, memory_config] {
    return std::make_unique<oop_agent::tools::MemorySearchTool>(memory_store, memory_config);
});

// Deny-list luon duoc uu tien hon allow-list.
registry.deny({"exec"});
```

Contract argument hien tai:

- `exec`: `command`.
- `read_file`: `path`.
- `write_file`: `path`, `content`, va `append` tuy chon (`true`/`false`).
- `web_search`: `query` va `max_results` tuy chon (mac dinh 5, toi da 10).
- `memory_save`: `content` va `tags` tuy chon.
- `memory_search`: `query` va `limit` tuy chon (mac dinh 5, toi da 20).

File tool mac dinh chan duong dan thoat khoi `root_directory` va gioi han doc 1 MiB.
`WebSearchTool` goi `GET /search` cua SearXNG voi `format=json`. SearXNG instance can bat JSON format; neu bi tat, server co the tra ve HTTP 403.
Memory duoc luu ben vung trong SQLite. `memory_search` tim keyword literal trong ca noi dung va tags, uu tien memory moi nhat.

## Dependencies

Module `OllamaClient` va `WebSearchTool` dung `libcurl` de gui HTTP request va `nlohmann/json` de xu ly JSON. Hai memory tool dung SQLite3.

Tren MSYS2 CLANG64 co the cai bang:

```bash
pacman -S --needed mingw-w64-clang-x86_64-cmake mingw-w64-clang-x86_64-curl mingw-w64-clang-x86_64-nlohmann-json mingw-w64-clang-x86_64-sqlite3
```

Build voi CMake:

```bash
cd /c/OOP\ -\ Project/OOP---Project
cmake -S . -B build
cmake --build build
```

## Git theo doi dong gop

Repo co them tai lieu de gioi thieu cach theo doi lich su commit/push tung thanh vien:

- [Git Workflow](docs/git-workflow.md)
- [Contribution Log Template](docs/contribution-log-template.md)
- [Contributing Guide](CONTRIBUTING.md)
