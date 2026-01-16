# SPDX-FileCopyrightText: 2024 Chris Rizzitello <sithlord48@gmail.com>
# SPDX-License-Identifier: MIT

# HACK This is set when the files is included so its the real path
# calling CMAKE_CURRENT_LIST_DIR after include would return the wrong scope var
set(MY_DIR ${CMAKE_CURRENT_LIST_DIR})
set(OSX_BUNDLE ${BUILD_OSX_BUNDLE})

if(CMAKE_OSX_ARCHITECTURES MATCHES ";")
  set(OS_STRING "macos-universal")
else()
  set(OS_STRING "macos-${BUILD_ARCHITECTURE}")
endif()

if(NOT DEFINED OSX_CODESIGN_IDENTITY)
  set(OSX_CODESIGN_IDENTITY "-")
endif()

if (OSX_BUNDLE)
  # Consolidated Deployment & Signing Block
  # This ensures macdeployqt runs first, then we bundle our extras, then clean attributes, then sign.
  find_package(OpenSSL QUIET)
  set(OSSL_MOD_PATH "")
  if (OPENSSL_FOUND)
      get_filename_component(OPENSSL_LIB_DIR "${OPENSSL_CRYPTO_LIBRARY}" DIRECTORY)
      if (EXISTS "${OPENSSL_LIB_DIR}/ossl-modules")
          set(OSSL_MOD_PATH "${OPENSSL_LIB_DIR}/ossl-modules")
          message(STATUS "Deployment will bundle OpenSSL modules from: ${OSSL_MOD_PATH}")
      endif()
  endif()

  install(CODE "
    set(APP_PATH \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_PROJECT_PROPER_NAME}.app\")

    # Sanity Check: Ensure the bundle actually exists and has the main binary
    if (NOT EXISTS \"\${APP_PATH}/Contents/MacOS/${CMAKE_PROJECT_PROPER_NAME}\")
        message(FATAL_ERROR \"Critical error: Main executable missing from bundle before signing at: \${APP_PATH}/Contents/MacOS/${CMAKE_PROJECT_PROPER_NAME}\")
    endif()

    message(STATUS \"Running macdeployqt...\")
    execute_process(COMMAND \"${DEPLOYQT}\" \"\${APP_PATH}\" COMMAND_ECHO STDOUT)

    if (NOT \"${OSSL_MOD_PATH}\" STREQUAL \"\")
        message(STATUS \"Bundling OpenSSL modules...\")
        execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory \"${OSSL_MOD_PATH}\" \"\${APP_PATH}/Contents/Frameworks/ossl-modules\" COMMAND_ECHO STDOUT)
    endif()

    message(STATUS \"Cleaning extended attributes...\")
    execute_process(COMMAND xattr -cr \"\${APP_PATH}\" COMMAND_ECHO STDOUT)

    if (NOT \"${OSX_CODESIGN_IDENTITY}\" STREQUAL \"-\")
        message(STATUS \"Deep signing the bundle with identity: ${OSX_CODESIGN_IDENTITY}\")
        execute_process(COMMAND codesign --force --deep --options=runtime --entitlements \"${MY_DIR}/Deskflow-HID.entitlements\" -v --sign \"${OSX_CODESIGN_IDENTITY}\" \"\${APP_PATH}\" COMMAND_ECHO STDOUT)

        message(STATUS \"Verifying signature...\")
        execute_process(COMMAND codesign --verify --deep --verbose=4 \"\${APP_PATH}\" COMMAND_ECHO STDOUT)
    endif()
  ")

  set(CPACK_PACKAGE_ICON "${MY_DIR}/dmg-volume.icns")
  set(CPACK_DMG_BACKGROUND_IMAGE "${MY_DIR}/dmg-background.tiff")
  set(CPACK_DMG_DS_STORE_SETUP_SCRIPT "${MY_DIR}/generate_ds_store.applescript")
  set(CPACK_DMG_VOLUME_NAME "${CMAKE_PROJECT_PROPER_NAME}")
  set(CPACK_DMG_SLA_USE_RESOURCE_FILE_LICENSE ON)
  set(CPACK_GENERATOR "DragNDrop")
endif()
