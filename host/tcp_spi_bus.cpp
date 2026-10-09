#include "tcp_spi_bus.h"
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include "../screen/spi_wire.h"

bool TcpSpiBus::connect(const std::string& host, int port, int timeoutMs) {
    for (int waited = 0; waited <= timeoutMs; waited += 200) {
        addrinfo hints{}, *res = nullptr;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &res) == 0) {
            int fd = socket(AF_INET, SOCK_STREAM, 0);
            if (::connect(fd, res->ai_addr, res->ai_addrlen) == 0) {
                int one = 1;
                setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
                fd_ = fd;
                freeaddrinfo(res);
                return true;
            }
            close(fd);
            freeaddrinfo(res);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    return false;
}

void TcpSpiBus::frame(uint8_t kind, const uint8_t* bytes, size_t n) {
    uint8_t h[5] = {kind, uint8_t(n), uint8_t(n >> 8), uint8_t(n >> 16), uint8_t(n >> 24)};
    out_.insert(out_.end(), h, h + 5);
    out_.insert(out_.end(), bytes, bytes + n);
}

void TcpSpiBus::command(uint8_t cmd) { frame(screen_wire::FRAME_CMD, &cmd, 1); }
void TcpSpiBus::data(const uint8_t* b, size_t n) { frame(screen_wire::FRAME_DATA, b, n); }

void TcpSpiBus::flush() {
    size_t off = 0;
    while (fd_ >= 0 && off < out_.size()) {
        ssize_t n = send(fd_, out_.data() + off, out_.size() - off, MSG_NOSIGNAL);
        if (n <= 0) { close(fd_); fd_ = -1; break; }
        off += size_t(n);
    }
    out_.clear();
}
