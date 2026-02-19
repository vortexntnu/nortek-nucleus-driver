
# Nortek Nucleus Driver

A modern C++ driver for communicating with the **Nortek Nucleus** sonar system.

This driver provides a clean, C++17-based interface for receiving and parsing data from a Nortek Nucleus using TCP networking via **Asio** or **Boost.Asio**.

---

## Features

* Modern **C++17** interface
* Header-only networking via **Asio** or **Boost.Asio**
* Minimal dependencies
* Easy CMake integration
* Designed for real-time data streaming

---

## Requirements

* **C++17** or newer
* One of:

  * Standalone `asio` (header-only), or
  * `Boost.Asio`
* CMake 3.14+ recommended
* Familiarity with the **Nortek Nucleus User Manual** is strongly recommended

---

## Supported Data Formats

The driver supports parsing the following Nortek data formats:

* IMU Data
* Magnetometer Data
* Bottom Track Data
* Water Track Data
* Altimeter Data
* AHRS and INS Data V2
* Current Profile Data
* String Data
* Spectrum Data V3

---

## Building

```bash
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j
```

---

## Installing

To install the driver system-wide:

```bash
cd build
sudo make install
```

This will install headers and CMake config files for easy use in other projects.

---

## Using the Driver in Your Project

### Include the Header

```cpp
#include <nortek_nucleus_driver.hpp>
```

### CMake Integration

```cmake
find_package(nortek_nucleus_driver REQUIRED)
target_link_libraries(your_target PRIVATE nortek_nucleus_driver)
```

---

## Network Setup

Before running:

* Ensure the Nucleus and host PC are on the **same subnet**
* Verify the correct IP address and port from the Nucleus configuration
* Ensure required TCP ports are not blocked by a firewall

---

## Basic Example

```cpp
#include <nortek_nucleus_driver.hpp>

int main() {
    asio::io_context io;

    ConnectionParams params;
    params.remote_ip = "192.169.53.100";
    params.data_remote_port = 9000;
    params.password = "very_secret_password";

    NortekNucleusDriver driver(io, callback);

    driver.open_tcp_sockets(params);

    // If the Nucleus is configured to use a password
    driver.enter_password(params);


    driver.start_read();

    // Settings should be configured before start_nucleus()

    driver.start_nucleus();


    io.run();
}
```

---

## Configuration

Typical configuration options include:

* Device IP address
* TCP port
* Enabled data formats
* Output rates

Example:

```cpp

struct MagnetometerSettings {
    int freq;
    MagnetometerMethod mode;
    NucleusDataStreamSettings data_stream_settings;
    DataSeriesId data_format;
};

MagnetometerSettings mag_settings{};
mag_settings.freq = 2;
mag_settings.mode = MagnetometerMethod::Auto;
mag_settings.data_stream_settings = NucleusDataStreamSettings::On;
data_format.data_format = DataSeriesId::MagnometerData;

auto status_code = set_magnetometer_settings(mag_settings);

if (status_code != NucleusStatusCode::Ok){
    // Handle error
}

```

Refer to the Nortek Nucleus manual for valid configuration values.

---

## Notes

* The driver does not manage threading beyond the provided `asio::io_context`
* All callbacks are executed in the context of the Asio event loop

---

## Author

**Nathaniel Førrisdahl**
