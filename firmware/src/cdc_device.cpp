#include "picosd/cdc_device.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>

#include "tusb.h"

namespace {
constexpr std::uint8_t kCdcInterface = 0;
constexpr std::uint8_t kCdcNotificationEndpoint = 0x81;
constexpr std::uint8_t kCdcOutputEndpoint = 0x02;
constexpr std::uint8_t kCdcInputEndpoint = 0x82;
constexpr std::uint16_t kUsbVersion = 0x0200;
constexpr std::uint16_t kVendorId = 0x2E8A; // Raspberry Pi vendor ID.
constexpr std::uint16_t kProductId = 0x000A;

tusb_desc_device_t const device_descriptor = {
    sizeof(tusb_desc_device_t),
    TUSB_DESC_DEVICE,
    kUsbVersion,
    TUSB_CLASS_MISC,
    MISC_SUBCLASS_COMMON,
    MISC_PROTOCOL_IAD,
    CFG_TUD_ENDPOINT0_SIZE,
    kVendorId,
    kProductId,
    0x0100,
    1,
    2,
    3,
    1,
};

constexpr std::uint16_t kConfigurationLength = TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN;
std::uint8_t const configuration_descriptor[] = {
    TUD_CONFIG_DESCRIPTOR(1, 2, 0, kConfigurationLength, 0, 100),
    TUD_CDC_DESCRIPTOR(kCdcInterface, 4, kCdcNotificationEndpoint, 8, kCdcOutputEndpoint,
                       kCdcInputEndpoint, 64),
};

constexpr const char *strings[] = {
    nullptr, "Pico SD Emulator", "Pico SD Emulator CDC", "00000001", "Image service",
};
constexpr std::size_t kStringCount = sizeof(strings) / sizeof(strings[0]);
std::array<std::uint16_t, 32> string_descriptor{};
constexpr std::size_t kTransmitBufferSize = 1024;
std::array<char, kTransmitBufferSize> transmit_buffer{};
std::size_t transmit_length = 0;
std::size_t transmit_offset = 0;
} // namespace

extern "C" std::uint8_t const *tud_descriptor_device_cb() {
    return reinterpret_cast<std::uint8_t const *>(&device_descriptor);
}

extern "C" std::uint8_t const *tud_descriptor_configuration_cb(std::uint8_t) {
    return configuration_descriptor;
}

extern "C" std::uint16_t const *tud_descriptor_string_cb(std::uint8_t index, std::uint16_t) {
    if (index == 0) {
        string_descriptor[1] = 0x0409;
        string_descriptor[0] = static_cast<std::uint16_t>((TUSB_DESC_STRING << 8) | 4);
        return string_descriptor.data();
    }
    if (index >= kStringCount || strings[index] == nullptr)
        return nullptr;

    const std::size_t count = std::min(std::strlen(strings[index]), string_descriptor.size() - 1);
    for (std::size_t position = 0; position < count; ++position) {
        string_descriptor[position + 1] = static_cast<std::uint8_t>(strings[index][position]);
    }
    string_descriptor[0] = static_cast<std::uint16_t>((TUSB_DESC_STRING << 8) | (2 * count + 2));
    return string_descriptor.data();
}

namespace picosd::firmware {

void initialize_cdc_device() {
    tusb_init();
}

void poll_cdc_device() {
    tud_task();
    if (!tud_cdc_connected()) {
        transmit_length = 0;
        transmit_offset = 0;
        return;
    }
    if (transmit_offset == transmit_length)
        return;
    const auto available = static_cast<std::size_t>(tud_cdc_write_available());
    const auto count = std::min(available, transmit_length - transmit_offset);
    if (count == 0)
        return;
    const auto written = static_cast<std::size_t>(
        tud_cdc_write(transmit_buffer.data() + transmit_offset, static_cast<std::uint32_t>(count)));
    transmit_offset += written;
    tud_cdc_write_flush();
    if (transmit_offset == transmit_length) {
        transmit_length = 0;
        transmit_offset = 0;
    }
}

bool read_cdc_byte(std::uint8_t &value) {
    if (!tud_cdc_connected() || tud_cdc_available() == 0)
        return false;
    return tud_cdc_read(&value, 1) == 1;
}

bool write_cdc(const char *data, std::size_t length) {
    if (!tud_cdc_connected() || data == nullptr || length == 0 || length > transmit_buffer.size() ||
        transmit_length != 0)
        return false;
    std::memcpy(transmit_buffer.data(), data, length);
    transmit_length = length;
    transmit_offset = 0;
    return true;
}

bool cdc_connected() {
    return tud_cdc_connected();
}

} // namespace picosd::firmware
