cmake_minimum_required(VERSION 3.18)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT DEFINED DMOD_MODULE_VERSION)
    set(DMOD_MODULE_VERSION "0.1" CACHE STRING "DMOD module version")
endif()

# If a local dmod checkout is provided, use it instead of fetching from git
if(DEFINED DMOD_DIR AND NOT "${DMOD_DIR}" STREQUAL "")
    set(FETCHCONTENT_SOURCE_DIR_DMOD "${DMOD_DIR}" CACHE PATH "" FORCE)
endif()

include(FetchContent)
FetchContent_Declare(
    dmod
    GIT_REPOSITORY https://github.com/choco-technologies/dmod.git
    GIT_TAG        develop
)

set(DMOD_MODE "DMOD_MODULE" CACHE STRING "DMOD build mode")
set(DMOD_BUILD_TESTS OFF CACHE BOOL "Build tests")
set(DMOD_BUILD_EXAMPLES OFF CACHE BOOL "Build examples")
set(DMOD_BUILD_TOOLS OFF CACHE BOOL "Build tools")
set(DMOD_BUILD_TEMPLATES OFF CACHE BOOL "Build templates")

FetchContent_MakeAvailable(dmod)

project(phoneui VERSION ${DMOD_MODULE_VERSION}
    DESCRIPTION "phoneui DMOD graphical interface of the phone"
    LANGUAGES C CXX)

set(DMOD_DIR ${dmod_SOURCE_DIR} CACHE PATH "DMOD source directory")
set(DMOD_SCRIPTS_DIR ${DMOD_DIR}/scripts CACHE PATH "DMOD scripts directory")
include(${DMOD_DIR}/paths.cmake)
dmod_setup_external_module()

# ======================================================================
#               The views: screens/<screen>/index.html
# ======================================================================
# One page for each kind of screen (docs/screens.md): screens/<screen>/index.html, with index.html.ini for the options of its conversion.
# dmod's DMOD_ASSETS_PATHS does all of it at build time: todmvs lays the page out and writes the view with the fonts and images it draws,
# and they become the views (.dmv, .dmvf, .dmvi) - see "Pages" in dmod's scripts/CMakeLists.txt. The size the page is laid out for is
# the end of the screen's id. Every screen is a bundle of the package phoneui_view (its .dmr declares it with [screen=<id>]); dmf-get picks
# one with --screen <id>.
set(PHONEUI_SCREENS epaper-240x320)                 # the ids of the screens (the names of the directories under screens/)

# ======================================================================
#               phoneui_view - the views (data)
# ======================================================================
set(DMOD_MODULE_NAME        phoneui_view)
set(DMOD_AUTHOR_NAME        "Patryk Kubiak")
set(DMOD_STACK_SIZE         1024)
set(DMOD_PRIORITY           0)
set(DMOD_DMR_PATH           ${CMAKE_CURRENT_SOURCE_DIR}/phoneui_view.dmr)
set(DMOD_ASSETS_PATHS       ${CMAKE_CURRENT_SOURCE_DIR}/screens)
set(DMOD_ASSETS_SCREENS     ${PHONEUI_SCREENS})
set(DMOD_MAL_IMPLS          )

dmod_add_executable(${DMOD_MODULE_NAME} ${DMOD_MODULE_VERSION}
    src/phoneui_view.c
)

# ======================================================================
#               phoneui - the application (the service)
# ======================================================================
set(DMOD_MODULE_NAME        phoneui)
set(DMOD_AUTHOR_NAME        "Patryk Kubiak")
# (starting an application loads its module on this stack: 4096 overflowed on the ESP32-S3)
set(DMOD_STACK_SIZE         6144)
set(DMOD_PRIORITY           1)
set(DMOD_DMR_PATH           ${CMAKE_CURRENT_SOURCE_DIR}/phoneui.dmr)
set(DMOD_MAL_IMPLS          )

dmod_add_executable(${DMOD_MODULE_NAME} ${DMOD_MODULE_VERSION}
    src/phoneui.c
    src/ui.c
    src/backend_stub.c
)

# libdmview: the display claimed, the channel of the claim (dmdrvi: its types). libsystemd: the stop request of the service. dmosi: the waits.
dmod_link_modules(${DMOD_MODULE_NAME}
    dmdrvi@>=2.15
    dmosi
    libdmview@>=0.20
    libsystemd
)

enable_testing()
add_subdirectory(tests)

# dmvshot: the interface on a PC (not shipped; the build of the host) and its tour of the pages (ctest)
if(NOT DMOD_TOOLS_NAME OR DMOD_TOOLS_NAME MATCHES "x86_64")
    add_subdirectory(tools/dmvshot)
endif()
