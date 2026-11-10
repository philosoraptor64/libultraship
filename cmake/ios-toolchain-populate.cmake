# LUS_IOS_PLATFORM=OS64: device only (e.g. a host app's game module built
# with Ninja); the default builds device + simulator (Xcode generator only).
if (DEFINED LUS_IOS_PLATFORM)
    set(PLATFORM "${LUS_IOS_PLATFORM}")
else()
    set(PLATFORM "OS64COMBINED")
endif()
include(FetchContent)
FetchContent_Declare(iostoolchain
    GIT_REPOSITORY https://github.com/leetal/ios-cmake
    GIT_TAG 06465b27698424cf4a04a5ca4904d50a3c966c45
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
)
FetchContent_GetProperties(iostoolchain)
if(NOT iostoolchain_POPULATED)
    FetchContent_Populate(iostoolchain)
endif()
set(CMAKE_IOS_TOOLCHAIN_FILE ${iostoolchain_SOURCE_DIR}/ios.toolchain.cmake)
set_property(GLOBAL PROPERTY IOS_TOOLCHAIN_FILE ${CMAKE_IOS_TOOLCHAIN_FILE})
include(${CMAKE_IOS_TOOLCHAIN_FILE})