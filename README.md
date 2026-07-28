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

Repo dang o giai doan trien khai cac module cot loi. `OllamaClient` da co logic HTTP POST text-only den Ollama `/api/chat`. `OllamaEmbeddingClient` goi `/api/embed` voi model mac dinh `nomic-embed-text` va tra ve `std::vector<double>`. Cac interface, tool, AgentLoop va benchmark Harness da san sang de chay tap 10 task tu dong.

## Module chinh

- `src/client`: `LLMClient`, `OllamaClient`, `EmbeddingClient`, `OllamaEmbeddingClient`.
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
registry.registerFactory("calculator", [] {
    return std::make_unique<oop_agent::tools::CalculatorTool>();
});

oop_agent::tools::WebSearchConfig search;
search.base_url = "http://localhost:8080"; // SearXNG instance
registry.registerFactory("web_search", [search] {
    return std::make_unique<oop_agent::tools::WebSearchTool>(search);
});

oop_agent::tools::MemoryToolConfig memory_config;
memory_config.database_path = "data/memory.sqlite3";
auto memory_store = oop_agent::tools::makeSqliteMemoryStore(memory_config);
auto embedder =
    std::make_shared<oop_agent::client::OllamaEmbeddingClient>();
registry.registerFactory("memory_save", [memory_store, memory_config, embedder] {
    return std::make_unique<oop_agent::tools::MemorySaveTool>(
        memory_store, memory_config, embedder);
});
registry.registerFactory("memory_search", [memory_store, memory_config, embedder] {
    return std::make_unique<oop_agent::tools::MemorySearchTool>(
        memory_store, memory_config, embedder);
});

// Deny-list luon duoc uu tien hon allow-list.
registry.deny({"exec"});
```

Contract argument hien tai:

- `exec`: `command`.
- `calculator`: `expression`, ho tro `+`, `-`, `*`, `/`, ngoac tron va dau am/duong don ngoi.
- `read_file`: `path`.
- `write_file`: `path`, `content`, va `append` tuy chon (`true`/`false`).
- `web_search`: `query` va `max_results` tuy chon (mac dinh 5, toi da 10).
- `memory_save`: `content` va `tags` tuy chon.
- `memory_search`: `query` va `limit` tuy chon (mac dinh 5, toi da 20).
- `time`: khong co argument.
- `list_directory`: `path` va `max_entries` tuy chon.
- `text_stats`: `text`.

File tool mac dinh chan duong dan thoat khoi `root_directory` va gioi han doc 1 MiB.
`WebSearchTool` goi `GET /search` cua SearXNG voi `format=json`. SearXNG instance can bat JSON format; neu bi tat, server co the tra ve HTTP 403.
Memory duoc luu ben vung trong SQLite. Embedding nam trong bang `memory_embeddings`, lien ket mot-mot voi `memories`. `memory_search` embed query, tinh cosine similarity trong C++ va sap xep giam dan. Neu embedding client loi hoac database cu chua co vector, tool tu dong fallback sang keyword search trong content/tags. Co the tat fallback bang `memory_config.fallback_to_keyword_search = false`.

## Embedding voi Ollama

`EmbeddingClient` la abstraction de Persistent Memory khong phu thuoc truc tiep vao Ollama. Client mac dinh dung API hien hanh `/api/embed`:

```cpp
oop_agent::client::OllamaEmbeddingConfig config;
config.base_url = "http://localhost:11434";
config.endpoint = "/api/embed";
config.model_name = "nomic-embed-text";
config.timeout_seconds = 30;

oop_agent::client::OllamaEmbeddingClient embedder(config);
const auto result = embedder.embed("Noi dung can tao vector");
if (result.success) {
    const std::vector<double> &vector = result.embedding;
}
```

Can tai model mot lan truoc khi chay that: `ollama pull nomic-embed-text`. Unit test cua client dung fake HTTP transport, nen khong can Ollama hay model that.

## Dependencies

Module `OllamaClient`, `OllamaEmbeddingClient` va `WebSearchTool` dung `libcurl` de gui HTTP request va `nlohmann/json` de xu ly JSON. Hai memory tool dung SQLite3.

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

## Chay benchmark 10 task

Chuan bi Ollama:

```bash
ollama pull qwen2.5:7b
ollama pull nomic-embed-text
```

Chay toan bo benchmark:

```bash
./build/oop_agent_benchmark
```

Runner tu dong:

1. Doc va validate dung 10 task trong `benchmark/tasks.json`.
2. Tao workspace sach tai `benchmark/runtime_workspace`.
3. Dang ky tat ca tool, KeywordEvaluator va FunctionalEvaluator.
4. Chay tung task voi `max_steps` rieng.
5. Ghi `trajectory_<task_id>.json` cho moi task.
6. Ghi success rate tong hop vao `benchmark/results/benchmark_summary.json`.

Tuy chinh backend:

```bash
./build/oop_agent_benchmark \
  --ollama-url http://localhost:11434 \
  --model qwen2.5:7b \
  --embedding-model nomic-embed-text \
  --searxng-url http://localhost:8080
```

Dung `./build/oop_agent_benchmark --help` de xem tat ca tuy chon. SearXNG
khong bat buoc cho 10 task hien tai, nhung `web_search` van duoc dang ky de
demo ToolRegistry day du.

## Git theo doi dong gop

Repo co them tai lieu de gioi thieu cach theo doi lich su commit/push tung thanh vien:

- [Git Workflow](docs/git-workflow.md)
- [Contribution Log Template](docs/contribution-log-template.md)
- [Contributing Guide](CONTRIBUTING.md)
