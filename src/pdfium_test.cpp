// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <functional>
#include <iterator>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <algorithm>

#include "pdfium/include/cpp/fpdf_scopers.h"
#include "pdfium/include/fpdf_annot.h"
#include "pdfium/include/fpdf_attachment.h"
#include "pdfium/include/fpdf_dataavail.h"
#include "pdfium/include/fpdf_edit.h"
#include "pdfium/include/fpdf_ext.h"
#include "pdfium/include/fpdf_formfill.h"
#include "pdfium/include/fpdf_progressive.h"
#include "pdfium/include/fpdf_structtree.h"
#include "pdfium/include/fpdf_text.h"
#include "pdfium/include/fpdfview.h"
//#include "src/pdfium_test_dump_helper.h"
//#include "src/pdfium_test_event_helper.h"
#include "src/pdfium_test_write_helper.h"
// #include "testing/test_loader.h"
// #include "testing/utils/file_util.h"
// #include "testing/utils/hash.h"
// #include "testing/utils/path_service.h"
// #include "third_party/abseil-cpp/absl/types/optional.h"

#include "src/i.h"

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#ifdef ENABLE_CALLGRIND
#include <valgrind/callgrind.h>
#endif // ENABLE_CALLGRIND

#ifdef _WIN32
#define access _access
#define snprintf _snprintf
#define R_OK 4
#endif

// wordexp is a POSIX function that is only available on macOS and non-Android
// Linux platforms.
#if defined(__APPLE__) || (defined(__linux__) && !defined(__ANDROID__))
#define WORDEXP_AVAILABLE
#endif

#ifdef WORDEXP_AVAILABLE
#include <wordexp.h>
#endif // WORDEXP_AVAILABLE

enum class OutputFormat
{
  kNone,
  kPng,
};

namespace
{

  struct Options
  {
    Options() = default;

    bool show_config = false;
    bool use_load_mem_document = false;
    bool render_oneshot = false;
    bool lcd_text = false;
    bool no_nativetext = false;
    bool grayscale = false;
    bool forced_color = false;
    bool fill_to_stroke = false;
    bool limit_cache = false;
    bool force_halftone = false;
    bool printing = false;
    bool no_smoothtext = false;
    bool no_smoothimage = false;
    bool no_smoothpath = false;
    bool reverse_byte_order = false;
    bool save_images = false;
    bool save_rendered_images = false;
    bool save_thumbnails = false;
    bool save_thumbnails_decoded = false;
    bool save_thumbnails_raw = false;
    bool pages = false;
#ifdef ENABLE_CALLGRIND
    bool callgrind_delimiters = false;
#endif
#if defined(__APPLE__) || (defined(__linux__) && !defined(__ANDROID__))
    bool linux_no_system_fonts = false;
#endif
    OutputFormat output_format = OutputFormat::kPng;
    std::string password;
    std::string scale_factor_as_string;
    int first_page = 0; // First 0-based page number to renderer.
    int last_page = 0;  // Last 0-based page number to renderer.
    time_t time = -1;

    std::string width_as_string;
    std::string height_as_string;
    bool maintain_aspect_ratio = false;
    bool allow_enlargement = false;
  };

  int PageRenderFlagsFromOptions(const Options &options)
  {
    int flags = FPDF_ANNOT;
    if (options.lcd_text)
      flags |= FPDF_LCD_TEXT;
    if (options.no_nativetext)
      flags |= FPDF_NO_NATIVETEXT;
    if (options.grayscale)
      flags |= FPDF_GRAYSCALE;
    if (options.fill_to_stroke)
      flags |= FPDF_CONVERT_FILL_TO_STROKE;
    if (options.limit_cache)
      flags |= FPDF_RENDER_LIMITEDIMAGECACHE;
    if (options.force_halftone)
      flags |= FPDF_RENDER_FORCEHALFTONE;
    if (options.printing)
      flags |= FPDF_PRINTING;
    if (options.no_smoothtext)
      flags |= FPDF_RENDER_NO_SMOOTHTEXT;
    if (options.no_smoothimage)
      flags |= FPDF_RENDER_NO_SMOOTHIMAGE;
    if (options.no_smoothpath)
      flags |= FPDF_RENDER_NO_SMOOTHPATH;
    if (options.reverse_byte_order)
      flags |= FPDF_REVERSE_BYTE_ORDER;
    return flags;
  }

