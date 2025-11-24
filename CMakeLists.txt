cmake_minimum_required(VERSION 3.16)
project(nortek_nucleus_driver VERSION 1.0.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

add_library(nortek_nucleus_driver
  src/nortek_nucleus_driver.cpp
)
target_compile_features(nortek_nucleus_driver PUBLIC cxx_std_20)


target_include_directories(nortek_nucleus_driver
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)


add_library(nortek_nucleus_driver::nortek_nucleus_driver ALIAS nortek_nucleus_driver)

# Install targets + headers
install(TARGETS nortek_nucleus_driver
  EXPORT nortek_nucleus_driverTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# Generate package config files
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/nortek_nucleus_driverConfigVersion.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)
configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/nortek_nucleus_driverConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/nortek_nucleus_driverConfig.cmake"
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/nortek_nucleus_driver
)

install(EXPORT nortek_nucleus_driverTargets
  NAMESPACE nortek_nucleus_driver::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/nortek_nucleus_driver
)
install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/nortek_nucleus_driverConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/nortek_nucleus_driverConfigVersion.cmake"
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/nortek_nucleus_driver
)
