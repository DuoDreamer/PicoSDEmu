#include "cdc_transport.hpp"

namespace picosd::host {
namespace {
constexpr std::size_t kMaximumLineLength = 2048;
bool valid_line(std::string_view line) {
    return !line.empty() && line.size() <= kMaximumLineLength &&
           line.find('\r') == std::string_view::npos && line.find('\n') == std::string_view::npos;
}
}  // namespace

CdcTransportError MemoryCdcTransport::open(std::string_view port) {
    if (port.empty()) return CdcTransportError::InvalidLine;
    open_ = true;
    return CdcTransportError::None;
}
void MemoryCdcTransport::close() { open_ = false; }
bool MemoryCdcTransport::is_open() const { return open_; }
CdcTransportError MemoryCdcTransport::write_line(std::string_view line) {
    if (!open_) return CdcTransportError::NotOpen;
    if (line.size() > kMaximumLineLength) return CdcTransportError::LineTooLong;
    if (!valid_line(line)) return CdcTransportError::InvalidLine;
    written_lines_.emplace_back(line);
    return CdcTransportError::None;
}
CdcTransportError MemoryCdcTransport::read_line(std::string& line) {
    if (!open_) return CdcTransportError::NotOpen;
    if (received_lines_.empty()) return CdcTransportError::InvalidLine;
    line = received_lines_.front(); received_lines_.pop_front();
    return CdcTransportError::None;
}
void MemoryCdcTransport::inject_received_line(std::string line) { received_lines_.push_back(std::move(line)); }
const std::deque<std::string>& MemoryCdcTransport::written_lines() const { return written_lines_; }
}  // namespace picosd::host
