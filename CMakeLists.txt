cmake_minimum_required(VERSION 3.31)
project(nxTimer)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_AUTOMOC ON)
set(CAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# allow explicit overrides from the command line / CLion
set(Qt6_DIR "" CACHE PATH "Path to directory containing Qt6Config.cmake (explicit override)")
set(QT_ROOT "" CACHE PATH "Qt installation root (e.g. C:/msys64/mingw64) - optional helper for detection")

# Auto-detect Qt6Config.cmake from a short list of common locations (MSYS2/Windows)
if (NOT Qt6_DIR)
    set(_candidates
            "${QT_ROOT}"
            "C:/msys64/mingw64"
            "/mingw64"
            "/c/msys64/mingw64"
            "C:/Qt/6"
            "C:/Program Files/Qt"
            "C:/Program Files (x86)/Qt"
            "/mingw64/lib/cmake/Qt6"
            "/mingw64/lib/qt6/lib/cmake/Qt6"
    )
    foreach(_c IN LISTS _candidates)
        if (NOT _c OR _c STREQUAL "")
            continue()
        endif()
        # two likely config locations under a prefix and absolute candidate paths
        set(_cfg1 "${_c}/lib/cmake/Qt6/Qt6Config.cmake")
        set(_cfg2 "${_c}/share/qt6/cmake/Qt6Config.cmake")
        set(_cfg3 "${_c}") # if user passed full path already
        if (EXISTS "${_cfg1}")
            get_filename_component(_cfg_dir "${_cfg1}" DIRECTORY)
            set(Qt6_DIR "${_cfg_dir}" CACHE PATH "Detected Qt6Config.cmake" FORCE)
            message(STATUS "Auto-detected Qt6Config.cmake at: ${_cfg1}")
            break()
        elseif (EXISTS "${_cfg2}")
            get_filename_component(_cfg_dir "${_cfg2}" DIRECTORY)
            set(Qt6_DIR "${_cfg_dir}" CACHE PATH "Detected Qt6Config.cmake" FORCE)
            message(STATUS "Auto-detected Qt6Config.cmake at: ${_cfg2}")
            break()
        elseif (EXISTS "${_cfg3}/Qt6Config.cmake")
            get_filename_component(_cfg_dir "${_cfg3}" DIRECTORY)
            set(Qt6_DIR "${_cfg_dir}" CACHE PATH "Detected Qt6Config.cmake (direct)" FORCE)
            message(STATUS "Auto-detected Qt6Config.cmake at: ${_cfg3}/Qt6Config.cmake")
            break()
        endif()
    endforeach()
endif()

# If still not found, append candidate roots to CMAKE_PREFIX_PATH so find_package can search them
if (NOT Qt6_DIR)
    foreach(_r IN LISTS _candidates)
        if (_r AND EXISTS "${_r}")
            list(FIND CMAKE_PREFIX_PATH "${_r}" _found)
            if (_found EQUAL -1)
                list(APPEND CMAKE_PREFIX_PATH "${_r}")
            endif()
        endif()
    endforeach()
    set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH}" CACHE PATH "Appended candidate prefixes for Qt detection" FORCE)
    message(STATUS "CMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}")
endif()

# Final guidance if detection failed
if (NOT Qt6_DIR)
    message(STATUS "Qt6Config.cmake not auto-detected. You can pass one of these to CMake/CLion:")
    message(STATUS "  -DQt6_DIR=\"C:/msys64/mingw64/lib/cmake/Qt6\"")
    message(STATUS "  -DQT_ROOT=\"C:/msys64/mingw64\" (then rerun CMake to pick it up)")
    message(STATUS "Or run CMake from the MSYS2 MinGW64 shell where mingw-w64-x86_64-qt6 is installed (pacman -S mingw-w64-x86_64-qt6).")
endif()

# Ensure Qt / mingw bin directories are on PATH so moc.exe (and other Qt tools) can load their DLLs.
# This runs before find_package / AUTOMOC tests and avoids skipping on CMake re-runs.
# It will prepend candidates only if they exist and are not already on PATH.
set(_runtime_bin_candidates)

