# SPDX-FileCopyrightText: 2026 Sri Rang
# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.24)
project(kastword VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

include(FetchContent)
include(GNUInstallDirs)
find_package(ECM 6.0 REQUIRED NO_MODULE)
list(APPEND CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
find_package(PkgConfig REQUIRED)
pkg_check_modules(Sodium REQUIRED IMPORTED_TARGET libsodium)
find_package(Qt6 6.6 REQUIRED COMPONENTS Concurrent Core DBus Gui Multimedia Network Quick QuickControls2 Widgets)
find_package(KF6 6.20 REQUIRED COMPONENTS Config CoreAddons DBusAddons Declarative GlobalAccel I18n Kirigami Notifications StatusNotifierItem Wallet)

option(KASTWORD_FETCH_WHISPER "Download the pinned whisper.cpp dependency" ON)
option(KASTWORD_FETCH_DEFAULT_MODEL "Download the legacy default English Whisper model" OFF)
option(KASTWORD_ENABLE_COVERAGE "Instrument Kastword targets for test coverage" OFF)
option(KASTWORD_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
if(KASTWORD_FETCH_WHISPER)
    set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(WHISPER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(WHISPER_BUILD_SERVER OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(whisper
        GIT_REPOSITORY https://github.com/ggml-org/whisper.cpp.git
        GIT_TAG a91dd3be72f70dd1b3cb6e252f35fa17b93f596c
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(whisper)
else()
    find_package(whisper CONFIG REQUIRED)
endif()

set(KASTWORD_MODEL_DIR "${CMAKE_BINARY_DIR}/models")
set(KASTWORD_DEFAULT_MODEL "${KASTWORD_MODEL_DIR}/ggml-base.en.bin")
if(KASTWORD_FETCH_DEFAULT_MODEL)
    add_custom_command(
        OUTPUT "${KASTWORD_DEFAULT_MODEL}"
        COMMAND "${CMAKE_COMMAND}"
            -DOUTPUT=${KASTWORD_DEFAULT_MODEL}
            -DURL=https://huggingface.co/ggerganov/whisper.cpp/resolve/4496f29dabb6f37d8e6c45c3ec89ccbe66a832ea/ggml-base.en.bin
            -DSHA256=a03779c86df3323075f5e796cb2ce5029f00ec8869eee3fdfb897afe36c6d002
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/DownloadModel.cmake"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/cmake/DownloadModel.cmake"
        COMMENT "Downloading checksum-verified Whisper base.en model (142 MiB)"
        VERBATIM)
    add_custom_target(kastword-default-model ALL DEPENDS "${KASTWORD_DEFAULT_MODEL}")
endif()

qt_add_executable(kastword
    src/main.cpp
    src/AppController.cpp
    src/AppController.h
    src/DictationHistory.cpp
    src/DictationHistory.h
    src/AudioConversion.cpp
    src/AudioConversion.h
    src/AudioCapture.cpp
    src/AudioCapture.h
    src/ModelLocator.cpp
    src/ModelLocator.h
    src/ModelCatalog.cpp
    src/ModelCatalog.h
    src/ModelManager.cpp
    src/ModelManager.h
    src/PlatformIntegration.cpp
    src/PlatformIntegration.h
    src/RuntimeSecurity.cpp
    src/RuntimeSecurity.h
    src/WhisperEngine.cpp
    src/WhisperEngine.h
    src/TextOutput.cpp
    src/TextOutput.h
    src/TranscriptionWorker.cpp
    src/TranscriptionWorker.h
)

set_source_files_properties(src/qml/Main.qml PROPERTIES QT_RESOURCE_ALIAS Main.qml)
set_source_files_properties(src/qml/HistoryEntryCard.qml PROPERTIES QT_RESOURCE_ALIAS HistoryEntryCard.qml)
set_source_files_properties(src/qml/FooterContainer.qml PROPERTIES QT_RESOURCE_ALIAS FooterContainer.qml)
qt_add_qml_module(kastword
    URI io.github.shape_machine.Kastword
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml
    NO_IMPORT_SCAN
    QML_FILES src/qml/Main.qml src/qml/HistoryEntryCard.qml src/qml/FooterContainer.qml
)

target_link_libraries(kastword PRIVATE
    Qt6::Concurrent Qt6::Core Qt6::DBus Qt6::Gui Qt6::Widgets Qt6::Quick Qt6::QuickControls2
    Qt6::Multimedia Qt6::Network
    KF6::ConfigCore KF6::CoreAddons KF6::DBusAddons KF6::GlobalAccel KF6::I18n
    KF6::I18nQml KF6::Kirigami KF6::Notifications KF6::StatusNotifierItem whisper)
target_link_libraries(kastword PRIVATE KF6::Wallet PkgConfig::Sodium)

function(kastword_enable_coverage target)
    if(KASTWORD_ENABLE_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE --coverage -O0 -g)
        target_link_options(${target} PRIVATE --coverage)
    endif()
endfunction()
function(kastword_enable_sanitizers target)
    if(KASTWORD_ENABLE_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(${target} PRIVATE -fsanitize=address,undefined)
    endif()
endfunction()
kastword_enable_coverage(kastword)
kastword_enable_sanitizers(kastword)

add_executable(kastword_screenshot_generator EXCLUDE_FROM_ALL
    tools/ScreenshotGenerator.cpp
    tools/ScreenshotPlatform.h
    tests/FakeAppController.h)
target_include_directories(kastword_screenshot_generator PRIVATE tests tools)
target_link_libraries(kastword_screenshot_generator PRIVATE
    Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Widgets
    KF6::I18n KF6::I18nQml KF6::Kirigami)
qt_add_resources(kastword_screenshot_generator "screenshot-qml-resources"
    PREFIX "/"
    FILES src/qml/Main.qml src/qml/HistoryEntryCard.qml src/qml/FooterContainer.qml)
kastword_enable_sanitizers(kastword_screenshot_generator)

if(KASTWORD_FETCH_DEFAULT_MODEL)
    add_dependencies(kastword kastword-default-model)
endif()

configure_file(data/io.github.shape_machine.Kastword.desktop.in
    ${CMAKE_CURRENT_BINARY_DIR}/io.github.shape_machine.Kastword.desktop @ONLY)

install(TARGETS kastword RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Kastword)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/io.github.shape_machine.Kastword.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications COMPONENT Kastword)
install(FILES data/io.github.shape_machine.Kastword.metainfo.xml DESTINATION share/metainfo COMPONENT Kastword)
install(FILES README.md DESTINATION share/doc/kastword COMPONENT Kastword)
install(FILES docs/DEVELOPMENT.md docs/AUTOMATIC_PASTE.md docs/DICTATION_HISTORY.md
        DESTINATION share/doc/kastword/docs COMPONENT Kastword)
install(FILES
        screenshots/01-offline-dictation.png
        screenshots/02-history.png
        screenshots/03-speech-models.png
        screenshots/04-audio-input.png
        screenshots/05-settings.png
        DESTINATION share/doc/kastword/screenshots COMPONENT Kastword)
install(FILES LICENSES/GPL-3.0-or-later.txt
        DESTINATION share/doc/kastword/LICENSES COMPONENT Kastword)
if(KASTWORD_FETCH_DEFAULT_MODEL)
    install(FILES "${KASTWORD_DEFAULT_MODEL}" DESTINATION share/kastword/models COMPONENT Kastword)
endif()
set(CMAKE_INSTALL_DEFAULT_COMPONENT_NAME Kastword)
ki18n_install(po)
unset(CMAKE_INSTALL_DEFAULT_COMPONENT_NAME)

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 6.6 REQUIRED COMPONENTS QuickTest Test)
    add_test(NAME application-smoke COMMAND kastword --smoke-test)
    set_tests_properties(application-smoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    add_executable(kastword_textoutput_test
        tests/TextOutputTest.cpp
        src/TextOutput.cpp
        src/TextOutput.h)
    target_include_directories(kastword_textoutput_test PRIVATE src)
    target_link_libraries(kastword_textoutput_test PRIVATE
        Qt6::Core Qt6::DBus Qt6::Gui Qt6::Test KF6::I18n)
    kastword_enable_coverage(kastword_textoutput_test)
    kastword_enable_sanitizers(kastword_textoutput_test)
    add_test(NAME text-output COMMAND kastword_textoutput_test)
    set_tests_properties(text-output PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    add_executable(kastword_core_test
        tests/CoreTest.cpp
        src/AudioConversion.cpp
        src/AudioConversion.h
        src/ModelLocator.cpp
        src/ModelLocator.h
        src/RuntimeSecurity.cpp
        src/RuntimeSecurity.h
        src/WhisperEngine.cpp
        src/WhisperEngine.h)
    target_include_directories(kastword_core_test PRIVATE src)
    target_link_libraries(kastword_core_test PRIVATE
        Qt6::Core Qt6::Multimedia Qt6::Test KF6::I18n whisper)
    kastword_enable_coverage(kastword_core_test)
    kastword_enable_sanitizers(kastword_core_test)
    add_test(NAME core COMMAND kastword_core_test)

    add_executable(kastword_model_manager_test
        tests/ModelManagerTest.cpp
        src/ModelCatalog.cpp
        src/ModelCatalog.h
        src/ModelManager.cpp
        src/ModelManager.h)
    target_include_directories(kastword_model_manager_test PRIVATE src)
    target_link_libraries(kastword_model_manager_test PRIVATE
        Qt6::Concurrent Qt6::Core Qt6::Network Qt6::Test KF6::I18n whisper)
    kastword_enable_coverage(kastword_model_manager_test)
    kastword_enable_sanitizers(kastword_model_manager_test)
    add_test(NAME model-manager COMMAND kastword_model_manager_test)

    add_executable(kastword_history_test
        tests/DictationHistoryTest.cpp
        src/DictationHistory.cpp
        src/DictationHistory.h)
    target_include_directories(kastword_history_test PRIVATE src)
    target_link_libraries(kastword_history_test PRIVATE
        Qt6::Concurrent Qt6::Core Qt6::Test KF6::I18n KF6::Wallet PkgConfig::Sodium)
    kastword_enable_coverage(kastword_history_test)
    kastword_enable_sanitizers(kastword_history_test)
    add_test(NAME history COMMAND kastword_history_test)

    add_executable(kastword_controller_test
        tests/AppControllerTest.cpp
        src/AppController.cpp
        src/AppController.h
        src/DictationHistory.cpp
        src/DictationHistory.h
        src/AudioCapture.cpp
        src/AudioCapture.h
        src/AudioConversion.cpp
        src/AudioConversion.h
        src/ModelLocator.cpp
        src/ModelLocator.h
        src/ModelCatalog.cpp
        src/ModelCatalog.h
        src/ModelManager.cpp
        src/ModelManager.h
        src/PlatformIntegration.cpp
        src/PlatformIntegration.h
        src/TextOutput.cpp
        src/TextOutput.h
        src/TranscriptionWorker.cpp
        src/TranscriptionWorker.h
        src/WhisperEngine.cpp
        src/WhisperEngine.h)
    target_include_directories(kastword_controller_test PRIVATE src)
    target_link_libraries(kastword_controller_test PRIVATE
        Qt6::Concurrent Qt6::Core Qt6::DBus Qt6::Gui Qt6::Multimedia Qt6::Network Qt6::Test Qt6::Widgets
        KF6::ConfigCore KF6::GlobalAccel KF6::I18n KF6::Notifications KF6::Wallet
        PkgConfig::Sodium whisper)
    kastword_enable_coverage(kastword_controller_test)
    kastword_enable_sanitizers(kastword_controller_test)
    add_test(NAME controller COMMAND kastword_controller_test)
    set_tests_properties(controller PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    add_executable(kastword_qml_test tests/QmlTestSetup.cpp tests/FakeAppController.h)
    target_compile_definitions(kastword_qml_test PRIVATE
        QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/qml")
    target_link_libraries(kastword_qml_test PRIVATE
        Qt6::Qml Qt6::Quick Qt6::QuickTest KF6::I18n KF6::I18nQml KF6::Kirigami)
    qt_add_resources(kastword_qml_test "qml-test-resources"
        PREFIX "/"
        FILES src/qml/Main.qml src/qml/HistoryEntryCard.qml src/qml/FooterContainer.qml)
    kastword_enable_sanitizers(kastword_qml_test)
    add_dependencies(kastword_qml_test kastword_screenshot_generator)
    add_test(NAME qml COMMAND kastword_qml_test)
    set_tests_properties(qml PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_FONT_DPI=192")

    add_test(NAME screenshot-workflow
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/check-screenshot-workflow.sh
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/capture-screenshots.sh)
    add_test(NAME screenshot-rejects-headless
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/check-screenshot-headless.sh
            $<TARGET_FILE:kastword_screenshot_generator>)

    add_executable(kastword_screenshot_platform_test tests/ScreenshotPlatformTest.cpp)
    target_include_directories(kastword_screenshot_platform_test PRIVATE tools)
    target_link_libraries(kastword_screenshot_platform_test PRIVATE Qt6::Core Qt6::Test)
    kastword_enable_coverage(kastword_screenshot_platform_test)
    kastword_enable_sanitizers(kastword_screenshot_platform_test)
    add_test(NAME screenshot-platform COMMAND kastword_screenshot_platform_test)

    add_test(NAME documentation-links
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tools/check-documentation-links.sh
            ${CMAKE_CURRENT_SOURCE_DIR})

    add_test(NAME translation-catalog
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/check-translations.sh)
    set_tests_properties(translation-catalog PROPERTIES
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif()
