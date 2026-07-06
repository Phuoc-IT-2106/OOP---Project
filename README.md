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
include/oop_agent/   Header va interface public cua framework
src/                 Implementation theo tung module
skills/              Markdown instruction files cho SkillLoader
benchmark/           Task benchmark va runner danh gia
tests/               Unit/integration tests
docs/                UML, sequence diagram, component diagram va bao cao
config/              Cau hinh agent, tool policy va runtime path
scripts/             Script build, run, benchmark, format
cmake/               CMake helper modules
third_party/         Ghi chu dependency ben thu ba
```

## Trang thai hien tai

Repo dang o buoc scaffold kien truc. Cac file C++ hien la placeholder de dinh vi module, chua co logic trien khai.