# derive candidates from Qt6_DIR if available
if (Qt6_DIR)
    get_filename_component(_p1 "${Qt6_DIR}" DIRECTORY)    # .../lib/cmake
    get_filename_component(_p2 "${_p1}" DIRECTORY)        # .../lib
    get_filename_component(_qt_root "${_p2}" DIRECTORY)   # .../<root>
    list(APPEND _runtime_bin_candidates
            "${_qt_root}/bin"
            "${_p2}/bin"
            "${_p1}/bin"
    )
endif()

# QT_ROOT and first CMAKE_PREFIX_PATH entry
if (QT_ROOT)
    list(APPEND _runtime_bin_candidates "${QT_ROOT}/bin" "${QT_ROOT}")
endif()
if (CMAKE_PREFIX_PATH)
    list(GET CMAKE_PREFIX_PATH 0 _first_prefix)
    if (_first_prefix)
        list(APPEND _runtime_bin_candidates "${_first_prefix}/bin" "${_first_prefix}")
    endif()
endif()

# common MSYS2 / mingw64 locations
list(APPEND _runtime_bin_candidates
        "C:/msys64/mingw64/bin"
        "/mingw64/bin"
        "/c/msys64/mingw64/bin"
)

# Prepend first existing candidate bins to PATH (if not already present) and try to find moc
set(_moc_found "")

foreach(_b IN LISTS _runtime_bin_candidates)
    if (_b AND EXISTS "${_b}")
        # Only prepend if not already present
        string(FIND "$ENV{PATH}" "${_b}" _pos)
        if (_pos EQUAL -1)
            # Prepend using ';' as separator on Windows; this still works on MSYS/CMake on Windows
            set(ENV{PATH} "${_b};$ENV{PATH}")
            message(STATUS "Prepended to PATH for Qt tools: ${_b}")
        else()
            message(STATUS "Qt bin already in PATH: ${_b}")
        endif()

        # After updating PATH, try to locate moc (moc.exe or moc)
        find_program(_MOC_EXECUTABLE NAMES moc.exe moc HINTS "${_b}" PATHS "${_b}" NO_DEFAULT_PATH)
        if (NOT _MOC_EXECUTABLE)
            # allow default PATH search (in case we prepended it)
            find_program(_MOC_EXECUTABLE NAMES moc.exe moc)
        endif()
        if (_MOC_EXECUTABLE)
            message(STATUS "Found moc executable: ${_MOC_EXECUTABLE}")
            set(CMAKE_MOC_EXECUTABLE "${_MOC_EXECUTABLE}" CACHE FILEPATH "Location of moc for AUTOMOC" FORCE)
            set(_moc_found TRUE)
            break()
        endif()
    endif()
endforeach()

if (NOT _moc_found)
    # as a last resort, try searching the system PATH for moc
    find_program(_MOC_EXECUTABLE NAMES moc.exe moc)
    if (_MOC_EXECUTABLE)
        message(STATUS "Found moc on PATH: ${_MOC_EXECUTABLE}")
        set(CMAKE_MOC_EXECUTABLE "${_MOC_EXECUTABLE}" CACHE FILEPATH "Location of moc for AUTOMOC" FORCE)
    else()
        message(WARNING "Could not find moc executable or ensure Qt bin is on PATH. AUTOMOC tests may fail. Consider passing -DQt6_DIR or -DQT_ROOT or run CMake from MSYS2 MinGW64 shell.")
    endif()
endif()

# Use CONFIG mode and provide HINTS / PATHS to help find_package locate the Qt config files
find_package(Qt6 CONFIG
        COMPONENTS Core Gui Widgets
        REQUIRED
        HINTS ${Qt6_DIR}
        PATHS ${CMAKE_PREFIX_PATH}
 )

# Build as a Windows GUI app (no console window). 'WIN32' sets the subsystem, and
# WIN32_EXECUTABLE property is set below for extra safety on some toolchains.
add_executable(${PROJECT_NAME} WIN32 main.cpp)


