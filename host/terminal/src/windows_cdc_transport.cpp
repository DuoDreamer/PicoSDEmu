#include "cdc_transport.hpp"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace picosd::host {
namespace { constexpr std::size_t kMaximumLineLength = 2048; }
WindowsCdcTransport::~WindowsCdcTransport() { close(); }
CdcTransportError WindowsCdcTransport::open(std::string_view port) {
    close();
    if (port.empty()) return CdcTransportError::InvalidLine;
    std::string device = "\\\\.\\" + std::string(port);
    const HANDLE handle = CreateFileA(device.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (handle == INVALID_HANDLE_VALUE) return CdcTransportError::NotOpen;
    DCB settings {}; settings.DCBlength = sizeof(settings);
    if (!GetCommState(handle, &settings)) { CloseHandle(handle); return CdcTransportError::NotOpen; }
    settings.BaudRate = CBR_115200; settings.ByteSize = 8; settings.Parity = NOPARITY; settings.StopBits = ONESTOPBIT;
    if (!SetCommState(handle, &settings)) { CloseHandle(handle); return CdcTransportError::NotOpen; }
    COMMTIMEOUTS timeouts {}; timeouts.ReadIntervalTimeout = MAXDWORD;
    if (!SetCommTimeouts(handle, &timeouts)) { CloseHandle(handle); return CdcTransportError::NotOpen; }
    handle_ = handle; return CdcTransportError::None;
}
void WindowsCdcTransport::close() { if (handle_ != nullptr) { CloseHandle(static_cast<HANDLE>(handle_)); handle_ = nullptr; } pending_.clear(); }
bool WindowsCdcTransport::is_open() const { return handle_ != nullptr; }
CdcTransportError WindowsCdcTransport::write_line(std::string_view line) {
    if (!is_open()) return CdcTransportError::NotOpen;
    if (line.empty() || line.size() > kMaximumLineLength || line.find_first_of("\r\n") != std::string_view::npos) return CdcTransportError::InvalidLine;
    std::string wire(line); wire.push_back('\n'); DWORD count = 0;
    return WriteFile(static_cast<HANDLE>(handle_), wire.data(), static_cast<DWORD>(wire.size()), &count, nullptr) && count == wire.size() ? CdcTransportError::None : CdcTransportError::NotOpen;
}
CdcTransportError WindowsCdcTransport::read_line(std::string& line) {
    if (!is_open()) return CdcTransportError::NotOpen;
    char bytes[256]; DWORD count = 0;
    if (!ReadFile(static_cast<HANDLE>(handle_), bytes, sizeof(bytes), &count, nullptr)) return CdcTransportError::NotOpen;
    pending_.append(bytes, count); const auto newline = pending_.find('\n');
    if (newline == std::string::npos) return pending_.size() > kMaximumLineLength ? CdcTransportError::LineTooLong : CdcTransportError::InvalidLine;
    line = pending_.substr(0, newline); pending_.erase(0, newline + 1); if (!line.empty() && line.back() == '\r') line.pop_back();
    return line.empty() || line.size() > kMaximumLineLength ? CdcTransportError::InvalidLine : CdcTransportError::None;
}
}  // namespace picosd::host
#endif
