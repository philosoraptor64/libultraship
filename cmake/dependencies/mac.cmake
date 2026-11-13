include(FetchContent)

#=================== spdlog ===================
# macports has issues with this because of fmt
# brew doesn't support building multiarch
find_package(spdlog QUIET)
# A game module inside a host app (LUS_OBJC_PREFIX set, e.g. i64): link spdlog's static archive. A shared spdlog
# outlives the module's runs and is shared with the host's other games; its registry then points into memory the
# module resets between runs.
if (spdlog_FOUND AND LUS_OBJC_PREFIX AND TARGET spdlog::spdlog)
    get_target_property(_spdlog_dylib spdlog::spdlog IMPORTED_LOCATION_RELEASE)
    if (NOT _spdlog_dylib)
        get_target_property(_spdlog_dylib spdlog::spdlog IMPORTED_LOCATION_NOCONFIG)
    endif()
    get_filename_component(_spdlog_dir "${_spdlog_dylib}" DIRECTORY)
    if (EXISTS "${_spdlog_dir}/libspdlog.a")
        set_target_properties(spdlog::spdlog PROPERTIES
            IMPORTED_LOCATION_RELEASE "${_spdlog_dir}/libspdlog.a"
            IMPORTED_LOCATION_NOCONFIG "${_spdlog_dir}/libspdlog.a"
            IMPORTED_LOCATION "${_spdlog_dir}/libspdlog.a")
        message(STATUS "game module: static spdlog ${_spdlog_dir}/libspdlog.a")
    endif()
endif()
if (NOT ${spdlog_FOUND})
    FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG v1.14.1
        OVERRIDE_FIND_PACKAGE
    )
    FetchContent_MakeAvailable(spdlog)
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

find_package(SDL2 REQUIRED)
target_link_libraries(ImGui PUBLIC SDL2::SDL2)

find_package(GLEW REQUIRED)
target_link_libraries(ImGui PUBLIC ${OPENGL_opengl_LIBRARY} GLEW::GLEW)
set_target_properties(ImGui PROPERTIES
    XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC YES
)

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
