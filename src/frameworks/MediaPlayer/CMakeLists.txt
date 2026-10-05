project(MediaPlayer)

set(DYLIB_COMPAT_VERSION "1.0.0")
set(DYLIB_CURRENT_VERSION "1.0.0")

set(FRAMEWORK_VERSION "A")

add_framework(MediaPlayer
    FAT
    CURRENT_VERSION
    VERSION ${FRAMEWORK_VERSION}

    SOURCES
        src/MediaPlayer.m

    DEPENDENCIES
        system
        objc
        CoreFoundation
        Foundation
)
