#pragma once

#include <cstddef>
#include <deque>
#include <string>
#include <string_view>

namespace picosd::host {

enum class CdcTransportError { None, NotOpen, LineTooLong, InvalidLine };

// Host-facing USB CDC transport boundary. Platform serial implementations will
// supply bytes; the session layer consumes complete newline-delimited lines.
class CdcTransport {
public:
    virtual ~CdcTransport() = default;
    virtual CdcTransportError open(std::string_view port) = 0;
    virtual void close() = 0;
    [[nodiscard]] virtual bool is_open() const = 0;
    virtual CdcTransportError write_line(std::string_view line) = 0;
    virtual CdcTransportError read_line(std::string& line) = 0;
};

// Deterministic in-memory CDC endpoint for protocol/session tests. Incoming
// lines are injected by the test peer; written lines are retained for review.
class MemoryCdcTransport final : public CdcTransport {
public:
    CdcTransportError open(std::string_view port) override;
    void close() override;
    [[nodiscard]] bool is_open() const override;
    CdcTransportError write_line(std::string_view line) override;
    CdcTransportError read_line(std::string& line) override;
    void inject_received_line(std::string line);
    [[nodiscard]] const std::deque<std::string>& written_lines() const;

private:
    bool open_ = false;
    std::deque<std::string> received_lines_;
    std::deque<std::string> written_lines_;
};

#if !defined(_WIN32)
class PosixCdcTransport final : public CdcTransport {
public:
    ~PosixCdcTransport() override;
    CdcTransportError open(std::string_view port) override;
    void close() override;
    [[nodiscard]] bool is_open() const override;
    CdcTransportError write_line(std::string_view line) override;
    CdcTransportError read_line(std::string& line) override;
private:
    int descriptor_ = -1;
    std::string pending_;
};
#endif

#if defined(_WIN32)
class WindowsCdcTransport final : public CdcTransport {
public:
    ~WindowsCdcTransport() override;
    CdcTransportError open(std::string_view port) override;
    void close() override;
    [[nodiscard]] bool is_open() const override;
    CdcTransportError write_line(std::string_view line) override;
    CdcTransportError read_line(std::string& line) override;
private:
    void* handle_ = nullptr;
    std::string pending_;
};
#endif

}  // namespace picosd::host
