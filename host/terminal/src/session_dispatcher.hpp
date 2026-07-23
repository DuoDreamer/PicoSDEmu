#pragma once

#include <string>
#include <string_view>

#include "image_file.hpp"

namespace picosd::host {

class SessionDispatcher {
public:
    SessionDispatcher(ImageFile& image, std::string card_type, bool writable);
    std::string dispatch(std::string_view request);

private:
    ImageFile& image_;
    std::string card_type_;
    bool writable_;
    bool ejected_ = false;
};

}  // namespace picosd::host