  struct FPDF_FORMFILLINFO_PDFiumTest final : public FPDF_FORMFILLINFO
  {
    // Hold a map of the currently loaded pages in order to avoid them
    // to get loaded twice.
    std::map<int, ScopedFPDFPage> loaded_pages;

    // Hold a pointer of FPDF_FORMHANDLE so that PDFium app hooks can
    // make use of it.
    FPDF_FORMHANDLE form_handle;
  };

  FPDF_FORMFILLINFO_PDFiumTest *ToPDFiumTestFormFillInfo(
      FPDF_FORMFILLINFO *form_fill_info)
  {
    return static_cast<FPDF_FORMFILLINFO_PDFiumTest *>(form_fill_info);
  }

#ifdef PDF_ENABLE_XFA
  FPDF_BOOL ExamplePopupMenu(FPDF_FORMFILLINFO *pInfo,
                             FPDF_PAGE page,
                             FPDF_WIDGET always_null,
                             int flags,
                             float x,
                             float y)
  {
    printf("Popup: x=%2.1f, y=%2.1f, flags=0x%x\n", x, y, flags);
    return true;
  }
#endif // PDF_ENABLE_XFA

  void ExampleUnsupportedHandler(UNSUPPORT_INFO *, int type)
  {
    std::string feature = "Unknown";
    switch (type)
    {
    case FPDF_UNSP_DOC_XFAFORM:
      feature = "XFA";
      break;
    case FPDF_UNSP_DOC_PORTABLECOLLECTION:
      feature = "Portfolios_Packages";
      break;
    case FPDF_UNSP_DOC_ATTACHMENT:
    case FPDF_UNSP_ANNOT_ATTACHMENT:
      feature = "Attachment";
      break;
    case FPDF_UNSP_DOC_SECURITY:
      feature = "Rights_Management";
      break;
    case FPDF_UNSP_DOC_SHAREDREVIEW:
      feature = "Shared_Review";
      break;
    case FPDF_UNSP_DOC_SHAREDFORM_ACROBAT:
    case FPDF_UNSP_DOC_SHAREDFORM_FILESYSTEM:
    case FPDF_UNSP_DOC_SHAREDFORM_EMAIL:
      feature = "Shared_Form";
      break;
    case FPDF_UNSP_ANNOT_3DANNOT:
      feature = "3D";
      break;
    case FPDF_UNSP_ANNOT_MOVIE:
      feature = "Movie";
      break;
    case FPDF_UNSP_ANNOT_SOUND:
      feature = "Sound";
      break;
    case FPDF_UNSP_ANNOT_SCREEN_MEDIA:
    case FPDF_UNSP_ANNOT_SCREEN_RICHMEDIA:
      feature = "Screen";
      break;
    case FPDF_UNSP_ANNOT_SIG:
      feature = "Digital_Signature";
      break;
    }
    printf("Unsupported feature: %s.\n", feature.c_str());
  }

  // |arg| is expected to be "--key=value", and |key| is "--key=".
  bool ParseSwitchKeyValue(const std::string &arg,
                           const std::string &key,
                           std::string *value)
  {
    if (arg.size() <= key.size() || arg.compare(0, key.size(), key) != 0)
      return false;

    *value = arg.substr(key.size());
    return true;
  }