target_sources(${PROJECT_NAME}


        PRIVATE
        FILE_SET CXX_MODULES
        FILES
        GameAddresses.cpp
        GameMemory.cpp
        TimerWorker.cpp
        Settings.cpp
        GUIFrame.cpp
)
target_link_libraries(${PROJECT_NAME}
        Qt::Core
        Qt::Gui
        Qt::Widgets
)

# Final: copy MinGW runtime DLLs from the *exact compiler bin* (must be last)
if (WIN32 AND MINGW)
    get_filename_component(_nx_cxx_bin "${CMAKE_CXX_COMPILER}" DIRECTORY)
    if (EXISTS "${_nx_cxx_bin}")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E remove -f
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/libwinpthread-1.dll"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/libgcc_s_seh-1.dll"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/libstdc++-6.dll"
            COMMENT "Removing stale MinGW runtime DLLs"
        )
        if (EXISTS "${_nx_cxx_bin}/libwinpthread-1.dll")
            add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_nx_cxx_bin}/libwinpthread-1.dll"
                    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/libwinpthread-1.dll"
                COMMENT "Copying libwinpthread-1.dll from compiler bin"
            )
        endif()
        if (EXISTS "${_nx_cxx_bin}/libgcc_s_seh-1.dll")
            add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_nx_cxx_bin}/libgcc_s_seh-1.dll"
                    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/libgcc_s_seh-1.dll"
                COMMENT "Copying libgcc_s_seh-1.dll from compiler bin"
            )
        endif()
        if (EXISTS "${_nx_cxx_bin}/libstdc++-6.dll")
            add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_nx_cxx_bin}/libstdc++-6.dll"
                    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/libstdc++-6.dll"
                COMMENT "Copying libstdc++-6.dll from compiler bin"
            )
        endif()
    else()
        message(WARNING "Compiler bin not found; cannot copy MinGW runtime DLLs.")
    endif()
endif()

