#ifndef NORTEK_NUCLEUS_MESSAGES_HPP_
#define NORTEK_NUCLEUS_MESSAGES_HPP_

#include <cstdint>

struct HeaderData {
    uint8_t sync_byte;
    uint8_t header_size;
    uint8_t data_series_id;
    uint8_t family_id;
    uint16_t data_size;
    uint16_t data_checksum;
    uint16_t header_checksum;
};

struct CommonData {
    uint8_t version;
    uint8_t offset_data;
    uint8_t flags;
    uint8_t reserved[1];

    uint32_t timestamp;
    uint32_t microseconds;
};

#endif  // NORTEK_NUCLEUS_MESSAGES_HPP_