  bool ParseCommandLine(const std::vector<std::string> &args,
                        Options *options,
                        std::vector<std::string> *files)
  {
    if (args.empty())
      return false;

    size_t cur_idx = 1;
    std::string value;
    for (; cur_idx < args.size(); ++cur_idx)
    {
      const std::string &cur_arg = args[cur_idx];
      if (cur_arg == "--show-config")
      {
        options->show_config = true;
      }
      else if (cur_arg == "--mem-document")
      {
        options->use_load_mem_document = true;
      }
      else if (cur_arg == "--render-oneshot")
      {
        options->render_oneshot = true;
      }
      else if (cur_arg == "--lcd-text")
      {
        options->lcd_text = true;
      }
      else if (cur_arg == "--no-nativetext")
      {
        options->no_nativetext = true;
      }
      else if (cur_arg == "--grayscale")
      {
        options->grayscale = true;
      }
      else if (cur_arg == "--forced-color")
      {
        options->forced_color = true;
      }
      else if (cur_arg == "--fill-to-stroke")
      {
        options->fill_to_stroke = true;
      }
      else if (cur_arg == "--limit-cache")
      {
        options->limit_cache = true;
      }
      else if (cur_arg == "--force-halftone")
      {
        options->force_halftone = true;
      }
      else if (cur_arg == "--printing")
      {
        options->printing = true;
      }
      else if (cur_arg == "--no-smoothtext")
      {
        options->no_smoothtext = true;
      }
      else if (cur_arg == "--no-smoothimage")
      {
        options->no_smoothimage = true;
      }
      else if (cur_arg == "--no-smoothpath")
      {
        options->no_smoothpath = true;
      }
      else if (cur_arg == "--reverse-byte-order")
      {
        options->reverse_byte_order = true;
      }
      else if (cur_arg == "--save-images")
      {
        if (options->save_rendered_images)
        {
          fprintf(stderr,
                  "--save-rendered-images conflicts with --save-images\n");
          return false;
        }
        options->save_images = true;
      }
      else if (cur_arg == "--save-rendered-images")
      {
        if (options->save_images)
        {
          fprintf(stderr,
                  "--save-images conflicts with --save-rendered-images\n");
          return false;
        }
        options->save_rendered_images = true;
      }
      else if (cur_arg == "--save-thumbs")
      {
        options->save_thumbnails = true;
      }
      else if (cur_arg == "--save-thumbs-dec")
      {
        options->save_thumbnails_decoded = true;
      }
      else if (cur_arg == "--save-thumbs-raw")
      {
        options->save_thumbnails_raw = true;
#ifdef ENABLE_CALLGRIND
      }
      else if (cur_arg == "--callgrind-delim")
      {
        options->callgrind_delimiters = true;
#endif
#if defined(__APPLE__) || (defined(__linux__) && !defined(__ANDROID__))
      }
      else if (cur_arg == "--no-system-fonts")
      {
        options->linux_no_system_fonts = true;
#endif
      }
      else if (cur_arg == "--png")
      {
        if (options->output_format != OutputFormat::kNone)
        {
          fprintf(stderr, "Duplicate or conflicting --png argument\n");
          return false;
        }
        options->output_format = OutputFormat::kPng;
      }
      else if (cur_arg == "--maintain-aspect-ratio")
      {
        options->maintain_aspect_ratio = true;
      }
      else if (cur_arg == "--allow-enlargement")
      {
        options->allow_enlargement = true;
      }
      else if (ParseSwitchKeyValue(cur_arg, "--password=", &value))
      {
        if (!options->password.empty())
        {
          fprintf(stderr, "Duplicate --password argument\n");
          return false;
        }
        options->password = value;
      }
      else if (ParseSwitchKeyValue(cur_arg, "--scale=", &value))
      {
        if (!options->scale_factor_as_string.empty())
        {
          fprintf(stderr, "Duplicate --scale argument\n");
          return false;
        }
        options->scale_factor_as_string = value;
      }
      else if (ParseSwitchKeyValue(cur_arg, "--pages=", &value) || ParseSwitchKeyValue(cur_arg, "--page=", &value))
      {
        if (options->pages)
        {
          fprintf(stderr, "Duplicate --pages argument\n");
          return false;
        }
        options->pages = true;
        const std::string pages_string = value;
        size_t first_dash = pages_string.find('-');
        if (first_dash == std::string::npos)
        {
          std::stringstream(pages_string) >> options->first_page;
          options->last_page = options->first_page;
        }
        else
        {
          std::stringstream(pages_string.substr(0, first_dash)) >>
              options->first_page;
          std::stringstream(pages_string.substr(first_dash + 1)) >>
              options->last_page;
        }
      }
      else if (ParseSwitchKeyValue(cur_arg, "--time=", &value))
      {
        if (options->time > -1)
        {
          fprintf(stderr, "Duplicate --time argument\n");
          return false;
        }
        const std::string time_string = value;
        std::stringstream(time_string) >> options->time;
        if (options->time < 0)
        {
          fprintf(stderr, "Invalid --time argument, must be non-negative\n");
          return false;
        }
      }
      else if (ParseSwitchKeyValue(cur_arg, "--width=", &value))
      {
        if (!options->width_as_string.empty())
        {
          fprintf(stderr, "Duplicate --width argument\n");
          return false;
        }
        options->width_as_string = value;
      }
      else if (ParseSwitchKeyValue(cur_arg, "--height=", &value))
      {
        if (!options->height_as_string.empty())
        {
          fprintf(stderr, "Duplicate --height argument\n");
          return false;
        }
        options->height_as_string = value;
      }
      else if (cur_arg.size() >= 2 && cur_arg[0] == '-' && cur_arg[1] == '-')
      {
        fprintf(stderr, "Unrecognized argument %s\n", cur_arg.c_str());
        return false;
      }
      else
      {
        break;
      }
    }
    for (size_t i = cur_idx; i < args.size(); i++)
      files->push_back(args[i]);

    return true;
  }

