// Placeholder: abstract Environment interface.
#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

class Environment {
public:
    virtual ~Environment() = default;

    virtual void setup() = 0;

    virtual void cleanup() = 0;
};

class NativeEnvironment : public Environment {
public:
    void setup() override {
        // Có thể tạo thư mục output,
        // xóa kết quả cũ hoặc chuẩn bị file test.
    }

    void cleanup() override {
        // Dọn tài nguyên sau khi chạy task.
    }
};

#endif
