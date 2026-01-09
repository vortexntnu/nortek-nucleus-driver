# Nortek Nucleus Driver

Driver for the Nortek Nucleus 

---

## Features

* Modern C++17 interface
* Header-only networking via **Asio** or **Boost.Asio**
* Minimal dependencies and easy CMake integration

---

## Requirements

* **C++17** or newer
* One of:
  * Standalone `asio` (header-only), or
  * `Boost.Asio`
* Familiarity with the **Nortek User Manual** is recommended

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

After building, install the driver system-wide:

```bash
cd build
sudo make install
```

---

## Using the Driver in Your Project

### Include Header

```cpp
#include <nortek_nucleus_driver.hpp>
```

---

## Supported Data Formats

// TODO

---

## Basic Usage

### Network Setup

Ensure that:

* The sonar and host PC are on the **same subnet**
* Required TCP ports are open and not blocked by a firewall

---

### Example

```cpp
int main() {
    }
```
---
## Configuration



```c++
```

---

## Notes

* The driver does not manage threading beyond the provided `asio::io_context`.

---

## Author

**Nathaniel Førrisdahl**

---