  void PrintLastError()
  {
    unsigned long err = FPDF_GetLastError();
    fprintf(stderr, "Load pdf docs unsuccessful: ");
    switch (err)
    {
    case FPDF_ERR_SUCCESS:
      fprintf(stderr, "Success");
      break;
    case FPDF_ERR_UNKNOWN:
      fprintf(stderr, "Unknown error");
      break;
    case FPDF_ERR_FILE:
      fprintf(stderr, "File not found or could not be opened");
      break;
    case FPDF_ERR_FORMAT:
      fprintf(stderr, "File not in PDF format or corrupted");
      break;
    case FPDF_ERR_PASSWORD:
      fprintf(stderr, "Password required or incorrect password");
      break;
    case FPDF_ERR_SECURITY:
      fprintf(stderr, "Unsupported security scheme");
      break;
    case FPDF_ERR_PAGE:
      fprintf(stderr, "Page not found or content error");
      break;
    default:
      fprintf(stderr, "Unknown error %ld", err);
    }
    fprintf(stderr, ".\n");
  }

  FPDF_BOOL Is_Data_Avail(FX_FILEAVAIL *avail, size_t offset, size_t size)
  {
    return true;
  }

  void Add_Segment(FX_DOWNLOADHINTS *hints, size_t offset, size_t size) {}

  FPDF_PAGE GetPageForIndex(FPDF_FORMFILLINFO *param,
                            FPDF_DOCUMENT doc,
                            int index)
  {
    FPDF_FORMFILLINFO_PDFiumTest *form_fill_info =
        ToPDFiumTestFormFillInfo(param);
    auto &loaded_pages = form_fill_info->loaded_pages;
    auto iter = loaded_pages.find(index);
    if (iter != loaded_pages.end())
      return iter->second.get();

    ScopedFPDFPage page(FPDF_LoadPage(doc, index));
    if (!page)
      return nullptr;

    // Mark the page as loaded first to prevent infinite recursion.
    FPDF_PAGE page_ptr = page.get();
    loaded_pages[index] = std::move(page);

    FPDF_FORMHANDLE &form_handle = form_fill_info->form_handle;
    FORM_OnAfterLoadPage(page_ptr, form_handle);
    FORM_DoPageAAction(page_ptr, form_handle, FPDFPAGE_AACTION_OPEN);
    return page_ptr;
  }

  // Note, for a client using progressive rendering you'd want to determine if you
  // need the rendering to pause instead of always saying |true|. This is for
  // testing to force the renderer to break whenever possible.
  FPDF_BOOL NeedToPauseNow(IFSDK_PAUSE *p)
  {
    return true;
  }

