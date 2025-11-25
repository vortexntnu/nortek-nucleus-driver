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

struct AhrsDataV2 {
    uint32_t serial_number;
    uint8_t operation_mode;
    uint8_t reserved[3];

    float figure_of_merit;
    float fom_field_calibration;

    float data_roll;
    float data_pitch;
    float data_heading;

    float data_quaternion_w;
    float data_quaternion_x;
    float data_quaternion_y;
    float data_quaternion_z;

    float data_rotation_matrix[3 * 3];
    float declination;
    float depth;
};

struct InsDataV2 {
    float figure_of_merit_ins;
    uint32_t ins_status;
    float course_over_ground;

    float temperature;
    float pressure;
    float altitude;

    double latitude;
    double longditude;
    double reserved[1];

    float position_ned_x;
    float position_ned_y;
    float position_ned_z;

    float velocity_ned_x;
    float velocity_ned_y;
    float velocity_ned_z;

    float speed_over_ground;

    float turn_rate_x;
    float turn_rate_y;
    float turn_rate_z;
};

struct ImuData {
    uint32_t imu_status;

    float accelerometer_x;
    float accelerometer_y;
    float accelerometer_z;

    float gyro_x;
    float gyro_y;
    float gyro_z;

    float imu_temperature;
};

struct MagnetoMeterData {
    uint32_t magnetometer_status;

    float magnetometer_x;
    float magnetometer_y;
    float magnetometer_z;
};

struct AltimeterData {
    uint32_t altimeter_status;
    uint32_t serial_number;

    float sound_velocity;
    float water_temperature;
    float pressure;
    float distance;
};

struct FieldCalibrationData {
    float hard_iron_x;
    float hard_iron_y;
    float hard_iron_z;

    float soft_iron_matrix[3 * 3];
    float reserved[3];

    float figure_of_merit;
    float reserved2[1];
};

struct FastPressureData {
    float fast_pressure;
};

struct BottomTrackData {
    uint32_t status;
    uint32_t serial_number;

    float sound_velocity;
    float water_temperature;
    float pressure;

    float velocity_beam_1;
    float velocity_beam_2;
    float velocity_beam_3;

    float distance_beam_1;
    float distance_beam_2;
    float distance_beam_3;

    float uncertainty_beam_1;
    float uncertainty_beam_2;
    float uncertainty_beam_3;

    float delta_t_beam_1;
    float delta_t_beam_2;
    float delta_t_beam_3;

    float time_estimate_beam_1;
    float time_estimate_beam_2;
    float time_estimate_beam_3;

    float velocity_x;
    float velocity_y;
    float velocity_z;

    float uncertainty_x;
    float uncertainty_y;
    float uncertainty_z;

    float delta_t_xyz;

    uint8_t reserved[4];
};

struct WaterTrackData {
    uint32_t status;
    uint32_t serial_number;

    float sound_velocity;
    float water_temperature;
    float pressure;

    float velocity_beam_1;
    float velocity_beam_2;
    float velocity_beam_3;

    float distance_beam_1;
    float distance_beam_2;
    float distance_beam_3;

    float uncertainty_beam_1;
    float uncertainty_beam_2;
    float uncertainty_beam_3;

    float delta_t_beam_1;
    float delta_t_beam_2;
    float delta_t_beam_3;

    float time_estimate_beam_1;
    float time_estimate_beam_2;
    float time_estimate_beam_3;

    float velocity_x;
    float velocity_y;
    float velocity_z;

    float uncertainty_x;
    float uncertainty_y;
    float uncertainty_z;

    float delta_t_xyz;

    uint8_t reserved[4];
};

struct CurrentProfileData {
    uint32_t serial_number;
    uint8_t current_profile_configuraiton;
    uint8_t reserved[3];
    float sound_velocity;
    float water_temperature;
    float pressure;

    float cell_size;
    float blanking3;
    uint16_t num_cells;
    int16_t ambiguity_velocity;

    // TODO add variable amount elements
};

struct SpectrumDataV3 {
    uint8_t version;
    uint8_t offset_data;
    uint16_t configuration;
    uint32_t serial_number;

    uint8_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minutes;
    uint8_t seconds;
    uint16_t hundred_microseconds;

    uint16_t speed_of_sound;
    int16_t temperature;
    uint32_t pressure;
    uint8_t reserved[6];

    uint16_t num_beam_bins;
    uint8_t reserved1[2];
    uint16_t blanking;
    uint8_t temp_pressure_sensor;
    uint8_t reserved2[16];

    uint16_t data_set_description;

    uint8_t reserved3[3];
    int8_t power_level;

    uint8_t reserved4[2];

    int16_t real_time_clock_temp;
    uint16_t error_status;
    uint32_t extended_status;

    uint32_t ensamble_counter;

    // TODO add spectrum frequency data
};

#endif  // NORTEK_NUCLEUS_MESSAGES_HPP_