# --- Auto-deploy Qt and MinGW runtime (optional, best-effort) ---
# Try to locate windeployqt and run it to collect Qt DLLs/plugins next to the exe.
# Also try to find the mingw64/bin used to build so we can copy libwinpthread/libgcc/libstdc++.
if (WIN32)
    option(NXTIMER_RUN_WINDEPLOYQT "Run windeployqt post-build" OFF)

    # candidate windeployqt locations (from detected Qt install)
    find_program(WINDEPLOYQT_EXECUTABLE NAMES windeployqt.exe HINTS
        "${QT_INSTALL_PATH}/bin"
        "${Qt6_DIR}/../bin"
        "${Qt6_DIR}/../../bin"
        "$ENV{PATH}"
    )

    if (WINDEPLOYQT_EXECUTABLE AND NXTIMER_RUN_WINDEPLOYQT)
        message(STATUS "Found windeployqt: ${WINDEPLOYQT_EXECUTABLE}")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}" "$<TARGET_FILE:${PROJECT_NAME}>"
            COMMENT "Running windeployqt to collect Qt DLLs/plugins"
            VERBATIM
        )
    else()
        message(WARNING "windeployqt not found. Run windeployqt manually from your Qt installation to bundle Qt DLLs/plugins.")
    endif()

    # Optional: extra runtime copies (OFF to avoid mismatched DLLs)
    option(NXTIMER_EXTRA_RUNTIME_COPY "Also copy MinGW runtime from additional fallback locations" OFF)

    # First: try to derive MinGW runtime location from the actual compiler used to build.
    set(_compiler_bin "")
    if (CMAKE_CXX_COMPILER)
        get_filename_component(_compiler_path "${CMAKE_CXX_COMPILER}" ABSOLUTE)
        get_filename_component(_compiler_dir "${_compiler_path}" DIRECTORY)
        if (_compiler_dir)
            # Usually compiler lives in .../bin (e.g. C:/msys64/mingw64/bin)
            set(_compiler_bin "${_compiler_dir}")
            message(STATUS "Detected C++ compiler directory: ${_compiler_bin}")
        endif()
    endif()

    # If compiler dir not useful, fall back to previous candidate list
    if (NOT _compiler_bin OR NOT EXISTS "${_compiler_bin}")
        set(_mingw_candidates
            "C:/msys64/mingw64/bin"
            "/mingw64/bin"
            "/c/msys64/mingw64/bin"
            "$ENV{MSYS2_PATH}/mingw64/bin"
            "$ENV{MINGW64_HOME}/bin"
            "$ENV{MINGW_BIN}"
        )

        foreach(_m IN LISTS _mingw_candidates)
            if (_m AND EXISTS "${_m}")
                set(_compiler_bin "${_m}")
                message(STATUS "Falling back to candidate MinGW bin: ${_compiler_bin}")
                break()
            endif()
        endforeach()
    endif()

    if (NXTIMER_EXTRA_RUNTIME_COPY AND _compiler_bin AND EXISTS "${_compiler_bin}")
        # List of runtime DLLs to copy from the exact compiler location used to build
        set(_compiler_runtime_dlls
            libstdc++-6.dll
            libgcc_s_seh-1.dll
            libwinpthread-1.dll
            libiconv-2.dll
            libintl-8.dll
        )

        foreach(_dll IN LISTS _compiler_runtime_dlls)
            set(_src "${_compiler_bin}/${_dll}")
            if (EXISTS "${_src}")
                get_filename_component(_dll_name "${_src}" NAME)
                add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${_src}"
                        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/${_dll_name}"
                    COMMENT "Copy runtime ${_dll_name} from ${_compiler_bin}"
                )
                message(STATUS "Will copy runtime DLL from compiler dir: ${_src}")
            else()
                message(STATUS "Compiler-dir runtime DLL not found (ok if not present): ${_src}")
            endif()
        endforeach()
    else()
        message(STATUS "Skipping extra runtime copy from compiler bin (NXTIMER_EXTRA_RUNTIME_COPY=OFF).")
    endif()

    # Existing fallback: copy from detected Qt bin as additional helpers (existing code continues)
    if (NXTIMER_EXTRA_RUNTIME_COPY AND _compiler_bin)
        set(_mingw_runtime_dlls
            libwinpthread-1.dll
            libgcc_s_seh-1.dll
            libstdc++-6.dll
            libwinpthread-1.dll
        )

        foreach(_dll IN LISTS _mingw_runtime_dlls)
            set(_src "${_mingw_bin}/${_dll}")
            if (EXISTS "${_src}")
                get_filename_component(_dll_name "${_src}" NAME)
                add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${_src}"
                        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/${_dll_name}"
                    COMMENT "Copy runtime ${_dll_name} from ${_mingw_bin}"
                )
                message(STATUS "Will copy runtime DLL: ${_src}")
            else()
                message(STATUS "Runtime DLL not found at candidate location: ${_src}")
            endif()
        endforeach()
    else()
        message(STATUS "Skipping extra runtime copy from fallback locations (NXTIMER_EXTRA_RUNTIME_COPY=OFF).")
    endif()
endif()

