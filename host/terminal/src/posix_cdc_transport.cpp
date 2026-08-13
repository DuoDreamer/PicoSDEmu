#include "cdc_transport.hpp"

#if !defined(_WIN32)
#include <cerrno>
#include <fcntl.h>
#include <poll.h>
#include <termios.h>
#include <unistd.h>

namespace picosd::host {
PosixCdcTransport::~PosixCdcTransport() { close(); }
CdcTransportError PosixCdcTransport::open(std::string_view port) {
    close();
    if (port.empty()) return CdcTransportError::InvalidLine;
    descriptor_ = ::open(std::string(port).c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (descriptor_ < 0) return CdcTransportError::NotOpen;
    termios settings {};
    if (tcgetattr(descriptor_, &settings) != 0) { close(); return CdcTransportError::NotOpen; }
    cfmakeraw(&settings);
    cfsetispeed(&settings, B115200); cfsetospeed(&settings, B115200);
    if (tcsetattr(descriptor_, TCSANOW, &settings) != 0) { close(); return CdcTransportError::NotOpen; }
    return CdcTransportError::None;
}
void PosixCdcTransport::close() { if (descriptor_ >= 0) { (void)::close(descriptor_); descriptor_ = -1; } pending_.clear(); discarding_oversized_line_ = false; }
bool PosixCdcTransport::is_open() const { return descriptor_ >= 0; }
CdcTransportError PosixCdcTransport::write_line(std::string_view line) {
    if (!is_open()) return CdcTransportError::NotOpen;
    if (line.empty() || line.size() > kMaximumCdcLineLength || line.find_first_of("\r\n") != std::string_view::npos) return CdcTransportError::InvalidLine;
    std::string wire(line); wire.push_back('\n');
    std::size_t offset = 0;
    while (offset < wire.size()) {
        const auto count = ::write(descriptor_, wire.data() + offset, wire.size() - offset);
        if (count > 0) {
            offset += static_cast<std::size_t>(count);
            continue;
        }
        if (count < 0 && errno == EINTR) continue;
        if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            pollfd writable{descriptor_, POLLOUT, 0};
            int ready = 0;
            do { ready = ::poll(&writable, 1, 1000); } while (ready < 0 && errno == EINTR);
            if (ready > 0 && (writable.revents & POLLOUT) != 0) continue;
        }
        return CdcTransportError::NotOpen;
    }
    return CdcTransportError::None;
}
CdcTransportError PosixCdcTransport::read_line(std::string& line) {
    if (!is_open()) return CdcTransportError::NotOpen;
    auto newline = pending_.find('\n');
    if (newline != std::string::npos) {
        line = pending_.substr(0, newline); pending_.erase(0, newline + 1);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.size() > kMaximumCdcLineLength) return CdcTransportError::LineTooLong;
        return line.empty() ? CdcTransportError::InvalidLine : CdcTransportError::None;
    }
    char bytes[256];
    ssize_t count = 0;
    do { count = ::read(descriptor_, bytes, sizeof(bytes)); } while (count < 0 && errno == EINTR);
    if (count > 0 && discarding_oversized_line_) {
        const std::string_view received(bytes, static_cast<std::size_t>(count));
        const auto discarded_newline = received.find('\n');
        if (discarded_newline == std::string_view::npos) return CdcTransportError::WouldBlock;
        discarding_oversized_line_ = false;
        pending_.append(received.substr(discarded_newline + 1));
    } else if (count > 0) {
        pending_.append(bytes, static_cast<std::size_t>(count));
    }
    if (count == 0) return CdcTransportError::NotOpen;
    if (count < 0 && errno != EAGAIN && errno != EWOULDBLOCK) return CdcTransportError::NotOpen;
    newline = pending_.find('\n');
    if (newline == std::string::npos) {
        if (pending_.size() <= kMaximumCdcLineLength) return CdcTransportError::WouldBlock;
        pending_.clear();
        discarding_oversized_line_ = true;
        return CdcTransportError::LineTooLong;
    }
    line = pending_.substr(0, newline); pending_.erase(0, newline + 1);
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.size() > kMaximumCdcLineLength) return CdcTransportError::LineTooLong;
    return line.empty() ? CdcTransportError::InvalidLine : CdcTransportError::None;
}
}  // namespace picosd::host
#endif
