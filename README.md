# AI Agent with Ollama API

An Object-Oriented AI Agent framework developed from scratch in modern C++ (C++17/20/23), featuring a ReAct reasoning loop, dynamic tool and skill registration, loop detection, evaluation harness, persistent vector memory, and multi-agent coordination.

---

## 1. Team Members

| Student ID | Full Name | Task |
| :--- | :--- | :--- | 
| *24127059* | *Dư Bá Khoa* | Harness Layer & Evaluator, Environment, Trajectory format, GUI Agent |
| *24127511* | *Trần Hữu Phước* | LLM Client Layer, Agent Loop, Skill System, Multi – Agent Coordination |
| *24127579* | *Nguyễn Thành Tuân* | Tool Registry Layer, 5 Basic Tools, Loop Detection, Persistent Memory with Vector Search |

---

## 2. Project Overview

The **AI Agent Framework** is designed to model modern agentic architectures (such as OpenClaw, LangChain, and LlamaIndex) using pure C++ without heavy external agent runtimes. The agent connects to an Ollama LLM server, accepts human goals, autonomously plans execution, calls tools, handles failures, and reports reproducible trajectories.

### Key Architectural Layers
1. **LLM Client Layer**: Decoupled HTTP interface supporting chat completions (`/api/chat`) and vector embeddings (`/api/embed`).
2. **Tool Layer**: Registry and dynamic execution system managing sandboxed file I/O, mathematical computation, shell commands, web search, and vector-based persistent memory.
3. **Skill Layer**: Dynamic prompt injection matching user goals against Markdown skill definitions.
4. **Agent Loop Layer**: ReAct execution cycle (`Observe -> Think -> Act -> Observe`) equipped with loop detection algorithms.
5. **Harness & Evaluation Layer**: Isolated benchmarking infrastructure recording step-by-step trajectories and scoring task completion.

### Design Patterns Implemented
- **Strategy Pattern**: 
  - `Evaluator` hierarchy (`KeywordEvaluator`, `FunctionalEvaluator`, `VLMEvaluator`) encapsulates evaluation strategies behind a unified `evaluate()` interface.
  - `Environment` hierarchy (`NativeEnvironment`, `SandboxEnvironment`) enables interchangeable execution spaces.
- **Template Method Pattern**: `AgentLoop::run()` defines the overarching skeleton of the ReAct cycle, delegating individual steps (`think()`, `act()`, `observe()`) to customizable methods.
- **Registry / Factory Pattern**: `ToolRegistry` dynamically stores tool factory callbacks by name at runtime and applies allow/deny security policies.
- **Observer / Hook Pattern**: `HarnessRunner` injects a `StepHook` into `AgentLoop` to intercept events and record detailed execution trajectories without coupling the agent to the benchmark framework.

### Advanced Capabilities (Bonus Features)
- **Persistent Memory with Vector Search**: Embeddings are computed via `OllamaEmbeddingClient` (`nomic-embed-text`), stored in an SQLite database, and retrieved using in-memory Cosine Similarity with automatic fallback to keyword search.
- **Multi-Agent Coordination**: `MultiAgentCoordinator` spawns independent `AgentLoop` instances on dedicated `std::thread`s, communicating asynchronously through thread-safe `MessageQueue`s.

---

## 3. Requirements

- **C++17+ Compiler**: Microsoft Visual Studio 2022 / 2019 (MSVC v142/v143 with C++ Desktop Development workload), or GCC 10+ / Clang 11+.
- **CMake**: Version 3.20 or higher.
- **Git**: Git for Windows or Linux.
- **libcurl**: HTTP/REST transport for Ollama communication and web requests.
- **nlohmann/json**: Modern C++ JSON parsing, payload building, and trajectory logging.
- **SQLite3**: Lightweight relational database for persistent memory and vector embeddings.
- **Ollama**: Backend inference engine running locally or remotely (e.g., via Google Colab with GPU).

---

## 4. Installation

