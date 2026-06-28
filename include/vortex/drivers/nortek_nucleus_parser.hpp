#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <type_traits>

#include "vortex/drivers/nortek_nucleus_messages.hpp"

namespace vortex::drivers::dvl {

template <typename T>
[[nodiscard]] T read_from_buffer(const std::uint8_t* data,
                                 std::size_t len,
                                 std::size_t offset) {
    static_assert(
        std::is_trivially_copyable_v<T>,
        "read_from_buffer requires trivially copyable types");

    if (data == nullptr) {
        throw std::invalid_argument("Cannot read from a null buffer");
    }

    if (offset > len || sizeof(T) > len - offset) {
        throw std::out_of_range("Buffer is too short for requested read");
    }

    T value{};
    std::memcpy(&value, data + offset, sizeof(T));
    return value;
}

[[nodiscard]] SpectrumDatagram parse_spectrum_data(
    const std::uint8_t* data,
    std::size_t len);

[[nodiscard]] ImuData parse_imu(
    const std::uint8_t* data,
    std::size_t len,
    std::size_t offset);

[[nodiscard]] AhrsDataV2 parse_ahrsv2_data(
    const std::uint8_t* data,
    std::size_t len,
    std::size_t offset);

[[nodiscard]] CurrentProfileDatagram parse_current_profile_data(
    const std::uint8_t* data,
    std::size_t len,
    std::size_t data_offset);

[[nodiscard]] std::uint16_t calculate_checksum(
    const std::uint8_t* packet,
    std::size_t len);

}  // namespace vortex::drivers::dvl
