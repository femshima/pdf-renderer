#include <stdlib.h>
#include <memory>
#include <string.h>
#include "lib/span.h"
#include <lib/notreached.h>

namespace pdfium
{

  // Used with std::unique_ptr to free() objects that can't be deleted.
  struct FreeDeleter
  {
    inline void operator()(void *ptr) const { free(ptr); }
  };

}

std::unique_ptr<char, pdfium::FreeDeleter> GetFileContents(const char *filename,
                                                           size_t *retlen);

class TestLoader
{
public:
  explicit TestLoader(pdfium::span<const char> span);

  static int GetBlock(void *param,
                      unsigned long pos,
                      unsigned char *pBuf,
                      unsigned long size);

private:
  const pdfium::span<const char> m_Span;
};