#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#define byte BYTE
#include <gdiplus.h>
#undef byte
#pragma comment(lib, "gdiplus.lib")

#include "IllustratorSDK.h"

#include "Plugin.hpp"
#include "SDKAboutPluginsHelper.h"
#include "SDKDef.h"
#include "Suites.hpp"
#include "ATETextSuitesImportHelper.h"

#include "AIDocument.h"
#include "AIDocumentView.h"
#include "AIFileFormat.h"
#include "AIArtboard.h"
#include "AILayer.h"
#include "AIPathStyle.h"
#include "AIRealMath.h"
#include "AITextFrame.h"
#include "AITransformArt.h"

#include "../core/DxfParser.h"
#include "../core/RenderPlan.h"
#include "ModaDxfImportFilter.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

extern "C" {
AIUnicodeStringSuite* sAIUnicodeString = nullptr;
SPBlocksSuite* sSPBlocks = nullptr;
AIFileFormatSuite* sAIFileFormat = nullptr;
AIDocumentSuite* sAIDocument = nullptr;
AIDocumentViewSuite* sAIDocumentView = nullptr;
AITextFrameSuite* sAITextFrame = nullptr;
AIArtSuite* sAIArt = nullptr;
AIPathSuite* sAIPath = nullptr;
AIPathStyleSuite* sAIPathStyle = nullptr;
AILayerSuite* sAILayer = nullptr;
AIMdMemorySuite* sAIMdMemory = nullptr;
AIArtboardSuite* sAIArtboard = nullptr;
AIRealMathSuite* sAIRealMath = nullptr;
AITransformArtSuite* sAITransformArt = nullptr;
EXTERN_TEXT_SUITES
}

ImportSuite gImportSuites[] = {
    kAIUnicodeStringSuite, kAIUnicodeStringSuiteVersion, &sAIUnicodeString,
    kSPBlocksSuite, kSPBlocksSuiteVersion, &sSPBlocks,
    kAIFileFormatSuite, kAIFileFormatVersion, &sAIFileFormat,
    kAIDocumentSuite, kAIDocumentVersion, &sAIDocument,
    kAIDocumentViewSuite, kAIDocumentViewVersion, &sAIDocumentView,
    kAITextFrameSuite, kAITextFrameVersion, &sAITextFrame,
    kAIArtSuite, kAIArtSuiteVersion, &sAIArt,
    kAIPathSuite, kAIPathVersion, &sAIPath,
    kAIPathStyleSuite, kAIPathStyleSuiteVersion, &sAIPathStyle,
    kAILayerSuite, kAILayerVersion, &sAILayer,
    kAIMdMemorySuite, kAIMdMemorySuiteVersion, &sAIMdMemory,
    kAIArtboardSuite, kAIArtboardSuiteVersion, &sAIArtboard,
    kAIRealMathSuite, kAIRealMathSuiteVersion, &sAIRealMath,
    kAITransformArtSuite, kAITransformArtSuiteVersion, &sAITransformArt,
    IMPORT_TEXT_SUITES
    nullptr, 0, nullptr};

namespace {

constexpr char kPluginName[] = "ModaDXF";
constexpr char kFormatName[] = "ModaDXF Modaris DXF";
constexpr double kDegreesToRadians = 3.14159265358979323846 / 180.0;
constexpr double kDefaultScalePercent = 100.0;
constexpr double kDefaultUnitScaleValue = 1.0;

enum class DxfScaleMode {
  Original,
  FitArtboard,
  Percent
};

enum class DxfScaleUnit {
  Points,
  Picas,
  Inches,
  Millimeters,
  Centimeters,
  Pixels,
  Feet
};

struct DxfImportOptions {
  DxfScaleMode scaleMode = DxfScaleMode::FitArtboard;
  double percent = kDefaultScalePercent;
  double unitScaleSource = kDefaultUnitScaleValue;
  double unitScaleTarget = kDefaultUnitScaleValue;
  DxfScaleUnit unit = DxfScaleUnit::Points;
  bool scaleLineWeight = false;
  bool mergeLayers = false;
  bool centerArt = true;
};

const wchar_t* UnitLabel(DxfScaleUnit unit) {
  switch (unit) {
    case DxfScaleUnit::Picas:
      return L"Picas";
    case DxfScaleUnit::Inches:
      return L"Polegadas";
    case DxfScaleUnit::Millimeters:
      return L"Mil\u00EDmetros";
    case DxfScaleUnit::Centimeters:
      return L"Cent\u00EDmetros";
    case DxfScaleUnit::Pixels:
      return L"Pixels";
    case DxfScaleUnit::Feet:
      return L"P\u00E9s";
    case DxfScaleUnit::Points:
    default:
      return L"Pontos";
  }
}

const char* UnitLogName(DxfScaleUnit unit) {
  switch (unit) {
    case DxfScaleUnit::Picas:
      return "picas";
    case DxfScaleUnit::Inches:
      return "inches";
    case DxfScaleUnit::Millimeters:
      return "millimeters";
    case DxfScaleUnit::Centimeters:
      return "centimeters";
    case DxfScaleUnit::Pixels:
      return "pixels";
    case DxfScaleUnit::Feet:
      return "feet";
    case DxfScaleUnit::Points:
    default:
      return "points";
  }
}

double PointsPerUnit(DxfScaleUnit unit) {
  switch (unit) {
    case DxfScaleUnit::Picas:
      return 12.0;
    case DxfScaleUnit::Inches:
      return 72.0;
    case DxfScaleUnit::Millimeters:
      return 72.0 / 25.4;
    case DxfScaleUnit::Centimeters:
      return 72.0 / 2.54;
    case DxfScaleUnit::Pixels:
      return 1.0;
    case DxfScaleUnit::Feet:
      return 864.0;
    case DxfScaleUnit::Points:
    default:
      return 1.0;
  }
}

DxfScaleUnit UnitFromIndex(int index) {
  switch (index) {
    case 1:
      return DxfScaleUnit::Picas;
    case 2:
      return DxfScaleUnit::Inches;
    case 3:
      return DxfScaleUnit::Millimeters;
    case 4:
      return DxfScaleUnit::Centimeters;
    case 5:
      return DxfScaleUnit::Pixels;
    case 6:
      return DxfScaleUnit::Feet;
    case 0:
    default:
      return DxfScaleUnit::Points;
  }
}

int UnitIndex(DxfScaleUnit unit) {
  switch (unit) {
    case DxfScaleUnit::Picas:
      return 1;
    case DxfScaleUnit::Inches:
      return 2;
    case DxfScaleUnit::Millimeters:
      return 3;
    case DxfScaleUnit::Centimeters:
      return 4;
    case DxfScaleUnit::Pixels:
      return 5;
    case DxfScaleUnit::Feet:
      return 6;
    case DxfScaleUnit::Points:
    default:
      return 0;
  }
}

double UnitScaleFactorToPoints(const DxfImportOptions& options) {
  if (!std::isfinite(options.unitScaleSource) || !std::isfinite(options.unitScaleTarget) ||
      options.unitScaleSource <= 0.0 || options.unitScaleTarget <= 0.0) {
    return 1.0;
  }

  const double factor = (options.unitScaleTarget / options.unitScaleSource) * PointsPerUnit(options.unit);
  return std::isfinite(factor) && factor > 0.0 ? factor : 1.0;
}

class GdiPlusSession {
 public:
  static void EnsureStarted() {
    static GdiPlusSession session;
  }

 private:
  GdiPlusSession() {
    Gdiplus::GdiplusStartupInput input;
    if (Gdiplus::GdiplusStartup(&token_, &input, nullptr) != Gdiplus::Ok) {
      token_ = 0;
    }
  }

  ~GdiPlusSession() {
    if (token_) {
      Gdiplus::GdiplusShutdown(token_);
    }
  }

  ULONG_PTR token_ = 0;
};

Gdiplus::Color GdiColor(COLORREF color, BYTE alpha = 255) {
  return Gdiplus::Color(alpha, GetRValue(color), GetGValue(color), GetBValue(color));
}

RECT MakeRect(int left, int top, int right, int bottom) {
  RECT rect{left, top, right, bottom};
  return rect;
}

Gdiplus::RectF GdiRectF(const RECT& rect, float inset = 0.0f) {
  return Gdiplus::RectF(static_cast<float>(rect.left) + inset, static_cast<float>(rect.top) + inset,
                        static_cast<float>(rect.right - rect.left) - (inset * 2.0f),
                        static_cast<float>(rect.bottom - rect.top) - (inset * 2.0f));
}

void AddRoundedRectPath(Gdiplus::GraphicsPath* path, const RECT& rect, float radius, float inset = 0.5f) {
  if (!path) {
    return;
  }
  Gdiplus::RectF bounds = GdiRectF(rect, inset);
  if (bounds.Width <= 0.0f || bounds.Height <= 0.0f) {
    return;
  }

  radius = std::min(radius, std::min(bounds.Width, bounds.Height) / 2.0f);
  const float diameter = radius * 2.0f;
  path->AddArc(bounds.X, bounds.Y, diameter, diameter, 180.0f, 90.0f);
  path->AddArc(bounds.X + bounds.Width - diameter, bounds.Y, diameter, diameter, 270.0f, 90.0f);
  path->AddArc(bounds.X + bounds.Width - diameter, bounds.Y + bounds.Height - diameter, diameter, diameter, 0.0f,
               90.0f);
  path->AddArc(bounds.X, bounds.Y + bounds.Height - diameter, diameter, diameter, 90.0f, 90.0f);
  path->CloseFigure();
}

void DrawRoundedFill(HDC hdc, const RECT& rect, COLORREF fill, COLORREF border, float radius,
                     float borderWidth = 1.0f) {
  GdiPlusSession::EnsureStarted();
  Gdiplus::Graphics graphics(hdc);
  graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
  graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);

