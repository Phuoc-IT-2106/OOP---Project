# Architecture Overview

He thong duoc chia thanh cac tang doc lap:

- `client`: abstraction cho LLM backend.
- `tools`: tool interface, registry, factory va cac tool cu the.
- `skills`: load va chon markdown instruction files.
- `agent`: ReAct loop, prompt builder va loop detector.
- `harness`: setup environment, run agent, evaluate va record.
- `evaluator`: Strategy hierarchy cho cac cach cham diem.
- `recorder`: ghi trajectory JSON.
- `environment`: abstraction cho native/sandbox execution.

Nguyen tac thiet ke:

- `AgentLoop` khong phu thuoc `HarnessRunner`.
- Tool khong phu thuoc `AgentLoop`.
- Evaluator chi nhin ket qua va artifacts.
- LLM backend co the thay bang class moi ke thua `LLMClient`.
