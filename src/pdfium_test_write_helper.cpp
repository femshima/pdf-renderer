// Copyright 2018 The PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/pdfium_test_write_helper.h"

#include <limits.h>

#include <string>
#include <utility>
#include <vector>

#include "pdfium/include/cpp/fpdf_scopers.h"
#include "pdfium/include/fpdf_annot.h"
#include "pdfium/include/fpdf_attachment.h"
#include "pdfium/include/fpdf_edit.h"
#include "pdfium/include/fpdf_thumbnail.h"
// #include "testing/fx_string_testhelpers.h"
// #include "testing/image_diff/image_diff_png.h"
// #include "third_party/base/notreached.h"

#include "src/i.h"
#include "lib/span.h"
#include "lib/image_diff_png.h"

namespace {

bool CheckDimensions(int stride, int width, int height) {
  if (stride < 0 || width < 0 || height < 0)
    return false;
  if (height > 0 && stride > INT_MAX / height)
    return false;
  return true;
}


std::vector<uint8_t> EncodePng(pdfium::span<const uint8_t> input,
                               int width,
                               int height,
                               int stride,
                               int format) {
  std::vector<uint8_t> png;
  switch (format) {
    case FPDFBitmap_Unknown:
      break;
    case FPDFBitmap_Gray:
      png = image_diff_png::EncodeGrayPNG(input, width, height, stride);
      break;
    case FPDFBitmap_BGR:
      png = image_diff_png::EncodeBGRPNG(input, width, height, stride);
      break;
    case FPDFBitmap_BGRx:
      png = image_diff_png::EncodeBGRAPNG(input, width, height, stride,
                                          /*discard_transparency=*/true);
      break;
    case FPDFBitmap_BGRA:
      png = image_diff_png::EncodeBGRAPNG(input, width, height, stride,
                                          /*discard_transparency=*/false);
      break;
    default:
      NOTREACHED();
  }
  return png;
}


}  // namespace


// std::string WritePng(const char* pdf_name,
//                      int num,
//                      void* buffer,
//                      int stride,
//                      int width,
//                      int height) {
//   if (!CheckDimensions(stride, width, height))
//     return "";

//   auto input =
//       pdfium::make_span(static_cast<uint8_t*>(buffer), stride * height);
//   std::vector<uint8_t> png_encoding =
//       EncodePng(input, width, height, stride, FPDFBitmap_BGRA);
//   if (png_encoding.empty()) {
//     fprintf(stderr, "Failed to convert bitmap to PNG\n");
//     return "";
//   }

//   char filename[256];
//   int chars_formatted =
//       snprintf(filename, sizeof(filename), "%s.%d.png", pdf_name, num);
//   if (chars_formatted < 0 ||
//       static_cast<size_t>(chars_formatted) >= sizeof(filename)) {
//     fprintf(stderr, "Filename %s is too long\n", filename);
//     return "";
//   }

//   FILE* fp = fopen(filename, "wb");
//   if (!fp) {
//     fprintf(stderr, "Failed to open %s for output\n", filename);
//     return "";
//   }

//   size_t bytes_written =
//       fwrite(&png_encoding.front(), 1, png_encoding.size(), fp);
//   if (bytes_written != png_encoding.size())
//     fprintf(stderr, "Failed to write to %s\n", filename);

//   (void)fclose(fp);
//   return std::string(filename);
// }


std::string WritePng(const char* out_name,
                     int num,
                     void* buffer,
                     int stride,
                     int width,
                     int height) {
  if (!CheckDimensions(stride, width, height))
    return "";

  auto input =
      pdfium::make_span(static_cast<uint8_t*>(buffer), stride * height);
  std::vector<uint8_t> png_encoding =
      EncodePng(input, width, height, stride, FPDFBitmap_BGRA);
  if (png_encoding.empty()) {
    fprintf(stderr, "Failed to convert bitmap to PNG\n");
    return "";
  }

  char filename[256];
  int chars_formatted =
    num>0
      ? snprintf(filename, sizeof(filename), "%s.%d.png", out_name, num)
      : snprintf(filename, sizeof(filename), "%s.png", out_name);
  if (chars_formatted < 0 ||
      static_cast<size_t>(chars_formatted) >= sizeof(filename)) {
    fprintf(stderr, "Filename %s is too long\n", filename);
    return "";
  }

  FILE* fp = fopen(filename, "wb");
  if (!fp) {
    fprintf(stderr, "Failed to open %s for output\n", filename);
    return "";
  }

  size_t bytes_written =
      fwrite(&png_encoding.front(), 1, png_encoding.size(), fp);
  if (bytes_written != png_encoding.size())
    fprintf(stderr, "Failed to write to %s\n", filename);

  (void)fclose(fp);
  return std::string(filename);
}


