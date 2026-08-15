# HƯỚNG DẪN CÀI ĐẶT VÀ CHẠY ĐỒ ÁN OOP AI AGENT (WINDOWS)

Tài liệu này hướng dẫn chi tiết cách cài đặt môi trường, biên dịch source code C++ và vận hành đồ án **OOP AI Agent Framework** trên máy tính sử dụng hệ điều hành **Windows**.

---

## 📋 I. Yêu Cầu Tiền Đề (Prerequisites)

Trước khi bắt đầu, máy tính của bạn cần được cài đặt các công cụ sau:

1. **Visual Studio 2022** (hoặc VS 2019):
   * Tải về tại: [Visual Studio Community](https://visualstudio.microsoft.com/vs/community/)
   * Khi cài đặt, bắt buộc tích chọn Workload: **Desktop development with C++** (Lập trình ứng dụng Desktop với C++).
2. **Git for Windows**:
   * Tải về tại: [git-scm.com](https://git-scm.com/)
3. **CMake** (phiên bản 3.20 trở lên):
   * Tải về tại: [cmake.org](https://cmake.org/download/)
   * *(Lưu ý: Tích chọn "Add CMake to system PATH" trong quá trình cài đặt)*

---

## 🛠️ II. Cài Đặt Thư Viện Phụ Thuộc Bằng vcpkg

Đồ án sử dụng 3 thư viện C++ chính: `libcurl` (gửi HTTP), `nlohmann-json` (xử lý JSON), và `sqlite3` (lưu trữ Memory). Cách dễ nhất để cài đặt trên Windows là dùng **vcpkg**.

Mở **PowerShell** hoặc **Command Prompt (cmd)** với quyền Admin (hoặc người dùng thường) và thực hiện các bước sau:

### Bước 1: Tải và khởi tạo vcpkg
```powershell
# Di chuyển đến thư mục muốn lưu vcpkg (Ví dụ: C:\vcpkg)
cd C:\
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
```

### Bước 2: Cài đặt các thư viện C++ (x64-windows)
```powershell
.\vcpkg install curl nlohmann-json sqlite3 --triplet x64-windows
```

---

## 🏗️ III. Biên Dịch Project (Build Code)

### Bước 1: Mở Terminal tại thư mục Đồ Án
Mở **PowerShell** và di chuyển vào thư mục dự án `OOP---Project`:
```powershell
cd "D:\Path\To\OOP---Project"  # Thay bằng đường dẫn tới project của bạn
```

### Bước 2: Tạo cấu hình CMake với vcpkg Toolchain
Thay `C:/vcpkg` bằng đường dẫn thực tế tới thư mục vcpkg bạn đã cài ở **Phần II**:

```powershell
cmake -B build-vcpkg -S . -DCMAKE_TOOLCHAIN_FILE="C:/vcpkg/scripts/buildsystems/vcpkg.cmake"
```

### Bước 3: Biên dịch chương trình (Build Release)
```powershell
cmake --build build-vcpkg --config Release
```

Sau khi biên dịch xong, các file thực thi (`.exe`) sẽ được tạo ra tại thư mục:
`.\build-vcpkg\Release\`

---

## 🚀 IV. Hướng Dẫn Chạy Đồ Án

> ⚠️ **LƯU Ý:** Đảm bảo bạn đã khởi động Google Colab chứa Ollama và nhận được **Cloudflare Tunnel URL** (Ví dụ: `https://xxxx-xxxx.trycloudflare.com`).

### 1. Chạy Single Agent (Giao diện dòng lệnh CLI)
Chạy agent xử lý 1 task đơn lẻ do bạn tự nhập:

```powershell
.\build-vcpkg\Release\oop_agent.exe `
  --task "Tính 15 * 17 và lưu kết quả vào file result.txt" `
  --workspace runtime_workspace `
  --ollama-url https://xxxx-xxxx.trycloudflare.com `
  --model qwen2.5:7b `
  --embedding-model nomic-embed-text
```

* **Kết quả:** Agent sẽ suy luận, tự gọi tool tính toán và lưu file `result.txt` vào trong thư mục `runtime_workspace/result.txt`.

---

### 2. Chạy Đánh Giá Tự Động Benchmark (10 Tasks)
Đánh giá tỷ lệ thành công của Agent trên tập 10 task định nghĩa trong `benchmark/tasks.json`:

```powershell
.\build-vcpkg\Release\oop_agent_benchmark.exe `
  --ollama-url https://xxxx-xxxx.trycloudflare.com `
  --model qwen2.5:7b `
  --embedding-model nomic-embed-text
```

* **Kết quả:** Chương trình sẽ lần lượt chạy 10 tasks, in ra bảng kết quả `PASS` / `FAIL` và lưu báo cáo đầy đủ tại `benchmark/results/benchmark_summary.json`.

---

### 3. Chạy Kiểm Thử Unit Test (Nếu Thầy/Cô yêu cầu Demo)
Để kiểm tra tính nguyên vẹn của từng module C++ (Client, Tools, Core, Harness):

```powershell
ctest --test-dir build-vcpkg -C Release
```
*(Nếu hiển thị 100% tests passed là đạt chuẩn)*

---

## ❓ V. Các Sự Cố Thường Gặp & Cách Khắc Phục (Troubleshooting)

1. **Lỗi `Failed to connect to host` / `HTTP 530`:**
   * Nguyên nhân: Tunnel Cloudflare trên Colab bị ngắt kết nối hoặc hết hạn.
   * Khắc phục: Chạy lại cell trên Google Colab để lấy URL Cloudflare mới và thay vào `--ollama-url`.

2. **Lỗi `CMake Error: Could not find a package configuration file...`:**
   * Nguyên nhân: Chưa truyền đúng đường dẫn `vcpkg.cmake`.
   * Khắc phục: Kiểm tra lại đường dẫn trong lệnh `cmake -B ... -DCMAKE_TOOLCHAIN_FILE="..."`.

3. **Lỗi Font chữ Tiếng Việt hiển thị lạ trên Terminal Windows:**
   * Khắc phục: Nhập lệnh `chcp 65001` trong PowerShell trước khi chạy chương trình để bật mã hóa UTF-8.
