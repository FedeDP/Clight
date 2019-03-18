cmake_minimum_required(VERSION 3.5)

project(clight VERSION 3.2 LANGUAGES C)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

include(GNUInstallDirs)
find_package(PkgConfig)


# Options
set(CLIGHT_CONFDIR "${CMAKE_INSTALL_FULL_SYSCONFDIR}/default"
    CACHE FILEPATH "Path for config file")


# Create program target
file(GLOB_RECURSE SOURCES src/*.c)
add_executable(${PROJECT_NAME} ${SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE
                           # Internal headers
                           "${CMAKE_CURRENT_SOURCE_DIR}/src"
                           "${CMAKE_CURRENT_SOURCE_DIR}/src/conf"
                           "${CMAKE_CURRENT_SOURCE_DIR}/src/modules"
                           "${CMAKE_CURRENT_SOURCE_DIR}/src/utils"
)
target_compile_definitions(${PROJECT_NAME} PRIVATE
    -D_GNU_SOURCE
    -DVERSION="${PROJECT_VERSION}"
    -DCONFDIR="${CLIGHT_CONFDIR}"
)
set_property(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD 99)


# Required dependencies
pkg_check_modules(REQ_LIBS REQUIRED popt gsl libconfig)
pkg_search_module(LOGIN_LIBS REQUIRED libelogind libsystemd>=221)

# Avoid float versioning for libsystemd/libelogind
string(REPLACE "." ";" LOGIN_LIBS_VERSION_LIST ${LOGIN_LIBS_VERSION})
list(GET LOGIN_LIBS_VERSION_LIST 0 LOGIN_LIBS_VERSION_MAJOR)
message(STATUS "Found lib${LOGIN_LIBS_LIBRARIES} version ${LOGIN_LIBS_VERSION_MAJOR}")

target_link_libraries(${PROJECT_NAME}
                      m
                      ${REQ_LIBS_LIBRARIES}
                      ${LOGIN_LIBS_LIBRARIES}
)
target_include_directories(${PROJECT_NAME} PRIVATE
                           "${REQ_LIBS_INCLUDE_DIRS}"
                           "${LOGIN_LIBS_INCLUDE_DIRS}"
)
target_compile_definitions(${PROJECT_NAME} PRIVATE
    -DLIBSYSTEMD_VERSION=${LOGIN_LIBS_VERSION_MAJOR}
)
list(APPEND COMBINED_LDFLAGS ${REQ_LIBS_LDFLAGS})
list(APPEND COMBINED_LDFLAGS ${LOGIN_LIBS_LDFLAGS})

# Convert ld flag list from list to space separated string.
string(REPLACE ";" " " COMBINED_LDFLAGS "${COMBINED_LDFLAGS}")

# Set the LDFLAGS target property
set_target_properties(
    ${PROJECT_NAME} PROPERTIES
    LINK_FLAGS "${COMBINED_LDFLAGS}"
)

# Installation of targets (must be before file configuration to work)
install(TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

# Configure files with install paths
set(EXTRA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Extra")

configure_file(${EXTRA_DIR}/org.clight.clight.service
               org.clight.clight.service
               @ONLY)

# Installation of files
pkg_get_variable(COMPLETIONS_DIR bash-completion completionsdir)
pkg_get_variable(SESSION_BUS_DIR dbus-1 session_bus_services_dir)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.clight.clight.service
        DESTINATION ${SESSION_BUS_DIR})
install(FILES ${EXTRA_DIR}/clight.conf
        DESTINATION ${CLIGHT_CONFDIR})
install(FILES ${EXTRA_DIR}/desktop/clight.desktop
        DESTINATION /etc/xdg/autostart)
install(FILES ${EXTRA_DIR}/desktop/clightc.desktop
        DESTINATION /usr/share/applications)

if (COMPLETIONS_DIR)
    install(FILES ${EXTRA_DIR}/clight
            DESTINATION ${COMPLETIONS_DIR})
endif()
install(FILES ${EXTRA_DIR}/icons/clight.svg
        DESTINATION /usr/share/icons/hicolor/scalable/apps)