  bool ProcessPage(const std::string &name,
                   const std::string &out_name,
                   FPDF_DOCUMENT doc,
                   FPDF_FORMHANDLE form,
                   FPDF_FORMFILLINFO_PDFiumTest *form_fill_info,
                   const int page_index,
                   const Options &options,
                   const std::function<void()> &idler,
                   bool single_page)
  {
    FPDF_PAGE page = GetPageForIndex(form_fill_info, doc, page_index);
    if (!page)
      return false;
    if (options.save_images)
      WriteImages(page, name.c_str(), page_index);
    if (options.save_rendered_images)
      WriteRenderedImages(doc, page, name.c_str(), page_index);
    if (options.save_thumbnails)
      WriteThumbnail(page, name.c_str(), page_index);
    if (options.save_thumbnails_decoded)
      WriteDecodedThumbnailStream(page, name.c_str(), page_index);
    if (options.save_thumbnails_raw)
      WriteRawThumbnailStream(page, name.c_str(), page_index);

    ScopedFPDFTextPage text_page(FPDFText_LoadPage(page));
    double scale = 1.0;
    if (!options.scale_factor_as_string.empty())
      std::stringstream(options.scale_factor_as_string) >> scale;

    auto render_width = static_cast<int>(FPDF_GetPageWidthF(page) * scale);
    auto render_height = static_cast<int>(FPDF_GetPageHeightF(page) * scale);

    auto image_width = render_width;
    auto image_height = render_height;

    int setting_width = -1;
    if (!options.width_as_string.empty())
      std::stringstream(options.width_as_string) >> setting_width;
    int setting_height = -1;
    if (!options.height_as_string.empty())
      std::stringstream(options.height_as_string) >> setting_height;

    if (setting_height > 0 || setting_width > 0)
    {
      int calc_aspect_height = render_height * setting_width / render_width;
      int calc_aspect_width = render_width * setting_height / render_height;

      if (setting_height < 0)
      {
        setting_height = calc_aspect_height;
      }
      else if (setting_width < 0)
      {
        setting_width = calc_aspect_width;
      }

      if (options.maintain_aspect_ratio)
      {
        if (options.allow_enlargement)
        {
          render_height = std::max(setting_height, calc_aspect_height);
          render_width = std::max(setting_width, calc_aspect_width);
          image_height = render_height;
          image_width = render_width;
        }
        else
        {
          render_height = std::min(setting_height, calc_aspect_height);
          render_width = std::min(setting_width, calc_aspect_width);
          image_height = setting_height;
          image_width = setting_width;
        }
      }
      else
      {
        render_width = setting_width;
        render_height = setting_height;
        image_width = setting_width;
        image_height = setting_height;
      }
    }

    int alpha = FPDFPage_HasTransparency(page) ? 1 : 0;
    ScopedFPDFBitmap bitmap(FPDFBitmap_Create(image_width, image_height, alpha));

    if (bitmap)
    {
      FPDF_DWORD fill_color = alpha ? 0x00000000 : 0xFFFFFFFF;
      FPDFBitmap_FillRect(bitmap.get(), 0, 0, image_width, image_height, fill_color);

      int flags = PageRenderFlagsFromOptions(options);
      if (options.render_oneshot)
      {
        // Note, client programs probably want to use this method instead of the
        // progressive calls. The progressive calls are if you need to pause the
        // rendering to update the UI, the PDF renderer will break when possible.
        FPDF_RenderPageBitmap(bitmap.get(), page, 0, 0, render_width, render_height, 0, flags);
      }
      else
      {
        IFSDK_PAUSE pause;
        pause.version = 1;
        pause.NeedToPauseNow = &NeedToPauseNow;

        // Client programs will be setting these values when rendering.
        // This is a sample color scheme with distinct colors.
        // Used only when |options.forced_color| is true.
        const FPDF_COLORSCHEME color_scheme{
            /*path_fill_color=*/0xFFFF0000, /*path_stroke_color=*/0xFF00FF00,
            /*text_fill_color=*/0xFF0000FF, /*text_stroke_color=*/0xFF00FFFF};

        int rv = FPDF_RenderPageBitmapWithColorScheme_Start(
            bitmap.get(), page, 0, 0, render_width, render_height, 0, flags,
            options.forced_color ? &color_scheme : nullptr, &pause);
        while (rv == FPDF_RENDER_TOBECONTINUED)
          rv = FPDF_RenderPage_Continue(page, &pause);
      }

      FPDF_FFLDraw(form, bitmap.get(), page, 0, 0, render_width, render_height, 0, flags);
      idler();

      if (!options.render_oneshot)
      {
        FPDF_RenderPage_Close(page);
        idler();
      }

      int stride = FPDFBitmap_GetStride(bitmap.get());
      void *buffer = FPDFBitmap_GetBuffer(bitmap.get());

      std::string image_file_name;

      switch (options.output_format)
      {

      case OutputFormat::kPng:
      {
        size_t extension_pos = out_name.find(".png");
        if (extension_pos == std::string::npos)
        {
          extension_pos = out_name.size();
        }
        image_file_name =
            WritePng(out_name.substr(0, extension_pos).c_str(), single_page ? -1 : page_index, buffer, stride, image_width, image_height);
        break;
      }
      default:
        break;
      }
    }
    else
    {
      fprintf(stderr, "Page was too large to be rendered.\n");
    }

    FORM_DoPageAAction(page, form, FPDFPAGE_AACTION_CLOSE);
    idler();

    FORM_OnBeforeClosePage(page, form);
    idler();

    return !!bitmap;
  }

