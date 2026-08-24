#include "picosd/physical_sd_backend.hpp"

#include <array>
#include <limits>

#include "hardware/gpio.h"
#include "pico/time.h"

namespace picosd::firmware {
namespace {

constexpr std::uint8_t kIdle = 0x01;
constexpr std::uint8_t kReadToken = 0xfe;

} // namespace

PhysicalSdBackend::PhysicalSdBackend(spi_inst_t *spi, Pins pins, std::uint32_t baud_hz,
                                     std::uint32_t timeout_ms)
    : spi_(spi), pins_(pins), baud_hz_(baud_hz), timeout_ms_(timeout_ms) {}

std::uint8_t PhysicalSdBackend::transfer(std::uint8_t output) const {
    std::uint8_t input = 0xff;
    spi_write_read_blocking(spi_, &output, &input, 1);
    return input;
}

bool PhysicalSdBackend::select() const {
    gpio_put(pins_.chip_select, false);
    for (unsigned int attempt = 0; attempt < 8; ++attempt) {
        if (transfer(0xff) == 0xff) {
            return true;
        }
    }
    deselect();
    return false;
}

void PhysicalSdBackend::deselect() const {
    gpio_put(pins_.chip_select, true);
    static_cast<void>(transfer(0xff));
}

std::uint8_t PhysicalSdBackend::command(std::uint8_t index, std::uint32_t argument,
                                        std::uint8_t crc, std::uint32_t *trailing) const {
    if (!select()) {
        return 0xff;
    }
    const std::array<std::uint8_t, 6> frame{
        static_cast<std::uint8_t>(0x40U | index),   static_cast<std::uint8_t>(argument >> 24U),
        static_cast<std::uint8_t>(argument >> 16U), static_cast<std::uint8_t>(argument >> 8U),
        static_cast<std::uint8_t>(argument),        crc};
    for (const auto byte : frame) {
        static_cast<void>(transfer(byte));
    }

    std::uint8_t response = 0xff;
    for (unsigned int attempt = 0; attempt < 16; ++attempt) {
        response = transfer(0xff);
        if ((response & 0x80U) == 0) {
            break;
        }
    }
    if (trailing != nullptr && response != 0xff) {
        *trailing = 0;
        for (unsigned int byte = 0; byte < 4; ++byte) {
            *trailing = (*trailing << 8U) | transfer(0xff);
        }
    }
    return response;
}

bool PhysicalSdBackend::wait_byte(std::uint8_t expected) const {
    const auto deadline = make_timeout_time_ms(timeout_ms_);
    do {
        if (!socket_has_card()) {
            return false;
        }
        if (transfer(0xff) == expected) {
            return true;
        }
    } while (!time_reached(deadline));
    return false;
}

bool PhysicalSdBackend::initialize() {
    deinitialize();
    if (pins_.card_detect != Pins::unused) {
        gpio_init(pins_.card_detect);
        gpio_set_dir(pins_.card_detect, GPIO_IN);
        gpio_pull_up(pins_.card_detect);
    }
    if (pins_.write_protect != Pins::unused) {
        gpio_init(pins_.write_protect);
        gpio_set_dir(pins_.write_protect, GPIO_IN);
        gpio_pull_up(pins_.write_protect);
    }
    if (!socket_has_card()) {
        return false;
    }
    spi_init(spi_, 400'000);
    gpio_set_function(pins_.clock, GPIO_FUNC_SPI);
    gpio_set_function(pins_.mosi, GPIO_FUNC_SPI);
    gpio_set_function(pins_.miso, GPIO_FUNC_SPI);
    gpio_init(pins_.chip_select);
    gpio_set_dir(pins_.chip_select, GPIO_OUT);
    gpio_put(pins_.chip_select, true);

    for (unsigned int byte = 0; byte < 10; ++byte) {
        static_cast<void>(transfer(0xff));
    }
    auto response = command(0, 0, 0x95);
    deselect();
    if (response != kIdle) {
        return false;
    }

    std::uint32_t interface_condition = 0;
    response = command(8, 0x1aa, 0x87, &interface_condition);
    deselect();
    if (response != kIdle || (interface_condition & 0xfffU) != 0x1aaU) {
        return false; // This first driver intentionally supports SD v2 cards only.
    }

    const auto deadline = make_timeout_time_ms(timeout_ms_);
    do {
        response = command(55, 0, 0x01);
        deselect();
        if (response > kIdle) {
            return false;
        }
        response = command(41, 0x40000000U, 0x01);
        deselect();
        if (response == 0) {
            break;
        }
    } while (!time_reached(deadline));
    if (response != 0) {
        return false;
    }

    std::uint32_t ocr = 0;
    response = command(58, 0, 0x01, &ocr);
    deselect();
    if (response != 0) {
        return false;
    }
    high_capacity_ = (ocr & 0x40000000U) != 0;

    response = command(9, 0, 0x01);
    if (response != 0 || !wait_byte(kReadToken)) {
        deselect();
        return false;
    }
    std::array<std::uint8_t, 16> csd{};
    for (auto &byte : csd) {
        byte = transfer(0xff);
    }
    static_cast<void>(transfer(0xff));
    static_cast<void>(transfer(0xff));
    deselect();
    if ((csd[0] >> 6U) != 1U) {
        return false; // Capacity decoding below is for the SDHC/SDXC CSD layout.
    }
    const std::uint32_t c_size = (static_cast<std::uint32_t>(csd[7] & 0x3fU) << 16U) |
                                 (static_cast<std::uint32_t>(csd[8]) << 8U) | csd[9];
    const std::uint64_t blocks = (static_cast<std::uint64_t>(c_size) + 1U) * 1024U;
    if (blocks > std::numeric_limits<std::size_t>::max()) {
        return false;
    }
    blocks_ = static_cast<std::size_t>(blocks);
    spi_set_baudrate(spi_, baud_hz_);
    initialized_ = true;
    return true;
}

void PhysicalSdBackend::deinitialize() {
    initialized_ = false;
    high_capacity_ = false;
    blocks_ = 0;
    if (spi_ != nullptr) {
        spi_deinit(spi_);
    }
}

bool PhysicalSdBackend::socket_has_card() const {
    return pins_.card_detect == Pins::unused || !gpio_get(pins_.card_detect);
}

bool PhysicalSdBackend::media_present() const {
    return initialized_ && socket_has_card();
}

bool PhysicalSdBackend::write_protected() const {
    return pins_.write_protect != Pins::unused && gpio_get(pins_.write_protect);
}

std::size_t PhysicalSdBackend::block_count() const {
    return media_present() ? blocks_ : 0;
}

picosd::protocol::BlockOperationResult PhysicalSdBackend::flush() {
    if (!media_present()) {
        return picosd::protocol::BlockOperationResult::Failed;
    }
    if (!select()) {
        return picosd::protocol::BlockOperationResult::Pending;
    }
    const auto ready = wait_byte(0xff);
    deselect();
    if (!ready) {
        return socket_has_card() ? picosd::protocol::BlockOperationResult::Pending
                                 : picosd::protocol::BlockOperationResult::Failed;
    }
    const auto status = command(13, 0, 0x01);
    const auto second_status = transfer(0xff);
    deselect();
    return status == 0 && second_status == 0 ? picosd::protocol::BlockOperationResult::Complete
                                             : picosd::protocol::BlockOperationResult::Failed;
}

std::uint32_t PhysicalSdBackend::address(std::size_t lba) const {
    return high_capacity_ ? static_cast<std::uint32_t>(lba)
                          : static_cast<std::uint32_t>(lba * 512U);
}

picosd::protocol::BlockOperationResult
PhysicalSdBackend::read(std::size_t lba, picosd::protocol::SdBlock &output) const {
    if (!media_present() || lba >= blocks_ || lba > UINT32_MAX) {
        return picosd::protocol::BlockOperationResult::Failed;
    }
    if (command(17, address(lba), 0x01) != 0 || !wait_byte(kReadToken)) {
        deselect();
        return socket_has_card() ? picosd::protocol::BlockOperationResult::Pending
                                 : picosd::protocol::BlockOperationResult::Failed;
    }
    for (auto &byte : output) {
        byte = transfer(0xff);
    }
    static_cast<void>(transfer(0xff));
    static_cast<void>(transfer(0xff));
    deselect();
    return picosd::protocol::BlockOperationResult::Complete;
}

picosd::protocol::BlockOperationResult
PhysicalSdBackend::write(std::size_t lba, const picosd::protocol::SdBlock &input) {
    if (!media_present() || write_protected() || lba >= blocks_ || lba > UINT32_MAX) {
        return picosd::protocol::BlockOperationResult::Failed;
    }
    if (command(24, address(lba), 0x01) != 0) {
        deselect();
        return picosd::protocol::BlockOperationResult::Pending;
    }
    static_cast<void>(transfer(0xff));
    static_cast<void>(transfer(kReadToken));
    for (const auto byte : input) {
        static_cast<void>(transfer(byte));
    }
    static_cast<void>(transfer(0xff));
    static_cast<void>(transfer(0xff));
    const auto accepted = (transfer(0xff) & 0x1fU) == 0x05U;
    const auto ready = accepted && wait_byte(0xff);
    deselect();
    return ready ? picosd::protocol::BlockOperationResult::Complete
                 : (socket_has_card() ? picosd::protocol::BlockOperationResult::Pending
                                      : picosd::protocol::BlockOperationResult::Failed);
}

} // namespace picosd::firmware