# compute a usable QT_INSTALL_PATH (root containing bin/plugins) for post-build copy
if (WIN32 AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    # try derive from Qt6_DIR first (common layout: <root>/lib/cmake/Qt6)
    set(QT_INSTALL_PATH "")
    if (Qt6_DIR)
        # try a few parent levels to reach the installation root
        get_filename_component(_p1 "${Qt6_DIR}" DIRECTORY)    # .../lib/cmake
        get_filename_component(_p2 "${_p1}" DIRECTORY)       # .../lib
        get_filename_component(_p3 "${_p2}" DIRECTORY)       # .../<root>
        if (EXISTS "${_p3}/bin")
            set(QT_INSTALL_PATH "${_p3}")
        elseif (EXISTS "${_p2}/bin")
            set(QT_INSTALL_PATH "${_p2}")
        elseif (EXISTS "${_p1}/bin")
            set(QT_INSTALL_PATH "${_p1}")
        endif()
    endif()

    # fallback to first entry of CMAKE_PREFIX_PATH if available
    if (NOT QT_INSTALL_PATH AND CMAKE_PREFIX_PATH)
        list(GET CMAKE_PREFIX_PATH 0 _first_prefix)
        if (EXISTS "${_first_prefix}/bin")
            set(QT_INSTALL_PATH "${_first_prefix}")
        endif()
    endif()

    # if user provided QT_ROOT, prefer that
    if (NOT QT_INSTALL_PATH AND QT_ROOT AND EXISTS "${QT_ROOT}/bin")
        set(QT_INSTALL_PATH "${QT_ROOT}")
    endif()

    # --- NEW: ensure QT_INSTALL_PATH/bin is prepended to PATH and find moc there ---
    if (QT_INSTALL_PATH AND EXISTS "${QT_INSTALL_PATH}/bin")
        # Prepend Qt bin to PATH for the configure/run of moc/AUTOMOC
        string(FIND "$ENV{PATH}" "${QT_INSTALL_PATH}/bin" _pos_bin)
        if (_pos_bin EQUAL -1)
            set(ENV{PATH} "${QT_INSTALL_PATH}/bin;$ENV{PATH}")
            message(STATUS "Prepended QT_INSTALL_PATH/bin to PATH for config: ${QT_INSTALL_PATH}/bin")
        else()
            message(STATUS "QT_INSTALL_PATH/bin already on PATH: ${QT_INSTALL_PATH}/bin")
        endif()

        # Try to locate moc in that bin and set CMAKE_MOC_EXECUTABLE so AUTOMOC can run
        if (NOT CMAKE_MOC_EXECUTABLE)
            find_program(_MOC_EXECUTABLE NAMES moc.exe moc HINTS "${QT_INSTALL_PATH}/bin" PATHS "${QT_INSTALL_PATH}/bin")
            if (_MOC_EXECUTABLE)
                message(STATUS "Setting CMAKE_MOC_EXECUTABLE to: ${_MOC_EXECUTABLE}")
                set(CMAKE_MOC_EXECUTABLE "${_MOC_EXECUTABLE}" CACHE FILEPATH "Location of moc for AUTOMOC" FORCE)
            else()
                message(WARNING "Could not find moc under ${QT_INSTALL_PATH}/bin. AUTOMOC may fail (set -DQt6_DIR or run from MSYS2 MinGW64 shell).")
            endif()
        endif()
    endif()
    # --- end NEW ---

    if (NOT QT_INSTALL_PATH)
        message(WARNING "Could not determine QT_INSTALL_PATH for post-build DLL copy. Skipping DLL copy steps. Set -DQt6_DIR or -DQT_ROOT to help detection.")
    else()
        message(STATUS "Using QT_INSTALL_PATH=${QT_INSTALL_PATH} for post-build copy")
        set(DEBUG_SUFFIX "")
        if (MSVC AND CMAKE_BUILD_TYPE MATCHES "Debug")
            set(DEBUG_SUFFIX "d")
        endif ()

        # create a minimal qt.conf in the build dir so we can copy it next to the exe
        set(_qtconf_in "${CMAKE_BINARY_DIR}/qt.conf")
        file(WRITE "${_qtconf_in}" "[Paths]\nPlugins = plugins\n")
        message(STATUS "Created qt.conf at ${_qtconf_in}")

        # --- NEW: detect actual plugins source directory from common locations and warn if missing ---
        set(_plugins_candidates
                "${QT_INSTALL_PATH}/plugins"
                "${QT_INSTALL_PATH}/lib/qt6/plugins"
                "${QT_INSTALL_PATH}/lib/plugins"
                "${QT_INSTALL_PATH}/share/qt6/plugins"
                "${QT_INSTALL_PATH}/qt/plugins"
        )
        set(_plugins_src "")
        foreach(_pc IN LISTS _plugins_candidates)
            if (EXISTS "${_pc}")
                set(_plugins_src "${_pc}")
                message(STATUS "Detected Qt plugins folder: ${_plugins_src}")
                break()
            endif()
        endforeach()
        if (NOT _plugins_src)
            message(WARNING "Could not find Qt plugins directory under QT_INSTALL_PATH. Plugins (platforms) may not be copied.")
        endif()
        # --- end NEW ---

        # 1) copy entire plugins tree (preferred) so Qt can find platform and other plugins
        if (_plugins_src)
            add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins"
            )
            add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${_plugins_src}"
                    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins"
            )
        else()
            # fallback: copy only platforms if available in the generic location
            if (EXISTS "${QT_INSTALL_PATH}/plugins/platforms/qwindows${DEBUG_SUFFIX}.dll")
                add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                        COMMAND ${CMAKE_COMMAND} -E make_directory
                        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins/platforms/")
                add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                        COMMAND ${CMAKE_COMMAND} -E copy
                        "${QT_INSTALL_PATH}/plugins/platforms/qwindows${DEBUG_SUFFIX}.dll"
                        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins/platforms/")
            endif()
        endif()

        # 2) copy qt.conf next to the exe so Qt uses the local plugins folder
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_qtconf_in}"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/qt.conf"
        )

        # 3) copy only the exact Qt DLLs the target links against (avoid overwriting MinGW runtime)
        set(_qt_bin "${QT_INSTALL_PATH}/bin")
        if (EXISTS "${_qt_bin}")
            configure_file(
                "${CMAKE_SOURCE_DIR}/clean_qt_dlls.cmake"
                "${CMAKE_BINARY_DIR}/clean_qt_dlls.cmake"
                COPYONLY
            )
            add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND}
                    "-DDIR=$<TARGET_FILE_DIR:${PROJECT_NAME}>"
                    -P "${CMAKE_BINARY_DIR}/clean_qt_dlls.cmake"
                COMMENT "Removing stale Qt DLLs from output directory"
                VERBATIM
            )
            foreach(_qt_dll Qt6Core.dll Qt6Gui.dll Qt6Widgets.dll)
                if (EXISTS "${_qt_bin}/${_qt_dll}")
                    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                            "${_qt_bin}/${_qt_dll}"
                            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/${_qt_dll}"
                        COMMENT "Copying ${_qt_dll} from ${_qt_bin}"
                    )
                endif()
            endforeach()
        endif()

        # 4) copy additional helper DLL patterns (icu, zlib, image codecs, etc.)
        if (EXISTS "${_qt_bin}")
            file(GLOB ADD_DLLS
                    "${_qt_bin}/icu*.dll"
                    "${_qt_bin}/libz*.dll"
                    "${_qt_bin}/libb2-1.dll"
                    "${_qt_bin}/libdouble-conversion*.dll"
                    "${_qt_bin}/libicuin*.dll"
                    "${_qt_bin}/libicuuc*.dll"
                    "${_qt_bin}/libpcre2-16-0.dll"
                    "${_qt_bin}/libpcre2-8-0.dll"
                    "${_qt_bin}/libfreetype-6.dll"
                    "${_qt_bin}/libharfbuzz-0.dll"
                    "${_qt_bin}/libmd4c.dll"
                    "${_qt_bin}/libicudt78.dll"
                    "${_qt_bin}/libpng16-16.dll"
                    "${_qt_bin}/libbrotlidec.dll"
                    "${_qt_bin}/libbz2-1.dll"
                    "${_qt_bin}/libglib-2.0-0.dll"
                    "${_qt_bin}/libgraphite2.dll"
                    "${_qt_bin}/libbrotlicommon.dll"
                    "${_qt_bin}/libiconv-2.dll"
                    "${_qt_bin}/libintl-*.dll"
                    "${_qt_bin}/zlib1.dll"
            )
            foreach(_ad IN LISTS ADD_DLLS)
                get_filename_component(_ad_name "${_ad}" NAME)
                add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${_ad}"
                        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/${_ad_name}")
            endforeach()
        endif()

        # 5) write a small launcher that sets PATH so you can run the exe in CLion/Explorer reliably
        set(_launcher_in "${CMAKE_BINARY_DIR}/run_${PROJECT_NAME}.bat")
        # Use forward slashes for injected path; Windows accepts them in PATH
        file(WRITE "${_launcher_in}"
                "@echo off\r\n"
                "rem Auto-generated launcher: prepends Qt bin and launches the exe\r\n"
                "setlocal\r\n"
                "set \"THIS_DIR=%~dp0\"\r\n"
                "set \"QT_BIN=${QT_INSTALL_PATH}/bin\"\r\n"
                "set \"PATH=%THIS_DIR%;%QT_BIN%;%PATH%\"\r\n"
                "\"%THIS_DIR%\\${PROJECT_NAME}.exe\" %*\r\n"
                "endlocal\r\n"
        )
        message(STATUS "Wrote launcher: ${_launcher_in}")

        # copy launcher next to the exe
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_launcher_in}"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/run_${PROJECT_NAME}.bat"
        )

        # add: configure-time checks and a debug launcher that sets QT_DEBUG_PLUGINS=1
        # warn if the platform plugin is missing at the detected Qt location
        if (EXISTS "${QT_INSTALL_PATH}/plugins/platforms/qwindows${DEBUG_SUFFIX}.dll")
            message(STATUS "Platform plugin found: ${QT_INSTALL_PATH}/plugins/platforms/qwindows${DEBUG_SUFFIX}.dll")
        else()
            message(WARNING "Platform plugin qwindows${DEBUG_SUFFIX}.dll NOT found at ${QT_INSTALL_PATH}/plugins/platforms. The app may fail to initialize the Qt platform plugin.")
        endif()

        # warn about common ABI/toolchain mismatch: building with MSVC while Qt looks like MinGW/MSYS build
        if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
            if (Qt6_DIR MATCHES "mingw" OR Qt6_DIR MATCHES "msys" OR CMAKE_PREFIX_PATH MATCHES "mingw")
                message(WARNING "Compiler = MSVC but Qt installation appears to be MinGW/MSYS. Use a MinGW toolchain in CLion or install an MSVC-built Qt. ABI mismatch will prevent plugins/DLLs from loading.")
            endif()
        endif()

        # write a debug launcher that prints plugin folder contents and enables QT_DEBUG_PLUGINS
        set(_launcher_dbg "${CMAKE_BINARY_DIR}/run_${PROJECT_NAME}_debug.bat")
        file(WRITE "${_launcher_dbg}"
                "@echo off\r\n"
                "rem Auto-generated debug launcher: prepends Qt bin, enables QT_DEBUG_PLUGINS and shows diagnostic info\r\n"
                "setlocal\r\n"
                "set \"THIS_DIR=%~dp0\"\r\n"
                "set \"QT_BIN=${QT_INSTALL_PATH}/bin\"\r\n"
                "set \"PATH=%THIS_DIR%;%QT_BIN%;%PATH%\"\r\n"
                "set QT_DEBUG_PLUGINS=1\r\n"
                "echo ---- QT_INSTALL_PATH=${QT_INSTALL_PATH} ----\r\n"
                "echo ---- Checking plugins/platforms ----\r\n"
                "if exist \"%THIS_DIR%plugins\\platforms\\qwindows.dll\" (echo Found qwindows.dll) else (echo qwindows.dll NOT found)\r\n"
                "echo ---- Listing plugins/platforms folder ----\r\n"
                "dir \"%THIS_DIR%plugins\\platforms\" || echo (no platforms folder)\r\n"
                "echo ---- PATH ----\r\n"
                "echo %PATH%\r\n"
                "\"%THIS_DIR%\\${PROJECT_NAME}.exe\" %*\r\n"
                "echo ---- EXIT CODE: %ERRORLEVEL% ----\r\n"
                "pause\r\n"
                "endlocal\r\n"
        )
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_launcher_dbg}"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/run_${PROJECT_NAME}_debug.bat"
        )


    endif()
endif()