  Gdiplus::GraphicsPath path;
  AddRoundedRectPath(&path, rect, radius);
  Gdiplus::SolidBrush brush(GdiColor(fill));
  graphics.FillPath(&brush, &path);
  if (borderWidth > 0.0f) {
    Gdiplus::Pen pen(GdiColor(border), borderWidth);
    graphics.DrawPath(&pen, &path);
  }
}

void DrawSoftLine(HDC hdc, int x1, int y1, int x2, int y2, COLORREF color, float width = 1.0f) {
  GdiPlusSession::EnsureStarted();
  Gdiplus::Graphics graphics(hdc);
  graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
  Gdiplus::Pen pen(GdiColor(color), width);
  pen.SetStartCap(Gdiplus::LineCapRound);
  pen.SetEndCap(Gdiplus::LineCapRound);
  graphics.DrawLine(&pen, static_cast<Gdiplus::REAL>(x1), static_cast<Gdiplus::REAL>(y1),
                    static_cast<Gdiplus::REAL>(x2), static_cast<Gdiplus::REAL>(y2));
}

void DrawSoftEllipse(HDC hdc, const RECT& rect, COLORREF fill, COLORREF border, float borderWidth = 1.0f) {
  GdiPlusSession::EnsureStarted();
  Gdiplus::Graphics graphics(hdc);
  graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
  graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
  Gdiplus::RectF ellipse = GdiRectF(rect, 0.5f);
  Gdiplus::SolidBrush brush(GdiColor(fill));
  Gdiplus::Pen pen(GdiColor(border), borderWidth);
  graphics.FillEllipse(&brush, ellipse);
  graphics.DrawEllipse(&pen, ellipse);
}

bool IsUsableOwnerWindow(HWND hwnd) {
  if (!hwnd || !IsWindow(hwnd) || !IsWindowVisible(hwnd) || IsIconic(hwnd)) {
    return false;
  }
  DWORD windowProcessId = 0;
  GetWindowThreadProcessId(hwnd, &windowProcessId);
  return windowProcessId == GetCurrentProcessId();
}

HWND RootOwnerIfUsable(HWND hwnd) {
  if (!hwnd) {
    return nullptr;
  }
  HWND root = GetAncestor(hwnd, GA_ROOT);
  return IsUsableOwnerWindow(root) ? root : nullptr;
}

struct OwnerWindowSearch {
  HWND illustrator = nullptr;
  HWND fallback = nullptr;
};

BOOL CALLBACK FindProcessWindowProc(HWND hwnd, LPARAM lParam) {
  auto* search = reinterpret_cast<OwnerWindowSearch*>(lParam);
  if (!IsUsableOwnerWindow(hwnd) || GetWindow(hwnd, GW_OWNER)) {
    return TRUE;
  }

  wchar_t title[256]{};
  wchar_t className[128]{};
  GetWindowTextW(hwnd, title, static_cast<int>(sizeof(title) / sizeof(title[0])));
  GetClassNameW(hwnd, className, static_cast<int>(sizeof(className) / sizeof(className[0])));
  const std::wstring titleText(title);
  const std::wstring classText(className);
  if (titleText.find(L"Illustrator") != std::wstring::npos ||
      classText.find(L"Illustrator") != std::wstring::npos) {
    search->illustrator = hwnd;
    return FALSE;
  }
  if (!search->fallback) {
    search->fallback = hwnd;
  }
  return TRUE;
}

HWND FindIllustratorOwnerWindow() {
  if (HWND active = RootOwnerIfUsable(GetActiveWindow())) {
    return active;
  }
  if (HWND foreground = RootOwnerIfUsable(GetForegroundWindow())) {
    return foreground;
  }

  OwnerWindowSearch search;
  EnumWindows(FindProcessWindowProc, reinterpret_cast<LPARAM>(&search));
  return search.illustrator ? search.illustrator : search.fallback;
}

class ModaDxfProgressPopup {
 public:
  explicit ModaDxfProgressPopup(const std::filesystem::path& sourcePath)
      : owner_(FindIllustratorOwnerWindow()),
        fileName_(sourcePath.filename().wstring()),
        startedAt_(std::chrono::steady_clock::now()) {
    GdiPlusSession::EnsureStarted();
    RegisterWindowClass();
    CreateFonts();
    CreatePopupWindow();
    Update(4, L"Preparando importacao ModaDXF", L"Analisando Molde DXF e ativando preservacao de moldes.");
  }

  ~ModaDxfProgressPopup() {
    if (hwnd_) {
      DestroyWindow(hwnd_);
      PumpMessages();
      hwnd_ = nullptr;
    }
    if (titleFont_) {
      DeleteObject(titleFont_);
    }
    if (bodyFont_) {
      DeleteObject(bodyFont_);
    }
    if (smallFont_) {
      DeleteObject(smallFont_);
    }
  }

  void Update(int progress, const std::wstring& step, const std::wstring& detail = L"") {
    if (progress < 0) {
      progress_ = 0;
    } else if (progress > 100) {
      progress_ = 100;
    } else {
      progress_ = progress;
    }
    step_ = step;
    detail_ = detail;
    if (hwnd_) {
      InvalidateRect(hwnd_, nullptr, false);
      UpdateWindow(hwnd_);
      PumpMessages();
    }
  }