enum class ThumbnailDecodeType { kBitmap, kRawStream, kDecodedStream };

bool GetThumbnailFilename(char* name_buf,
                          size_t name_buf_size,
                          const char* pdf_name,
                          int page_num,
                          ThumbnailDecodeType decode_type) {
  const char* format;
  switch (decode_type) {
    case ThumbnailDecodeType::kBitmap:
      format = "%s.thumbnail.%d.png";
      break;
    case ThumbnailDecodeType::kDecodedStream:
      format = "%s.thumbnail.decoded.%d.bin";
      break;
    case ThumbnailDecodeType::kRawStream:
      format = "%s.thumbnail.raw.%d.bin";
      break;
  }

  int chars_formatted =
      snprintf(name_buf, name_buf_size, format, pdf_name, page_num);
  if (chars_formatted < 0 ||
      static_cast<size_t>(chars_formatted) >= name_buf_size) {
    fprintf(stderr, "Filename %s for saving is too long.\n", name_buf);
    return false;
  }

  return true;
}

void WriteBufferToFile(const void* buf,
                       size_t buflen,
                       const char* filename,
                       const char* filetype) {
  FILE* fp = fopen(filename, "wb");
  if (!fp) {
    fprintf(stderr, "Failed to open %s for saving %s.", filename, filetype);
    return;
  }

  size_t bytes_written = fwrite(buf, 1, buflen, fp);
  if (bytes_written == buflen)
    fprintf(stderr, "Successfully wrote %s %s.\n", filetype, filename);
  else
    fprintf(stderr, "Failed to write to %s.\n", filename);
  fclose(fp);
}

std::vector<uint8_t> EncodeBitmapToPng(ScopedFPDFBitmap bitmap) {
  std::vector<uint8_t> png_encoding;
  int format = FPDFBitmap_GetFormat(bitmap.get());
  if (format == FPDFBitmap_Unknown)
    return png_encoding;

  int width = FPDFBitmap_GetWidth(bitmap.get());
  int height = FPDFBitmap_GetHeight(bitmap.get());
  int stride = FPDFBitmap_GetStride(bitmap.get());
  if (!CheckDimensions(stride, width, height))
    return png_encoding;

  auto input = pdfium::make_span(
      static_cast<const uint8_t*>(FPDFBitmap_GetBuffer(bitmap.get())),
      stride * height);

  png_encoding = EncodePng(input, width, height, stride, format);
  return png_encoding;
}

void WriteImages(FPDF_PAGE page, const char* pdf_name, int page_num) {
  for (int i = 0; i < FPDFPage_CountObjects(page); ++i) {
    FPDF_PAGEOBJECT obj = FPDFPage_GetObject(page, i);
    if (FPDFPageObj_GetType(obj) != FPDF_PAGEOBJ_IMAGE)
      continue;

    ScopedFPDFBitmap bitmap(FPDFImageObj_GetBitmap(obj));
    if (!bitmap) {
      fprintf(stderr, "Image object #%d on page #%d has an empty bitmap.\n",
              i + 1, page_num + 1);
      continue;
    }

    char filename[256];
    int chars_formatted = snprintf(filename, sizeof(filename), "%s.%d.%d.png",
                                   pdf_name, page_num, i);
    if (chars_formatted < 0 ||
        static_cast<size_t>(chars_formatted) >= sizeof(filename)) {
      fprintf(stderr, "Filename %s for saving image is too long.\n", filename);
      continue;
    }

    std::vector<uint8_t> png_encoding = EncodeBitmapToPng(std::move(bitmap));
    if (png_encoding.empty()) {
      fprintf(stderr,
              "Failed to convert image object #%d, on page #%d to png.\n",
              i + 1, page_num + 1);
      continue;
    }

    WriteBufferToFile(&png_encoding.front(), png_encoding.size(), filename,
                      "image");
  }
}

