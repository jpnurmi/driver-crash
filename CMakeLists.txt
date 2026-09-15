cmake_minimum_required(VERSION 3.24)
project(driver-crash LANGUAGES C CXX)

if(NOT WIN32 OR NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "This harness requires Windows x64 and MSVC")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "$<1:${CMAKE_BINARY_DIR}/bin>")
find_package(Qt6 REQUIRED COMPONENTS Quick OpenGL)

set(SENTRY_BACKEND native CACHE STRING "" FORCE)
set(SENTRY_INTEGRATION_WER OFF CACHE BOOL "" FORCE)
set(SENTRY_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(SENTRY_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SENTRY_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(../sentry-native sentry-native)

qt_standard_project_setup()
qt_add_executable(driver-crash main.cpp)
qt_add_resources(driver-crash scene PREFIX / FILES Main.qml)
target_compile_definitions(driver-crash PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(driver-crash PRIVATE Qt6::Quick Qt6::OpenGL sentry opengl32)
add_dependencies(driver-crash sentry-crash sentry-wer)

get_target_property(QMAKE_EXECUTABLE Qt6::qmake IMPORTED_LOCATION)
get_filename_component(QT_BIN_DIR "${QMAKE_EXECUTABLE}" DIRECTORY)
set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP TRUE)
include(InstallRequiredSystemLibraries)
add_custom_target(deploy
    COMMAND "${QT_BIN_DIR}/windeployqt.exe" --qmldir "${CMAKE_CURRENT_SOURCE_DIR}"
        "$<TARGET_FILE:driver-crash>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS}
        "$<TARGET_FILE_DIR:driver-crash>"
    DEPENDS driver-crash
    VERBATIM)
