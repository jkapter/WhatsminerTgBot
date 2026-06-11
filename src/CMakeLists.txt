cmake_minimum_required(VERSION 3.19)
project(WhatsminerTgBot LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

add_definitions(-D_WIN32_WINNT=0x0A00)
add_definitions(-DWIN32_LEAN_AND_MEAN)
add_definitions(-DNOMINMAX)
add_definitions(-DBOOST_ASIO_ENABLE_OLD_SERVICES)
add_definitions(-DHAVE_CURL)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Network)

set(THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../third_party")
set(TgBot_DIR "${THIRD_PARTY_DIR}/tgbot/lib/cmake/TgBot")
find_package(TgBot REQUIRED)

set(OPENSSL_ROOT_DIR       "${THIRD_PARTY_DIR}/openssl")
set(OPENSSL_INCLUDE_DIR    "${THIRD_PARTY_DIR}/openssl/include")
set(OPENSSL_SSL_LIBRARY    "${THIRD_PARTY_DIR}/openssl/lib/libssl.dll.a")
set(OPENSSL_CRYPTO_LIBRARY "${THIRD_PARTY_DIR}/openssl/lib/libcrypto.dll.a")
find_package(OpenSSL REQUIRED)

set(ZLIB_INCLUDE_DIR "${THIRD_PARTY_DIR}/zlib/include")
set(ZLIB_LIBRARY     "${THIRD_PARTY_DIR}/zlib/lib/libz.a")
find_package(ZLIB REQUIRED)

set(CURL_INCLUDE_DIR "${THIRD_PARTY_DIR}/curl/include")
set(CURL_LIBRARY     "${THIRD_PARTY_DIR}/curl/lib/libcurl.a")
find_package(CURL 7.58.0 REQUIRED)

set(BOOST_ROOT "C:/dev/boost_1_90_0")
find_package(Boost REQUIRED)

find_package(Threads REQUIRED)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${OPENSSL_INCLUDE_DIR}
    ${CURL_INCLUDE_DIRS}
    "${THIRD_PARTY_DIR}/tgbot/include"
    ${BOOST_ROOT}
)

qt_standard_project_setup()

qt_add_executable(WhatsminerTgBot
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    whatsminerdevice.cpp whatsminerdevice.h
    plaintextconsole.cpp plaintextconsole.h
    devicemanager.h devicemanager.cpp
    logger.cpp logger.h
    treetablemodels.h treetablemodels.cpp
    rsrc.qrc
    tgbotmanager.cpp tgbotmanager.h tgbotthreadwrapper.cpp tgbotthreadwrapper.h
    hintinputdialog.cpp hintinputdialog.h
)

target_link_libraries(WhatsminerTgBot
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt::Network
        TgBot::TgBot
        ${CURL_LIBRARIES}
        OpenSSL::SSL
        OpenSSL::Crypto
        ZLIB::ZLIB
        ${CMAKE_THREAD_LIBS_INIT}
)

install(TARGETS WhatsminerTgBot
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET WhatsminerTgBot
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
