cmake_minimum_required(VERSION 3.24)
project(kastword VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

include(FetchContent)
find_package(ECM 6.0 REQUIRED NO_MODULE)
list(APPEND CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
find_package(Qt6 6.6 REQUIRED COMPONENTS Core Gui Widgets Quick QuickControls2 Multimedia Concurrent)
find_package(KF6 REQUIRED COMPONENTS Config CoreAddons GlobalAccel I18n Notifications StatusNotifierItem)

option(KASTWORD_FETCH_WHISPER "Download the pinned whisper.cpp dependency" ON)
option(KASTWORD_FETCH_DEFAULT_MODEL "Download the default English Whisper model" ON)
if(KASTWORD_FETCH_WHISPER)
    set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(WHISPER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(WHISPER_BUILD_SERVER OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(whisper
        GIT_REPOSITORY https://github.com/ggml-org/whisper.cpp.git
        GIT_TAG v1.8.1
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
            -DURL=https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base.en.bin
            -DSHA1=137c40403d78fd54d454da0f9bd998f78703390c
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
    src/AudioCapture.cpp
    src/AudioCapture.h
    src/WhisperEngine.cpp
    src/WhisperEngine.h
    src/TextOutput.cpp
    src/TextOutput.h
)

qt_add_qml_module(kastword
    URI org.kde.kastword
    VERSION 1.0
    QML_FILES src/qml/Main.qml
)

target_link_libraries(kastword PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Quick Qt6::QuickControls2 Qt6::Multimedia Qt6::Concurrent
    KF6::ConfigCore KF6::CoreAddons KF6::GlobalAccel KF6::I18n
    KF6::Notifications KF6::StatusNotifierItem whisper)

if(KASTWORD_FETCH_DEFAULT_MODEL)
    add_dependencies(kastword kastword-default-model)
endif()

install(TARGETS kastword RUNTIME DESTINATION bin)
install(FILES data/org.kde.kastword.desktop DESTINATION share/applications)
install(FILES data/org.kde.kastword.metainfo.xml DESTINATION share/metainfo)
if(KASTWORD_FETCH_DEFAULT_MODEL)
    install(FILES "${KASTWORD_DEFAULT_MODEL}" DESTINATION share/kastword/models)
endif()

include(CTest)
