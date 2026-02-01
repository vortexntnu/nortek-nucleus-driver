#!/bin/bash
set -e

# ----------------------------- GLOBAL VARIABLES -----------------------------
NORTEK_NUCLEUS_DRIVER_DIR="${GITHUB_WORKSPACE}/nortek_nucleus_driver"
LOG_PREFIX="[$(date +%T)]"

# ----------------------------- HELPER FUNCTIONS -----------------------------
log_info() {
    echo -e "$LOG_PREFIX [INFO] $1"
}

log_error() {
    echo -e "$LOG_PREFIX [ERROR] $1" >&2
}

# ----------------------------- C++ DEPENDENCIES -----------------------------
install_cpp_dependencies() {
    log_info "Installing required C++ dependencies..."
    sudo apt-get update -qq
    sudo apt-get install -y \
        build-essential \
        cmake \
        git \
        libasio-dev
    log_info "C++ dependencies installed."
}

# ----------------------------- NORTEK_NUCLEUS_DRIVER INSTALLATION -----------------------------
install_nortek_nucleus_driver() {
    log_info "Building Nortek Nucleus driver..."
    mkdir -p "$NORTEK_NUCLEUS_DRIVER_DIR/build"
    cd "$NORTEK_NUCLEUS_DRIVER_DIR/build"

    cmake ..
    make -j"$(nproc)"
    sudo make install

    log_info "Nortek Nucleus driver installation complete."

}


# ----------------------------- EXECUTE INSTALLATION -----------------------------
install_cpp_dependencies
install_nortek_nucleus_driver

