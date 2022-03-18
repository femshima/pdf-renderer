#include "src/i.h"

TestLoader::TestLoader(pdfium::span<const char> span) : m_Span(span) {}

// static
int TestLoader::GetBlock(void* param,
                         unsigned long pos,
                         unsigned char* pBuf,
                         unsigned long size) {
  TestLoader* pLoader = static_cast<TestLoader*>(param);
  if (pos + size < pos || pos + size > pLoader->m_Span.size()) {
    NOTREACHED();
    return 0;
  }

  memcpy(pBuf, &pLoader->m_Span[pos], size);
  return 1;
}

std::unique_ptr<char, pdfium::FreeDeleter> GetFileContents(const char* filename,
                                                           size_t* retlen) {
  FILE* file = fopen(filename, "rb");
  if (!file) {
    fprintf(stderr, "Failed to open: %s\n", filename);
    return nullptr;
  }
  (void)fseek(file, 0, SEEK_END);
  size_t file_length = ftell(file);
  if (!file_length) {
    return nullptr;
  }
  (void)fseek(file, 0, SEEK_SET);
  std::unique_ptr<char, pdfium::FreeDeleter> buffer(
      static_cast<char*>(malloc(file_length)));
  if (!buffer) {
    return nullptr;
  }
  size_t bytes_read = fread(buffer.get(), 1, file_length, file);
  (void)fclose(file);
  if (bytes_read != file_length) {
    fprintf(stderr, "Failed to read: %s\n", filename);
    return nullptr;
  }
  *retlen = bytes_read;
  return buffer;
}