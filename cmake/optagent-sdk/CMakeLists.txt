cmake_minimum_required(VERSION 3.25)
project(OptAgentCpsat VERSION 9.15.6755 LANGUAGES C CXX)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
get_filename_component(_source_root "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
find_package(absl CONFIG REQUIRED)
find_package(protobuf 33.1.0 EXACT CONFIG REQUIRED)
find_package(OptAgentSupport CONFIG REQUIRED)
find_package(Threads REQUIRED)
if(NOT TARGET protobuf::protoc)
  if(NOT EXISTS "${OPTAGENT_HOST_PROTOC}")
    message(FATAL_ERROR "A version-matched host protoc is required")
  endif()
  add_executable(protobuf::protoc IMPORTED)
  set_target_properties(protobuf::protoc PROPERTIES IMPORTED_LOCATION "${OPTAGENT_HOST_PROTOC}")
endif()
include("${CMAKE_CURRENT_LIST_DIR}/sources.cmake")
set(_generated "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(_sources)
foreach(source IN LISTS OPTAGENT_CPSAT_SOURCES)
  list(APPEND _sources "${_source_root}/${source}")
endforeach()
set(_protos)
set(_generated_sources)
set(_generated_headers)
foreach(proto IN LISTS OPTAGENT_CPSAT_PROTOS)
  string(REGEX REPLACE "\\.proto$" ".pb.cc" cc "${proto}")
  string(REGEX REPLACE "\\.proto$" ".pb.h" hh "${proto}")
  list(APPEND _protos "${_source_root}/${proto}")
  list(APPEND _generated_sources "${_generated}/${cc}")
  list(APPEND _generated_headers "${_generated}/${hh}")
  get_filename_component(directory "${hh}" DIRECTORY)
  install(FILES "${_generated}/${hh}" DESTINATION "include/${directory}")
endforeach()
file(MAKE_DIRECTORY "${_generated}")
add_custom_command(OUTPUT ${_generated_sources} ${_generated_headers}
  COMMAND protobuf::protoc "--proto_path=${_source_root}"
    "--cpp_out=dllexport_decl=OR_PROTO_DLL:${_generated}" ${_protos}
  DEPENDS protobuf::protoc ${_protos} VERBATIM)
add_library(optagent_cpsat ${_sources} ${_generated_sources} session.cc)
add_library(OptAgentCpsat::cpsat ALIAS optagent_cpsat)
set_target_properties(optagent_cpsat PROPERTIES
  EXPORT_NAME cpsat OUTPUT_NAME optagent_cpsat
  VERSION 1.0.0 SOVERSION 1 WINDOWS_EXPORT_ALL_SYMBOLS ON)
target_compile_features(optagent_cpsat PUBLIC cxx_std_20)
target_include_directories(optagent_cpsat SYSTEM PUBLIC
  "$<BUILD_INTERFACE:${_source_root}>" "$<BUILD_INTERFACE:${_generated}>"
  "$<INSTALL_INTERFACE:include>")
target_include_directories(optagent_cpsat PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>" "$<INSTALL_INTERFACE:include>")
target_compile_definitions(optagent_cpsat PRIVATE
  OR_TOOLS_MAJOR=9 OR_TOOLS_MINOR=15 OR_TOOLS_PATCH=6755)
if(BUILD_SHARED_LIBS)
  if(WIN32)
    target_compile_definitions(optagent_cpsat PUBLIC OR_BUILD_DLL
      PRIVATE OR_EXPORT "OR_PROTO_DLL=__declspec(dllexport)"
      INTERFACE "OR_PROTO_DLL=__declspec(dllimport)")
  else()
    target_compile_definitions(optagent_cpsat PUBLIC "OR_PROTO_DLL=")
  endif()
else()
  target_compile_definitions(optagent_cpsat PUBLIC OPTAGENT_CPSAT_STATIC "OR_PROTO_DLL=")
endif()
# Only ABI-visible protobuf is public. In particular Windows Abseil flags
# archives must not be propagated into a second DLL.
target_link_libraries(optagent_cpsat PUBLIC protobuf::libprotobuf
  PRIVATE ${OPTAGENT_CPSAT_ABSL_TARGETS} Threads::Threads
    OptAgentSupport::zlib OptAgentSupport::bzip2)
if(MSVC)
  target_compile_definitions(optagent_cpsat PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
  target_compile_options(optagent_cpsat PRIVATE /bigobj /Zc:preprocessor /fp:precise)
  target_link_libraries(optagent_cpsat PRIVATE psapi ws2_32)
elseif(APPLE)
  target_link_libraries(optagent_cpsat PRIVATE "-framework CoreFoundation")
  set_target_properties(optagent_cpsat PROPERTIES INSTALL_RPATH "@loader_path")
elseif(UNIX)
  set_target_properties(optagent_cpsat PROPERTIES INSTALL_RPATH "$ORIGIN")
endif()
foreach(header IN LISTS OPTAGENT_CPSAT_HEADERS)
  get_filename_component(directory "${header}" DIRECTORY)
  install(FILES "${_source_root}/${header}" DESTINATION "include/${directory}")
endforeach()
install(DIRECTORY include/ DESTINATION include)
install(TARGETS optagent_cpsat EXPORT OptAgentCpsatTargets
  RUNTIME DESTINATION bin LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
install(EXPORT OptAgentCpsatTargets NAMESPACE OptAgentCpsat::
  DESTINATION lib/cmake/OptAgentCpsat)
configure_package_config_file(OptAgentCpsatConfig.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/OptAgentCpsatConfig.cmake"
  INSTALL_DESTINATION lib/cmake/OptAgentCpsat)
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/OptAgentCpsatConfigVersion.cmake"
  VERSION ${PROJECT_VERSION} COMPATIBILITY ExactVersion)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/OptAgentCpsatConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/OptAgentCpsatConfigVersion.cmake"
  DESTINATION lib/cmake/OptAgentCpsat)
option(OPTAGENT_CPSAT_BUILD_TESTS "Build the SDK session contract" OFF)
if(OPTAGENT_CPSAT_BUILD_TESTS)
  enable_testing()
  add_executable(cpsat_session_test session_test.cc)
  target_link_libraries(cpsat_session_test PRIVATE OptAgentCpsat::cpsat Threads::Threads)
  add_test(NAME cpsat_session_contract COMMAND cpsat_session_test)
endif()