  void ProcessPdf(const std::string &name,
                  const std::string &out_name,
                  const char *buf,
                  size_t len,
                  const Options &options,
                  const std::function<void()> &idler)
  {
    TestLoader loader({buf, len});

    FPDF_FILEACCESS file_access = {};
    file_access.m_FileLen = static_cast<unsigned long>(len);
    file_access.m_GetBlock = TestLoader::GetBlock;
    file_access.m_Param = &loader;

    FX_FILEAVAIL file_avail = {};
    file_avail.version = 1;
    file_avail.IsDataAvail = Is_Data_Avail;

    FX_DOWNLOADHINTS hints = {};
    hints.version = 1;
    hints.AddSegment = Add_Segment;

    // |pdf_avail| must outlive |doc|.
    ScopedFPDFAvail pdf_avail(FPDFAvail_Create(&file_avail, &file_access));

    // |doc| must outlive |form_callbacks.loaded_pages|.
    ScopedFPDFDocument doc;

    const char *password =
        options.password.empty() ? nullptr : options.password.c_str();
    bool is_linearized = false;
    if (options.use_load_mem_document)
    {
      doc.reset(FPDF_LoadMemDocument(buf, len, password));
    }
    else
    {
      if (FPDFAvail_IsLinearized(pdf_avail.get()) == PDF_LINEARIZED)
      {
        int avail_status = PDF_DATA_NOTAVAIL;
        doc.reset(FPDFAvail_GetDocument(pdf_avail.get(), password));
        if (doc)
        {
          while (avail_status == PDF_DATA_NOTAVAIL)
            avail_status = FPDFAvail_IsDocAvail(pdf_avail.get(), &hints);

          if (avail_status == PDF_DATA_ERROR)
          {
            fprintf(stderr, "Unknown error in checking if doc was available.\n");
            return;
          }
          avail_status = FPDFAvail_IsFormAvail(pdf_avail.get(), &hints);
          if (avail_status == PDF_FORM_ERROR ||
              avail_status == PDF_FORM_NOTAVAIL)
          {
            fprintf(stderr,
                    "Error %d was returned in checking if form was available.\n",
                    avail_status);
            return;
          }
          is_linearized = true;
        }
      }
      else
      {
        doc.reset(FPDF_LoadCustomDocument(&file_access, password));
      }
    }

    if (!doc)
    {
      PrintLastError();
      return;
    }

    if (!FPDF_DocumentHasValidCrossReferenceTable(doc.get()))
      fprintf(stderr, "Document has invalid cross reference table\n");

    (void)FPDF_GetDocPermissions(doc.get());

    FPDF_FORMFILLINFO_PDFiumTest form_callbacks = {};
#ifdef PDF_ENABLE_XFA
    form_callbacks.version = 2;
    form_callbacks.xfa_disabled =
        options.disable_xfa || options.disable_javascript;
    form_callbacks.FFI_PopupMenu = ExamplePopupMenu;
#else  // PDF_ENABLE_XFA
    form_callbacks.version = 1;
#endif // PDF_ENABLE_XFA
    form_callbacks.FFI_GetPage = GetPageForIndex;

    ScopedFPDFFormHandle form(
        FPDFDOC_InitFormFillEnvironment(doc.get(), &form_callbacks));
    form_callbacks.form_handle = form.get();

#ifdef PDF_ENABLE_XFA
    if (!options.disable_xfa && !options.disable_javascript)
    {
      int doc_type = FPDF_GetFormType(doc.get());
      if (doc_type == FORMTYPE_XFA_FULL || doc_type == FORMTYPE_XFA_FOREGROUND)
      {
        if (!FPDF_LoadXFA(doc.get()))
          fprintf(stderr, "LoadXFA unsuccessful, continuing anyway.\n");
      }
    }
#endif // PDF_ENABLE_XFA

    FPDF_SetFormFieldHighlightColor(form.get(), FPDF_FORMFIELD_UNKNOWN, 0xFFE4DD);
    FPDF_SetFormFieldHighlightAlpha(form.get(), 100);
    FORM_DoDocumentJSAction(form.get());
    FORM_DoDocumentOpenAction(form.get());

#if _WIN32
    if (options.output_format == OutputFormat::kPs2)
      FPDF_SetPrintMode(FPDF_PRINTMODE_POSTSCRIPT2);
    else if (options.output_format == OutputFormat::kPs3)
      FPDF_SetPrintMode(FPDF_PRINTMODE_POSTSCRIPT3);
    else if (options.output_format == OutputFormat::kPs3Type42)
      FPDF_SetPrintMode(FPDF_PRINTMODE_POSTSCRIPT3_TYPE42);
#endif

    int page_count = FPDF_GetPageCount(doc.get());
    int processed_pages = 0;
    int bad_pages = 0;
    int first_page = options.pages ? options.first_page : 0;
    int last_page = options.pages ? options.last_page + 1 : page_count;
    bool single_page = first_page == last_page - 1;
    for (int i = first_page; i < last_page; ++i)
    {
      if (is_linearized)
      {
        int avail_status = PDF_DATA_NOTAVAIL;
        while (avail_status == PDF_DATA_NOTAVAIL)
          avail_status = FPDFAvail_IsPageAvail(pdf_avail.get(), i, &hints);

        if (avail_status == PDF_DATA_ERROR)
        {
          fprintf(stderr, "Unknown error in checking if page %d is available.\n",
                  i);
          return;
        }
      }
      if (ProcessPage(name, out_name, doc.get(), form.get(), &form_callbacks, i, options,
                      idler, single_page))
      {
        ++processed_pages;
      }
      else
      {
        ++bad_pages;
      }
      idler();
    }

    FORM_DoDocumentAAction(form.get(), FPDFDOC_AACTION_WC);
    idler();

    fprintf(stderr, "Processed %d pages.\n", processed_pages);
    if (bad_pages)
      fprintf(stderr, "Skipped %d bad pages.\n", bad_pages);
  }

