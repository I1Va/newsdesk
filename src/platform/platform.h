#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace newsdesk::platform {

using SocketHandle = std::intptr_t;
using FileHandle = std::intptr_t;

constexpr SocketHandle kInvalidSocket = -1;
constexpr FileHandle kInvalidFile = -1;

enum class IoStatus {
    kOk,
    kWouldBlock,
    kClosed,
    kError,
};

struct IoResult {
    IoStatus status;
    size_t bytes = 0;
};

enum class PollInterest {
    kRead,
    kReadWrite,
};

struct PollEvent {
    SocketHandle socket;
    bool readable = false;
    bool writable = false;
    bool error = false;
};

// Интерфйес платформы (Windows/Posix) на которой будет запускаться приложение
// Будет иметь несколько реализаций в зависимости от конкретной платформы
class Platform {
public:
    virtual ~Platform() = default;

    // Сокеты (Неблокирующие)
    virtual SocketHandle Listen(const std::string& bind_address, uint16_t port) = 0;
    virtual SocketHandle Connect(const std::string& address, uint16_t port) = 0;

    // Возвращает kInvalidSocket, если нет ожидающего подключения
    virtual SocketHandle Accept(SocketHandle listener) = 0;

    virtual IoResult Read(SocketHandle socket, char* buffer, size_t size) = 0;
    virtual IoResult Write(SocketHandle socket, const char* data, size_t size) = 0;
    virtual void CloseSocket(SocketHandle socket) = 0;

    virtual void SetPollInterest(SocketHandle socket, PollInterest interest) = 0;
    virtual void RemoveFromPoll(SocketHandle socket) = 0;
    virtual std::vector<PollEvent> Poll(int timeout_ms) = 0;

    // Journal file I/O.
    virtual FileHandle OpenAppend(const std::string& path) = 0;
    virtual IoResult Append(FileHandle file, const char* data, size_t size) = 0;
    virtual void Sync(FileHandle file) = 0;
    virtual void CloseFile(FileHandle file) = 0;
    virtual std::string ReadAll(const std::string& path) = 0;

    // = 1, только когда получен SIGINT/SIGTERM. Main loop должен будет остановиться.
    virtual bool ShutdownRequested() = 0;
};

std::unique_ptr<Platform> CreatePlatform();

}  // namespace newsdesk::platform