void WriteRenderedImages(FPDF_DOCUMENT doc,
                         FPDF_PAGE page,
                         const char* pdf_name,
                         int page_num) {
  for (int i = 0; i < FPDFPage_CountObjects(page); ++i) {
    FPDF_PAGEOBJECT obj = FPDFPage_GetObject(page, i);
    if (FPDFPageObj_GetType(obj) != FPDF_PAGEOBJ_IMAGE)
      continue;

    ScopedFPDFBitmap bitmap(FPDFImageObj_GetRenderedBitmap(doc, page, obj));
    if (!bitmap) {
      fprintf(stderr, "Image object #%d on page #%d has an empty bitmap.\n",
              i + 1, page_num + 1);
      continue;
    }

    char filename[256];
    int chars_formatted = snprintf(filename, sizeof(filename), "%s.%d.%d.png",
                                   pdf_name, page_num, i);
    if (chars_formatted < 0 ||
        static_cast<size_t>(chars_formatted) >= sizeof(filename)) {
      fprintf(stderr, "Filename %s for saving image is too long.\n", filename);
      continue;
    }

    std::vector<uint8_t> png_encoding = EncodeBitmapToPng(std::move(bitmap));
    if (png_encoding.empty()) {
      fprintf(stderr,
              "Failed to convert image object #%d, on page #%d to png.\n",
              i + 1, page_num + 1);
      continue;
    }

    WriteBufferToFile(&png_encoding.front(), png_encoding.size(), filename,
                      "image");
  }
}

void WriteDecodedThumbnailStream(FPDF_PAGE page,
                                 const char* pdf_name,
                                 int page_num) {
  char filename[256];
  if (!GetThumbnailFilename(filename, sizeof(filename), pdf_name, page_num,
                            ThumbnailDecodeType::kDecodedStream)) {
    return;
  }

  unsigned long decoded_data_size =
      FPDFPage_GetDecodedThumbnailData(page, nullptr, 0u);

  // Only continue if there actually is a thumbnail for this page
  if (decoded_data_size == 0) {
    fprintf(stderr, "Failed to get decoded thumbnail for page #%d.\n",
            page_num + 1);
    return;
  }

  std::vector<uint8_t> thumb_buf(decoded_data_size);
  if (FPDFPage_GetDecodedThumbnailData(
          page, thumb_buf.data(), decoded_data_size) != decoded_data_size) {
    fprintf(stderr, "Failed to get decoded thumbnail data for %s.\n", filename);
    return;
  }

  WriteBufferToFile(thumb_buf.data(), decoded_data_size, filename,
                    "decoded thumbnail");
}

void WriteRawThumbnailStream(FPDF_PAGE page,
                             const char* pdf_name,
                             int page_num) {
  char filename[256];
  if (!GetThumbnailFilename(filename, sizeof(filename), pdf_name, page_num,
                            ThumbnailDecodeType::kRawStream)) {
    return;
  }

  unsigned long raw_data_size = FPDFPage_GetRawThumbnailData(page, nullptr, 0u);

  // Only continue if there actually is a thumbnail for this page
  if (raw_data_size == 0) {
    fprintf(stderr, "Failed to get raw thumbnail data for page #%d.\n",
            page_num + 1);
    return;
  }

  std::vector<uint8_t> thumb_buf(raw_data_size);
  if (FPDFPage_GetRawThumbnailData(page, thumb_buf.data(), raw_data_size) !=
      raw_data_size) {
    fprintf(stderr, "Failed to get raw thumbnail data for %s.\n", filename);
    return;
  }

  WriteBufferToFile(thumb_buf.data(), raw_data_size, filename, "raw thumbnail");
}

void WriteThumbnail(FPDF_PAGE page, const char* pdf_name, int page_num) {
  char filename[256];
  if (!GetThumbnailFilename(filename, sizeof(filename), pdf_name, page_num,
                            ThumbnailDecodeType::kBitmap)) {
    return;
  }

  ScopedFPDFBitmap thumb_bitmap(FPDFPage_GetThumbnailAsBitmap(page));
  if (!thumb_bitmap) {
    fprintf(stderr, "Thumbnail of page #%d has an empty bitmap.\n",
            page_num + 1);
    return;
  }

  std::vector<uint8_t> png_encoding =
      EncodeBitmapToPng(std::move(thumb_bitmap));
  if (png_encoding.empty()) {
    fprintf(stderr, "Failed to convert thumbnail of page #%d to png.\n",
            page_num + 1);
    return;
  }

  WriteBufferToFile(&png_encoding.front(), png_encoding.size(), filename,
                    "thumbnail");
}
