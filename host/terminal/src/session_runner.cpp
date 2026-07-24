#include "session_runner.hpp"
namespace picosd::host {
SessionRunResult process_one_request(CdcTransport& transport, SessionDispatcher& dispatcher) {
    std::string request;
    const auto read = transport.read_line(request);
    if (read == CdcTransportError::InvalidLine) return SessionRunResult::NoRequest;
    if (read != CdcTransportError::None) return SessionRunResult::TransportError;
    return transport.write_line(dispatcher.dispatch(request)) == CdcTransportError::None ? SessionRunResult::Processed : SessionRunResult::TransportError;
}
}  // namespace picosd::host
