cmake_minimum_required(VERSION 3.15)
project(NEStupid)

set(CMAKE_C_STANDARD 11)

# Find SDL2
find_package(SDL2 REQUIRED)

# Source files
set(SOURCES
    src/main.c
    src/gui.c
    src/rom.c
    src/memory.c
    src/mapper.c
    src/cpu.c
    src/ppu.c
    src/input.c
    src/apu.c
)

# App Bundle settings
set(MACOSX_BUNDLE_BUNDLE_NAME "NEStupid")
set(MACOSX_BUNDLE_GUI_IDENTIFIER "com.anthony.nestupid")
set(MACOSX_BUNDLE_ICON_FILE "nestupid.icns")
set(MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist")

# Executable
add_executable(NEStupid MACOSX_BUNDLE ${SOURCES})

# Include directories
target_include_directories(NEStupid PRIVATE src)
target_include_directories(NEStupid PRIVATE ${SDL2_INCLUDE_DIRS})

# Link libraries
target_link_libraries(NEStupid PRIVATE ${SDL2_LIBRARIES})

# Copy icon to Resources
set_source_files_properties(nestupid.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
target_sources(NEStupid PRIVATE nestupid.icns)
