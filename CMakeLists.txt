cmake_minimum_required(VERSION 3.16)
project(ro-assist VERSION 0.2.5 LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

enable_testing()

find_package(Qt6 6.6 REQUIRED COMPONENTS Widgets Network Test Concurrent)
find_package(Qt6 6.6 QUIET COMPONENTS LinguistTools)

set(RO_ASSIST_SOURCES
    include/roassist/mainwindow.h
    include/roassist/system_services.h
    include/roassist/ui_texts.h
    include/roassist/update_helpers.h
    src/main.cpp
    src/mainwindow.cpp
    src/system_services.cpp
    src/ui_texts.cpp
    src/update_helpers.cpp
    resources/resources.qrc
)

add_executable(ro-assist ${RO_ASSIST_SOURCES})
target_include_directories(ro-assist PRIVATE include)
target_compile_definitions(ro-assist PRIVATE APP_VERSION="${PROJECT_VERSION}")
target_link_libraries(ro-assist PRIVATE Qt6::Widgets Qt6::Network Qt6::Concurrent)

# Qt Linguist is only needed while generating .qm files. The application also
# ships its runtime text catalogue, so local development remains buildable on
# minimal Qt installations where LinguistTools is not installed.
if(TARGET Qt6::lrelease)
    qt_add_translations(ro-assist
        TS_FILES
            translations/ro-assist_en.ts
            translations/ro-assist_tr.ts
            translations/ro-assist_es.ts
    )
endif()

configure_file(packaging/linux/ro-assist.in
        ${CMAKE_CURRENT_BINARY_DIR}/packaging/ro-assist
        @ONLY)

install(TARGETS ro-assist RUNTIME DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/ro-assist)
install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/packaging/ro-assist
        DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES packaging/linux/ro-assist.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES packaging/linux/ro-assist-autostart.desktop
        DESTINATION /etc/xdg/autostart)
install(FILES packaging/linux/io.github.project_ro_asd.ro_assist.metainfo.xml
        DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
install(FILES resources/icons/ro-assist.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)

add_executable(ro-assist-ui-tests
    include/roassist/mainwindow.h
    include/roassist/system_services.h
    include/roassist/ui_texts.h
    include/roassist/update_helpers.h
    src/mainwindow.cpp
    src/system_services.cpp
    src/ui_texts.cpp
    src/update_helpers.cpp
    resources/resources.qrc
    tests/mainwindow_integration_test.cpp
)
if(TARGET Qt6::lrelease)
    qt_add_translations(ro-assist-ui-tests
        TS_FILES
            translations/ro-assist_en.ts
            translations/ro-assist_tr.ts
            translations/ro-assist_es.ts
    )
endif()
target_include_directories(ro-assist-ui-tests PRIVATE include)
target_compile_definitions(ro-assist-ui-tests PRIVATE APP_VERSION="${PROJECT_VERSION}" RO_ASSIST_TESTING)
target_link_libraries(ro-assist-ui-tests PRIVATE Qt6::Test Qt6::Widgets Qt6::Network Qt6::Concurrent)

add_executable(ro-assist-unit-tests
    include/roassist/system_services.h
    include/roassist/update_helpers.h
    src/system_services.cpp
    src/update_helpers.cpp
    tests/update_helpers_test.cpp
)
target_include_directories(ro-assist-unit-tests PRIVATE include)
target_link_libraries(ro-assist-unit-tests PRIVATE Qt6::Test Qt6::Core)

add_test(NAME ro-assist-unit-tests COMMAND ro-assist-unit-tests)
add_test(NAME ro-assist-ui-tests COMMAND ro-assist-ui-tests)
add_test(NAME ro-assist-smoke-test COMMAND ro-assist --smoke-test)
set_tests_properties(ro-assist-ui-tests PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;RO_ASSIST_SKIP_SYSTEM_CHECKS=1"
)
set_tests_properties(ro-assist-smoke-test PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;RO_ASSIST_SKIP_SYSTEM_CHECKS=1"
)

set(CPACK_GENERATOR "RPM")
set(CPACK_PACKAGE_NAME "ro-assist")
set(CPACK_PACKAGE_VERSION "0.2.5")
set(CPACK_PACKAGE_RELEASE 1)
set(CPACK_PACKAGE_CONTACT "Ebubekir Bulut <ebubekir.bulut99@gmail.com>")
set(CPACK_PACKAGE_VENDOR "Project Ro-ASD")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/Project-Ro-ASD/ro-Assist")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Ro-ASD first-run and maintenance center")
set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set(CPACK_PACKAGE_DIRECTORY "${CMAKE_BINARY_DIR}/packages")
set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
set(CPACK_RPM_FILE_NAME "RPM-DEFAULT")
set(CPACK_RPM_PACKAGE_LICENSE "GPL-3.0-or-later")
set(CPACK_RPM_PACKAGE_GROUP "Applications/System")
set(CPACK_RPM_PACKAGE_URL "${CPACK_PACKAGE_HOMEPAGE_URL}")
set(CPACK_RPM_PACKAGE_DESCRIPTION "ro-Assist is a Qt desktop application focused on controlled Ro-ASD system updates, first-run guidance, recommended setup actions, and safe handoff to ro Control for hardware and driver status.")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")

include(CPack)
