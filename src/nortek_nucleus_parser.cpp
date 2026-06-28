#include "vortex/drivers/nortek_nucleus_parser.hpp"

namespace vortex::drivers::dvl {

SpectrumDatagram parse_spectrum_data(const uint8_t* data, std::size_t len) {
    SpectrumDataV3 spectrum_data =
        read_from_buffer<SpectrumDataV3>(data, len, 0);

    const std::size_t data_offset = spectrum_data.data_offset;

    const uint16_t bins = spectrum_data.num_beam_bins & 0x1FFF;
    const uint8_t beams = spectrum_data.num_beam_bins >> 13;

    SpectrumFrequencyHeader spectrum_freq_header =
        read_from_buffer<SpectrumFrequencyHeader>(data, len, data_offset);

    std::vector<int16_t> spectrum_freq_data(bins * beams);
    const std::size_t num_bytes = sizeof(int16_t) * bins * beams;
    std::memcpy(spectrum_freq_data.data(), data + data_offset + 64, num_bytes);

    SpectrumDatagram spectrum_datagram{};
    spectrum_datagram.spectrum = spectrum_data;
    spectrum_datagram.freq_header = spectrum_freq_header;
    spectrum_datagram.freq_data = spectrum_freq_data;

    return spectrum_datagram;
}

ImuData parse_imu(const uint8_t* data, size_t len, size_t offset) {
    ImuData imu_data{};
    std::memcpy(&imu_data, data, 4);
    std::memcpy(reinterpret_cast<uint8_t*>(&imu_data) + 4, data + offset,
                sizeof(ImuData) - 4);
    return imu_data;
}

AhrsDataV2 parse_ahrsv2_data(const uint8_t* data, size_t len, size_t offset) {
    AhrsDataV2 ahrs_v2{};
    std::memcpy(&ahrs_v2, data + sizeof(CommonData), 24);
    std::memcpy(reinterpret_cast<uint8_t*>(&ahrs_v2) + 24, data + offset,
                sizeof(AhrsDataV2) - 24);
    return ahrs_v2;
}

CurrentProfileDatagram parse_current_profile_data(const uint8_t* data,
                                                  std::size_t len,
                                                  std::size_t data_offset) {
    CurrentProfileData current_profile_data =
        read_from_buffer<CurrentProfileData>(data, len, sizeof(CommonData));

    const uint16_t num_cells = current_profile_data.num_cells;

    std::vector<CurrentProfileVelocityData> velocity_data(num_cells);
    std::memcpy(velocity_data.data(), data + data_offset,
                num_cells * sizeof(CurrentProfileVelocityData));
    data_offset += num_cells * sizeof(CurrentProfileVelocityData);

    std::vector<CurrentProfileAmplitudeData> amplitude_data(num_cells);
    data_offset += num_cells * sizeof(CurrentProfileAmplitudeData);

    std::vector<CurrentProfileCorrelationData> correlation_data(num_cells);
    std::memcpy(correlation_data.data(), data + data_offset,
                num_cells * sizeof(CurrentProfileCorrelationData));

    CurrentProfileDatagram datagram{};
    datagram.current_profle = current_profile_data;
    datagram.velocity_data = velocity_data;
    datagram.amplitude_data = amplitude_data;
    datagram.correlation_data = correlation_data;
    return datagram;
}

uint16_t calculate_checksum(const uint8_t* packet, size_t len) {
    uint16_t sum = 0xB58C;

    for (size_t i = 0; i < len; i += 2) {
        sum += (uint16_t)(packet[i] | (packet[i + 1] << 8));
    }

    if (len & 1) {
        sum += (uint16_t)(packet[len - 1] << 8);
    }

    return sum;
}

};  // namespace vortex::drivers::dvl
