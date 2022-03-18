// Copyright 2018 The PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SAMPLES_PDFIUM_TEST_WRITE_HELPER_H_
#define SAMPLES_PDFIUM_TEST_WRITE_HELPER_H_

#include <string>

#include "pdfium/include/fpdfview.h"

// std::string WritePng(const char* pdf_name,
//                      int num,
//                      void* buffer,
//                      int stride,
//                      int width,
//                      int height);
std::string WritePng(const char* out_name,
                     int num,
                     void* buffer,
                     int stride,
                     int width,
                     int height);


void WriteImages(FPDF_PAGE page, const char* pdf_name, int page_num);
void WriteRenderedImages(FPDF_DOCUMENT doc,
                         FPDF_PAGE page,
                         const char* pdf_name,
                         int page_num);
void WriteDecodedThumbnailStream(FPDF_PAGE page,
                                 const char* pdf_name,
                                 int page_num);
void WriteRawThumbnailStream(FPDF_PAGE page,
                             const char* pdf_name,
                             int page_num);
void WriteThumbnail(FPDF_PAGE page, const char* pdf_name, int page_num);

#endif  // SAMPLES_PDFIUM_TEST_WRITE_HELPER_H_
