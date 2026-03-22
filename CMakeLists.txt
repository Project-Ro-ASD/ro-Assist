cmake_minimum_required(VERSION 3.16)
project(ro-assist VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

# ÖNCE UYGULAMAYI TANIMLA
add_executable(ro-assist main.cpp mainwindow.cpp resources.qrc)
target_link_libraries(ro-assist PRIVATE Qt6::Widgets Qt6::Network)

# SONRA YÜKLEME HEDEFİNİ BELİRLE (add_executable'dan sonra olmalı!)
install(TARGETS ro-assist DESTINATION bin)

# RPM PAKETLEME AYARLARI
set(CPACK_GENERATOR "RPM")
set(CPACK_PACKAGE_NAME "ro-assist")
set(CPACK_PACKAGE_VERSION "0.1.0")
set(CPACK_PACKAGE_RELEASE 1)
set(CPACK_PACKAGE_CONTACT "Ebubekir Bulut <mutemet91@gmail.com>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Fedora Linux Sistem Asistanı")

include(CPack)