#include "vortex/drivers/nortek_nucleus_driver.hpp"
#include <boost/asio/streambuf.hpp>
#include <boost/asio/write.hpp>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iomanip>
#include <sstream>
#include <string>
#include "vortex/drivers/nortek_nucleus_messages.hpp"
#include "vortex/drivers/nortek_nucleus_parser.hpp"

namespace vortex::drivers::dvl {

NortekNucleusDriver::NortekNucleusDriver(

    boost::asio::io_context& io,
    std::function<void(NortekNucleusFrame)> callback)
    : nucleus_sock_(io), callback_(callback) {}

boost::system::error_code NortekNucleusDriver::open_tcp_sockets(
    const NortekConnectionParams& params) {
    boost::system::error_code ec;

    const auto addr = boost::asio::ip::make_address(params.remote_ip, ec);

    if (ec) {
        return ec;
    }

    const boost::asio::ip::tcp::endpoint nucleus_endpoint(
        addr, params.data_remote_port);

    nucleus_sock_.connect(nucleus_endpoint, ec);

    if (ec) {
        return ec;
    }

    return {};
}

boost::system::error_code NortekNucleusDriver::enter_password(
    const NortekConnectionParams& params) {
    boost::system::error_code ec;

    boost::asio::streambuf buffer;

    boost::asio::read_until(nucleus_sock_, buffer,
                            "Please enter password:", ec);

    if (ec) {
        return ec;
    }

    const std::string msg = params.password + "\r\n";

    boost::asio::write(nucleus_sock_, boost::asio::buffer(msg), ec);

    return ec;
}

void NortekNucleusDriver::start_read() {
    nucleus_sock_.async_read_some(
        boost::asio::buffer(temp),
        [this](const boost::system::error_code& ec, std::size_t size) {
            if (ec) {
                return;
            }

            const auto old_size = buf.size();
            buf.resize(old_size + size);

            std::memcpy(buf.data() + old_size, temp.data(), size);

            parse_available();
            start_read();
        });
}

void NortekNucleusDriver::parse_available() {
    constexpr size_t MAX_FRAME = 1500;  // placeholder for now
    constexpr uint8_t SYNC_BYTE = 0xA5;

    static constexpr uint8_t PREAMBLE[2] = {0xA5, 0x0A};

    auto compact_if_needed = [&]() {
        if (read_index == 0)
            return;
        if (read_index < 64 * 1024 && read_index < buf.size() / 2)
            return;

        const auto remaining = buf.size() - read_index;
        if (remaining > 0) {
            std::memmove(buf.data(), buf.data() + read_index, remaining);
        }
        buf.resize(remaining);
        read_index = 0;
    };

    for (;;) {
        compact_if_needed();

        const size_t size = buf.size() - read_index;

        if (size < sizeof(HeaderData)) {
            return;
        }

        const auto begin_it = buf.begin() + read_index;
        const auto end_it = buf.end();

        auto it = std::search(begin_it, end_it, std::begin(PREAMBLE),
                              std::end(PREAMBLE));

        if (it == end_it) {
            if (size > 1) {
                read_index = buf.size() - 1;
            }
            return;
        }

        read_index = it - buf.begin();

        const uint8_t* frame = buf.data() + read_index;
        const size_t frame_size = buf.size() - read_index;

        HeaderData header = read_from_buffer<HeaderData>(frame, frame_size, 0);

        const uint8_t* payload = buf.data() + read_index + sizeof(HeaderData);
        const size_t payload_size = header.data_size;

        const size_t needed = sizeof(HeaderData) + payload_size;

        if (header.sync_byte != SYNC_BYTE) {
            goto resync;
        }

        if (calculate_checksum(frame, sizeof(HeaderData) - 2) !=
            header.header_checksum) {
            goto resync;
        }

        if (header.data_size > MAX_FRAME) {
            goto resync;
        }

        if (frame_size < needed) {
            return;  // not enough bytes
        }

        if (calculate_checksum(payload, payload_size) != header.data_checksum) {
            goto resync;
        }

        dispatch(payload, payload_size, header);
        read_index += needed;
        continue;
    resync: {
        auto next = std::search(buf.begin() + read_index + 1, buf.end(),
                                std::begin(PREAMBLE), std::end(PREAMBLE));
        if (next == buf.end()) {
            read_index = buf.size() ? buf.size() - 1 : 0;
            return;
        }
        read_index = static_cast<size_t>(std::distance(buf.begin(), next));
        continue;
    }
    }
}

void NortekNucleusDriver::dispatch(const uint8_t* payload,
                                   size_t payload_size,
                                   const HeaderData& header) {
    CommonData common_data_header =
        read_from_buffer<CommonData>(payload, payload_size, 0);

    constexpr size_t header_offset = sizeof(CommonData);
    const size_t data_offset = common_data_header.data_offset;
    const DataSeriesId id = static_cast<DataSeriesId>(header.data_series_id);

    switch (id) {
        case DataSeriesId::ImuData: {
            callback_(parse_imu(payload, payload_size, data_offset));
            break;
        }
        case DataSeriesId::MagnometerData: {
            callback_(read_from_buffer<MagnetoMeterData>(payload, payload_size,
                                                         header_offset));
            break;
        }
        case DataSeriesId::FieldCalibrationData: {
            callback_(read_from_buffer<FieldCalibrationData>(
                payload, payload_size, header_offset));
            break;
        }
        case DataSeriesId::FastPressureData: {
            callback_(read_from_buffer<FastPressureData>(payload, payload_size,
                                                         data_offset));
            break;
        }
        case DataSeriesId::AltimeterData: {
            callback_(read_from_buffer<AltimeterData>(payload, payload_size,
                                                      header_offset));
            break;
        }
        case DataSeriesId::BottomTrackData: {
            callback_(read_from_buffer<BottomTrackData>(payload, payload_size,
                                                        header_offset));
            break;
        }
        case DataSeriesId::WaterTrackData: {
            callback_(read_from_buffer<WaterTrackData>(payload, payload_size,
                                                       header_offset));
            break;
        }
        case DataSeriesId::CurrentProfileData: {
            callback_(
                parse_current_profile_data(payload, payload_size, data_offset));
            break;
        }
        case DataSeriesId::SpectrumDataV3: {
            callback_(parse_spectrum_data(payload, payload_size));
            break;
        }
        case DataSeriesId::AhrsData: {
            callback_(parse_ahrsv2_data(payload, payload_size, data_offset));
            break;
        }
        case DataSeriesId::InsData: {
            callback_(read_from_buffer<InsDataV2>(payload, payload_size,
                                                  data_offset + 72));
            break;
        }
        case DataSeriesId::StringData: {
            std::string data_string;
            data_string.resize(payload_size);
            std::memcpy(data_string.data(), payload, payload_size);
            callback_(data_string);
            break;
        }
        default:
            break;
    }
}

NucleusReply NortekNucleusDriver::send_command(const std::string& cmd) {
    NucleusReply reply{};

    boost::system::error_code ec;

    const std::string msg = cmd + "\r\n";

    boost::asio::write(nucleus_sock_, boost::asio::buffer(msg), ec);

    if (ec) {
        reply.status = NucleusStatusCode::SendFailed;
        return reply;
    }

    boost::asio::streambuf response_buffer;

    boost::asio::read_until(nucleus_sock_, response_buffer, "\r\n", ec);

    if (ec) {
        reply.status = NucleusStatusCode::ReadFailed;
        return reply;
    }

    std::istream stream(&response_buffer);

    std::string response;
    std::getline(stream, response);

    if (!response.empty() && response.back() == '\r') {
        response.pop_back();
    }

    reply.status = NucleusStatusCode::Ok;
    reply.payload = std::move(response);

    return reply;
}

NucleusStatusCode NortekNucleusDriver::start_nucleus() {
    const std::string cmd = "START";
    return send_command(cmd).status;
}

NucleusStatusCode NortekNucleusDriver::stop_nucleus() {
    const std::string cmd = "STOP";
    return send_command(cmd).status;
}

NucleusStatusCode NortekNucleusDriver::trigger_read() {
    const std::string cmd = "TRIG";
    return send_command(cmd).status;
}

NucleusReply NortekNucleusDriver::get_settings(const std::string& type) {
    std::string cmd = "GET" + type;
    return send_command(cmd);
}

NucleusReply NortekNucleusDriver::get_error() {
    std::string cmd = "GETERROR";
    return send_command(cmd);
}

NucleusStatusCode NortekNucleusDriver::set_bottom_track_settings(
    const BottomTrackSettings& settings) {
    std::string cmd = "SETBT,";

    switch (settings.mode) {
        case BottomTrackMode::FastACQ:
            cmd += "MODE=\"FAST_ACQ\"";
            break;
        case BottomTrackMode::Crawler:
            cmd += "MODE=\"CRAWLER\"";
            break;
        case BottomTrackMode::Auto:
            cmd += "MODE=\"AUTO\"";
            break;
    }
    cmd += ",";

    cmd += "VR=" + std::to_string(settings.velocity_range) + ",";

    if (settings.enable_watertrack) {
        cmd += "WT=\"ON\",";
    } else {
        cmd += "WT=\"OFF\",";
    }

    if (settings.power_level_user_defined) {
        cmd += "PLMODE=\"USER\",";
        cmd += "PL=" + std::to_string(settings.power_level) + ",";
    } else {
        cmd += "PLMODE=\"MAX\",";
    }

    switch (settings.data_stream_settings) {
        case NucleusDataStreamSettings::Off:
            cmd += "DS=\"OFF\",";
            break;
        case NucleusDataStreamSettings::On:
            cmd += "DS=\"ON\",";
            break;
        case NucleusDataStreamSettings::Cmd:
            cmd += "DS=\"CMD\",";
            break;
        case NucleusDataStreamSettings::Data:
            cmd += "DS=\"DATA\",";
            break;
    }

    switch (settings.data_format) {
        case NucleusDataFormats::BottomTrackBinaryFormat:
            cmd += "DF=180";
            break;
        case NucleusDataFormats::BottomTrackRDIPD6:
            cmd += "DF=156";
        default:
            break;
    }
    return send_command(cmd).status;
}

NucleusStatusCode NortekNucleusDriver::set_altimeter_settings(
    const AltimeterSettings& settings) {
    std::string cmd = "SETALTI,";
    cmd += "PL=" + std::to_string(settings.power_level) + ",";

    switch (settings.data_stream_settings) {
        case NucleusDataStreamSettings::Off:
            cmd += "DS=\"OFF\",";
            break;
        case NucleusDataStreamSettings::On:
            cmd += "DS=\"ON\",";
            break;
        case NucleusDataStreamSettings::Cmd:
            cmd += "DS=\"CMD\",";
            break;
        case NucleusDataStreamSettings::Data:
            cmd += "DS=\"DATA\",";
            break;
    }

    switch (settings.data_format) {
        case NucleusDataFormats::AltimeterFormat:
            cmd += "DF=170";
            break;
        default:
            break;
    }
    return send_command(cmd).status;
}

NucleusStatusCode NortekNucleusDriver::set_fast_pressure_settings(
    const FastPressureSettings& settings) {
    std::string cmd = "SETFASTPRESSURE,";

    if (settings.enable_fast_pressure) {
        cmd += "EN=1";
    } else {
        cmd += "EN=0";
    }

    cmd += "SR=" + std::to_string(settings.sampling_rate) + ",";

    switch (settings.data_stream_settings) {
        case NucleusDataStreamSettings::Off:
            cmd += "DS=\"OFF\",";
            break;
        case NucleusDataStreamSettings::On:
            cmd += "DS=\"ON\",";
            break;
        case NucleusDataStreamSettings::Cmd:
            cmd += "DS=\"CMD\",";
            break;
        case NucleusDataStreamSettings::Data:
            cmd += "DS=\"DATA\",";
            break;
    }

    switch (settings.data_format) {
        case NucleusDataFormats::FastPressureFormat:
            cmd += "DF=150";
            break;
        default:
            break;
    }
    return send_command(cmd).status;
}

NucleusStatusCode NortekNucleusDriver::set_magnetometer_settings(
    const MagnetometerSettings& settings) {
    std::string cmd = "SETMAG,";

    // cmd += "FREQ=" + std::to_string(settings.freq) + ",";

    switch (settings.mode) {
        case MagnetometerMethod::Off:
            cmd += "METHOD=\"OFF\",";
            break;
        case MagnetometerMethod::Auto:
            cmd += "METHOD=\"AUTO\",";
            break;
        case MagnetometerMethod::Wmm:
            cmd += "METHOD=\"WMM\",";
            break;
        default:
            break;
    }

    switch (settings.data_stream_settings) {
        case NucleusDataStreamSettings::Off:
            cmd += "DS=\"OFF\",";
            break;
        case NucleusDataStreamSettings::On:
            cmd += "DS=\"ON\",";
            break;
        case NucleusDataStreamSettings::Cmd:
            cmd += "DS=\"CMD\",";
            break;
        case NucleusDataStreamSettings::Data:
            cmd += "DS=\"DATA\",";
            break;
    }

    switch (settings.data_format) {
        case DataSeriesId::MagnometerData:
            cmd += "DF=135";
            break;
        default:
            break;
    }
    return send_command(cmd).status;
}

NucleusStatusCode NortekNucleusDriver::set_imu_settings(
    const ImuSettings& settings) {
    std::string cmd = "SETIMU,";

    cmd += "FREQ=" + std::to_string(settings.freq) + ",";

    switch (settings.data_stream_settings) {
        case NucleusDataStreamSettings::Off:
            cmd += "DS=\"OFF\",";
            break;
        case NucleusDataStreamSettings::On:
            cmd += "DS=\"ON\",";
            break;
        case NucleusDataStreamSettings::Cmd:
            cmd += "DS=\"CMD\",";
            break;
        case NucleusDataStreamSettings::Data:
            cmd += "DS=\"DATA\",";
            break;
    }

    switch (settings.data_format) {
        case DataSeriesId::ImuData:
            cmd += "DF=130";
            break;
        default:
            break;
    }
    return send_command(cmd).status;
}

NucleusStatusCode NortekNucleusDriver::set_ethernet_settings(
    const EthernetSettings& settings) {
    std::string cmd = "SETETH,";

    if (settings.use_static_ip) {
        cmd += "IPMETHOD=\"STATIC\",";
        cmd += "IP=\"" + settings.ip + "\",";
        cmd += "NETMASK=\"" + settings.netmask + "\",";
        cmd += "GATEWAY=\"" + settings.default_gateway + "\",";
    } else {
        cmd += "IPMETHOD=\"DHCP\",";
    }

    if (settings.use_password) {
        cmd += "PASSWORD=\"" + settings.password + "\",";
    } else {
        cmd += "PASSWORD=\"\"";
    }

    return send_command(cmd).status;
}

NucleusStatusCode NortekNucleusDriver::save_settings(
    const SaveSettings settings) {
    std::string cmd = "SAVE,";

    switch (settings) {
        case SaveSettings::All:
            cmd += "ALL";
            break;
        case SaveSettings::Config:
            cmd += "CONFIG";
            break;
        case SaveSettings::Comm:
            cmd += "COMM";
            break;
        case SaveSettings::Mission:
            cmd += "MISSION";
            break;
        case SaveSettings::Magcal:
            cmd += "MAGCAL";
            break;
        default:
            break;
    }

    return send_command(cmd).status;
}

NucleusStatusCode NortekNucleusDriver::set_instrument_settings(
    const InstrumentSettings& settings) {
    auto fmt = [](double v) {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(2) << v;
        return ss.str();
    };

    std::string cmd = "SETINST,";
    cmd += "ROTXY=" + fmt(settings.rotxy) + ",";
    cmd += "ROTYZ=" + fmt(settings.rotyz) + ",";
    cmd += "ROTXZ=" + fmt(settings.rotxz);

    auto sc = send_command(cmd).status;
    if (sc != NucleusStatusCode::Ok) {
        return sc;
    }

    return send_command("SAVE,CONFIG").status;
}

NucleusStatusCode NortekNucleusDriver::set_ahrs_settings(
    const AhrsSettings& settings) {
    std::string cmd = "SETAHRS,";

    cmd += "FREQ=" + std::to_string(settings.freq) + ",";

    switch (settings.mode) {
        case AhrsMode::FixedHardAndSoftIron:
            cmd += "METHOD=0,";
            break;
        case AhrsMode::HardIronEstimation:
            cmd += "METHOD=1,";
            break;
        case AhrsMode::HardAndSoftEstimation:
            cmd += "METHOD=2,";
            break;
        default:
            break;
    }

    switch (settings.data_stream_settings) {
        case NucleusDataStreamSettings::Off:
            cmd += "DS=\"OFF\",";
            break;
        case NucleusDataStreamSettings::On:
            cmd += "DS=\"ON\",";
            break;
        case NucleusDataStreamSettings::Cmd:
            cmd += "DS=\"CMD\",";
            break;
        case NucleusDataStreamSettings::Data:
            cmd += "DS=\"DATA\",";
            break;
    }

    switch (settings.data_format) {
        case DataSeriesId::AhrsData:
            cmd += "DF=210";
            break;
        default:
            break;
    }
    return send_command(cmd).status;
}

}  // namespace vortex::drivers::dvl
