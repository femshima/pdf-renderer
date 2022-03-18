add_executable(a.out main.cpp pdfium_test_write_helper.cpp i.cpp)
find_package(PDFium)
target_include_directories(a.out PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(a.out lib png pdfium)