include(FetchContent)

#=================== SDL2 ===================
# LUS_SHARED_SDL: the game is a module of a host app (i64) that embeds SDL as
# @rpath/libSDL2-2.0.0.dylib; link SDL dynamically so the module uses the
# host's copy at run time (the host's window is an SDL_Window of that copy).
# LUS_SDL2_PREBUILT=<dir with include/SDL2 and lib/libSDL2-2.0.0.dylib>: use
# the host's own SDL build (i64's deps-out/ios) instead of building one.
# (Same as the SpaghettiKart/BattleShip forks.)
if (LUS_SHARED_SDL)
    set(SDL_SHARED ON CACHE BOOL "" FORCE)
    set(SDL_STATIC OFF CACHE BOOL "" FORCE)
    set(SDL2_DISABLE_SDL2MAIN ON CACHE BOOL "" FORCE)
endif()
if (LUS_SHARED_SDL AND LUS_SDL2_PREBUILT)
    add_library(SDL2::SDL2 SHARED IMPORTED GLOBAL)
    set_target_properties(SDL2::SDL2 PROPERTIES
        IMPORTED_LOCATION ${LUS_SDL2_PREBUILT}/lib/libSDL2-2.0.0.dylib
        IMPORTED_SONAME @rpath/libSDL2-2.0.0.dylib
        INTERFACE_INCLUDE_DIRECTORIES "${LUS_SDL2_PREBUILT}/include;${LUS_SDL2_PREBUILT}/include/SDL2")
    set(SDL2_INCLUDE_DIRS ${LUS_SDL2_PREBUILT}/include/SDL2)
    set(SDL2_FOUND TRUE)
else()
    find_package(SDL2 QUIET)
endif()
if (NOT ${SDL2_FOUND})
    FetchContent_Declare(
        SDL2
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-2.28.1
        OVERRIDE_FIND_PACKAGE
    )
    FetchContent_MakeAvailable(SDL2)
endif()

#=================== nlohmann-json ===================
find_package(nlohmann_json QUIET)
if (NOT ${nlohmann_json_FOUND})
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
        OVERRIDE_FIND_PACKAGE
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

#=================== tinyxml2 ===================
find_package(tinyxml2 QUIET)
if (NOT ${tinyxml2_FOUND})
    set(tinyxml2_BUILD_TESTING OFF)
    FetchContent_Declare(
        tinyxml2
        GIT_REPOSITORY https://github.com/leethomason/tinyxml2.git
        GIT_TAG 10.0.0
        OVERRIDE_FIND_PACKAGE
    )
    FetchContent_MakeAvailable(tinyxml2)
endif()

#=================== spdlog ===================
find_package(spdlog QUIET)
if (NOT ${spdlog_FOUND})
    FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG v1.15.3
        OVERRIDE_FIND_PACKAGE
    )
    FetchContent_MakeAvailable(spdlog)
endif()

#=================== libzip ===================
find_package(libzip QUIET)
if (NOT ${libzip_FOUND})
    set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
    set(BUILD_TOOLS OFF)
    set(BUILD_REGRESS OFF)
    set(BUILD_EXAMPLES OFF)
    set(BUILD_DOC OFF)
    set(BUILD_OSSFUZZ OFF)
    set(BUILD_SHARED_LIBS OFF)
    FetchContent_Declare(
        libzip
        GIT_REPOSITORY https://github.com/nih-at/libzip.git
        GIT_TAG v1.10.1
        OVERRIDE_FIND_PACKAGE
    )
    FetchContent_MakeAvailable(libzip)
    list(APPEND ADDITIONAL_LIB_INCLUDES ${libzip_SOURCE_DIR}/lib ${libzip_BINARY_DIR})
endif()

#=================== Metal-cpp ===================
FetchContent_Declare(
    metalcpp
    GIT_REPOSITORY https://github.com/briaguya-ai/single-header-metal-cpp.git
    GIT_TAG macOS13_iOS16
)
FetchContent_MakeAvailable(metalcpp)
list(APPEND ADDITIONAL_LIB_INCLUDES ${metalcpp_SOURCE_DIR})

#=================== ImGui ===================
target_sources(ImGui
    PRIVATE
    ${imgui_SOURCE_DIR}/backends/imgui_impl_metal.mm
)

target_include_directories(ImGui PRIVATE ${metalcpp_SOURCE_DIR})
target_compile_definitions(ImGui PUBLIC IMGUI_IMPL_METAL_CPP)

if (LUS_SHARED_SDL)
    target_link_libraries(ImGui PUBLIC SDL2::SDL2)
else()
    target_link_libraries(ImGui PUBLIC SDL2::SDL2-static SDL2::SDL2main)
endif()

# LUS_OBJC_PREFIX=<game>: Objective-C class names are process wide, so two
# games inside one host app (i64) would both define ImGui's Metal backend
# classes ("Class X is implemented in both"): give this game's copies its own
# names.
if (LUS_OBJC_PREFIX)
    target_compile_definitions(ImGui PRIVATE
        MetalContext=${LUS_OBJC_PREFIX}MetalContext
        MetalBuffer=${LUS_OBJC_PREFIX}MetalBuffer
        FramebufferDescriptor=${LUS_OBJC_PREFIX}FramebufferDescriptor)
endif()
