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

Repo dang o buoc scaffold kien truc va bat dau trien khai module client. `OllamaClient` da co logic HTTP POST text-only den Ollama `/api/chat`, cac module khac van giu khung lop de tiep tuc phat trien theo tung task.

## Module chinh

- `src/client`: `LLMClient`, `OllamaClient`.
- `src/tools`: `Tool`, `ToolRegistry`, 5 tool bat buoc va 3 tool mo rong.
- `src/skills`: `SkillLoader`.
- `src/agent`: `AgentLoop`, `LoopDetector`.
- `src/harness`: `HarnessRunner`, `Trajectory`, `Evaluator`, `Environment`, hook va recorder.

## Dependencies

Module `OllamaClient` dung `libcurl` de gui HTTP POST va `nlohmann/json` de xu ly JSON.

Tren MSYS2 CLANG64 co the cai bang:

```bash
pacman -S --needed mingw-w64-clang-x86_64-cmake mingw-w64-clang-x86_64-curl mingw-w64-clang-x86_64-nlohmann-json
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
