#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <fstream>
#include <sstream>
#include <unordered_map>

#include "platform/platform.h"

namespace newsdesk::platform {

namespace {

volatile sig_atomic_t g_shutdown_requested = 0;

void HandleShutdownSignal(int) { g_shutdown_requested = 1; }

void SetNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

}  // namespace

class PosixPlatform : public Platform {
public:
    PosixPlatform() {
        struct sigaction action{};
        action.sa_handler = HandleShutdownSignal;
        sigaction(SIGINT, &action, nullptr);
        sigaction(SIGTERM, &action, nullptr);
    }

    SocketHandle Listen(const std::string& bind_address, uint16_t port) override {
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) {
            return kInvalidSocket;
        }
        int reuse = 1;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        if (bind_address.empty() || bind_address == "0.0.0.0") {
            addr.sin_addr.s_addr = INADDR_ANY;
        } else {
            inet_pton(AF_INET, bind_address.c_str(), &addr.sin_addr);
        }

        if (bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
            close(fd);
            return kInvalidSocket;
        }
        if (listen(fd, SOMAXCONN) < 0) {
            close(fd);
            return kInvalidSocket;
        }
        SetNonBlocking(fd);
        return fd;
    }

    SocketHandle Connect(const std::string& address, uint16_t port) override {
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) {
            return kInvalidSocket;
        }
        SetNonBlocking(fd);

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        inet_pton(AF_INET, address.c_str(), &addr.sin_addr);

        int result = connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
        if (result < 0 && errno != EINPROGRESS) {
            close(fd);
            return kInvalidSocket;
        }
        return fd;
    }

    SocketHandle Accept(SocketHandle listener) override {
        int fd = accept(static_cast<int>(listener), nullptr, nullptr);
        if (fd < 0) {
            return kInvalidSocket;
        }
        SetNonBlocking(fd);
        return fd;
    }

    IoResult Read(SocketHandle socket, char* buffer, size_t size) override {
        ssize_t n = recv(static_cast<int>(socket), buffer, size, 0);
        if (n > 0) {
            return IoResult{IoStatus::kOk, static_cast<size_t>(n)};
        }
        if (n == 0) {
            return IoResult{IoStatus::kClosed, 0};
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return IoResult{IoStatus::kWouldBlock, 0};
        }
        return IoResult{IoStatus::kError, 0};
    }

    IoResult Write(SocketHandle socket, const char* data, size_t size) override {
        ssize_t n = send(static_cast<int>(socket), data, size, MSG_NOSIGNAL);
        if (n >= 0) {
            return IoResult{IoStatus::kOk, static_cast<size_t>(n)};
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return IoResult{IoStatus::kWouldBlock, 0};
        }
        return IoResult{IoStatus::kError, 0};
    }

    void CloseSocket(SocketHandle socket) override {
        close(static_cast<int>(socket));
        poll_interest_.erase(socket);
    }

    void SetPollInterest(SocketHandle socket, PollInterest interest) override {
        poll_interest_[socket] = interest;
    }

    void RemoveFromPoll(SocketHandle socket) override { poll_interest_.erase(socket); }

    std::vector<PollEvent> Poll(int timeout_ms) override {
        std::vector<pollfd> fds;
        fds.reserve(poll_interest_.size());
        for (const auto& [socket, interest] : poll_interest_) {
            pollfd pfd{};
            pfd.fd = static_cast<int>(socket);
            pfd.events = POLLIN;
            if (interest == PollInterest::kReadWrite) {
                pfd.events |= POLLOUT;
            }
            fds.push_back(pfd);
        }

        int poll_timeout = timeout_ms;
        if (g_shutdown_requested) {
            poll_timeout = 0;
        }
        int ready = poll(fds.data(), fds.size(), poll_timeout);

        std::vector<PollEvent> events;
        if (ready <= 0) {
            return events;
        }
        for (const pollfd& pfd : fds) {
            if (pfd.revents == 0) {
                continue;
            }
            PollEvent event;
            event.socket = pfd.fd;
            event.readable = pfd.revents & (POLLIN | POLLHUP);
            event.writable = pfd.revents & POLLOUT;
            event.error = pfd.revents & (POLLERR | POLLNVAL);
            events.push_back(event);
        }
        return events;
    }

    FileHandle OpenAppend(const std::string& path) override {
        int fd = open(path.c_str(), O_CREAT | O_APPEND | O_WRONLY, 0644);
        return fd < 0 ? kInvalidFile : fd;
    }

    IoResult Append(FileHandle file, const char* data, size_t size) override {
        ssize_t n = write(static_cast<int>(file), data, size);
        if (n < 0) {
            return IoResult{IoStatus::kError, 0};
        }
        return IoResult{IoStatus::kOk, static_cast<size_t>(n)};
    }

    void Sync(FileHandle file) override { fsync(static_cast<int>(file)); }

    void CloseFile(FileHandle file) override { close(static_cast<int>(file)); }

    std::string ReadAll(const std::string& path) override {
        std::ifstream in(path, std::ios::binary);
        if (!in.is_open()) {
            return "";
        }
        std::ostringstream buffer;
        buffer << in.rdbuf();
        return buffer.str();
    }

    bool ShutdownRequested() override { return g_shutdown_requested != 0; }

private:
    std::unordered_map<SocketHandle, PollInterest> poll_interest_;
};

std::unique_ptr<Platform> CreatePlatform() { return std::make_unique<PosixPlatform>(); }

}  // namespace newsdesk::platform
