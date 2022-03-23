cmake_minimum_required(VERSION 3.10)

project(obs-healthbar-sensor-webcam-frame VERSION 1.0.0)

set(PLUGIN_AUTHOR "Jaime Manjavacas and Matthaios Patiño")

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/external")

set(CMAKE_INCLUDE_CURRENT_DIR ON)

# In case you need C++
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(LibObs REQUIRED)
find_package(FFmpeg REQUIRED
	COMPONENTS avcodec avfilter avdevice avutil swscale avformat swresample)
find_package(CURL REQUIRED)

configure_file(
    src/plugin-macros.h.in
    ../src/plugin-macros.generated.h
)
configure_file(
    installer/installer-macOS.pkgproj.in
    ../installer/installer-macOS.generated.pkgproj
)
configure_file(
    installer/installer-Windows.iss.in
    ../installer/installer-Windows.generated.iss
)

configure_file(
    ci/ci_includes.sh.in
    ../ci/ci_includes.generated.sh
)
configure_file(
    ci/ci_includes.cmd.in
    ../ci/ci_includes.generated.cmd
)

set(PLUGIN_SOURCES
    src/plugin-main.c
    src/healthbar-sensor-webcam-frame.c
    media-playback/decode.c
    media-playback/media.c
)

set(PLUGIN_HEADERS
    src/plugin-macros.generated.h
    media-playback/closest-format.h
    media-playback/decode.h
    media-playback/media.h
)

# --- Platform-independent build settings ---
add_library(${CMAKE_PROJECT_NAME} MODULE ${PLUGIN_SOURCES} ${PLUGIN_HEADERS})

#find_path(MEDIA_PLAYBACK_DIR NAMES media.h)

include_directories(
    ${LIBOBS_INCLUDE_DIR}
    ${FFMPEG_INCLUDE_DIRS}
    "${LIBOBS_INCLUDE_DIR}/../UI/obs-frontend-api"
    ${CURL_INCLUDE_DIR}
    #${MEDIA_PLAYBACK_DIR}
)
#add_subdirectory(${MEDIA_PLAYBACK_DIR} "${CMAKE_CURRENT_BINARY_DIR}/media-playback")

target_link_libraries(${CMAKE_PROJECT_NAME}
    ${LIBOBS_LIB}
    ${FFMPEG_LIBRARIES}
    ${OBS_FRONTEND_LIB}
    ${CURL_LIBRARY}
)

# --- End of section ---

# --- Windows-specific build settings and tasks ---
#[[
if(WIN32)
    # Enable Multicore Builds and disable FH4 (to not depend on VCRUNTIME140_1.DLL when building with VS2019)
    if (MSVC)
        add_definitions(/MP /d2FH4-)
    endif()

    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(ARCH_NAME "64bit")
        set(OBS_BUILDDIR_ARCH "build64")
    else()
        set(ARCH_NAME "32bit")
        set(OBS_BUILDDIR_ARCH "build32")
    endif()

    # --- Release package helper ---
    # The "release" folder has a structure similar OBS' one on Windows
    set(RELEASE_DIR "${PROJECT_SOURCE_DIR}/release")

    add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
        # If config is Release or RelWithDebInfo, package release files
        COMMAND if $<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>==1 (
            "${CMAKE_COMMAND}" -E make_directory
                "${RELEASE_DIR}/data/obs-plugins/${CMAKE_PROJECT_NAME}"
                "${RELEASE_DIR}/obs-plugins/${ARCH_NAME}"
        )

        COMMAND if $<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>==1 (
            "${CMAKE_COMMAND}" -E copy_directory
                "${PROJECT_SOURCE_DIR}/data"
                "${RELEASE_DIR}/data/obs-plugins/${CMAKE_PROJECT_NAME}"
        )

        COMMAND if $<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>==1 (
            "${CMAKE_COMMAND}" -E copy
                "$<TARGET_FILE:${CMAKE_PROJECT_NAME}>"
                "${RELEASE_DIR}/obs-plugins/${ARCH_NAME}"
        )

        # If config is RelWithDebInfo, copy the pdb file
        COMMAND if $<CONFIG:RelWithDebInfo>==1 (
            "${CMAKE_COMMAND}" -E copy
                "$<TARGET_PDB_FILE:${CMAKE_PROJECT_NAME}>"
                "${RELEASE_DIR}/obs-plugins/${ARCH_NAME}"
        )

        # Copy to obs-studio dev environment for immediate testing
        COMMAND if $<CONFIG:Debug>==1 (
            "${CMAKE_COMMAND}" -E copy
                "$<TARGET_FILE:${CMAKE_PROJECT_NAME}>"
                "${LIBOBS_INCLUDE_DIR}/../${OBS_BUILDDIR_ARCH}/rundir/$<CONFIG>/obs-plugins/${ARCH_NAME}"
        )

        COMMAND if $<CONFIG:Debug>==1 (
            "${CMAKE_COMMAND}" -E copy
                "$<TARGET_PDB_FILE:${CMAKE_PROJECT_NAME}>"
                "${LIBOBS_INCLUDE_DIR}/../${OBS_BUILDDIR_ARCH}/rundir/$<CONFIG>/obs-plugins/${ARCH_NAME}"
        )

        COMMAND if $<CONFIG:Debug>==1 (
            "${CMAKE_COMMAND}" -E make_directory
                "${LIBOBS_INCLUDE_DIR}/../${OBS_BUILDDIR_ARCH}/rundir/$<CONFIG>/data/obs-plugins/${CMAKE_PROJECT_NAME}"
        )

        COMMAND if $<CONFIG:Debug>==1 (
            "${CMAKE_COMMAND}" -E copy_directory
                "${PROJECT_SOURCE_DIR}/data"
                "${LIBOBS_INCLUDE_DIR}/../${OBS_BUILDDIR_ARCH}/rundir/$<CONFIG>/data/obs-plugins/${CMAKE_PROJECT_NAME}"
        )
    )
    # --- End of sub-section ---

endif()
]]
# --- End of section ---

# --- Linux-specific build settings and tasks ---
if(UNIX AND NOT APPLE)
    include(GNUInstallDirs)

    set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES PREFIX "")

    file(GLOB data_files data/*.*)
    file(GLOB locale_files data/locale/*.ini)

    install(TARGETS ${CMAKE_PROJECT_NAME}
        LIBRARY DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}/obs-plugins")

    install(FILES ${data_files}
        DESTINATION "${CMAKE_INSTALL_FULL_DATAROOTDIR}/obs/obs-plugins/${CMAKE_PROJECT_NAME}")

    install(FILES ${locale_files}
        DESTINATION "${CMAKE_INSTALL_FULL_DATAROOTDIR}/obs/obs-plugins/${CMAKE_PROJECT_NAME}/locale")
endif()
# --- End of section ---

# -- OS X specific build settings and tasks --
if(APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -fvisibility=default")

    set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES PREFIX "")
endif()
# -- End of section --