  void ShowConfig()
  {
    std::string config;
    std::string maybe_comma;
#ifdef V8_USE_EXTERNAL_STARTUP_DATA
    config.append(maybe_comma);
    config.append("V8_EXTERNAL");
    maybe_comma = ",";
#endif // V8_USE_EXTERNAL_STARTUP_DATA
#ifdef PDF_ENABLE_XFA
    config.append(maybe_comma);
    config.append("XFA");
    maybe_comma = ",";
#endif // PDF_ENABLE_XFA
#ifdef PDF_ENABLE_ASAN
    config.append(maybe_comma);
    config.append("ASAN");
    maybe_comma = ",";
#endif // PDF_ENABLE_ASAN
    printf("%s\n", config.c_str());
  }

  constexpr char kUsageString[] =
      "Usage: pdfium_test [OPTION] [INPUT FILE] [OUTPUT FILE]\n"
      "  --show-config          - print build options and exit\n"
      "  --mem-document         - load document with FPDF_LoadMemDocument()\n"
      "  --render-oneshot       - render image without using progressive "
      "renderer\n"
      "  --lcd-text             - render text optimized for LCD displays\n"
      "  --no-nativetext        - render without using the native text output\n"
      "  --grayscale            - render grayscale output\n"
      "  --forced-color         - render in forced color mode\n"
      "  --fill-to-stroke       - render fill as stroke in forced color mode\n"
      "  --limit-cache          - render limiting image cache size\n"
      "  --force-halftone       - render forcing halftone\n"
      "  --printing             - render as if for printing\n"
      "  --no-smoothtext        - render disabling text anti-aliasing\n"
      "  --no-smoothimage       - render disabling image anti-alisasing\n"
      "  --no-smoothpath        - render disabling path anti-aliasing\n"
      "  --reverse-byte-order   - render to BGRA, if supported by the output "
      "format\n"
      "  --save-images          - write raw embedded images "
      "<pdf-name>.<page-number>.<object-number>.png\n"
      "  --save-rendered-images - write embedded images as rendered on the page "
      "<pdf-name>.<page-number>.<object-number>.png\n"
      "  --save-thumbs          - write page thumbnails "
      "<pdf-name>.thumbnail.<page-number>.png\n"
      "  --save-thumbs-dec      - write page thumbnails' decoded stream data"
      "<pdf-name>.thumbnail.decoded.<page-number>.png\n"
      "  --save-thumbs-raw      - write page thumbnails' raw stream data"
      "<pdf-name>.thumbnail.raw.<page-number>.png\n"
#ifdef ENABLE_CALLGRIND
      "  --callgrind-delim      - delimit interesting section when using "
      "callgrind\n"
#endif
#if defined(__APPLE__) || (defined(__linux__) && !defined(__ANDROID__))
      "  --no-system-fonts      - do not use system fonts, overrides --font-dir\n"
#endif
      "  --scale=<number>       - scale output size by number (e.g. 0.5)\n"
      "  --password=<secret>    - password to decrypt the PDF with\n"
      "  --pages=<number>(-<number>) - only render the given 0-based page(s)\n"
      "  --png   - write page images <pdf-name>.<page-number>.png\n"
      "  --time=<number> - Seconds since the epoch to set system time.\n"
      "  --width=<width>          - override page width in pixels\n"
      "  --height=<height>        - override page height in pixels\n"
      "  --maintain-aspect-ratio  - Maintain aspect ratio when resizing the page\n"
      "  --allow-enlargement      - When maintaining the aspect ratio, allow one parameter to exceed the given height/width\n"
      "  --page=<number>(-<number>)- 0-based page number to be converted (default 0, alias of --pages)\n"
      "";

} // namespace

