include(GNUInstallDirs)

generate_prefixed_qdmi_headers(${QDMI_PREFIX})

file(GLOB_RECURSE PUBLIC_HEADER_FILES "${CMAKE_CURRENT_BINARY_DIR}/include/*.h")
list(APPEND PUBLIC_HEADER_FILES
     "${PROJECT_SOURCE_DIR}/include/pasqal-qdmi-device/constants.h")

add_subdirectory(internal)

add_library(${QDMI_TARGET_NAME} SHARED)
target_compile_features(${QDMI_TARGET_NAME} PRIVATE cxx_std_20)
set_target_properties(
  ${QDMI_TARGET_NAME}
  PROPERTIES VERSION ${PROJECT_VERSION}
             SOVERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
             C_VISIBILITY_PRESET hidden
             CXX_VISIBILITY_PRESET hidden
             VISIBILITY_INLINES_HIDDEN 1)
target_link_libraries(${QDMI_TARGET_NAME} PRIVATE qdmi::qdmi_project_warnings
                                                  pasqal::qdmi_device_internals)
# Keep the statically linked libcurl and cpr symbols out of the exported interface.
if(UNIX AND NOT APPLE)
  target_link_options(${QDMI_TARGET_NAME} PRIVATE "LINKER:--exclude-libs,ALL")
endif()
configure_qdmi_device_target(TARGET ${QDMI_TARGET_NAME} ID ${PASQAL_QDMI_DEVICE_ID}
                             PREFIX ${QDMI_PREFIX})

target_sources(
  ${QDMI_TARGET_NAME}
  PUBLIC FILE_SET
         public_headers
         TYPE
         HEADERS
         BASE_DIRS
         ${CMAKE_CURRENT_BINARY_DIR}/include
         ${PROJECT_SOURCE_DIR}/include
         FILES
         ${PUBLIC_HEADER_FILES})

# Device catalog: one entry per Pasqal backend. The IDs double as Slurm license names.
set(PASQAL_QDMI_CATALOG_NAME "${QDMI_TARGET_NAME}.qdmi.json")
set(PASQAL_QDMI_LIBRARY_FILE "$<TARGET_FILE_NAME:${QDMI_TARGET_NAME}>")
configure_file("${PROJECT_SOURCE_DIR}/cmake/${QDMI_TARGET_NAME}.qdmi.json.in"
               "${CMAKE_CURRENT_BINARY_DIR}/${QDMI_TARGET_NAME}.qdmi.json.gen.in" @ONLY)
set(PASQAL_QDMI_CATALOG "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/${PASQAL_QDMI_CATALOG_NAME}")
file(
  GENERATE
  OUTPUT "${PASQAL_QDMI_CATALOG}"
  INPUT "${CMAKE_CURRENT_BINARY_DIR}/${QDMI_TARGET_NAME}.qdmi.json.gen.in")
add_custom_command(
  TARGET ${QDMI_TARGET_NAME}
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PASQAL_QDMI_CATALOG}"
          "$<TARGET_FILE_DIR:${QDMI_TARGET_NAME}>/${PASQAL_QDMI_CATALOG_NAME}")
set_target_properties(${QDMI_TARGET_NAME} PROPERTIES QDMI_MANIFEST_NAME
                                                     ${PASQAL_QDMI_CATALOG_NAME})
set_property(
  TARGET ${QDMI_TARGET_NAME}
  APPEND
  PROPERTY EXPORT_PROPERTIES QDMI_MANIFEST_NAME)

set(${QDMI_PREFIX}_QDMI_CONFIG_INSTALL_DIR
    "${CMAKE_INSTALL_DATADIR}/cmake/${QDMI_TARGET_NAME}"
    CACHE INTERNAL "Config install directory")

include(CMakePackageConfigHelpers)
configure_package_config_file(
  "${PROJECT_SOURCE_DIR}/cmake/${QDMI_TARGET_NAME}-config.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/${QDMI_TARGET_NAME}-config.cmake"
  INSTALL_DESTINATION ${${QDMI_PREFIX}_QDMI_CONFIG_INSTALL_DIR}
  NO_SET_AND_CHECK_MACRO NO_CHECK_REQUIRED_COMPONENTS_MACRO)
write_basic_package_version_file(
  "${QDMI_TARGET_NAME}-config-version.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMinorVersion)

if(INSTALL_PASQAL_QDMI_DEVICE)
  install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/${QDMI_TARGET_NAME}-config.cmake"
          "${CMAKE_CURRENT_BINARY_DIR}/${QDMI_TARGET_NAME}-config-version.cmake"
    DESTINATION ${${QDMI_PREFIX}_QDMI_CONFIG_INSTALL_DIR}
    COMPONENT ${QDMI_TARGET_NAME}_Development)

  # cmake-format: off
  install(
    TARGETS ${QDMI_TARGET_NAME}
    EXPORT ${QDMI_TARGET_NAME}-targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            COMPONENT ${QDMI_TARGET_NAME}_Runtime
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT ${QDMI_TARGET_NAME}_Runtime
            NAMELINK_COMPONENT ${QDMI_TARGET_NAME}_Development
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT ${QDMI_TARGET_NAME}_Development
    FILE_SET public_headers
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
            COMPONENT ${QDMI_TARGET_NAME}_Development)
  # cmake-format: on

  install(
    FILES "${PASQAL_QDMI_CATALOG}"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}
    COMPONENT ${QDMI_TARGET_NAME}_Runtime)

  install(
    EXPORT ${QDMI_TARGET_NAME}-targets
    FILE ${QDMI_TARGET_NAME}-targets.cmake
    DESTINATION ${${QDMI_PREFIX}_QDMI_CONFIG_INSTALL_DIR}
    COMPONENT ${QDMI_TARGET_NAME}_Development)
endif()
