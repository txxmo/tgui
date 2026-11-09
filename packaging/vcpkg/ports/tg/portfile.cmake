# vcpkg port for tg. Use it as an overlay:
#   vcpkg install tg --overlay-ports=<tg>/packaging/vcpkg/ports
# Local checkout instead of a download (handy while developing tg):
#   set TG_SOURCE_DIR=<path to tg>   (also add TG_SOURCE_DIR to VCPKG_KEEP_ENV_VARS)
if( DEFINED ENV{TG_SOURCE_DIR} )
    set( SOURCE_PATH "$ENV{TG_SOURCE_DIR}" )
else()
    vcpkg_from_github(
        OUT_SOURCE_PATH SOURCE_PATH
        REPO txxmo/tgui
        REF "v${VERSION}"
        SHA512 0 # fill in for a release: vcpkg prints the real hash on the first install
        HEAD_REF main
    )
endif()

vcpkg_check_features( OUT_FEATURE_OPTIONS FEATURE_OPTIONS
    FEATURES
        app       TG_BUILD_APP
        freetype  TG_FREETYPE
        harfbuzz  TG_HARFBUZZ
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        ${FEATURE_OPTIONS}
        -DTG_INSTALL=ON
        -DTG_BUILD_EXAMPLES=OFF
        -DTG_BUILD_HEADLESS=OFF
        -DTG_WEBVIEW=OFF
)
vcpkg_cmake_install()
vcpkg_cmake_config_fixup( PACKAGE_NAME tg CONFIG_PATH lib/cmake/tg )

file( REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include" "${CURRENT_PACKAGES_DIR}/debug/share" )
set( VCPKG_POLICY_SKIP_COPYRIGHT_CHECK enabled ) # see below: tg has no license file yet
if( EXISTS "${SOURCE_PATH}/LICENSE" )
    vcpkg_install_copyright( FILE_LIST "${SOURCE_PATH}/LICENSE" )
else()
    file( WRITE "${CURRENT_PACKAGES_DIR}/share/${PORT}/copyright" "No license file in this tg checkout. Third-party notices: share/tg/third-party/\n" )
endif()
file( INSTALL "${CMAKE_CURRENT_LIST_DIR}/usage" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" )
