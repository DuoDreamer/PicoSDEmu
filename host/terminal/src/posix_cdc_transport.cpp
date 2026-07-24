#include "cdc_transport.hpp"

#if !defined(_WIN32)
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

namespace picosd::host {
namespace { constexpr std::size_t kMaximumLineLength = 2048; }
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
void PosixCdcTransport::close() { if (descriptor_ >= 0) { (void)::close(descriptor_); descriptor_ = -1; } pending_.clear(); }
bool PosixCdcTransport::is_open() const { return descriptor_ >= 0; }
CdcTransportError PosixCdcTransport::write_line(std::string_view line) {
    if (!is_open()) return CdcTransportError::NotOpen;
    if (line.empty() || line.size() > kMaximumLineLength || line.find_first_of("\r\n") != std::string_view::npos) return CdcTransportError::InvalidLine;
    std::string wire(line); wire.push_back('\n');
    return ::write(descriptor_, wire.data(), wire.size()) == static_cast<ssize_t>(wire.size()) ? CdcTransportError::None : CdcTransportError::NotOpen;
}
CdcTransportError PosixCdcTransport::read_line(std::string& line) {
    if (!is_open()) return CdcTransportError::NotOpen;
    char bytes[256]; const auto count = ::read(descriptor_, bytes, sizeof(bytes));
    if (count > 0) pending_.append(bytes, static_cast<std::size_t>(count));
    const auto newline = pending_.find('\n');
    if (newline == std::string::npos) return pending_.size() > kMaximumLineLength ? CdcTransportError::LineTooLong : CdcTransportError::InvalidLine;
    line = pending_.substr(0, newline); pending_.erase(0, newline + 1);
    if (!line.empty() && line.back() == '\r') line.pop_back();
    return line.empty() || line.size() > kMaximumLineLength ? CdcTransportError::InvalidLine : CdcTransportError::None;
}
}  // namespace picosd::host
#endif
