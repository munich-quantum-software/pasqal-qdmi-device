set(QDMI_INTERNAL_TARGET_NAME pasqal_qdmi_device_internals)

# The device implementation as an object library, shared by the device target
# and the unit tests.
add_library(${QDMI_INTERNAL_TARGET_NAME} OBJECT)
add_library(pasqal::qdmi_device_internals ALIAS ${QDMI_INTERNAL_TARGET_NAME})
target_compile_features(${QDMI_INTERNAL_TARGET_NAME} PRIVATE cxx_std_20)
set_target_properties(
  ${QDMI_INTERNAL_TARGET_NAME}
  PROPERTIES POSITION_INDEPENDENT_CODE ON
             C_VISIBILITY_PRESET hidden
             CXX_VISIBILITY_PRESET hidden
             VISIBILITY_INLINES_HIDDEN 1)
target_sources(
  ${QDMI_INTERNAL_TARGET_NAME}
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/pasqal_cloud.cpp
          ${CMAKE_CURRENT_SOURCE_DIR}/pasqal_specs.cpp
          ${PROJECT_SOURCE_DIR}/src/pasqal_device.cpp)
target_include_directories(
  ${QDMI_INTERNAL_TARGET_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/include
                                      ${CMAKE_CURRENT_BINARY_DIR}/../include)
get_target_property(qdmi_library_VERSION qdmi::qdmi VERSION)
target_compile_definitions(
  ${QDMI_INTERNAL_TARGET_NAME}
  PRIVATE QDMI_VERSION="${qdmi_library_VERSION}"
          ${QDMI_PREFIX}_QDMI_DEVICE_VERSION="${PROJECT_VERSION}"
          ${QDMI_PREFIX}_QDMI_device_EXPORTS)
target_link_libraries(
  ${QDMI_INTERNAL_TARGET_NAME}
  PUBLIC cpr::cpr nlohmann_json::nlohmann_json
  PRIVATE qdmi::qdmi_project_warnings)
