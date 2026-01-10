#!/bin/bash

# Deskflow Task Runner

# Ensure required environment variables are set
if [ -z "$CMAKE_PREFIX_PATH" ] && [ "$(uname)" = "Darwin" ]; then
    echo "Warning: CMAKE_PREFIX_PATH is not set. Build may fail."
fi

process_input() {
    local input="$1"
    local input_lower=$(echo "$input" | tr '[:upper:]' '[:lower:]')
    local os_name=$(uname -s)

    case "$input_lower" in
        1|"build")
            if [ ! -d "build" ]; then
                echo "--- CONFIGURING (NEW BUILD) ---"
                if [ "$os_name" = "Darwin" ]; then
                    cmake -B build -G "Unix Makefiles" \
                      -DCMAKE_PREFIX_PATH="$CMAKE_PREFIX_PATH" \
                      -DCMAKE_OSX_SYSROOT="$CMAKE_OSX_SYSROOT" \
                      -DCMAKE_BUILD_TYPE=Release \
                      -DSKIP_BUILD_TESTS=ON \
                      -DBUILD_TESTS=OFF \
                      -DDESKFLOW_PAYPAL_ACCOUNT="$PAYPAL_ACCOUNT" \
                      -DDESKFLOW_PAYPAL_URL="$PAYPAL_URL"
                else
                     # Linux Configuration
                     cmake -B build -G "Unix Makefiles" \
                      -DCMAKE_BUILD_TYPE=Release \
                      -DSKIP_BUILD_TESTS=ON \
                      -DBUILD_TESTS=OFF \
                      -DDESKFLOW_PAYPAL_ACCOUNT="$PAYPAL_ACCOUNT" \
                      -DDESKFLOW_PAYPAL_URL="$PAYPAL_URL"
                fi
            fi
            echo "--- BUILDING ---"
            cmake --build build -j$(sysctl -n hw.ncpu 2>/dev/null || nproc)
            if [ $? -eq 0 ]; then
                if [ "$os_name" = "Darwin" ]; then
                    echo "--- DEPLOYING QT ---"
                    $CMAKE_PREFIX_PATH/bin/macdeployqt build/bin/Deskflow-HID.app
                    echo "--- SIGNING ---"
                    codesign --force --deep --sign "$APPLE_CODESIGN_DEV" build/bin/Deskflow-HID.app
                fi
                echo "Build complete."
            else
                echo "Build failed."
            fi
            ;;
        2|"build pristine")
            echo "--- CLEANING AND RECONFIGURING (PRISTINE) ---"
            rm -rf build
            if [ "$os_name" = "Darwin" ]; then
                cmake -B build -G "Unix Makefiles" \
                  -DCMAKE_PREFIX_PATH="$CMAKE_PREFIX_PATH" \
                  -DCMAKE_OSX_SYSROOT="$CMAKE_OSX_SYSROOT" \
                  -DCMAKE_BUILD_TYPE=Release \
                  -DSKIP_BUILD_TESTS=ON \
                  -DBUILD_TESTS=OFF \
                  -DDESKFLOW_PAYPAL_ACCOUNT="$PAYPAL_ACCOUNT" \
                  -DDESKFLOW_PAYPAL_URL="$PAYPAL_URL"
            else
                 # Linux Configuration
                 cmake -B build -G "Unix Makefiles" \
                  -DCMAKE_BUILD_TYPE=Release \
                  -DSKIP_BUILD_TESTS=ON \
                  -DBUILD_TESTS=OFF \
                  -DDESKFLOW_PAYPAL_ACCOUNT="$PAYPAL_ACCOUNT" \
                  -DDESKFLOW_PAYPAL_URL="$PAYPAL_URL"
            fi
            echo "--- BUILDING ---"
            cmake --build build -j$(sysctl -n hw.ncpu 2>/dev/null || nproc)
            if [ $? -eq 0 ]; then
                if [ "$os_name" = "Darwin" ]; then
                    echo "--- DEPLOYING QT ---"
                    $CMAKE_PREFIX_PATH/bin/macdeployqt build/bin/Deskflow-HID.app
                    echo "--- SIGNING ---"
                    codesign --force --deep --sign "$APPLE_CODESIGN_DEV" build/bin/Deskflow-HID.app
                fi
                echo "Build complete."
            else
                echo "Build failed."
            fi
            ;;
        4|"release")
            echo "--- CLEANING AND RECONFIGURING (RELEASE) ---"
            rm -rf build
            if [ "$os_name" = "Darwin" ]; then
                cmake -B build -G "Unix Makefiles" \
                  -DCMAKE_PREFIX_PATH="$CMAKE_PREFIX_PATH" \
                  -DCMAKE_OSX_SYSROOT="$CMAKE_OSX_SYSROOT" \
                  -DCMAKE_BUILD_TYPE=Release \
                  -DSKIP_BUILD_TESTS=ON \
                  -DBUILD_TESTS=OFF \
                  -DDESKFLOW_PAYPAL_ACCOUNT="$PAYPAL_ACCOUNT" \
                  -DDESKFLOW_PAYPAL_URL="$PAYPAL_URL" \
                  -DDESKFLOW_CDC_PUBLIC_KEY="$DESKFLOW_CDC_PUBLIC_KEY" \
                  -DDESKFLOW_ESP32_ENCRYPTION_KEY="$DESKFLOW_ESP32_ENCRYPTION_KEY"
            else
                 # Linux Configuration
                 cmake -B build -G "Unix Makefiles" \
                  -DCMAKE_BUILD_TYPE=Release \
                  -DSKIP_BUILD_TESTS=ON \
                  -DBUILD_TESTS=OFF \
                  -DDESKFLOW_PAYPAL_ACCOUNT="$PAYPAL_ACCOUNT" \
                  -DDESKFLOW_PAYPAL_URL="$PAYPAL_URL" \
                  -DDESKFLOW_CDC_PUBLIC_KEY="$DESKFLOW_CDC_PUBLIC_KEY" \
                  -DDESKFLOW_ESP32_ENCRYPTION_KEY="$DESKFLOW_ESP32_ENCRYPTION_KEY"
            fi
            echo "--- BUILDING ---"
            cmake --build build -j$(sysctl -n hw.ncpu 2>/dev/null || nproc)
            if [ $? -eq 0 ]; then
                if [ "$os_name" = "Darwin" ]; then
                    echo "--- DEPLOYING QT ---"
                    $CMAKE_PREFIX_PATH/bin/macdeployqt build/bin/Deskflow-HID.app
                    echo "--- SIGNING ---"
                    codesign --force --deep --sign "$APPLE_CODESIGN_DEV" build/bin/Deskflow-HID.app
                fi
                echo "Build Release complete."
            else
                echo "Build Release failed."
            fi
            ;;
        3|"launch")
            if [ "$os_name" = "Darwin" ]; then
                if [ ! -d "build/bin/Deskflow-HID.app" ]; then
                    echo "Error: Application not built. Select '1' or '2' first."
                else
                    echo "--- LAUNCHING DESKFLOW-HID ---"
                    build/bin/Deskflow-HID.app/Contents/MacOS/Deskflow-HID
                fi
            else
                # Linux Launch
                 if [ ! -f "build/bin/deskflow-hid" ]; then
                    echo "Error: Application not built. Select '1' or '2' first."
                else
                     echo "--- LAUNCHING DESKFLOW-HID ---"
                     ./build/bin/deskflow-hid
                fi
            fi
            ;;
        q|"quit"|"exit")
            echo "Exiting..."
            return 1
            ;;
        *)
            echo "Invalid selection: $input"
            ;;
    esac
    return 0
}

# If argument supplied, run it and exit
if [ "$#" -gt 0 ]; then
    process_input "$@"
    exit $?
fi

# Interactive mode
while true; do
    echo ""
    echo "=================================================="
    echo "           Deskflow Interactive Build             "
    echo "=================================================="
    echo "  1) Build"
    echo "  2) Build Pristine (Clean & Reconfigure)"
    echo "  3) Launch"
    echo "  4) Build Release (with Production Keys)"
    echo "  q) Quit"
    echo ""
    read -p "Select a task (1-4 or q): " input

    process_input "$input"
    if [ $? -ne 0 ]; then
        break
    fi
done
