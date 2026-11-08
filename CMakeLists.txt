cmake_minimum_required(VERSION 3.20)
project(fh2anna VERSION 1.1.0 LANGUAGES C CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_subdirectory(platform)   # fh2mod_platform (DWrite.dll) and fh2mod_api

add_library(fh2anna SHARED src/fh2anna.cpp src/anna_xaml.cpp src/actions.cpp src/car.cpp src/drive.cpp src/pause.cpp src/lang.cpp src/voice.cpp src/cinematic.cpp src/textures.cpp src/resources.rc)
# rc files don't track what they embed
file(GLOB FH3_TEXTURES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/assets/*.swatchbin ${CMAKE_SOURCE_DIR}/assets/*.xaml)
set_source_files_properties(src/resources.rc PROPERTIES OBJECT_DEPENDS "${FH3_TEXTURES}")
target_include_directories(fh2anna PRIVATE include)
target_link_libraries(fh2anna PRIVATE fh2mod_api user32 ole32 sapi)
set_target_properties(fh2anna PROPERTIES OUTPUT_NAME FH2ANNA PREFIX "")
target_compile_definitions(fh2anna PRIVATE FH2ANNA_VERSION="${PROJECT_VERSION}"
                           _WIN32_WINNT=0x0A00 WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
if(MSVC)
    target_compile_options(fh2anna PRIVATE /W4 /utf-8)
else()
    # -fms-extensions: __try/__except around the calls into the game's UI code.
    target_compile_options(fh2anna PRIVATE -Wall -Wextra -fms-extensions -O2)
    target_link_options(fh2anna PRIVATE -static -s)
endif()