The project uses [vcpkg](https://github.com/microsoft/vcpkg) for seamless C++ dependency management on Windows.

### Step 1: Install and Bootstrap vcpkg
Open **PowerShell** and clone `vcpkg`:
```powershell
cd C:\
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
```

### Step 2: Install Project Dependencies
Install the 64-bit Windows static/dynamic libraries:
```powershell
.\vcpkg install curl nlohmann-json sqlite3 --triplet x64-windows
```

*(Note for Linux/Debian users: dependencies can alternatively be installed via `sudo apt-get install libcurl4-openssl-dev nlohmann-json3-dev libsqlite3-dev`)*

---

## 5. Configuration

### Backend Setup: Running Ollama on Google Colab with Cloudflare Tunnel
To leverage free cloud GPU acceleration, run Ollama on Google Colab and expose it to your local laptop via a secure Cloudflare Tunnel:

#### Step 1: Install and Launch Ollama on Colab
Execute the following commands in Google Colab cells:
```python
# 1. Update packages and install zstd
!sudo apt-get update
!sudo apt-get install -y zstd

# 2. Install Ollama
!curl -fsSL https://ollama.com/install.sh | sh

# 3. Verify installation
!which ollama
!ollama --version

# 4. Start Ollama server in background (binding to all interfaces)
!nohup bash -c 'OLLAMA_HOST=0.0.0.0:11434 ollama serve' > ollama.log 2>&1 &

# 5. Check if server is running
!curl http://127.0.0.1:11434/api/tags

# 6. Pull the required models
!ollama pull qwen2.5:7b
!ollama pull nomic-embed-text

# 7. List installed models
!ollama list
```

#### Step 2: Create a Cloudflare Tunnel
Run Cloudflared on Colab to expose port `11434` to the internet:
```python
# 8. Download and make cloudflared executable
!wget -q https://github.com/cloudflare/cloudflared/releases/latest/download/cloudflared-linux-amd64 -O cloudflared
!chmod +x cloudflared

# 9. Verify cloudflared version
!./cloudflared --version

# 10. Start Cloudflare tunnel forwarding to port 11434
!nohup ./cloudflared tunnel --url http://127.0.0.1:11434 --no-autoupdate > cloudflared.log 2>&1 &

# 11. Extract your public tunnel URL
!grep -o 'https://[-a-zA-Z0-9]*\.trycloudflare\.com' cloudflared.log
```

#### Step 3: Verify the Tunnel Endpoint
The command above will output a public URL, for example:
`https://conditional-hazards-ship-trailer.trycloudflare.com`

Verify connectivity directly in Colab:
```python
!curl https://<your-tunnel-url>.trycloudflare.com/api/tags
```
If this returns JSON containing `qwen2.5:7b` and `nomic-embed-text:latest`, your remote backend is ready. Simply copy this URL to pass to the local AI Agent on your laptop using the `--ollama-url` parameter.

---

## 6. Build

### 6.1 Windows Build (Visual Studio & vcpkg)
Build the project from source on Windows using CMake and the vcpkg toolchain file.

#### Step 1: Open Terminal in Project Directory
```powershell
cd "D:\Path\To\OOP---Project"
```

#### Step 2: Configure Project with CMake
Point `CMAKE_TOOLCHAIN_FILE` to your vcpkg installation:
```powershell
cmake -B build-vcpkg -S . -DCMAKE_TOOLCHAIN_FILE="C:/vcpkg/scripts/buildsystems/vcpkg.cmake"
```

#### Step 3: Compile Release Binaries
```powershell
cmake --build build-vcpkg --config Release
```

All compiled binaries will be located under `.\build-vcpkg\Release\`:
- `oop_agent.exe` (Single task CLI agent)
- `oop_agent_benchmark.exe` (Automated 10-task evaluation runner)
- Unit test executables (`oop_agent_client_tests.exe`, `oop_agent_tool_tests.exe`, `oop_agent_core_tests.exe`, `oop_agent_harness_tests.exe`)

#### Step 4: Run Unit Tests
Validate module integrity by running CTest:
```powershell
ctest --test-dir build-vcpkg -C Release --output-on-failure
```
*(Expected output: 100% tests passed)*

---

### 6.2 Linux Build with GUI Support (Ubuntu Virtual Machine on VMware)
For demonstrating the Desktop GUI Automation Agent (+8 bonus points), the system is configured and built on an **Ubuntu Linux Virtual Machine (VMware Workstation)** within an **X11 (Xorg)** desktop session.

#### Step 1: Install System Dependencies
Install compiler toolchains, C++ libraries, `libxdo` (for mouse/keyboard automation), and `scrot` (for desktop screen capture):
```bash
sudo apt update
# Install desktop automation (libxdo) and screenshot utility (scrot)
sudo apt install -y libxdo-dev scrot
# Install C++ development toolchain and core libraries
sudo apt install -y build-essential cmake libcurl4-openssl-dev nlohmann-json3-dev libsqlite3-dev
```

#### Step 2: Configure and Compile with GUI Enabled
If you copied the project folder directly from Windows, clean any Windows cache artifacts first:
```bash
rm -rf build build-vcpkg
```
Then configure and compile with the `OOP_AGENT_ENABLE_GUI` option enabled:
```bash
cmake -B build -S . -DOOP_AGENT_ENABLE_GUI=ON
cmake --build build
```
This generates the core binaries (`./build/oop_agent`, `./build/oop_agent_benchmark`) as well as the specialized `oop_agent_gui_tests` executable.

---

## 7. Run

### 7.1 Running on Windows (PowerShell)

#### 1. Interactive Single-Task Agent (`oop_agent.exe`)
Run the agent on a specific natural language instruction:
```powershell
.\build-vcpkg\Release\oop_agent.exe `
  --task "Calculate 15 * 17 and save the result to result.txt" `
  --workspace runtime_workspace `
  --ollama-url https://<your-tunnel-url>.trycloudflare.com `
  --model qwen2.5:7b `
  --embedding-model nomic-embed-text
```
The agent reasons through the prompt, selects the `calculator` tool, generates `255`, executes `write_file`, and saves the file under `runtime_workspace/result.txt`.

#### 2. Automated 10-Task Benchmark (`oop_agent_benchmark.exe`)
Run the full evaluation suite defined in `benchmark/tasks.json`:
```powershell
.\build-vcpkg\Release\oop_agent_benchmark.exe `
  --ollama-url https://<your-tunnel-url>.trycloudflare.com `
  --model qwen2.5:7b `
  --embedding-model nomic-embed-text
```
The harness iterates through all 10 tasks, generates detailed per-task trajectories in `benchmark/results/trajectory_<task_id>.json`, and produces a comprehensive summary in `benchmark/results/benchmark_summary.json`.

---

### 7.2 Running on Ubuntu Linux Virtual Machine (VMware / Bash)

#### 1. Interactive Single-Task Agent (`./build/oop_agent`)
Execute tasks on Linux using standard Bash syntax (using forward slashes `/` and backslashes `\` for line breaks):
```bash
./build/oop_agent \
  --task "Calculate 15 * 17 and save the result to result.txt" \
  --workspace runtime_workspace \
  --ollama-url https://<your-tunnel-url>.trycloudflare.com \
  --model qwen2.5:7b \
  --embedding-model nomic-embed-text
```

#### 2. Automated 10-Task Benchmark (`./build/oop_agent_benchmark`)
Run the automated evaluation benchmark on Ubuntu Linux:
```bash
./build/oop_agent_benchmark \
  --ollama-url https://<your-tunnel-url>.trycloudflare.com \
  --model qwen2.5:7b \
  --embedding-model nomic-embed-text
```
The results will be written to `benchmark/results/benchmark_summary.json` and `benchmark/results/trajectory_<task_id>.json`.

#### 3. GUI Agent Live Desktop Demo (X11 Desktop Session)
The Desktop GUI Agent is evaluated inside an Ubuntu Virtual Machine running on VMware Workstation. Make sure your desktop session is running under X11 (`echo $XDG_SESSION_TYPE` outputs `x11`).

##### A. Automated Mouse & Screen Capture Verification
Run the dedicated GUI unit test to verify that `ScreenshotTool` and `ActionExecutor` correctly drive the desktop cursor and capture real framebuffers:
```bash
./build/oop_agent_gui_tests
```
* **Visual observation:** The cursor automatically moves to coordinates `(100, 100)`, dispatches a mouse click, and takes a full-screen screenshot.
* **Inspect the captured screenshot:**
  ```bash
  xdg-open output/screenshots/gui_test.png
  ```

##### B. Autonomous Browser Launch, Search & Screen Capture
Instruct the agent to launch the Firefox browser, query Google for "HCMUS", allow rendering, and record the live visual state to `hcmus.png`:
```bash
./build/oop_agent \
  --task "First use tool exec to run: nohup firefox 'https://www.google.com/search?q=HCMUS' >/dev/null 2>&1 & . Then use tool exec to run: sleep 4 && scrot hcmus.png" \
  --workspace runtime_workspace \
  --ollama-url https://<your-tunnel-url>.trycloudflare.com \
  --model qwen2.5:7b
```
* **Visual observation:** The Firefox browser automatically launches, navigates to the Google Search results for HCMUS, waits 4 seconds for page rendering, and `scrot` automatically captures the browser window into `hcmus.png`.
* **View the resulting image:**
  ```bash
  xdg-open hcmus.png
  ```

---

## 8. Available Tools

The `ToolRegistry` provides the following sandboxed capabilities:

| Tool Name | Arguments | Return Type | Description |
| :--- | :--- | :--- | :--- |
| `calculator` | `expression` (string) | `string` | Evaluates arithmetic expressions supporting `+`, `-`, `*`, `/`, unary operators, and parentheses. |
| `read_file` | `path` (string) | `string` | Reads file content with 1 MiB boundary protection within the configured workspace. |
| `write_file` | `path` (string), `content` (string), `append` (optional bool) | `string` | Writes or appends text content safely inside workspace boundaries. |
| `exec` | `command` (string) | `ToolResult` | Executes system shell commands and captures combined stdout/stderr and exit codes. |
| `web_search` | `query` (string), `max_results` (optional int) | `string` (JSON) | Queries a SearXNG instance for real-time web information. |
| `memory_save` | `content` (string), `tags` (optional string) | `string` | Stores text into SQLite and automatically generates vector embeddings. |
| `memory_search`| `query` (string), `limit` (optional int) | `string` | Semantic similarity search using Cosine Similarity with keyword fallback. |
| `time` | *(None)* | `string` | Returns the current formatted system timestamp. |
| `list_directory` | `path` (string), `max_entries` (optional int) | `string` | Lists directory contents with safety checks against path traversal. |
| `text_stats` | `text` (string) | `string` | Computes statistical metrics (line count, word count, character count). |

---

## 9. Available Skills

The `SkillLoader` scans the `skills/` directory for modular Markdown instruction files. Skills are selected dynamically per task via keyword matching and injected into the agent's system prompt prior to execution:

- **`skills/task_planner.md`**: Guides the agent to decompose complex user prompts into discrete, verifiable sub-goals.
- **`skills/error_recovery.md`**: Provides recovery protocols when tool invocations fail (e.g., fallback actions, inspecting error messages).
- **`skills/tool_usage.md`**: Establishes strict JSON formatting requirements, tool parameter schemas, and termination guidelines.

---

## 10. Benchmark

The benchmark harness tests the robustness, reasoning, and tool proficiency of the agent across 10 diverse tasks categorized by difficulty:

### Task Distribution
- **Simple (Tasks 1–4)**: Direct tool invocations (arithmetic calculation, line counting, timestamp retrieval, workspace listing).
- **Medium (Tasks 5–8)**: Multi-step tool chaining (calculating then writing to file, text analysis, semantic memory retrieval, reading skill files).
- **Hard (Tasks 9–10)**: Autonomous ordering and multi-hop execution (chained arithmetic, file generation, and memory storage; conditional file checking and summarization).

### Evaluation Engines
- **`KeywordEvaluator`**: Validates whether essential keywords appear in the agent's final answer.
- **`FunctionalEvaluator`**: Verifies physical side effects within the workspace (file existence, exact content verification, non-empty outputs).

---

## 11. Example Output

### ReAct Execution Trace (Task 1: Calculate 15 * 17)
```json
{
  "thought": "I need to calculate 15 * 17 and save the result to result.txt.",
  "action": {
    "type": "tool_call",
    "tool": "calculator",
    "args": {
      "expression": "15 * 17"
    }
  }
}
```
**Observation**:
```json
{
  "type": "tool_result",
  "success": true,
  "output": "255",
  "error": ""
}
```

```json
{
  "thought": "The result is 255. Now I will write this value to result.txt.",
  "action": {
    "type": "tool_call",
    "tool": "write_file",
    "args": {
      "path": "result.txt",
      "content": "255",
      "append": false
    }
  }
}
```
**Observation**:
```json
{
  "type": "tool_result",
  "success": true,
  "output": "wrote 3 bytes to runtime_workspace/result.txt",
  "error": ""
}
```

```json
{
  "thought": "The file result.txt has been written successfully. The task is complete.",
  "action": {
    "type": "final",
    "answer": "The calculation 15 * 17 equals 255, and the result has been saved to result.txt."
  }
}
```

### Benchmark Summary Output (`benchmark_summary.json`)
```json
{
  "total_tasks": 10,
  "passed_tasks": 10,
  "failed_tasks": 0,
  "success_rate": 1.0,
  "total_duration_ms": 54210,
  "tasks": [
    {
      "id": "task_001",
      "difficulty": "simple",
      "status": "PASS",
      "score": 1.0,
      "steps_taken": 3,
      "feedback": "Expected file content found."
    }
  ]
}
```
