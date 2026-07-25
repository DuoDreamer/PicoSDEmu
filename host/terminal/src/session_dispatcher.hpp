#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "image_file.hpp"

namespace picosd::host {

class SessionDispatcher {
public:
    SessionDispatcher(ImageFile& image, std::string card_type, bool writable,
                      std::string session_id);
    std::string dispatch(std::string_view request);

private:
    ImageFile& image_;
    std::string card_type_;
    std::string session_id_;
    bool writable_;
    bool established_ = false;
    bool ejected_ = false;
    std::uint64_t last_request_id_ = 0;
};

}  // namespace picosd::host
