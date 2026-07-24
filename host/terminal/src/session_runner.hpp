#pragma once
#include "cdc_transport.hpp"
#include "session_dispatcher.hpp"
namespace picosd::host {
enum class SessionRunResult { Processed, NoRequest, TransportError };
SessionRunResult process_one_request(CdcTransport& transport, SessionDispatcher& dispatcher);
}  // namespace picosd::host
