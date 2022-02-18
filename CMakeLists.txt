cmake_minimum_required(VERSION 3.4) # WINDOWS_EXPORT_ALL_SYMBOLS needs version >= 3.4
project(razor)

option(RAZOR_BUILD_SHARED "Build razor as shared library" OFF)
option(RAZOR_BUILD_SIM_TRANSPORT "Build sim_transport" OFF)
option(RAZOR_BUILD_RAZOR_TESTS "Build unittests" OFF)

file(GLOB_RECURSE RAZOR_SRC_FILES 
    bbr/*
    cc/*
    common/*
    estimator/*
    pacing/*
    remb/*
)

if (WIN32)
    list(FILTER RAZOR_SRC_FILES EXCLUDE REGEX "posix/posix.c")
else()
    list(FILTER RAZOR_SRC_FILES EXCLUDE REGEX "windows/mscc.c")
endif()

if (RAZOR_BUILD_SHARED)
    add_library(razor STATIC ${RAZOR_SRC_FILES})
else ()
    add_library(razor SHARED ${RAZOR_SRC_FILES})
endif ()
target_include_directories(razor PUBLIC
    bbr
    cc
    common
    estimator
    pacing
    remb
)

set_target_properties(razor
PROPERTIES
    C_STANDARD 99
    POSITION_INDEPENDENT_CODE ON
)

if (WIN32)
    target_link_libraries(razor
    PUBLIC
        advapi32
        iphlpapi
        psapi
        shell32
        userenv
        ws2_32
    )

    if (MSVC)
        # Razor does not provide a 'def' file or uss 'dllexport' decoration, thus we export all symbles
        set_target_properties(razor
        PROPERTIES
            WINDOWS_EXPORT_ALL_SYMBOLS ON
        )
    endif ()
endif ()

if (RAZOR_BUILD_RAZOR_TESTS)
    file(GLOB_RECURSE RAZOR_UNITTESTS
        test/*
    )
    find_package(Threads REQUIRED)
    add_executable(razor_unittests ${RAZOR_UNITTESTS})
    if (NOT WIN32)
        target_link_libraries(razor_unittests razor m Threads::Threads)
    endif ()
endif()

if (RAZOR_BUILD_SIM_TRANSPORT)
    file(GLOB_RECURSE SIM_TRANSPORT 
        sim_transport/*
    )
    add_library(sim_transport ${SIM_TRANSPORT})
    target_include_directories(sim_transport PUBLIC
        sim_transport
        sim_transport/fec
    )
    target_link_libraries(sim_transport PUBLIC
        $<TARGET_NAME:razor>
    )
    set_target_properties(sim_transport
    PROPERTIES
        C_STANDARD 99
    )
endif()
