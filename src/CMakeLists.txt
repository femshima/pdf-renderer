add_executable(pdf-renderer main.cpp pdfium_test_write_helper.cpp i.cpp)
find_package(PDFium)
target_include_directories(pdf-renderer PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(pdf-renderer lib png pdfium)