int main(int argc, const char *argv[])
{
  std::vector<std::string> args(argv, argv + argc);
  Options options;
  std::vector<std::string> files;
  if (!ParseCommandLine(args, &options, &files))
  {
    fprintf(stderr, "%s", kUsageString);
    return 1;
  }

  if (options.show_config)
  {
    ShowConfig();
    return 0;
  }

  if (files.size() != 2)
  {
    fprintf(stderr, "Please specify one input file and one output file.\n");
    return 1;
  }

  FPDF_LIBRARY_CONFIG config;
  config.version = 3;
  config.m_pUserFontPaths = nullptr;
  config.m_pIsolate = nullptr;
  config.m_v8EmbedderSlot = 0;
  config.m_pPlatform = nullptr;

  std::function<void()> idler = []() {};

  const char *path_array[2] = {nullptr, nullptr};

  FPDF_InitLibraryWithConfig(&config);

  UNSUPPORT_INFO unsupported_info = {};
  unsupported_info.version = 1;
  unsupported_info.FSDK_UnSupport_Handler = ExampleUnsupportedHandler;

  FSDK_SetUnSpObjProcessHandler(&unsupported_info);

  if (options.time > -1)
  {
    // This must be a static var to avoid explicit capture, so the lambda can be
    // converted to a function ptr.
    static time_t time_ret = options.time;
    FSDK_SetTimeFunction([]()
                         { return time_ret; });
    FSDK_SetLocaltimeFunction([](const time_t *tp)
                              { return gmtime(tp); });
  }

  const std::string &filename = files[0];
  const std::string &out_filename = files[1];

  size_t file_length = 0;
  std::unique_ptr<char, pdfium::FreeDeleter> file_contents =
      GetFileContents(filename.c_str(), &file_length);
  if (!file_contents)
    return -1;
  fprintf(stderr, "Processing PDF file %s.\n", filename.c_str());

#ifdef ENABLE_CALLGRIND
  if (options.callgrind_delimiters)
    CALLGRIND_START_INSTRUMENTATION;
#endif // ENABLE_CALLGRIND

  ProcessPdf(filename, out_filename, file_contents.get(), file_length, options,
             idler);
  idler();

#ifdef ENABLE_CALLGRIND
  if (options.callgrind_delimiters)
    CALLGRIND_STOP_INSTRUMENTATION;
#endif // ENABLE_CALLGRIND

  FPDF_DestroyLibrary();

  return 0;
}
