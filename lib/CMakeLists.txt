add_library(lib
    STATIC image_diff_png.cpp
)
target_include_directories(lib PUBLIC ${PROJECT_SOURCE_DIR})
target_include_directories(lib
    PUBLIC ${PROJECT_SOURCE_DIR}/lib
)