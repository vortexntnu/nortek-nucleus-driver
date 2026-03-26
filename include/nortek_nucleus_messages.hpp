#ifndef NORTEK_NUCLEUS_MESSAGES_HPP_
#define NORTEK_NUCLEUS_MESSAGES_HPP_

#include <cstdint>
#include <string>
#include <vector>

enum class DataSeriesId : uint8_t {
    SpectrumDataV3 = 0x20,
    ImuData = 0x82,
    MagnometerData = 0x87,
    FieldCalibrationData = 0x8B,
    FastPressureData = 0x96,
    StringData = 0xA0,
    AltimeterData = 0xAA,
    BottomTrackData = 0xB4,
    WaterTrackData = 0xBE,
    CurrentProfileData = 0xC0,
    AhrsData = 0xD2,
    InsData = 0xDC,
};

enum class NucleusStatusCode : int {
    Ok = 0,
    Error = -1,
    SendFailed = -2,
    ReadFailed = -3,
};

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
    uint8_t data_offset;
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

    float velocity_body_x;
    float velocity_body_y;
    float velocity_body_z;

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

    uint32_t reserved;

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
    uint32_t reserved1[1];

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

    uint32_t reserved1;

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
};

struct CurrentProfileVelocityData {
    int16_t velocity_x;
    int16_t velocity_y;
    int16_t velocity_z;
};

struct CurrentProfileAmplitudeData {
    uint8_t amplitude[3];
};

struct CurrentProfileCorrelationData {
    uint8_t correlation[3];
};

struct CurrentProfileDatagram {
    CurrentProfileData current_profle;
    std::vector<CurrentProfileVelocityData> velocity_data;
    std::vector<CurrentProfileAmplitudeData> amplitude_data;
    std::vector<CurrentProfileCorrelationData> correlation_data;
};

struct SpectrumDataV3 {
    uint8_t version;
    uint8_t data_offset;
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
};

struct SpectrumFrequencyHeader {
    float start_frequency;
    float step_frequency;
};

struct SpectrumDatagram {
    SpectrumDataV3 spectrum;
    SpectrumFrequencyHeader freq_header;
    std::vector<int16_t> freq_data;
};

struct NucleusReply {
    NucleusStatusCode status;
    std::string payload;
};

enum class BottomTrackMode {
    FastACQ = 0,
    Crawler,
    Auto,
};

enum class NucleusDataStreamSettings {
    Off,
    On,
    Cmd,
    Data,
};

enum class NucleusDataFormats {
    FastPressureFormat = 150,
    BottomTrackRDIPD6 = 156,
    AltimeterFormat = 170,
    BottomTrackBinaryFormat = 180,
};

struct BottomTrackSettings {
    BottomTrackMode mode;
    int velocity_range;
    bool enable_watertrack;
    int power_level;
    bool power_level_user_defined;
    NucleusDataStreamSettings data_stream_settings;
    NucleusDataFormats data_format;
};

struct AltimeterSettings {
    int power_level;
    NucleusDataStreamSettings data_stream_settings;
    NucleusDataFormats data_format;
};

struct FastPressureSettings {
    bool enable_fast_pressure;
    int sampling_rate;
    NucleusDataStreamSettings data_stream_settings;
    NucleusDataFormats data_format;
};

enum class MagnetometerMethod {
    Auto,
    Off,
    Wmm,
};

struct MagnetometerSettings {
    int freq;
    MagnetometerMethod mode;
    NucleusDataStreamSettings data_stream_settings;
    DataSeriesId data_format;
};

struct ImuSettings {
    int freq;
    NucleusDataStreamSettings data_stream_settings;
    DataSeriesId data_format;
};

struct EthernetSettings {
    bool use_static_ip;
    std::string ip;
    std::string netmask;
    std::string default_gateway;
    bool use_password;
    std::string password;
};

enum class SaveSettings {
    All,
    Config,
    Comm,
    Mission,
    Magcal,
};

enum class AhrsMode {
    FixedHardAndSoftIron,
    HardIronEstimation,
    HardAndSoftEstimation,
};

struct AhrsSettings {
    int freq;
    AhrsMode mode;
    NucleusDataStreamSettings data_stream_settings;
    DataSeriesId data_format;
};

#endif  // NORTEK_NUCLEUS_MESSAGES_HPP_