  void Complete(bool success, const std::wstring& detail) {
    Update(100, success ? L"Importacao ModaDXF concluida" : L"Importacao ModaDXF interrompida", detail);
    const auto elapsed = std::chrono::steady_clock::now() - startedAt_;
    const auto minimum = std::chrono::milliseconds(850);
    if (elapsed < minimum) {
      std::this_thread::sleep_for(minimum - elapsed);
    } else {
      std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
    PumpMessages();
  }

 private:
  static constexpr wchar_t kWindowClassName[] = L"ModaDXFProgressPopup";

  static void RegisterWindowClass() {
    static bool registered = false;
    if (registered) {
      return;
    }

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = &ModaDxfProgressPopup::WindowProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = kWindowClassName;
    wc.style = CS_HREDRAW | CS_VREDRAW;
    RegisterClassExW(&wc);
    registered = true;
  }

  void CreateFonts() {
    titleFont_ = CreateFontW(-25, 0, 0, 0, FW_SEMIBOLD, false, false, false, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS,
                             CLIP_DEFAULT_PRECIS, CLEARTYPE_NATURAL_QUALITY, DEFAULT_PITCH | FF_SWISS,
                             L"Segoe UI Variable Text");
    bodyFont_ = CreateFontW(-16, 0, 0, 0, FW_NORMAL, false, false, false, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS,
                            CLIP_DEFAULT_PRECIS, CLEARTYPE_NATURAL_QUALITY, DEFAULT_PITCH | FF_SWISS,
                            L"Segoe UI Variable Text");
    smallFont_ = CreateFontW(-13, 0, 0, 0, FW_NORMAL, false, false, false, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS,
                             CLIP_DEFAULT_PRECIS, CLEARTYPE_NATURAL_QUALITY, DEFAULT_PITCH | FF_SWISS,
                             L"Segoe UI Variable Text");
  }

  void CreatePopupWindow() {
    const int width = 540;
    const int height = 226;
    RECT anchor{};
    if (!owner_ || !GetWindowRect(owner_, &anchor)) {
      SystemParametersInfoW(SPI_GETWORKAREA, 0, &anchor, 0);
    }

    const int x = anchor.left + ((anchor.right - anchor.left) - width) / 2;
    const int y = anchor.top + ((anchor.bottom - anchor.top) - height) / 2;

    hwnd_ = CreateWindowExW(WS_EX_TOOLWINDOW, kWindowClassName, L"ModaDXF", WS_POPUP, x, y, width, height, owner_,
                            nullptr, GetModuleHandleW(nullptr), this);
    if (!hwnd_) {
      return;
    }

    HRGN region = CreateRoundRectRgn(0, 0, width + 1, height + 1, 18, 18);
    SetWindowRgn(hwnd_, region, true);
    ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
    UpdateWindow(hwnd_);
    PumpMessages();
  }

  void PumpMessages() {
    if (!hwnd_) {
      return;
    }
    MSG msg{};
    while (PeekMessageW(&msg, hwnd_, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
  }

  static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_NCCREATE) {
      auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
      auto* popup = reinterpret_cast<ModaDxfProgressPopup*>(create->lpCreateParams);
      SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(popup));
    }

    auto* popup = reinterpret_cast<ModaDxfProgressPopup*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (popup && message == WM_PAINT) {
      popup->Paint(hwnd);
      return 0;
    }
    if (message == WM_ERASEBKGND) {
      return 1;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
  }

  void Paint(HWND hwnd) {
    PAINTSTRUCT ps{};
    HDC hdc = BeginPaint(hwnd, &ps);

    RECT rect{};
    GetClientRect(hwnd, &rect);
    HDC buffer = CreateCompatibleDC(hdc);
    HBITMAP bitmap = CreateCompatibleBitmap(hdc, rect.right - rect.left, rect.bottom - rect.top);
    HGDIOBJ oldBitmap = SelectObject(buffer, bitmap);

    DrawBackground(buffer, rect);
    DrawContent(buffer, rect);

    BitBlt(hdc, 0, 0, rect.right - rect.left, rect.bottom - rect.top, buffer, 0, 0, SRCCOPY);
    SelectObject(buffer, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(buffer);
    EndPaint(hwnd, &ps);
  }

  void DrawBackground(HDC hdc, const RECT& rect) {
    HBRUSH background = CreateSolidBrush(RGB(25, 28, 34));
    FillRect(hdc, &rect, background);
    DeleteObject(background);

    DrawRoundedFill(hdc, MakeRect(0, 0, rect.right, rect.bottom), RGB(25, 28, 34), RGB(72, 83, 96), 16.0f);
  }

  void DrawContent(HDC hdc, const RECT& rect) {
    SetBkMode(hdc, TRANSPARENT);

    RECT logo{28, 28, 88, 88};
    DrawRoundedFill(hdc, logo, RGB(20, 150, 130), RGB(57, 221, 189), 12.0f);

    SetTextColor(hdc, RGB(245, 250, 247));
    HGDIOBJ oldFont = SelectObject(hdc, titleFont_);
    RECT mark{logo.left, logo.top + 17, logo.right, logo.bottom};
    DrawTextW(hdc, L"DXF", -1, &mark, DT_CENTER | DT_TOP | DT_SINGLELINE);

    RECT title{108, 28, rect.right - 28, 60};
    DrawTextW(hdc, L"ModaDXF processando Molde", -1, &title, DT_LEFT | DT_TOP | DT_SINGLELINE);

    SelectObject(hdc, smallFont_);
    SetTextColor(hdc, RGB(164, 176, 190));
    RECT fileRect{110, 62, rect.right - 30, 84};
    std::wstring fileLine = L"Molde: " + fileName_;
    DrawTextW(hdc, fileLine.c_str(), -1, &fileRect, DT_LEFT | DT_TOP | DT_END_ELLIPSIS | DT_SINGLELINE);

    SelectObject(hdc, bodyFont_);
    SetTextColor(hdc, RGB(230, 236, 241));
    RECT stepRect{28, 110, rect.right - 28, 136};
    DrawTextW(hdc, step_.c_str(), -1, &stepRect, DT_LEFT | DT_TOP | DT_END_ELLIPSIS | DT_SINGLELINE);

    SelectObject(hdc, smallFont_);
    SetTextColor(hdc, RGB(145, 157, 171));
    RECT detailRect{28, 140, rect.right - 28, 164};
    DrawTextW(hdc, detail_.c_str(), -1, &detailRect, DT_LEFT | DT_TOP | DT_END_ELLIPSIS | DT_SINGLELINE);

    RECT bar{28, 180, rect.right - 28, 194};
    DrawProgressBar(hdc, bar);

    wchar_t percent[16]{};
    swprintf_s(percent, L"%d%%", progress_);
    SetTextColor(hdc, RGB(57, 221, 189));
    RECT percentRect{rect.right - 82, 199, rect.right - 28, 218};
    DrawTextW(hdc, percent, -1, &percentRect, DT_RIGHT | DT_TOP | DT_SINGLELINE);

    SetTextColor(hdc, RGB(119, 132, 147));
    RECT footer{28, 199, rect.right - 90, 218};
    DrawTextW(hdc, L"Reconstruindo agrupamentos do Molde", -1, &footer,
              DT_LEFT | DT_TOP | DT_END_ELLIPSIS | DT_SINGLELINE);

    SelectObject(hdc, oldFont);
  }

  void DrawProgressBar(HDC hdc, const RECT& bar) {
    DrawRoundedFill(hdc, bar, RGB(45, 52, 62), RGB(63, 73, 86), 7.0f);

    RECT fill = bar;
    fill.right = fill.left + ((bar.right - bar.left) * progress_) / 100;
    if (fill.right > fill.left + 4) {
      DrawRoundedFill(hdc, fill, RGB(57, 221, 189), RGB(57, 221, 189), 7.0f);
    }
  }

  HWND hwnd_ = nullptr;
  HWND owner_ = nullptr;
  HFONT titleFont_ = nullptr;
  HFONT bodyFont_ = nullptr;
  HFONT smallFont_ = nullptr;
  std::wstring fileName_;
  std::wstring step_;
  std::wstring detail_;
  int progress_ = 0;
  std::chrono::steady_clock::time_point startedAt_;
};

class DxfOptionsDialog {
 public:
  explicit DxfOptionsDialog(const std::filesystem::path& sourcePath) : fileName_(sourcePath.filename().wstring()) {
    GdiPlusSession::EnsureStarted();
    CreateFonts();
  }

  ~DxfOptionsDialog() {
    if (hwnd_) {
      DestroyWindow(hwnd_);
    }
    if (titleFont_) {
      DeleteObject(titleFont_);
    }
    if (bodyFont_) {
      DeleteObject(bodyFont_);
    }
    if (smallFont_) {
      DeleteObject(smallFont_);
    }
  }

  bool Show(DxfImportOptions* options) {
    RegisterWindowClass();

    owner_ = FindIllustratorOwnerWindow();
    CreateDialogWindow();
    if (!hwnd_) {
      if (options) {
        *options = options_;
      }
      return true;
    }

    if (owner_) {
      EnableWindow(owner_, false);
    }
    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);
    SetForegroundWindow(hwnd_);
    SetFocus(hwnd_);

    MSG msg{};
    while (!finished_ && GetMessageW(&msg, nullptr, 0, 0) > 0) {
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }

    if (owner_) {
      EnableWindow(owner_, true);
      SetForegroundWindow(owner_);
    }
    if (accepted_ && options) {
      options_.percent = ParsePercentText();
      options_.unitScaleSource = ParseUnitScaleText(unitScaleSourceText_);
      options_.unitScaleTarget = ParseUnitScaleText(unitScaleTargetText_);
      *options = options_;
    }
    return accepted_;
  }

 private:
  enum class HitTarget {
    None,
    Close,
    Original,
    FitArtboard,
    Percent,
    UnitScaleSource,
    UnitScaleTarget,
    UnitDropdown,
    LineWeight,
    MergeLayers,
    Ok,
    Cancel
  };

  enum class ActiveTextField {
    None,
    Percent,
    UnitScaleSource,
    UnitScaleTarget
  };

  static constexpr wchar_t kWindowClassName[] = L"ModaDXFDxfOptionsDialog";
  static constexpr COLORREF kBackground = RGB(25, 28, 34);
  static constexpr COLORREF kPanel = RGB(33, 38, 46);
  static constexpr COLORREF kPanelHover = RGB(43, 50, 60);
  static constexpr COLORREF kBorder = RGB(78, 92, 108);
  static constexpr COLORREF kAccent = RGB(57, 221, 189);
  static constexpr COLORREF kAccentDark = RGB(20, 150, 130);
  static constexpr COLORREF kText = RGB(245, 250, 247);
  static constexpr COLORREF kBody = RGB(230, 236, 241);
  static constexpr COLORREF kMuted = RGB(145, 157, 171);
  static constexpr COLORREF kDisabled = RGB(94, 105, 118);

  static RECT RectFrom(int left, int top, int right, int bottom) {
    RECT rect{left, top, right, bottom};
    return rect;
  }

  static bool Contains(const RECT& rect, POINT point) {
    return PtInRect(&rect, point) != 0;
  }

  static double ParseNumberText(std::wstring value, double fallback, double minValue, double maxValue) {
    for (wchar_t& ch : value) {
      if (ch == L',') {
        ch = L'.';
      }
    }
    wchar_t* end = nullptr;
    const double parsed = std::wcstod(value.c_str(), &end);
    if (end == value.c_str() || !std::isfinite(parsed)) {
      return fallback;
    }
    return std::clamp(parsed, minValue, maxValue);
  }

  double ParsePercentText() const {
    return ParseNumberText(percentText_, kDefaultScalePercent, 0.001, 100000.0);
  }

  double ParseUnitScaleText(const std::wstring& value) const {
    return ParseNumberText(value, kDefaultUnitScaleValue, 0.000001, 1000000.0);
  }

  static void RegisterWindowClass() {
    static bool registered = false;
    if (registered) {
      return;
    }

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = &DxfOptionsDialog::WindowProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = kWindowClassName;
    wc.style = CS_HREDRAW | CS_VREDRAW;
    RegisterClassExW(&wc);
    registered = true;
  }

  void CreateFonts() {
    titleFont_ = CreateFontW(-25, 0, 0, 0, FW_SEMIBOLD, false, false, false, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS,
                             CLIP_DEFAULT_PRECIS, CLEARTYPE_NATURAL_QUALITY, DEFAULT_PITCH | FF_SWISS,
                             L"Segoe UI Variable Text");
    bodyFont_ = CreateFontW(-15, 0, 0, 0, FW_NORMAL, false, false, false, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS,
                            CLIP_DEFAULT_PRECIS, CLEARTYPE_NATURAL_QUALITY, DEFAULT_PITCH | FF_SWISS,
                            L"Segoe UI Variable Text");
    smallFont_ = CreateFontW(-13, 0, 0, 0, FW_NORMAL, false, false, false, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS,
                             CLIP_DEFAULT_PRECIS, CLEARTYPE_NATURAL_QUALITY, DEFAULT_PITCH | FF_SWISS,
                             L"Segoe UI Variable Text");
  }

  void CreateDialogWindow() {
    constexpr int width = 540;
    constexpr int height = 430;

    RECT anchor{};
    if (owner_ && GetWindowRect(owner_, &anchor)) {
      // Center over Illustrator after the file-format selector closes.
    } else {
      SystemParametersInfoW(SPI_GETWORKAREA, 0, &anchor, 0);
    }

    const int x = anchor.left + ((anchor.right - anchor.left) - width) / 2;
    const int y = anchor.top + ((anchor.bottom - anchor.top) - height) / 2;

    hwnd_ = CreateWindowExW(WS_EX_TOOLWINDOW, kWindowClassName, L"ModaDXF", WS_POPUP, x, y, width, height, owner_,
                            nullptr, GetModuleHandleW(nullptr), this);
    if (!hwnd_) {
      return;
    }

    HRGN region = CreateRoundRectRgn(0, 0, width + 1, height + 1, 18, 18);
    SetWindowRgn(hwnd_, region, true);
  }

  static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_NCCREATE) {
      auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
      auto* dialog = reinterpret_cast<DxfOptionsDialog*>(create->lpCreateParams);
      SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(dialog));
    }

    auto* dialog = reinterpret_cast<DxfOptionsDialog*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (!dialog) {
      return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    switch (message) {
      case WM_ERASEBKGND:
        return 1;
      case WM_PAINT:
        dialog->Paint(hwnd);
        return 0;
      case WM_MOUSEMOVE:
        dialog->OnMouseMove(lParam);
        return 0;
      case WM_MOUSELEAVE:
        dialog->trackingMouse_ = false;
        dialog->SetHover(HitTarget::None);
        return 0;
      case WM_LBUTTONDOWN:
        dialog->OnClick(lParam);
        return 0;
      case WM_KEYDOWN:
        if (wParam == VK_RETURN) {
          dialog->Finish(true);
          return 0;
        }
        if (wParam == VK_ESCAPE) {
          dialog->Finish(false);
          return 0;
        }
        if (wParam == VK_TAB) {
          dialog->ActivateNextTextField();
          return 0;
        }
        if (wParam == VK_DELETE && dialog->ClearActiveTextField()) {
          return 0;
        }
        break;
      case WM_CHAR:
        if (dialog->OnChar(wParam)) {
          return 0;
        }
        break;
      case WM_CLOSE:
        dialog->Finish(false);
        return 0;
      case WM_NCDESTROY:
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        dialog->hwnd_ = nullptr;
        return 0;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
  }

  POINT PointFrom(LPARAM lParam) const {
    POINT point{static_cast<short>(LOWORD(lParam)), static_cast<short>(HIWORD(lParam))};
    return point;
  }

  void OnMouseMove(LPARAM lParam) {
    SetHover(HitTest(PointFrom(lParam)));
    if (!trackingMouse_ && hwnd_) {
      TRACKMOUSEEVENT event{};
      event.cbSize = sizeof(event);
      event.dwFlags = TME_LEAVE;
      event.hwndTrack = hwnd_;
      if (TrackMouseEvent(&event)) {
        trackingMouse_ = true;
      }
    }
  }

  void OnClick(LPARAM lParam) {
    if (hwnd_) {
      SetFocus(hwnd_);
    }
    switch (HitTest(PointFrom(lParam))) {
      case HitTarget::Close:
      case HitTarget::Cancel:
        Finish(false);
        break;
      case HitTarget::Ok:
        Finish(true);
        break;
      case HitTarget::Original:
        options_.scaleMode = DxfScaleMode::Original;
        activeTextField_ = ActiveTextField::None;
        Redraw();
        break;
      case HitTarget::FitArtboard:
        options_.scaleMode = DxfScaleMode::FitArtboard;
        activeTextField_ = ActiveTextField::None;
        Redraw();
        break;
      case HitTarget::Percent:
        options_.scaleMode = DxfScaleMode::Percent;
        ActivateTextField(ActiveTextField::Percent);
        Redraw();
        break;
      case HitTarget::UnitScaleSource:
        options_.scaleMode = DxfScaleMode::Percent;
        ActivateTextField(ActiveTextField::UnitScaleSource);
        Redraw();
        break;
      case HitTarget::UnitScaleTarget:
        options_.scaleMode = DxfScaleMode::Percent;
        ActivateTextField(ActiveTextField::UnitScaleTarget);
        Redraw();
        break;
      case HitTarget::UnitDropdown:
        options_.scaleMode = DxfScaleMode::Percent;
        activeTextField_ = ActiveTextField::None;
        ShowUnitDropdown();
        break;
      case HitTarget::LineWeight:
        options_.scaleLineWeight = !options_.scaleLineWeight;
        Redraw();
        break;
      case HitTarget::MergeLayers:
        options_.mergeLayers = !options_.mergeLayers;
        Redraw();
        break;
      case HitTarget::None:
        break;
    }
  }

  bool OnChar(WPARAM value) {
    if (options_.scaleMode != DxfScaleMode::Percent) {
      return false;
    }
    if (activeTextField_ == ActiveTextField::None) {
      ActivateTextField(ActiveTextField::Percent);
    }
    std::wstring* activeText = ActiveTextFieldValue();
    if (!activeText) {
      return false;
    }
    if (value == VK_BACK) {
      if (replaceActiveTextOnInput_) {
        activeText->clear();
        replaceActiveTextOnInput_ = false;
      } else if (!activeText->empty()) {
        activeText->pop_back();
      }
      Redraw();
      return true;
    }
    const wchar_t ch = static_cast<wchar_t>(value);
    if (ch >= L'0' && ch <= L'9') {
      if (replaceActiveTextOnInput_) {
        activeText->clear();
        replaceActiveTextOnInput_ = false;
      }
      if (activeText->size() < 10) {
        activeText->push_back(ch);
      }
      Redraw();
      return true;
    }
    if ((ch == L'.' || ch == L',') && activeText->find_first_of(L".,") == std::wstring::npos) {
      if (replaceActiveTextOnInput_) {
        activeText->clear();
        replaceActiveTextOnInput_ = false;
      }
      if (activeText->empty()) {
        *activeText = L"0";
      }
      activeText->push_back(ch);
      Redraw();
      return true;
    }
    return false;
  }

  HitTarget HitTest(POINT point) const {
    if (Contains(CloseRect(), point)) {
      return HitTarget::Close;
    }
    if (Contains(OkRect(), point)) {
      return HitTarget::Ok;
    }
    if (Contains(CancelRect(), point)) {
      return HitTarget::Cancel;
    }
    if (Contains(OriginalRect(), point)) {
      return HitTarget::Original;
    }
    if (Contains(FitRect(), point)) {
      return HitTarget::FitArtboard;
    }
    if (Contains(PercentRect(), point)) {
      return HitTarget::Percent;
    }
    if (Contains(UnitScaleSourceRect(), point)) {
      return HitTarget::UnitScaleSource;
    }
    if (Contains(UnitScaleTargetRect(), point)) {
      return HitTarget::UnitScaleTarget;
    }
    if (Contains(UnitDropdownRect(), point)) {
      return HitTarget::UnitDropdown;
    }
    if (Contains(LineWeightRect(), point)) {
      return HitTarget::LineWeight;
    }
    if (Contains(MergeLayersRect(), point)) {
      return HitTarget::MergeLayers;
    }
    return HitTarget::None;
  }

  void ActivateTextField(ActiveTextField field) {
    activeTextField_ = field;
    replaceActiveTextOnInput_ = true;
  }

  void ActivateNextTextField() {
    if (options_.scaleMode != DxfScaleMode::Percent) {
      options_.scaleMode = DxfScaleMode::Percent;
      ActivateTextField(ActiveTextField::Percent);
    } else {
      switch (activeTextField_) {
        case ActiveTextField::Percent:
          ActivateTextField(ActiveTextField::UnitScaleSource);
          break;
        case ActiveTextField::UnitScaleSource:
          ActivateTextField(ActiveTextField::UnitScaleTarget);
          break;
        case ActiveTextField::UnitScaleTarget:
        case ActiveTextField::None:
        default:
          ActivateTextField(ActiveTextField::Percent);
          break;
      }
    }
    Redraw();
  }

  bool ClearActiveTextField() {
    if (options_.scaleMode != DxfScaleMode::Percent) {
      return false;
    }
    std::wstring* activeText = ActiveTextFieldValue();
    if (!activeText) {
      return false;
    }
    activeText->clear();
    replaceActiveTextOnInput_ = false;
    Redraw();
    return true;
  }

  std::wstring* ActiveTextFieldValue() {
    switch (activeTextField_) {
      case ActiveTextField::Percent:
        return &percentText_;
      case ActiveTextField::UnitScaleSource:
        return &unitScaleSourceText_;
      case ActiveTextField::UnitScaleTarget:
        return &unitScaleTargetText_;
      case ActiveTextField::None:
      default:
        return nullptr;
    }
  }

  void ShowUnitDropdown() {
    if (!hwnd_) {
      return;
    }
    HMENU menu = CreatePopupMenu();
    if (!menu) {
      Redraw();
      return;
    }

    for (int index = 0; index < 7; ++index) {
      const DxfScaleUnit unit = UnitFromIndex(index);
      UINT flags = MF_STRING;
      if (index == UnitIndex(options_.unit)) {
        flags |= MF_CHECKED;
      }
      AppendMenuW(menu, flags, static_cast<UINT_PTR>(index + 1), UnitLabel(unit));
    }

    RECT rect = UnitDropdownRect();
    POINT point{rect.left, rect.bottom};
    ClientToScreen(hwnd_, &point);
    SetForegroundWindow(hwnd_);
    const int command =
        TrackPopupMenu(menu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD, point.x, point.y, 0, hwnd_, nullptr);
    if (command >= 1 && command <= 7) {
      options_.unit = UnitFromIndex(command - 1);
    }
    DestroyMenu(menu);
    Redraw();
  }

  void SetHover(HitTarget target) {
    if (hover_ != target) {
      hover_ = target;
      Redraw();
    }
  }

  void Finish(bool accepted) {
    accepted_ = accepted;
    finished_ = true;
    if (hwnd_) {
      DestroyWindow(hwnd_);
      hwnd_ = nullptr;
    }
  }

  void Redraw() {
    if (hwnd_) {
      InvalidateRect(hwnd_, nullptr, false);
      UpdateWindow(hwnd_);
    }
  }

  void Paint(HWND hwnd) {
    PAINTSTRUCT ps{};
    HDC hdc = BeginPaint(hwnd, &ps);

    RECT rect{};
    GetClientRect(hwnd, &rect);
    HDC buffer = CreateCompatibleDC(hdc);
    HBITMAP bitmap = CreateCompatibleBitmap(hdc, rect.right - rect.left, rect.bottom - rect.top);
    HGDIOBJ oldBitmap = SelectObject(buffer, bitmap);

    DrawBackground(buffer, rect);
    DrawHeader(buffer, rect);
    DrawOptions(buffer, rect);
    DrawButtons(buffer, rect);

    BitBlt(hdc, 0, 0, rect.right - rect.left, rect.bottom - rect.top, buffer, 0, 0, SRCCOPY);
    SelectObject(buffer, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(buffer);
    EndPaint(hwnd, &ps);
  }

  void DrawBackground(HDC hdc, const RECT& rect) {
    HBRUSH background = CreateSolidBrush(kBackground);
    FillRect(hdc, &rect, background);
    DeleteObject(background);

    DrawRoundedFill(hdc, RectFrom(0, 0, rect.right, rect.bottom), kBackground, kBorder, 16.0f);
  }

  void DrawHeader(HDC hdc, const RECT& rect) {
    SetBkMode(hdc, TRANSPARENT);

    RECT logo{28, 28, 88, 88};
    DrawRoundedFill(hdc, logo, kAccentDark, kAccent, 12.0f);

    DrawTextBlock(hdc, L"DXF", RectFrom(logo.left, logo.top + 15, logo.right, logo.bottom), titleFont_, kText,
                  DT_CENTER | DT_TOP | DT_SINGLELINE);
    DrawTextBlock(hdc, L"Op\u00E7\u00F5es do Molde DXF", RectFrom(108, 27, rect.right - 52, 61), titleFont_, kText,
                  DT_LEFT | DT_TOP | DT_SINGLELINE);

    std::wstring fileLine = L"Molde: " + fileName_;
    DrawTextBlock(hdc, fileLine, RectFrom(110, 62, rect.right - 56, 84), smallFont_, RGB(164, 176, 190),
                  DT_LEFT | DT_TOP | DT_END_ELLIPSIS | DT_SINGLELINE);
    DrawTextBlock(hdc, L"\u00D7", CloseRect(), titleFont_, hover_ == HitTarget::Close ? kAccent : kMuted,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE);
  }

  void DrawOptions(HDC hdc, const RECT&) {
    DrawSectionTitle(hdc, L"Dimensionar arte", 28, 106);
    DrawRadio(hdc, OriginalRect(), L"Tamanho original", options_.scaleMode == DxfScaleMode::Original,
              hover_ == HitTarget::Original, true);
    DrawRadio(hdc, FitRect(), L"Ajustar \u00E0s dimens\u00F5es da prancheta",
              options_.scaleMode == DxfScaleMode::FitArtboard,
              hover_ == HitTarget::FitArtboard, true);
    DrawRadio(hdc, PercentRect(), L"Dimensionar em:", options_.scaleMode == DxfScaleMode::Percent,
              hover_ == HitTarget::Percent, true);
    const bool scaleByValue = options_.scaleMode == DxfScaleMode::Percent;
    const std::wstring percentLabel = percentText_ + L"%";
    DrawField(hdc, PercentValueRect(), percentLabel.c_str(), scaleByValue, false,
              activeTextField_ == ActiveTextField::Percent);

    DrawTextBlock(hdc, L"Dimensionar:", RectFrom(28, 214, 116, 236), smallFont_, scaleByValue ? kBody : kDisabled,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    DrawField(hdc, UnitScaleSourceRect(), unitScaleSourceText_.c_str(), scaleByValue, false,
              activeTextField_ == ActiveTextField::UnitScaleSource);
    DrawTextBlock(hdc, L"Unidade(s) =", RectFrom(184, 214, 274, 236), smallFont_, scaleByValue ? kBody : kDisabled,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    DrawField(hdc, UnitScaleTargetRect(), unitScaleTargetText_.c_str(), scaleByValue, false,
              activeTextField_ == ActiveTextField::UnitScaleTarget);
    DrawField(hdc, UnitDropdownRect(), UnitLabel(options_.unit), scaleByValue, true);

    DrawCheckbox(hdc, LineWeightRect(), L"Dimensionar espessuras de linha", options_.scaleLineWeight,
                 hover_ == HitTarget::LineWeight, true);

    DrawSectionTitle(hdc, L"Op\u00E7\u00F5es", 28, 282);
    DrawTextBlock(hdc, L"Layout:", RectFrom(28, 312, 74, 334), smallFont_, kBody, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    DrawField(hdc, RectFrom(80, 309, 160, 335), L"Model", true, true);
    DrawCheckbox(hdc, CenterArtRect(), L"Centralizar arte", options_.centerArt, false, false);
    DrawCheckbox(hdc, MergeLayersRect(), L"Mesclar camadas", options_.mergeLayers, hover_ == HitTarget::MergeLayers,
                 true);
  }

  void DrawButtons(HDC hdc, const RECT&) {
    DrawButton(hdc, OkRect(), L"OK", true, hover_ == HitTarget::Ok);
    DrawButton(hdc, CancelRect(), L"Cancelar", false, hover_ == HitTarget::Cancel);
  }

  void DrawSectionTitle(HDC hdc, const wchar_t* text, int x, int y) {
    DrawTextBlock(hdc, text, RectFrom(x, y - 2, x + 180, y + 24), bodyFont_, kBody,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    DrawSoftLine(hdc, x, y + 24, 512, y + 24, RGB(45, 52, 62), 1.0f);
  }

  void DrawRadio(HDC hdc, const RECT& row, const wchar_t* text, bool selected, bool hover, bool enabled) {
    if (hover && enabled) {
      DrawRoundFill(hdc, row, kPanelHover, kPanelHover, 8);
    }

    const int cy = row.top + (row.bottom - row.top) / 2;
    RECT circle{row.left + 3, cy - 7, row.left + 17, cy + 7};
    const COLORREF ring = selected ? kAccent : (enabled ? RGB(195, 204, 214) : kDisabled);
    DrawSoftEllipse(hdc, circle, kBackground, ring, 1.25f);
    if (selected) {
      RECT dot{circle.left + 4, circle.top + 4, circle.right - 4, circle.bottom - 4};
      DrawSoftEllipse(hdc, dot, kAccent, kAccent, 1.0f);
    }

    DrawTextBlock(hdc, text, RectFrom(row.left + 28, row.top, row.right, row.bottom), smallFont_,
                  enabled ? kBody : kDisabled, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
  }

  void DrawCheckbox(HDC hdc, const RECT& row, const wchar_t* text, bool checked, bool hover, bool enabled) {
    if (hover && enabled) {
      DrawRoundFill(hdc, row, kPanelHover, kPanelHover, 8);
    }

    const int cy = row.top + (row.bottom - row.top) / 2;
    RECT box{row.left + 3, cy - 7, row.left + 17, cy + 7};
    DrawRoundFill(hdc, box, checked ? kAccentDark : kBackground, enabled ? RGB(195, 204, 214) : kDisabled, 3);
    if (checked) {
      DrawSoftLine(hdc, box.left + 3, box.top + 7, box.left + 6, box.top + 10, kText, 2.0f);
      DrawSoftLine(hdc, box.left + 6, box.top + 10, box.left + 11, box.top + 4, kText, 2.0f);
    }

    DrawTextBlock(hdc, text, RectFrom(row.left + 28, row.top, row.right, row.bottom), smallFont_,
                  enabled ? kBody : kDisabled, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
  }

  void DrawField(HDC hdc, const RECT& rect, const wchar_t* text, bool enabled, bool dropdown = false, bool focused = false) {
    const COLORREF fill = enabled ? kPanel : RGB(30, 34, 40);
    const COLORREF border = enabled ? (focused ? kAccent : RGB(68, 82, 98)) : RGB(43, 51, 61);
    DrawRoundFill(hdc, rect, fill, border, 5);
    DrawTextBlock(hdc, text, RectFrom(rect.left + 12, rect.top, rect.right - (dropdown ? 22 : 8), rect.bottom),
                  smallFont_, enabled ? kBody : kDisabled, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    if (dropdown) {
      const int midY = rect.top + ((rect.bottom - rect.top) / 2) + 2;
      const COLORREF arrow = enabled ? kMuted : kDisabled;
      DrawSoftLine(hdc, rect.right - 17, midY - 3, rect.right - 12, midY + 2, arrow, 1.2f);
      DrawSoftLine(hdc, rect.right - 12, midY + 2, rect.right - 7, midY - 3, arrow, 1.2f);
    }
  }

  void DrawButton(HDC hdc, const RECT& rect, const wchar_t* text, bool primary, bool hover) {
    const COLORREF fill = primary ? (hover ? RGB(69, 235, 204) : kAccent) : (hover ? kPanelHover : kBackground);
    const COLORREF border = primary ? fill : RGB(174, 185, 197);
    DrawRoundFill(hdc, rect, fill, border, 10);
    DrawTextBlock(hdc, text, rect, smallFont_, primary ? RGB(6, 33, 35) : kText,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE);
  }

  void DrawRoundFill(HDC hdc, const RECT& rect, COLORREF fill, COLORREF border, int radius) {
    DrawRoundedFill(hdc, rect, fill, border, static_cast<float>(radius));
  }

  void DrawTextBlock(HDC hdc, const std::wstring& text, const RECT& rect, HFONT font, COLORREF color, UINT format) {
    HGDIOBJ oldFont = SelectObject(hdc, font);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    RECT copy = rect;
    DrawTextW(hdc, text.c_str(), -1, &copy, format);
    SelectObject(hdc, oldFont);
  }

  RECT CloseRect() const {
    return RectFrom(494, 20, 524, 50);
  }

  RECT OriginalRect() const {
    return RectFrom(28, 132, 230, 156);
  }

  RECT FitRect() const {
    return RectFrom(28, 156, 320, 180);
  }

  RECT PercentRect() const {
    return RectFrom(28, 180, 258, 204);
  }

  RECT PercentValueRect() const {
    return RectFrom(170, 178, 240, 203);
  }

  RECT UnitScaleSourceRect() const {
    return RectFrom(116, 212, 170, 237);
  }

  RECT UnitScaleTargetRect() const {
    return RectFrom(274, 212, 328, 237);
  }

  RECT UnitDropdownRect() const {
    return RectFrom(342, 212, 452, 237);
  }

  RECT LineWeightRect() const {
    return RectFrom(28, 246, 286, 270);
  }

  RECT CenterArtRect() const {
    return RectFrom(28, 342, 198, 366);
  }

  RECT MergeLayersRect() const {
    return RectFrom(28, 366, 198, 390);
  }

  RECT OkRect() const {
    return RectFrom(326, 388, 416, 416);
  }

  RECT CancelRect() const {
    return RectFrom(428, 388, 518, 416);
  }

  HWND hwnd_ = nullptr;
  HWND owner_ = nullptr;
  HFONT titleFont_ = nullptr;
  HFONT bodyFont_ = nullptr;
  HFONT smallFont_ = nullptr;
  std::wstring fileName_;
  DxfImportOptions options_;
  std::wstring percentText_ = L"100";
  std::wstring unitScaleSourceText_ = L"1";
  std::wstring unitScaleTargetText_ = L"1";
  HitTarget hover_ = HitTarget::None;
  ActiveTextField activeTextField_ = ActiveTextField::None;
  bool accepted_ = false;
  bool finished_ = false;
  bool trackingMouse_ = false;
  bool replaceActiveTextOnInput_ = false;
};

bool ShowDxfOptionsDialog(const std::filesystem::path& path, DxfImportOptions* options) {
  DxfOptionsDialog dialog(path);
  return dialog.Show(options);
}

std::string SanitizeName(const std::string& value, const std::string& fallback) {
  std::string out = value.empty() ? fallback : value;
  for (char& ch : out) {
    if (ch == '/' || ch == '\\' || ch == ':' || ch == '*' || ch == '?' || ch == '"' || ch == '<' || ch == '>' ||
        ch == '|') {
      ch = '_';
    }
  }
  return out.empty() ? fallback : out;
}

std::wstring ToWideString(const std::string& value) {
  if (value.empty()) {
    return L"";
  }

  const int size = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
  if (size <= 1) {
    return std::wstring(value.begin(), value.end());
  }

  std::wstring out(static_cast<size_t>(size - 1), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, out.data(), size);
  return out;
}

AIReal ToAIY(double y) {
  return static_cast<AIReal>(y);
}

AIReal ToAIX(double x) {
  return static_cast<AIReal>(x);
}

AIRealPoint ToPoint(double x, double y) {
  AIRealPoint point{};
  point.h = ToAIX(x);
  point.v = ToAIY(y);
  return point;
}

AIRealRect NormalizedRect(const AIRealRect& value) {
  AIRealRect normalized{};
  normalized.left = value.left < value.right ? value.left : value.right;
  normalized.right = value.left > value.right ? value.left : value.right;
  normalized.bottom = value.bottom < value.top ? value.bottom : value.top;
  normalized.top = value.bottom > value.top ? value.bottom : value.top;
  return normalized;
}

AIReal RectWidth(const AIRealRect& value) {
  return value.right - value.left;
}

AIReal RectHeight(const AIRealRect& value) {
  return value.top - value.bottom;
}

AIRealPoint RectCenter(const AIRealRect& value) {
  AIRealPoint center{};
  center.h = value.left + (RectWidth(value) / 2.0f);
  center.v = value.bottom + (RectHeight(value) / 2.0f);
  return center;
}

void IncludeBounds(AIRealRect& combined, const AIRealRect& candidate, bool& hasBounds) {
  const AIReal left = candidate.left < candidate.right ? candidate.left : candidate.right;
  const AIReal right = candidate.left > candidate.right ? candidate.left : candidate.right;
  const AIReal bottom = candidate.bottom < candidate.top ? candidate.bottom : candidate.top;
  const AIReal top = candidate.bottom > candidate.top ? candidate.bottom : candidate.top;

  if (!std::isfinite(left) || !std::isfinite(right) || !std::isfinite(bottom) || !std::isfinite(top)) {
    return;
  }
  if ((right - left) <= 0.0f || (top - bottom) <= 0.0f) {
    return;
  }

  if (!hasBounds) {
    combined.left = left;
    combined.right = right;
    combined.bottom = bottom;
    combined.top = top;
    hasBounds = true;
    return;
  }

  combined.left = combined.left < left ? combined.left : left;
  combined.right = combined.right > right ? combined.right : right;
  combined.bottom = combined.bottom < bottom ? combined.bottom : bottom;
  combined.top = combined.top > top ? combined.top : top;
}

bool ComputeArtworkBounds(const std::vector<AIArtHandle>& artwork, AIRealRect* bounds) {
  bool hasBounds = false;
  for (AIArtHandle art : artwork) {
    if (!art) {
      continue;
    }
    AIRealRect artBounds{};
    if (!sAIArt->GetArtBounds(art, &artBounds)) {
      IncludeBounds(*bounds, artBounds, hasBounds);
    }
  }

  return hasBounds && RectWidth(*bounds) > 0.0f && RectHeight(*bounds) > 0.0f;
}

AIErr GetActiveArtboardBounds(AIRealRect* bounds) {
  if (!sAIArtboard) {
    return kNoErr;
  }

  ai::ArtboardList artboardList;
  AIErr error = sAIArtboard->GetArtboardList(artboardList);
  if (error) {
    return error;
  }

  ai::ArtboardID count = 0;
  error = sAIArtboard->GetCount(artboardList, count);
  if (error || count <= 0) {
    sAIArtboard->ReleaseArtboardList(artboardList);
    return error;
  }

  ai::ArtboardID active = 0;
  error = sAIArtboard->GetActive(artboardList, active);
  if (error || active < 0 || active >= count) {
    active = 0;
  }

  ai::ArtboardProperties properties;
  error = sAIArtboard->GetArtboardProperties(artboardList, active, properties);
  if (!error) {
    error = sAIArtboard->GetPosition(properties, *bounds);
  }
  sAIArtboard->ReleaseArtboardList(artboardList);
  if (!error) {
    *bounds = NormalizedRect(*bounds);
  }
  return error;
}

void CenterViewOnBounds(const AIRealRect& bounds) {
  if (!sAIDocumentView) {
    return;
  }
  const AIRealPoint center = RectCenter(bounds);
  sAIDocumentView->SetDocumentViewCenter(nullptr, &center);
  AIRealRect invalidRect = bounds;
  sAIDocumentView->SetDocumentViewInvalidDocumentRect(nullptr, &invalidRect);
}

AIErr TransformArtwork(const std::vector<AIArtHandle>& artwork,
                       AIRealMatrix* matrix,
                       AIReal lineScale,
                       bool scaleLines) {
  if (!sAITransformArt) {
    return kNoErr;
  }
  ai::int32 flags = kTransformObjects | kTransformChildren;
  if (scaleLines) {
    flags |= kScaleLines;
  }
  for (AIArtHandle art : artwork) {
    if (!art) {
      continue;
    }
    const AIErr error = sAITransformArt->TransformArt(art, matrix, lineScale, flags);
    if (error) {
      return error;
    }
  }
  return kNoErr;
}

AIErr ApplyImportOptionsToArtwork(const std::vector<AIArtHandle>& artwork, const DxfImportOptions& options) {
  if (artwork.empty()) {
    return kNoErr;
  }
  if (!sAIRealMath) {
    return kNoErr;
  }

  AIRealRect artworkBounds{};
  if (!ComputeArtworkBounds(artwork, &artworkBounds)) {
    return kNoErr;
  }

  if (options.scaleMode == DxfScaleMode::Original) {
    CenterViewOnBounds(artworkBounds);
    return kNoErr;
  }

  AIRealRect targetBounds{};
  AIErr error = GetActiveArtboardBounds(&targetBounds);
  if (error) {
    return error;
  }

  AIReal scale = 1.0f;
  AIRealPoint targetCenter = RectCenter(artworkBounds);
  if (options.scaleMode == DxfScaleMode::FitArtboard) {
    if (RectWidth(targetBounds) <= 0.0f || RectHeight(targetBounds) <= 0.0f) {
      CenterViewOnBounds(artworkBounds);
      return kNoErr;
    }
    const AIReal horizontalScale = RectWidth(targetBounds) / RectWidth(artworkBounds);
    const AIReal verticalScale = RectHeight(targetBounds) / RectHeight(artworkBounds);
    scale = horizontalScale < verticalScale ? horizontalScale : verticalScale;
    targetCenter = options.centerArt ? RectCenter(targetBounds) : RectCenter(artworkBounds);
  } else if (options.scaleMode == DxfScaleMode::Percent) {
    scale = static_cast<AIReal>((options.percent / 100.0) * UnitScaleFactorToPoints(options));
    if (options.centerArt && RectWidth(targetBounds) > 0.0f && RectHeight(targetBounds) > 0.0f) {
      targetCenter = RectCenter(targetBounds);
    }
  }

  if (!std::isfinite(scale) || scale <= 0.0f) {
    return kNoErr;
  }

  const AIRealPoint sourceCenter = RectCenter(artworkBounds);
  AIRealMatrix matrix{};
  sAIRealMath->AIRealMatrixSetTranslate(&matrix, -sourceCenter.h, -sourceCenter.v);
  sAIRealMath->AIRealMatrixConcatScale(&matrix, scale, scale);
  sAIRealMath->AIRealMatrixConcatTranslate(&matrix, targetCenter.h, targetCenter.v);
  error = TransformArtwork(artwork, &matrix, scale, options.scaleLineWeight);
  if (error) {
    return error;
  }

  AIRealRect transformedBounds{};
  if (ComputeArtworkBounds(artwork, &transformedBounds)) {
    CenterViewOnBounds(transformedBounds);
  }
  return kNoErr;
}

std::filesystem::path DiagnosticDirectory() {
  const char* localAppData = std::getenv("LOCALAPPDATA");
  std::filesystem::path root =
      localAppData && *localAppData ? std::filesystem::path(localAppData) : std::filesystem::temp_directory_path();
  return root / "ModaDXF" / "logs";
}

void AppendRuntimeLog(const std::string& message) {
  try {
    std::filesystem::create_directories(DiagnosticDirectory());
    std::ofstream out(DiagnosticDirectory() / "runtime.log", std::ios::binary | std::ios::app);
    out << message << "\n";
  } catch (...) {
  }
}

void WriteDiagnostic(const std::filesystem::path& sourcePath,
                     const ModaDxf::DxfModel& model,
                     const ModaDxf::IllustratorImportPlan& plan) {
  try {
    const auto renderPlan = ModaDxf::CreateRenderPlan(model);
    std::filesystem::create_directories(DiagnosticDirectory());
    const std::filesystem::path logPath =
        DiagnosticDirectory() / (SanitizeName(sourcePath.filename().string(), "modadxf") + ".import.json");

    std::ofstream out(logPath, std::ios::binary);
    out << "{\n";
    out << "  \"source\": \"" << sourcePath.string() << "\",\n";
    out << "  \"summary\": \"ModaDXF leu o Molde DXF, reconstruiu os grupos principais e preparou o resultado para o Illustrator.\",\n";
    out << "  \"whatWasDone\": [\n";
    out << "    \"Leitura do Molde DXF concluida.\",\n";
    out << "    \"Agrupamentos principais reconstruidos.\",\n";
    out << "    \"Camadas, caminhos, textos e pontos contabilizados.\",\n";
    out << "    \"Avisos registrados quando encontrados.\"\n";
    out << "  ],\n";
    out << "  \"acadVersion\": \"" << model.acadVersion << "\",\n";
    out << "  \"blocks\": " << model.blocks.size() << ",\n";
    out << "  \"topLevelEntities\": " << model.entities.size() << ",\n";
    out << "  \"topLevelInserts\": " << renderPlan.stats.topLevelInserts << ",\n";
    out << "  \"principalGroups\": " << plan.principalGroups.size() << ",\n";
    out << "  \"layers\": " << model.layers.size() << ",\n";
    out << "  \"paths\": " << renderPlan.stats.paths << ",\n";
    out << "  \"texts\": " << renderPlan.stats.texts << ",\n";
    out << "  \"points\": " << renderPlan.stats.points << ",\n";
    out << "  \"warnings\": [";
    for (size_t i = 0; i < plan.warnings.size(); ++i) {
      out << (i ? ", " : "") << "\"" << plan.warnings[i] << "\"";
    }
    out << "]\n";
    out << "}\n";
  } catch (...) {
  }
}

ASErr ApplyStrokeOnlyStyle(AIArtHandle art) {
  AIPathStyle style{};
  AIErr error = sAIPathStyle->GetInitialPathStyle(&style);
  if (error) {
    return error;
  }
  style.fillPaint = false;
  style.strokePaint = true;
  style.stroke.width = 1.0f;
  style.stroke.color.kind = kFourColor;
  style.stroke.color.c.f.cyan = 0;
  style.stroke.color.c.f.magenta = 0;
  style.stroke.color.c.f.yellow = 0;
  style.stroke.color.c.f.black = 1;
  return sAIPathStyle->SetPathStyle(art, &style);
}

ASErr CreatePathArt(AIArtHandle parent, const std::vector<ModaDxf::DxfVertex>& vertices, bool closed, const std::string& name) {
  if (vertices.size() < 2) {
    return kNoErr;
  }

  AIArtHandle path = nullptr;
  AIErr error = sAIArt->NewArt(kPathArt, kPlaceInsideOnTop, parent, &path);
  if (error) {
    return error;
  }

  const auto count = static_cast<ai::int16>(std::min<size_t>(vertices.size(), 32000));
  std::vector<AIPathSegment> segments(count);
  for (ai::int16 i = 0; i < count; ++i) {
    segments[i].p = ToPoint(vertices[i].x, vertices[i].y);
    segments[i].in = segments[i].p;
    segments[i].out = segments[i].p;
    segments[i].corner = true;
  }

  error = sAIPath->SetPathSegmentCount(path, count);
  if (!error) {
    error = sAIPath->SetPathSegments(path, 0, count, segments.data());
  }
  if (!error) {
    error = sAIPath->SetPathClosed(path, closed ? true : false);
  }
  if (!error) {
    error = ApplyStrokeOnlyStyle(path);
  }
  if (!error && !name.empty()) {
    error = sAIArt->SetArtName(path, ai::UnicodeString(name));
  }
  return error;
}

ASErr CreateTextArt(AIArtHandle parent, const ModaDxf::IllustratorTextCommand& command, AIArtHandle* created = nullptr) {
  if (command.text.empty()) {
    return kNoErr;
  }

  AIArtHandle textArt = nullptr;
  const ai::int16 paintOrder =
      parent ? static_cast<ai::int16>(kPlaceInsideOnTop) : static_cast<ai::int16>(kPlaceAboveAll);
  AIErr error = sAITextFrame->NewPointText(paintOrder, parent, kHorizontalTextOrientation,
                                           ToPoint(command.x, command.y), &textArt);
  if (error) {
    return error;
  }
  if (created) {
    *created = textArt;
  }

  TextRangeRef textRange = nullptr;
  error = sAITextFrame->GetATETextRange(textArt, &textRange);
  if (!error) {
    ATE::ITextRange range(textRange);
    range.InsertAfter(ai::UnicodeString(command.text).as_ASUnicode().c_str(),
                      static_cast<ASInt32>(ai::UnicodeString(command.text).length()));
    if (command.height > 0.0) {
      ATE::ICharFeatures features;
      features.SetFontSize(std::clamp<ATETextDOM::Real>(static_cast<ATETextDOM::Real>(command.height), 0.1, 1296.0));
      range.SetLocalCharFeatures(features);
    }
  }
  if (!error && std::abs(command.rotation) > 0.001 && sAITransformArt && sAIRealMath) {
    const AIRealPoint anchor = ToPoint(command.x, command.y);
    AIRealMatrix matrix{};
    sAIRealMath->AIRealMatrixSetTranslate(&matrix, -anchor.h, -anchor.v);
    sAIRealMath->AIRealMatrixConcatRotate(&matrix, static_cast<AIReal>(command.rotation * kDegreesToRadians));
    sAIRealMath->AIRealMatrixConcatTranslate(&matrix, anchor.h, anchor.v);
    error = sAITransformArt->TransformArt(textArt, &matrix, 1.0, kTransformObjects);
  }
  if (!error) {
    error = sAIArt->SetArtName(textArt, ai::UnicodeString("TEXT " + command.layer));
  }
  return error;
}

class LayerCache {
 public:
  ASErr GetOrCreate(const std::string& layerName, AILayerHandle* layer) {
    const std::string cleanName = layerName.empty() ? "0" : layerName;
    const auto cached = layers_.find(cleanName);
    if (cached != layers_.end()) {
      *layer = cached->second;
      return kNoErr;
    }

    AILayerHandle current = nullptr;
    AIErr error = sAILayer->GetFirstLayer(&current);
    while (!error && current) {
      ai::UnicodeString title;
      if (!sAILayer->GetLayerTitle(current, title) && title.as_Roman() == cleanName) {
        layers_[cleanName] = current;
        *layer = current;
        return kNoErr;
      }
      AILayerHandle next = nullptr;
      error = sAILayer->GetNextLayer(current, &next);
      current = next;
    }

    error = sAILayer->InsertLayer(nullptr, kPlaceAboveAll, layer);
    if (!error) {
      error = sAILayer->SetLayerTitle(*layer, ai::UnicodeString(cleanName));
    }
    if (!error) {
      layers_[cleanName] = *layer;
    }
    return error;
  }

 private:
  std::map<std::string, AILayerHandle> layers_;
};

class ModaDxfPlugin : public Plugin {
 public:
  explicit ModaDxfPlugin(SPPluginRef pluginRef) : Plugin(pluginRef) {
    strncpy(fPluginName, kPluginName, kMaxStringLength);
  }

  FIXUP_VTABLE_EX(ModaDxfPlugin, Plugin);

 protected:
  ASErr Message(char* caller, char* selector, void* message) override {
    ASErr error = kNoErr;
    try {
      error = Plugin::Message(caller, selector, message);
    } catch (ai::Error& ex) {
      error = ex;
    } catch (...) {
      error = kCantHappenErr;
    }
    if (error && error != kUnhandledMsgErr && error != kCanceledErr) {
      Plugin::ReportError(error, caller, selector, message);
    }
    return error == kUnhandledMsgErr ? kNoErr : error;
  }

  ASErr StartupPlugin(SPInterfaceMessage* message) override {
    ASErr error = Plugin::StartupPlugin(message);
    if (error) {
      return error;
    }
    error = AddFileFormats(message);
    if (!error) {
      AppendRuntimeLog("ModaDXF carregado: importador de Moldes DXF registrado no Illustrator.");
    }
    return error;
  }

  ASErr GoFileFormat(AIFileFormatMessage* message) override {
    if (!(message->option & kFileFormatRead)) {
      return kNoErr;
    }

    char pathName[4096]{};
    message->GetFilePath().GetFullPath().as_Roman(pathName, sizeof(pathName));
    const bool promptOptions = !(message->option & kFileFormatSuppressUI);
    return ImportDxf(std::filesystem::path(pathName), promptOptions);
  }

 private:
  AIFileFormatHandle fileFormat_ = nullptr;

  ASErr AddFileFormats(SPInterfaceMessage* message) {
    PlatformAddFileFormatData data;
    data.title = ai::UnicodeString(kFormatName);
    data.titleOrder = 0;
    data.extension = ai::UnicodeString("dxf");
    return sAIFileFormat->AddFileFormat(message->d.self, kFormatName, &data, kFileFormatRead, &fileFormat_,
                                        kNoExtendedOptions);
  }

  ASErr ImportDxf(const std::filesystem::path& path, bool promptOptions) {
    DxfImportOptions options;
    if (promptOptions && !ShowDxfOptionsDialog(path, &options)) {
      AppendRuntimeLog("Importacao cancelada: o usuario fechou as opcoes do Molde DXF.");
      return kCanceledErr;
    }

    ModaDxfProgressPopup progress(path);
    AppendRuntimeLog("Importacao iniciada: Molde DXF selecionado no Illustrator.");
    std::ostringstream optionLog;
    optionLog << "Import options: scaleMode="
              << (options.scaleMode == DxfScaleMode::Original
                      ? "original"
                      : (options.scaleMode == DxfScaleMode::FitArtboard ? "fit-artboard" : "percent"))
              << " percent=" << options.percent << " unitScale=" << options.unitScaleSource << ":"
              << options.unitScaleTarget << " " << UnitLogName(options.unit)
              << " scaleLineWeight=" << (options.scaleLineWeight ? "yes" : "no")
              << " mergeLayers=" << (options.mergeLayers ? "yes" : "no");
    AppendRuntimeLog(optionLog.str());
    ModaDxf::DxfModel model;
    try {
      progress.Update(12, L"Lendo Molde DXF", L"Interpretando estrutura, camadas e entidades do Molde.");
      model = ModaDxf::ParseDxfFile(path);
    } catch (...) {
      progress.Complete(false, L"Nao foi possivel ler o DXF selecionado.");
      AppendRuntimeLog("Importacao interrompida: nao foi possivel ler o Molde DXF.");
      return kBadParameterErr;
    }

    progress.Update(34, L"Reconstruindo estrutura do Molde", L"Mapeando os agrupamentos principais para o Illustrator.");
    ModaDxf::IllustratorImportPlan plan = ModaDxf::BuildIllustratorImportPlan(path, model);
    LayerCache layerCache;
    std::vector<AIArtHandle> createdArtwork;

    std::wstringstream planDetail;
    planDetail << plan.principalGroups.size() << L" grupos principais planejados a partir do Molde.";
    progress.Update(52, L"Criando grupos no Illustrator", planDetail.str());

    const size_t totalGroups = std::max<size_t>(plan.principalGroups.size(), 1);
    size_t groupIndex = 0;
    for (const auto& group : plan.principalGroups) {
      const int groupProgress = 52 + static_cast<int>((static_cast<double>(groupIndex) / totalGroups) * 36.0);
      std::wstringstream groupDetail;
      groupDetail << L"Grupo " << (groupIndex + 1) << L" de " << plan.principalGroups.size() << L": "
                  << ToWideString(group.name);
      progress.Update(groupProgress, L"Renderizando peca/tamanho", groupDetail.str());

      const std::string targetLayer = options.mergeLayers ? "ModaDXF" : group.layer;
      AILayerHandle layer = nullptr;
      AIErr error = layerCache.GetOrCreate(targetLayer, &layer);
      if (error) {
        progress.Complete(false, L"Falha ao preparar layer do Illustrator.");
        return error;
      }
      error = sAILayer->SetCurrentLayer(layer);
      if (error) {
        progress.Complete(false, L"Falha ao ativar layer de destino.");
        return error;
      }

      AIArtHandle groupArt = nullptr;
      error = sAIArt->NewArt(kGroupArt, kPlaceAboveAll, nullptr, &groupArt);
      if (error) {
        progress.Complete(false, L"Falha ao criar grupo principal no Illustrator.");
        return error;
      }
      error = sAIArt->SetArtName(groupArt, ai::UnicodeString(group.name));
      if (error) {
        progress.Complete(false, L"Falha ao nomear grupo principal.");
        return error;
      }
      createdArtwork.push_back(groupArt);

      for (const auto& pathCommand : group.paths) {
        error = CreatePathArt(groupArt, pathCommand.vertices, pathCommand.closed, pathCommand.name + " " + pathCommand.layer);
        if (error) {
          progress.Complete(false, L"Falha ao criar path dentro do grupo.");
          return error;
        }
      }
      for (const auto& textCommand : group.texts) {
        error = CreateTextArt(groupArt, textCommand);
        if (error) {
          progress.Complete(false, L"Falha ao criar texto dentro do grupo.");
          return error;
        }
      }
      ++groupIndex;
    }

    progress.Update(90, L"Finalizando entidades soltas", L"Aplicando textos e diagnosticos complementares.");
    for (const auto& textCommand : plan.looseTexts) {
      const std::string targetLayer = options.mergeLayers ? "ModaDXF" : textCommand.layer;
      AILayerHandle layer = nullptr;
      AIErr error = layerCache.GetOrCreate(targetLayer, &layer);
      if (error) {
        progress.Complete(false, L"Falha ao preparar layer para texto solto.");
        return error;
      }
      error = sAILayer->SetCurrentLayer(layer);
      if (!error) {
        AIArtHandle textArt = nullptr;
        error = CreateTextArt(nullptr, textCommand, &textArt);
        if (!error) {
          createdArtwork.push_back(textArt);
        }
      }
      if (error) {
        progress.Complete(false, L"Falha ao criar texto solto.");
        return error;
      }
    }

    AIErr transformError = ApplyImportOptionsToArtwork(createdArtwork, options);
    if (transformError) {
      progress.Complete(false, L"Falha ao aplicar opcoes de dimensionamento do DXF.");
      return transformError;
    }

    progress.Update(96, L"Gravando logs ModaDXF", L"Registrando grupos, camadas, paths, textos, pontos e avisos.");
    WriteDiagnostic(path, model, plan);
    std::wstringstream doneDetail;
    doneDetail << plan.principalGroups.size()
               << L" grupos reconstruidos. Log salvo em %LOCALAPPDATA%\\ModaDXF\\logs.";
    progress.Complete(true, doneDetail.str());
    AppendRuntimeLog("Importacao concluida: Molde reconstruido em grupos no Illustrator.");
    return kNoErr;
  }
};

}  // namespace

Plugin* AllocatePlugin(SPPluginRef pluginRef) {
  return new ModaDxfPlugin(pluginRef);
}

void FixupReload(Plugin* plugin) {
  ModaDxfPlugin::FixupVTable(static_cast<ModaDxfPlugin*>(plugin));
}
