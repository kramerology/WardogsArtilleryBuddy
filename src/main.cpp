#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <shellapi.h>
#include <tlhelp32.h>
#include <dwmapi.h>

#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Graphics.Imaging.h>
#include <winrt/Windows.Media.Ocr.h>
#include <winrt/Windows.Storage.Streams.h>

#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <memory>
#include <iomanip>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "Gdi32.lib")
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "User32.lib")
#pragma comment(lib, "WindowsApp.lib")
#pragma comment(lib, "Dwmapi.lib")

namespace
{
constexpr wchar_t kMainClassName[] = L"WarDogsArtillery.MainWindow";
constexpr wchar_t kOverlayClassName[] = L"WarDogsArtillery.OverlayWindow";
constexpr wchar_t kRangeOverlayClassName[] = L"WarDogsArtillery.RangeOverlayWindow";

constexpr UINT kTrayMessage = WM_APP + 1;
constexpr UINT kTrayId = 1;
constexpr UINT_PTR kGamePollTimer = 1;
constexpr UINT kGamePollIntervalMs = 250;
constexpr UINT kOcrResultMessage = WM_APP + 2;
constexpr wchar_t kTargetProcessName[] = L"WardogsClient-Win64-Shipping.exe";

constexpr int kHotkeyFirst = 1;
constexpr int kHotkeySecond = 2;
constexpr int kHotkeyToggleOverlay = 3;
constexpr int kHotkeyCopyDistance = 4;
constexpr int kHotkeyToggleClickThrough = 5;

constexpr int kCommandCaptureFirst = 1001;
constexpr int kCommandCaptureSecond = 1002;
constexpr int kCommandToggleOverlay = 1003;
constexpr int kCommandCopyDistance = 1004;
constexpr int kCommandToggleClickThrough = 1005;
constexpr int kCommandExit = 1006;
constexpr int kControlRangeScale = 2001;

constexpr double kOcrScale = 2.0;
constexpr double kDefaultMetersPerCoordinateUnit = 100.0;

// Fixed L81 mortar range marks, from the lowest to the highest reference
// label. These are the values printed on the in-game ladder; they are not
// discrete firing setpoints. The game accepts the continuous range between
// them, so the nearest label is used only as a visual reference and the
// target's offset is interpolated between adjacent labels.
constexpr double kRangeReferenceValues[] = {
    132.0, 187.0, 240.0, 290.0, 340.0, 385.0, 430.0,
    470.0, 510.0, 545.0, 578.0, 609.0, 637.0, 661.0, 684.0};

// The range ladder moves behind the optic. An exact reference value belongs
// at the center aiming point of the 1920x1080 scope (about 540 px from the
// top); the guide is offset from there when the chosen reference value is only
// an approximation of the requested range.
constexpr double kRangeMarkerCenterFraction = 0.5;
// Adjacent supplied reference labels are 102 px apart in the 1920x1080
// screenshots. Fractions keep the guide proportional when the game window is
// resized.
constexpr double kRangeLineSpacingFraction = 102.0 / 1080.0;
// Keep the marker inside the left side of the reticle. The left edge reaches
// the ladder line, while the right edge leaves the label just to its right.
constexpr double kRangeMarkerBarLeftFraction = 0.369;
constexpr double kRangeMarkerBarRightFraction = 0.407;
constexpr double kRangeMarkerLabelGapFraction = 0.011;

// The horizontal traverse ladder uses 15-degree reference marks. The 0/360
// seam is circular, so the nearest reference is selected using a wrapped
// angular distance rather than a linear range lookup.
constexpr double kAngleStepDegrees = 15.0;
constexpr double kFullCircleDegrees = 360.0;
constexpr double kAngleReferenceValues[] = {
    0.0, 15.0, 30.0, 45.0, 60.0, 75.0, 90.0, 105.0,
    120.0, 135.0, 150.0, 165.0, 180.0, 195.0, 210.0, 225.0,
    240.0, 255.0, 270.0, 285.0, 300.0, 315.0, 330.0, 345.0};

// Adjacent horizontal ladder labels are about 84 px apart in the 1920 px
// wide mockup. The fraction keeps the guide proportional when the game window
// is resized.
constexpr double kAngleMarkerCenterFraction = 0.5;
constexpr double kAngleLineSpacingFraction = 84.0 / 1920.0;

// The vertical guide sits beneath the compass ladder, clear of the center
// reticle and the range labels, matching the supplied mockup.
constexpr double kAngleMarkerTopFraction = 0.312;
constexpr double kAngleMarkerBottomFraction = 0.344;
constexpr double kAngleMarkerLabelGapFraction = 0.005;

struct Coordinate
{
    double x{};
    double y{};
};

struct CapturedImage
{
    int width{};
    int height{};
    int stride{};
    std::vector<std::uint8_t> pixels;
};

struct CapturedRegion
{
    CapturedImage image;
    int cropLeft{};
    int cropTop{};
    int clientWidth{};
    int clientHeight{};
};

struct OcrResult
{
    bool firstPoint{};
    bool coordinateFound{};
    Coordinate coordinate{};
    std::wstring recognizedText;
    std::wstring error;
};

struct AppState
{
    HINSTANCE instance{};
    HWND mainWindow{};
    HWND overlayWindow{};
    HWND rangeOverlayWindow{};
    HWND targetWindow{};
    DWORD targetProcessId{};

    HWND rangeScaleEdit{};
    HFONT uiFont{};
    std::wstring status{L"Open the map, then capture player and target positions."};

    NOTIFYICONDATAW tray{};

    std::optional<Coordinate> first;
    std::optional<Coordinate> second;
    std::optional<double> distance;
    std::optional<double> bearing;
    std::wstring compassDirection;

    int targetClientWidth{};
    int targetClientHeight{};

    bool overlayEnabled{true};
    bool overlayVisible{false};
    bool clickThrough{true};
    bool closing{false};
    std::atomic_bool ocrInProgress{false};
};

AppState* g_app = nullptr;

std::wstring FormatNumber(double value, int precision = 2)
{
    if (std::abs(value) < 0.0000005)
    {
        value = 0.0;
    }

    std::wostringstream output;
    output << std::fixed << std::setprecision(precision) << value;
    return output.str();
}

std::wstring FormatCoordinate(const Coordinate& coordinate)
{
    return L"X: " + FormatNumber(coordinate.x, 3) + L"    Y: " +
           FormatNumber(coordinate.y, 3);
}

double NormalizeAngleDegrees(double angle)
{
    if (!std::isfinite(angle))
    {
        return angle;
    }

    angle = std::fmod(angle, kFullCircleDegrees);
    if (angle < 0.0)
    {
        angle += kFullCircleDegrees;
    }

    return angle;
}

double SignedAngleDifferenceDegrees(double angle, double reference)
{
    double difference = NormalizeAngleDegrees(angle - reference);
    if (difference > kFullCircleDegrees / 2.0)
    {
        difference -= kFullCircleDegrees;
    }

    return difference;
}

struct DirectionResult
{
    double bearing{};
    const wchar_t* compass{};
};

DirectionResult CalculateDirection(
    const Coordinate& player,
    const Coordinate& target)
{
    // The game's coordinate system is X=east/right and Y=north/up.
    // Bearing is measured clockwise from north.
    constexpr double kPi = 3.14159265358979323846;
    constexpr const wchar_t* kCompassPoints[] = {
        L"N", L"NE", L"E", L"SE", L"S", L"SW", L"W", L"NW"};

    const double dx = target.x - player.x;
    const double dy = target.y - player.y;
    double bearing = std::atan2(dx, dy) * 180.0 / kPi;
    if (bearing < 0.0)
    {
        bearing += 360.0;
    }

    const int sector = static_cast<int>((bearing + 22.5) / 45.0) % 8;
    return DirectionResult{bearing, kCompassPoints[sector]};
}

std::optional<size_t> SelectNearestAngleReference(double targetBearing)
{
    if (!std::isfinite(targetBearing))
    {
        return std::nullopt;
    }

    const double normalizedBearing = NormalizeAngleDegrees(targetBearing);
    size_t bestIndex = 0;
    double bestDifference = std::abs(
        SignedAngleDifferenceDegrees(normalizedBearing, kAngleReferenceValues[0]));

    for (size_t index = 1; index < _countof(kAngleReferenceValues); ++index)
    {
        const double difference = std::abs(
            SignedAngleDifferenceDegrees(
                normalizedBearing, kAngleReferenceValues[index]));
        // On an exact tie, keep the lower table entry. This also gives the
        // 0-degree label a stable preference at the 360-degree seam.
        if (difference < bestDifference)
        {
            bestDifference = difference;
            bestIndex = index;
        }
    }

    return bestIndex;
}

std::optional<double> NearestAngleReference(double targetBearing)
{
    const auto index = SelectNearestAngleReference(targetBearing);
    if (!index.has_value())
    {
        return std::nullopt;
    }

    return kAngleReferenceValues[*index];
}

std::optional<double> CalculateAngleGuideFraction(double targetBearing)
{
    const auto reference = NearestAngleReference(targetBearing);
    if (!reference.has_value())
    {
        return std::nullopt;
    }

    // Increasing compass angles run left-to-right on the horizontal ladder.
    // Move the selected printed step in the opposite direction of the
    // target's fractional offset so that the target angle lands at center.
    const double relativeStep =
        SignedAngleDifferenceDegrees(targetBearing, *reference) /
        kAngleStepDegrees;
    return std::clamp(
        kAngleMarkerCenterFraction -
            (relativeStep * kAngleLineSpacingFraction),
        0.0,
        1.0);
}

std::wstring FormatDirection(const DirectionResult& direction)
{
    return FormatNumber(direction.bearing, 0) + L"\u00B0 " + direction.compass;
}

bool SetClipboardText(const std::wstring& text)
{
    if (!OpenClipboard(g_app != nullptr ? g_app->mainWindow : nullptr))
    {
        return false;
    }

    EmptyClipboard();

    const SIZE_T bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL storage = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (storage == nullptr)
    {
        CloseClipboard();
        return false;
    }

    auto* destination = static_cast<wchar_t*>(GlobalLock(storage));
    if (destination == nullptr)
    {
        GlobalFree(storage);
        CloseClipboard();
        return false;
    }

    std::memcpy(destination, text.c_str(), bytes);
    GlobalUnlock(storage);

    if (SetClipboardData(CF_UNICODETEXT, storage) == nullptr)
    {
        GlobalFree(storage);
        CloseClipboard();
        return false;
    }

    CloseClipboard();
    return true;
}

std::optional<Coordinate> ParseOcrCoordinate(const std::wstring& text)
{
    // OCR normally returns the two labels on separate lines, for example:
    //   x100.85
    //   y105.89
    // Allow whitespace or punctuation between each label and its value.
    const std::wstring number = LR"([-+]?(?:\d+(?:\.\d*)?|\.\d+))";

    try
    {
        const std::wregex xPattern(L"[xX]\\s*[:=]?\\s*(" + number + L")");
        const std::wregex yPattern(L"[yY]\\s*[:=]?\\s*(" + number + L")");
        std::wsmatch xMatch;
        std::wsmatch yMatch;

        if (std::regex_search(text, xMatch, xPattern) && xMatch.size() >= 2 &&
            std::regex_search(text, yMatch, yPattern) && yMatch.size() >= 2)
        {
            const double x = std::stod(xMatch[1].str());
            const double y = std::stod(yMatch[1].str());
            if (std::isfinite(x) && std::isfinite(y))
            {
                return Coordinate{x, y};
            }
        }
    }
    catch (const std::exception&)
    {
        // Invalid numeric text is handled as a parse failure below.
    }

    return std::nullopt;
}

void SetStatus(const std::wstring& message)
{
    if (g_app != nullptr)
    {
        g_app->status = message;
        InvalidateRect(g_app->mainWindow, nullptr, FALSE);
    }
}

std::wstring OverlayStateText()
{
    if (g_app == nullptr)
    {
        return L"Overlay: unavailable";
    }

    std::wstring state;
    if (!g_app->overlayEnabled)
    {
        state = L"disabled by setting";
    }
    else if (g_app->targetWindow == nullptr)
    {
        state = L"waiting for WardogsClient-Win64-Shipping.exe";
    }
    else if (g_app->overlayVisible)
    {
        state = L"active over WardogsClient-Win64-Shipping.exe";
    }
    else
    {
        state = L"waiting for the game window";
    }

    return L"Overlay: " + state + L" | Click-through: " +
           (g_app->clickThrough ? L"on" : L"off");
}

void UpdateOverlay()
{
    if (g_app != nullptr && g_app->overlayWindow != nullptr)
    {
        InvalidateRect(g_app->overlayWindow, nullptr, TRUE);
    }

    if (g_app != nullptr && g_app->rangeOverlayWindow != nullptr)
    {
        InvalidateRect(g_app->rangeOverlayWindow, nullptr, TRUE);
    }
}

double ReadRangeScaleMeters()
{
    if (g_app == nullptr || g_app->rangeScaleEdit == nullptr)
    {
        return kDefaultMetersPerCoordinateUnit;
    }

    wchar_t buffer[64]{};
    GetWindowTextW(g_app->rangeScaleEdit, buffer, _countof(buffer));

    try
    {
        const double value = std::stod(buffer);
        if (std::isfinite(value) && value > 0.0 && value <= 100000.0)
        {
            return value;
        }
    }
    catch (const std::exception&)
    {
        // Treat an empty or partially edited field as the default for now.
    }

    return kDefaultMetersPerCoordinateUnit;
}

std::optional<double> CurrentRangeTargetMeters()
{
    if (g_app == nullptr || !g_app->distance.has_value())
    {
        return std::nullopt;
    }

    const double target = *g_app->distance * ReadRangeScaleMeters();
    if (!std::isfinite(target) || target < 0.0)
    {
        return std::nullopt;
    }

    return target;
}

std::optional<size_t> SelectNearestRangeReference(double targetMeters)
{
    if (!std::isfinite(targetMeters) ||
        targetMeters < kRangeReferenceValues[0] ||
        targetMeters > kRangeReferenceValues[_countof(kRangeReferenceValues) - 1])
    {
        return std::nullopt;
    }

    size_t bestIndex = 0;
    double bestDifference =
        std::abs(kRangeReferenceValues[0] - targetMeters);
    for (size_t index = 1; index < _countof(kRangeReferenceValues); ++index)
    {
        const double difference =
            std::abs(kRangeReferenceValues[index] - targetMeters);
        // On an exact tie, keep the lower reference. Its marker moves down
        // toward the next label, which is the same visual convention used for
        // targets just above that reference.
        if (difference < bestDifference)
        {
            bestDifference = difference;
            bestIndex = index;
        }
    }

    return bestIndex;
}

std::optional<double> NearestRangeReference(double targetMeters)
{
    const auto index = SelectNearestRangeReference(targetMeters);
    if (!index.has_value())
    {
        return std::nullopt;
    }

    return kRangeReferenceValues[*index];
}

std::optional<double> CalculateRangeGuideFraction(double targetMeters)
{
    const auto index = SelectNearestRangeReference(targetMeters);
    if (!index.has_value())
    {
        return std::nullopt;
    }

    const double reference = kRangeReferenceValues[*index];
    double relativeStep = 0.0;

    if (targetMeters > reference &&
        *index + 1 < _countof(kRangeReferenceValues))
    {
        const double next = kRangeReferenceValues[*index + 1];
        relativeStep = (targetMeters - reference) / (next - reference);
    }
    else if (targetMeters < reference && *index > 0)
    {
        const double previous = kRangeReferenceValues[*index - 1];
        relativeStep = (targetMeters - reference) / (reference - previous);
    }

    // A larger range is above a smaller range in the scope. Therefore, when
    // the target is larger than the selected reference, the selected line
    // must be placed below the optic center, and vice versa.
    return std::clamp(
        kRangeMarkerCenterFraction +
            (relativeStep * kRangeLineSpacingFraction),
        0.0,
        1.0);
}

std::wstring RangeCalibrationText()
{
    if (g_app == nullptr)
    {
        return L"RNG: unavailable";
    }

    const auto target = CurrentRangeTargetMeters();
    if (!target.has_value())
    {
        return L"RNG: waiting for two coordinates";
    }

    const auto reference = NearestRangeReference(*target);
    if (!reference.has_value())
    {
        return L"RNG: target outside L81 range (" +
               FormatNumber(kRangeReferenceValues[0], 0) + L"-" +
               FormatNumber(kRangeReferenceValues[_countof(kRangeReferenceValues) - 1], 0) +
               L"M)";
    }

    return L"RNG: align " + FormatNumber(*reference, 0) +
           L"M at marker for target " + FormatNumber(*target, 1) + L" m";
}

void UpdateDisplay()
{
    if (g_app == nullptr)
    {
        return;
    }

    UpdateOverlay();
    InvalidateRect(g_app->mainWindow, nullptr, FALSE);
    InvalidateRect(GetDlgItem(g_app->mainWindow, kCommandToggleOverlay), nullptr, TRUE);
    InvalidateRect(GetDlgItem(g_app->mainWindow, kCommandToggleClickThrough), nullptr, TRUE);
}

DWORD FindTargetProcessId()
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
    {
        return 0;
    }

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);

    DWORD processId = 0;
    if (Process32FirstW(snapshot, &entry) != FALSE)
    {
        do
        {
            if (_wcsicmp(entry.szExeFile, kTargetProcessName) == 0)
            {
                processId = entry.th32ProcessID;
                break;
            }
        } while (Process32NextW(snapshot, &entry) != FALSE);
    }

    CloseHandle(snapshot);
    return processId;
}

struct MainWindowSearch
{
    DWORD processId{};
    HWND window{};
};

BOOL CALLBACK FindMainWindowForProcess(HWND window, LPARAM parameter)
{
    auto* search = reinterpret_cast<MainWindowSearch*>(parameter);
    DWORD processId = 0;
    GetWindowThreadProcessId(window, &processId);

    if (processId != search->processId || !IsWindowVisible(window) ||
        GetWindow(window, GW_OWNER) != nullptr)
    {
        return TRUE;
    }

    search->window = window;
    return FALSE;
}

HWND FindTargetMainWindow(DWORD processId)
{
    if (processId == 0)
    {
        return nullptr;
    }

    MainWindowSearch search{processId, nullptr};
    EnumWindows(FindMainWindowForProcess, reinterpret_cast<LPARAM>(&search));
    return search.window;
}

void RefreshTargetGame()
{
    if (g_app == nullptr || g_app->overlayWindow == nullptr)
    {
        return;
    }

    const DWORD processId = FindTargetProcessId();
    const HWND targetWindow = FindTargetMainWindow(processId);

    const bool oldVisible = g_app->overlayVisible;
    const HWND oldTargetWindow = g_app->targetWindow;
    g_app->targetProcessId = processId;
    g_app->targetWindow = targetWindow;

    bool shouldShow = g_app->overlayEnabled && targetWindow != nullptr &&
                      IsWindowVisible(targetWindow) && !IsIconic(targetWindow);

    if (shouldShow)
    {
        RECT clientRect{};
        POINT position{0, 0};
        if (!GetClientRect(targetWindow, &clientRect) ||
            !ClientToScreen(targetWindow, &position))
        {
            shouldShow = false;
        }

        const int clientWidth = clientRect.right - clientRect.left;
        const int clientHeight = clientRect.bottom - clientRect.top;
        if (shouldShow && (clientWidth <= 0 || clientHeight <= 0))
        {
            shouldShow = false;
        }

        if (shouldShow)
        {
            g_app->targetClientWidth = clientWidth;
            g_app->targetClientHeight = clientHeight;

            const int infoWidth = 360;
            const int infoHeight = 188;
            const int infoX = position.x + std::min(24, std::max(0, clientWidth - infoWidth));
            const int infoY = position.y + std::min(24, std::max(0, clientHeight - infoHeight));

            if (g_app->rangeOverlayWindow != nullptr)
            {
                SetWindowPos(
                    g_app->rangeOverlayWindow,
                    HWND_TOPMOST,
                    position.x,
                    position.y,
                    clientWidth,
                    clientHeight,
                    SWP_NOACTIVATE | SWP_SHOWWINDOW);
            }

            // Keep the information panel above the guide if their regions
            // overlap on a smaller display.
            SetWindowPos(
                g_app->overlayWindow,
                HWND_TOPMOST,
                infoX,
                infoY,
                infoWidth,
                infoHeight,
                SWP_NOACTIVATE | SWP_SHOWWINDOW);
            g_app->overlayVisible = true;
        }
    }

    if (!shouldShow)
    {
        ShowWindow(g_app->overlayWindow, SW_HIDE);
        if (g_app->rangeOverlayWindow != nullptr)
        {
            ShowWindow(g_app->rangeOverlayWindow, SW_HIDE);
        }
        g_app->overlayVisible = false;
        g_app->targetClientWidth = 0;
        g_app->targetClientHeight = 0;
    }

    if (oldVisible != g_app->overlayVisible || oldTargetWindow != g_app->targetWindow)
    {
        UpdateDisplay();
    }
}

std::optional<CapturedRegion> CaptureClientRegion(
    HWND targetWindow,
    double leftFraction,
    double topFraction,
    double rightFraction,
    double bottomFraction,
    std::wstring& error)
{
    if (g_app == nullptr || targetWindow == nullptr || !IsWindow(targetWindow))
    {
        error = L"The game window is not available.";
        return std::nullopt;
    }

    DWORD foregroundProcessId = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &foregroundProcessId);
    if (foregroundProcessId != g_app->targetProcessId)
    {
        error = L"Bring WardogsClient to the foreground before pressing the hotkey.";
        return std::nullopt;
    }

    RECT clientRect{};
    if (!GetClientRect(targetWindow, &clientRect))
    {
        error = L"Could not determine the game client area.";
        return std::nullopt;
    }

    const int clientWidth = clientRect.right - clientRect.left;
    const int clientHeight = clientRect.bottom - clientRect.top;
    if (clientWidth <= 0 || clientHeight <= 0)
    {
        error = L"The game client area is empty.";
        return std::nullopt;
    }

    // In the supplied game layout, the coordinate labels are inside the
    // centered map panel. Keep the capture broad enough for window resizing,
    // while excluding the top-left utility overlay and most HUD elements.
    const int cropLeft = std::clamp(
        static_cast<int>(std::lround(clientWidth * leftFraction)),
        0,
        clientWidth - 1);
    const int cropTop = std::clamp(
        static_cast<int>(std::lround(clientHeight * topFraction)),
        0,
        clientHeight - 1);
    const int cropRight = std::clamp(
        static_cast<int>(std::lround(clientWidth * rightFraction)),
        cropLeft + 1,
        clientWidth);
    const int cropBottom = std::clamp(
        static_cast<int>(std::lround(clientHeight * bottomFraction)),
        cropTop + 1,
        clientHeight);
    const int captureWidth = cropRight - cropLeft;
    const int captureHeight = cropBottom - cropTop;

    POINT screenOrigin{0, 0};
    if (!ClientToScreen(targetWindow, &screenOrigin))
    {
        error = L"Could not locate the game on the screen.";
        return std::nullopt;
    }

    HDC screenDc = GetDC(nullptr);
    HDC memoryDc = screenDc != nullptr ? CreateCompatibleDC(screenDc) : nullptr;
    HBITMAP bitmap = screenDc != nullptr
        ? CreateCompatibleBitmap(screenDc, captureWidth, captureHeight)
        : nullptr;
    if (screenDc == nullptr || memoryDc == nullptr || bitmap == nullptr)
    {
        if (bitmap != nullptr)
        {
            DeleteObject(bitmap);
        }
        if (memoryDc != nullptr)
        {
            DeleteDC(memoryDc);
        }
        if (screenDc != nullptr)
        {
            ReleaseDC(nullptr, screenDc);
        }
        error = L"Could not allocate a screen capture buffer.";
        return std::nullopt;
    }

    HGDIOBJ previousBitmap = SelectObject(memoryDc, bitmap);
    const BOOL copied = BitBlt(
        memoryDc,
        0,
        0,
        captureWidth,
        captureHeight,
        screenDc,
        screenOrigin.x + cropLeft,
        screenOrigin.y + cropTop,
        SRCCOPY | CAPTUREBLT);
    SelectObject(memoryDc, previousBitmap);

    CapturedImage image;
    image.width = captureWidth;
    image.height = captureHeight;
    image.stride = captureWidth * 4;
    image.pixels.resize(static_cast<size_t>(image.stride) * image.height);

    BITMAPINFO bitmapInfo{};
    bitmapInfo.bmiHeader.biSize = sizeof(bitmapInfo.bmiHeader);
    bitmapInfo.bmiHeader.biWidth = captureWidth;
    bitmapInfo.bmiHeader.biHeight = -captureHeight;
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;

    const int scanLines = copied
        ? GetDIBits(
              screenDc,
              bitmap,
              0,
              static_cast<UINT>(captureHeight),
              image.pixels.data(),
              &bitmapInfo,
              DIB_RGB_COLORS)
        : 0;

    DeleteObject(bitmap);
    DeleteDC(memoryDc);
    ReleaseDC(nullptr, screenDc);

    if (!copied || scanLines == 0)
    {
        error = L"Could not capture the game window.";
        return std::nullopt;
    }

    CapturedRegion region;
    region.image = std::move(image);
    region.cropLeft = cropLeft;
    region.cropTop = cropTop;
    region.clientWidth = clientWidth;
    region.clientHeight = clientHeight;
    return region;
}

std::optional<CapturedImage> CaptureMapRegion(HWND targetWindow, std::wstring& error)
{
    // The coordinate labels are inside the centered map panel. Keep this
    // broad enough for resizing while excluding the top-left app overlay.
    auto region = CaptureClientRegion(
        targetWindow,
        0.20,
        0.08,
        0.80,
        0.92,
        error);
    if (!region.has_value())
    {
        return std::nullopt;
    }

    return std::move(region->image);
}

enum class OcrImageMode
{
    Original,
    BrightTextThreshold,
    DarkTextThreshold,
    LocalDarkTextThreshold,
};

CapturedImage PrepareOcrImage(
    const CapturedImage& source,
    OcrImageMode mode,
    int scale,
    bool blackForeground)
{
    CapturedImage prepared;
    prepared.width = source.width * scale;
    prepared.height = source.height * scale;
    prepared.stride = prepared.width * 4;
    prepared.pixels.resize(static_cast<size_t>(prepared.stride) * prepared.height);

    std::vector<std::uint8_t> luminances;
    std::vector<int> integral;
    if (mode == OcrImageMode::LocalDarkTextThreshold)
    {
        luminances.resize(static_cast<size_t>(source.width) * source.height);
        integral.resize(
            static_cast<size_t>(source.width + 1) * (source.height + 1),
            0);

        for (int sourceY = 0; sourceY < source.height; ++sourceY)
        {
            int rowSum = 0;
            for (int sourceX = 0; sourceX < source.width; ++sourceX)
            {
                const auto* sourcePixel = source.pixels.data() +
                    (static_cast<size_t>(sourceY) * source.stride) +
                    (static_cast<size_t>(sourceX) * 4);
                const int luminance =
                    (sourcePixel[2] * 299 + sourcePixel[1] * 587 + sourcePixel[0] * 114) /
                    1000;
                luminances[
                    static_cast<size_t>(sourceY) * source.width + sourceX] =
                    static_cast<std::uint8_t>(luminance);
                rowSum += luminance;
                integral[
                    static_cast<size_t>(sourceY + 1) * (source.width + 1) + sourceX + 1] =
                    integral[
                        static_cast<size_t>(sourceY) * (source.width + 1) + sourceX + 1] +
                    rowSum;
            }
        }
    }

    for (int y = 0; y < prepared.height; ++y)
    {
        const int sourceY = y / scale;
        for (int x = 0; x < prepared.width; ++x)
        {
            const int sourceX = x / scale;
            const auto* sourcePixel = source.pixels.data() +
                (static_cast<size_t>(sourceY) * source.stride) +
                (static_cast<size_t>(sourceX) * 4);
            auto* destinationPixel = prepared.pixels.data() +
                (static_cast<size_t>(y) * prepared.stride) +
                (static_cast<size_t>(x) * 4);

            if (mode != OcrImageMode::Original)
            {
                const int luminance = luminances.empty()
                    ? (sourcePixel[2] * 299 + sourcePixel[1] * 587 + sourcePixel[0] * 114) /
                          1000
                    : luminances[
                          static_cast<size_t>(sourceY) * source.width + sourceX];
                bool foreground = false;
                if (mode == OcrImageMode::BrightTextThreshold)
                {
                    foreground = luminance >= 180;
                }
                else if (mode == OcrImageMode::DarkTextThreshold)
                {
                    foreground = luminance <= 105;
                }
                else
                {
                    constexpr int kLocalRadius = 7;
                    constexpr int kDarkTextMargin = 18;
                    const int left = std::max(0, sourceX - kLocalRadius);
                    const int top = std::max(0, sourceY - kLocalRadius);
                    const int right = std::min(source.width - 1, sourceX + kLocalRadius);
                    const int bottom = std::min(source.height - 1, sourceY + kLocalRadius);
                    const int area = (right - left + 1) * (bottom - top + 1);
                    const auto sumAt = [&integral, sourceWidth = source.width](int x, int y)
                    {
                        return integral[
                            static_cast<size_t>(y) * (sourceWidth + 1) + x];
                    };
                    const int localSum =
                        sumAt(right + 1, bottom + 1) -
                        sumAt(left, bottom + 1) -
                        sumAt(right + 1, top) +
                        sumAt(left, top);
                    const int localMean = area > 0 ? localSum / area : luminance;
                    foreground = luminance + kDarkTextMargin < localMean;
                }

                const std::uint8_t value = blackForeground
                    ? (foreground ? 0 : 255)
                    : (foreground ? 255 : 0);
                destinationPixel[0] = value;
                destinationPixel[1] = value;
                destinationPixel[2] = value;
            }
            else
            {
                destinationPixel[0] = sourcePixel[0];
                destinationPixel[1] = sourcePixel[1];
                destinationPixel[2] = sourcePixel[2];
            }

            destinationPixel[3] = 255;
        }
    }

    return prepared;
}

winrt::Windows::Graphics::Imaging::SoftwareBitmap MakeSoftwareBitmap(
    const CapturedImage& image)
{
    using namespace winrt::Windows::Graphics::Imaging;
    using namespace winrt::Windows::Storage::Streams;

    const auto byteCount = static_cast<std::uint32_t>(image.pixels.size());
    Buffer buffer(byteCount);
    buffer.Length(byteCount);

    auto byteAccess = buffer.as<winrt::impl::IBufferByteAccess>();
    std::uint8_t* destination = nullptr;
    winrt::check_hresult(byteAccess->Buffer(&destination));
    std::memcpy(destination, image.pixels.data(), image.pixels.size());

    return SoftwareBitmap::CreateCopyFromBuffer(
        buffer,
        BitmapPixelFormat::Bgra8,
        image.width,
        image.height,
        BitmapAlphaMode::Ignore);
}

std::wstring RecognizeImageText(const CapturedImage& image, std::wstring& error)
{
    struct ApartmentGuard
    {
        bool initialized{false};

        ApartmentGuard()
        {
            winrt::init_apartment(winrt::apartment_type::multi_threaded);
            initialized = true;
        }

        ~ApartmentGuard()
        {
            if (initialized)
            {
                winrt::uninit_apartment();
            }
        }
    };

    try
    {
        ApartmentGuard apartment;
        auto engine = winrt::Windows::Media::Ocr::OcrEngine::TryCreateFromUserProfileLanguages();
        if (engine == nullptr)
        {
            error = L"Windows OCR is unavailable. Install an OCR language pack in Windows Settings.";
            return {};
        }

        std::wstring allRecognizedText;
        for (const OcrImageMode mode : {
                 // Keep the unmodified map first.  The thresholded passes
                 // are useful fallbacks, but can distort small decimal
                 // glyphs (for example, turning 35 into 55).
                 OcrImageMode::Original,
                 OcrImageMode::BrightTextThreshold,
                 OcrImageMode::DarkTextThreshold,
                 OcrImageMode::LocalDarkTextThreshold})
        {
            const CapturedImage prepared = PrepareOcrImage(
                image,
                mode,
                static_cast<int>(kOcrScale),
                false);
            const auto bitmap = MakeSoftwareBitmap(prepared);
            const auto result = engine.RecognizeAsync(bitmap).get();
            const std::wstring recognized = result.Text().c_str();

            if (!allRecognizedText.empty() && !recognized.empty())
            {
                allRecognizedText += L"\n";
            }
            allRecognizedText += recognized;
        }

        error = L"OCR completed, but no x###.## and y###.## coordinate pair was found.";
        return allRecognizedText;
    }
    catch (const winrt::hresult_error& exception)
    {
        error = L"Windows OCR failed: ";
        error += exception.message().c_str();
    }
    catch (const std::exception& exception)
    {
        error = L"OCR failed: ";
        const std::string message = exception.what();
        error += std::wstring(message.begin(), message.end());
    }

    return {};
}

void ApplyCapturedCoordinate(bool firstPoint, const Coordinate& coordinate)
{
    if (g_app == nullptr)
    {
        return;
    }

    if (firstPoint)
    {
        g_app->first = coordinate;
        g_app->second.reset();
        g_app->distance.reset();
        g_app->bearing.reset();
        g_app->compassDirection.clear();
        SetStatus(L"Captured the player's position. Press Hotkey 2 over the target position.");
    }
    else
    {
        if (!g_app->first.has_value())
        {
            SetStatus(L"Capture the player's position with Hotkey 1 first.");
            MessageBeep(MB_ICONWARNING);
            return;
        }

        g_app->second = coordinate;
        const double dx = coordinate.x - g_app->first->x;
        const double dy = coordinate.y - g_app->first->y;
        g_app->distance = std::hypot(dx, dy);
        const DirectionResult direction = CalculateDirection(*g_app->first, coordinate);
        g_app->bearing = direction.bearing;
        g_app->compassDirection = direction.compass;
        SetStatus(
            L"Distance: " + FormatNumber(*g_app->distance, 2) + L" | Direction: " +
            FormatDirection(direction));
    }

    UpdateDisplay();
}

bool BringTargetGameToForeground(std::wstring& error)
{
    if (g_app == nullptr || g_app->targetWindow == nullptr ||
        !IsWindow(g_app->targetWindow))
    {
        error = L"The game window is not available.";
        return false;
    }

    if (IsIconic(g_app->targetWindow))
    {
        ShowWindow(g_app->targetWindow, SW_RESTORE);
    }

    DWORD foregroundProcessId = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &foregroundProcessId);
    if (foregroundProcessId != g_app->targetProcessId)
    {
        SetForegroundWindow(g_app->targetWindow);
        BringWindowToTop(g_app->targetWindow);
        Sleep(150);
    }

    foregroundProcessId = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &foregroundProcessId);
    if (foregroundProcessId != g_app->targetProcessId)
    {
        error = L"Bring WardogsClient to the foreground before pressing the hotkey.";
        return false;
    }

    return true;
}

void StartOcrCapture(bool firstPoint)
{
    if (g_app == nullptr)
    {
        return;
    }

    if (g_app->ocrInProgress.exchange(true))
    {
        SetStatus(L"An OCR capture is already in progress.");
        return;
    }

    RefreshTargetGame();
    if (g_app->targetWindow == nullptr || !IsWindow(g_app->targetWindow))
    {
        g_app->ocrInProgress = false;
        SetStatus(L"WardogsClient-Win64-Shipping.exe is not running or has no visible window.");
        MessageBeep(MB_ICONWARNING);
        return;
    }

    std::wstring focusError;
    if (!BringTargetGameToForeground(focusError))
    {
        g_app->ocrInProgress = false;
        SetStatus(focusError);
        MessageBeep(MB_ICONWARNING);
        return;
    }

    const bool infoOverlayWasVisible =
        g_app->overlayWindow != nullptr && IsWindowVisible(g_app->overlayWindow) != FALSE;
    const bool rangeOverlayWasVisible =
        g_app->rangeOverlayWindow != nullptr &&
        IsWindowVisible(g_app->rangeOverlayWindow) != FALSE;
    if (infoOverlayWasVisible)
    {
        ShowWindow(g_app->overlayWindow, SW_HIDE);
    }
    if (rangeOverlayWasVisible)
    {
        ShowWindow(g_app->rangeOverlayWindow, SW_HIDE);
    }

    std::wstring captureError;
    const auto captured = CaptureMapRegion(g_app->targetWindow, captureError);

    if (infoOverlayWasVisible)
    {
        ShowWindow(g_app->overlayWindow, SW_SHOWNOACTIVATE);
    }
    if (rangeOverlayWasVisible)
    {
        ShowWindow(g_app->rangeOverlayWindow, SW_SHOWNOACTIVATE);
    }
    UpdateOverlay();

    if (!captured.has_value())
    {
        g_app->ocrInProgress = false;
        SetStatus(captureError);
        MessageBeep(MB_ICONWARNING);
        return;
    }

    const HWND mainWindow = g_app->mainWindow;
    SetStatus(firstPoint ? L"Reading the player's position from the game map..."
                         : L"Reading the target position from the game map...");

    std::thread(
        [mainWindow, firstPoint, image = std::move(*captured)]() mutable
        {
            auto* result = new OcrResult();
            result->firstPoint = firstPoint;
            result->recognizedText = RecognizeImageText(image, result->error);

            if (!result->recognizedText.empty())
            {
                const auto coordinate = ParseOcrCoordinate(result->recognizedText);
                if (coordinate.has_value())
                {
                    result->coordinate = *coordinate;
                    result->coordinateFound = true;
                }
            }

            if (!PostMessageW(
                    mainWindow,
                    kOcrResultMessage,
                    0,
                    reinterpret_cast<LPARAM>(result)))
            {
                delete result;
            }
        })
        .detach();
}

void HandleOcrResult(OcrResult* result)
{
    std::unique_ptr<OcrResult> ownedResult(result);
    if (g_app == nullptr || result == nullptr)
    {
        return;
    }

    g_app->ocrInProgress = false;
    if (!result->coordinateFound)
    {
        std::wstring message = result->error.empty()
            ? L"OCR did not find a valid coordinate pair. Open the game map and try again."
            : result->error;
        if (!result->recognizedText.empty())
        {
            std::wstring preview = result->recognizedText;
            for (wchar_t& character : preview)
            {
                if (character == L'\r' || character == L'\n' || character == L'\t')
                {
                    character = L' ';
                }
            }
            if (preview.size() > 100)
            {
                preview.resize(97);
                preview += L"...";
            }
            message += L" Read: " + preview;
        }
        SetStatus(message);
        MessageBeep(MB_ICONWARNING);
        return;
    }

    ApplyCapturedCoordinate(result->firstPoint, result->coordinate);
}

void CopyDistance()
{
    if (g_app == nullptr || !g_app->distance.has_value())
    {
        SetStatus(L"There is no calculated distance to copy yet.");
        MessageBeep(MB_ICONWARNING);
        return;
    }

    const std::wstring result = FormatNumber(*g_app->distance, 2);
    if (SetClipboardText(result))
    {
        SetStatus(L"Copied distance " + result + L" to the clipboard.");
    }
    else
    {
        SetStatus(L"Could not write the distance to the clipboard.");
        MessageBeep(MB_ICONWARNING);
    }
}

void ApplyOverlayHitTesting()
{
    if (g_app == nullptr || g_app->overlayWindow == nullptr)
    {
        return;
    }

    LONG_PTR style = GetWindowLongPtrW(g_app->overlayWindow, GWL_EXSTYLE);
    style |= WS_EX_NOACTIVATE;

    if (g_app->clickThrough)
    {
        style |= WS_EX_TRANSPARENT;
    }
    else
    {
        style &= ~static_cast<LONG_PTR>(WS_EX_TRANSPARENT);
    }

    SetWindowLongPtrW(g_app->overlayWindow, GWL_EXSTYLE, style);
    SetWindowPos(
        g_app->overlayWindow,
        HWND_TOPMOST,
        0,
        0,
        0,
        0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

void ToggleOverlay()
{
    if (g_app == nullptr || g_app->overlayWindow == nullptr)
    {
        return;
    }

    g_app->overlayEnabled = !g_app->overlayEnabled;
    SetStatus(
        g_app->overlayEnabled
            ? L"Overlay enabled; it will appear whenever the game is running."
            : L"Overlay disabled by setting.");
    RefreshTargetGame();
    UpdateDisplay();
}

void ToggleClickThrough()
{
    if (g_app == nullptr)
    {
        return;
    }

    g_app->clickThrough = !g_app->clickThrough;
    ApplyOverlayHitTesting();
    SetStatus(
        g_app->clickThrough ? L"Overlay is now click-through."
                            : L"Overlay now accepts mouse input.");
    UpdateDisplay();
}

void ShowMainWindow()
{
    if (g_app == nullptr || g_app->mainWindow == nullptr)
    {
        return;
    }

    ShowWindow(g_app->mainWindow, SW_SHOW);
    ShowWindow(g_app->mainWindow, SW_RESTORE);
    SetForegroundWindow(g_app->mainWindow);
}

void AddTrayIcon()
{
    if (g_app == nullptr)
    {
        return;
    }

    g_app->tray = {};
    g_app->tray.cbSize = sizeof(g_app->tray);
    g_app->tray.hWnd = g_app->mainWindow;
    g_app->tray.uID = kTrayId;
    g_app->tray.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_app->tray.uCallbackMessage = kTrayMessage;
    g_app->tray.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wcsncpy_s(
        g_app->tray.szTip,
        _countof(g_app->tray.szTip),
        L"War Dogs Artillery",
        _TRUNCATE);

    Shell_NotifyIconW(NIM_ADD, &g_app->tray);
    g_app->tray.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &g_app->tray);
}

void RemoveTrayIcon()
{
    if (g_app != nullptr && g_app->tray.hWnd != nullptr)
    {
        Shell_NotifyIconW(NIM_DELETE, &g_app->tray);
        g_app->tray = {};
    }
}

void ShowTrayMenu(HWND owner)
{
    POINT point{};
    GetCursorPos(&point);

    HMENU menu = CreatePopupMenu();
    if (menu == nullptr)
    {
        return;
    }

    AppendMenuW(menu, MF_STRING, kCommandCaptureFirst, L"Capture first coordinate");
    AppendMenuW(menu, MF_STRING, kCommandCaptureSecond, L"Capture second coordinate");
    AppendMenuW(menu, MF_STRING, kCommandCopyDistance, L"Copy distance");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kCommandToggleOverlay, L"Enable/disable overlay");
    AppendMenuW(menu, MF_STRING, kCommandToggleClickThrough, L"Toggle click-through");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kCommandExit, L"Exit");

    SetForegroundWindow(owner);
    TrackPopupMenu(
        menu,
        TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | TPM_LEFTALIGN,
        point.x,
        point.y,
        0,
        owner,
        nullptr);
    PostMessageW(owner, WM_NULL, 0, 0);
    DestroyMenu(menu);
}

bool RegisterGlobalHotkeys(HWND window)
{
    bool allRegistered = true;
    const UINT modifiers = MOD_CONTROL | MOD_ALT | MOD_NOREPEAT;

    allRegistered &= RegisterHotKey(window, kHotkeyFirst, modifiers, '1') != FALSE;
    allRegistered &= RegisterHotKey(window, kHotkeySecond, modifiers, '2') != FALSE;
    allRegistered &= RegisterHotKey(window, kHotkeyToggleOverlay, modifiers, 'O') != FALSE;
    allRegistered &= RegisterHotKey(window, kHotkeyCopyDistance, modifiers, 'C') != FALSE;
    allRegistered &= RegisterHotKey(window, kHotkeyToggleClickThrough, modifiers, 'T') != FALSE;

    return allRegistered;
}

void UnregisterGlobalHotkeys(HWND window)
{
    UnregisterHotKey(window, kHotkeyFirst);
    UnregisterHotKey(window, kHotkeySecond);
    UnregisterHotKey(window, kHotkeyToggleOverlay);
    UnregisterHotKey(window, kHotkeyCopyDistance);
    UnregisterHotKey(window, kHotkeyToggleClickThrough);
}

// A small shared palette keeps the utility and HUD visually consistent.
constexpr COLORREF kBackground = RGB(16, 21, 27);
constexpr COLORREF kSurface = RGB(24, 33, 42);
constexpr COLORREF kBorder = RGB(43, 57, 69);
constexpr COLORREF kText = RGB(239, 244, 247);
constexpr COLORREF kMuted = RGB(155, 173, 188);
constexpr COLORREF kMint = RGB(112, 225, 184);
constexpr COLORREF kCoral = RGB(255, 126, 125);

void UiText(HDC dc, const std::wstring& text, RECT rect, int size = 14,
            COLORREF color = kText, int weight = FW_NORMAL, UINT flags = DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS)
{
    HFONT font = CreateFontW(-size, 0, 0, 0, weight, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    auto old = SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    DrawTextW(dc, text.c_str(), -1, &rect, flags);
    SelectObject(dc, old);
    DeleteObject(font);
}

void Card(HDC dc, RECT rect, COLORREF fill = kSurface, COLORREF edge = kBorder, int radius = 12)
{
    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, 1, edge);
    auto oldBrush = SelectObject(dc, brush);
    auto oldPen = SelectObject(dc, pen);
    RoundRect(dc, rect.left, rect.top, rect.right, rect.bottom, radius, radius);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
}

std::wstring RangeValue()
{
    auto value = CurrentRangeTargetMeters();
    return value ? FormatNumber(*value, 1) + L" m" : L"\u2014 m";
}

std::wstring BearingValue()
{
    return g_app->bearing ? FormatNumber(*g_app->bearing, 0) + L"\u00b0 " + g_app->compassDirection : L"\u2014";
}

std::wstring GuideValue(bool compact)
{
    const auto range = CurrentRangeTargetMeters();
    if (!range) return L"Capture two positions to calculate a solution";
    const auto reference = NearestRangeReference(*range);
    if (!reference) return L"Outside L81 range  \u00b7  132\u2013684 m";
    return L"RNG " + FormatNumber(*reference, 0) + L"M" +
        (compact ? L"  \u00b7  Align with red guide" : L"  \u00b7  Align this line with the red guide");
}

void CreateMainControls(HWND window)
{
    g_app->uiFont = CreateFontW(-15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    auto button = [&](int id, const wchar_t* label, int x, int y, int w, int h) {
        HWND control = CreateWindowW(L"BUTTON", label, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
            x, y, w, h, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), g_app->instance, nullptr);
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(g_app->uiFont), TRUE);
    };
    button(kCommandCaptureFirst, L"Capture player", 16, 241, 170, 34);
    button(kCommandCaptureSecond, L"Capture target", 198, 241, 170, 34);
    button(kCommandToggleOverlay, L"Overlay", 16, 313, 100, 32);
    button(kCommandToggleClickThrough, L"Click-through", 124, 313, 142, 32);
    button(kCommandCopyDistance, L"Copy distance  \u00b7  Ctrl+Alt+C", 164, 398, 204, 28);
    g_app->rangeScaleEdit = CreateWindowExW(0, L"EDIT", L"100.0",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
        286, 319, 72, 22, window,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kControlRangeScale)), g_app->instance, nullptr);
    SendMessageW(g_app->rangeScaleEdit, WM_SETFONT, reinterpret_cast<WPARAM>(g_app->uiFont), TRUE);
    SendMessageW(g_app->rangeScaleEdit, EM_SETLIMITTEXT, 12, 0);
    BOOL dark = TRUE;
    DwmSetWindowAttribute(window, 20, &dark, sizeof(dark));
    COLORREF caption = kBackground;
    DwmSetWindowAttribute(window, 35, &caption, sizeof(caption));
}

void PaintMain(HDC dc)
{
    UiText(dc, L"\u2295", {16, 13, 47, 48}, 30, kMint);
    UiText(dc, L"WAR DOGS", {55, 14, 240, 47}, 21, kText, FW_SEMIBOLD);
    UiText(dc, g_app->targetWindow ? L"\u2022  CONNECTED" : L"\u2022  OFFLINE",
        {245, 16, 368, 46}, 10, g_app->targetWindow ? kMint : kMuted,
        FW_NORMAL, DT_RIGHT | DT_SINGLELINE | DT_VCENTER);
    UiText(dc, L"RANGE", {18, 63, 182, 82}, 10, kMuted, FW_SEMIBOLD);
    UiText(dc, L"BEARING", {200, 63, 366, 82}, 10, kMuted, FW_SEMIBOLD);
    UiText(dc, RangeValue(), {16, 83, 190, 131}, 34, kText, FW_SEMIBOLD);
    UiText(dc, BearingValue(), {198, 83, 368, 131}, 34, kText, FW_SEMIBOLD);
    UiText(dc, GuideValue(true), {16, 138, 368, 164}, 12,
        CurrentRangeTargetMeters() ? kCoral : kMuted);
    for (int i = 0; i < 2; ++i)
    {
        int x = 16 + i * 182;
        const auto point = i ? g_app->second : g_app->first;
        UiText(dc, i ? L"TARGET" : L"PLAYER", {x, 181, x+170, 199}, 10, kMuted);
        UiText(dc, point ? FormatCoordinate(*point) : L"Not captured", {x, 204, x+170, 229}, 14);
        UiText(dc, i ? L"Ctrl+Alt+2" : L"Ctrl+Alt+1", {x, 278, x+170, 298}, 11, kMuted,
            FW_NORMAL, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
    }
    Card(dc, {274, 313, 368, 345});
    UiText(dc, L"Ctrl+Alt+O", {16, 347, 116, 366}, 10, kMuted);
    UiText(dc, L"Ctrl+Alt+T", {124, 347, 266, 366}, 10, kMuted);
    UiText(dc, L"m / unit", {274, 347, 368, 366}, 10, kMuted);
    UiText(dc, g_app->status, {16, 374, 368, 395}, 11, kMuted);
    UiText(dc, L"L81  /  RNG", {16, 403, 145, 421}, 10, kMuted);
}

void PaintOverlay(HWND window, HDC dc)
{
    RECT client{};
    GetClientRect(window, &client);
    Card(dc, client, kBackground);
    const int w = client.right;
    UiText(dc, L"WAR DOGS", {16, 10, 150, 32}, 13, kText, FW_SEMIBOLD);
    UiText(dc, g_app->clickThrough ? L"\u2022  LIVE" : L"\u2022  MOUSE ON", {160, 10, w-16, 32}, 11, kMint,
        FW_NORMAL, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    UiText(dc, RangeValue(), {16, 37, w/2, 82}, 30, kText, FW_SEMIBOLD);
    UiText(dc, BearingValue(), {w/2+8, 37, w-16, 82}, 30, kText, FW_SEMIBOLD);
    UiText(dc, GuideValue(true), {16, 85, w-16, 108}, 12,
        CurrentRangeTargetMeters() ? kCoral : kMuted);
    UiText(dc, L"PLAYER", {16, 117, w/2, 133}, 10, kMuted);
    UiText(dc, L"TARGET", {w/2+8, 117, w-16, 133}, 10, kMuted);
    UiText(dc, g_app->first ? FormatCoordinate(*g_app->first) : L"Not captured", {16, 134, w/2, 154}, 12);
    UiText(dc, g_app->second ? FormatCoordinate(*g_app->second) : L"Not captured", {w/2+8, 134, w-16, 154}, 12);
    UiText(dc, L"Ctrl+Alt+1", {16, 158, w/2, 177}, 10, kMuted);
    UiText(dc, L"Ctrl+Alt+2", {w/2+8, 158, w-16, 177}, 10, kMuted);
}

LRESULT CALLBACK OverlayWndProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_PAINT:
    {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(window, &paint);
        PaintOverlay(window, dc);
        EndPaint(window, &paint);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;

    case WM_NCHITTEST:
        if (g_app != nullptr && g_app->clickThrough)
        {
            return HTTRANSPARENT;
        }
        break;

    case WM_LBUTTONDOWN:
        if (g_app != nullptr && !g_app->clickThrough)
        {
            ReleaseCapture();
            SendMessageW(window, WM_NCLBUTTONDOWN, HTCAPTION, 0);
        }
        return 0;

    default:
        break;
    }

    return DefWindowProcW(window, message, wParam, lParam);
}

void PaintRangeOverlay(HWND window, HDC dc)
{
    RECT client{};
    GetClientRect(window, &client);

    // The range/angle overlay uses black as a color key, so everything except
    // the guides and their labels remains fully transparent over the game.
    HBRUSH transparentBackground = CreateSolidBrush(RGB(0, 0, 0));
    FillRect(dc, &client, transparentBackground);
    DeleteObject(transparentBackground);

    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    if (width <= 0 || height <= 0)
    {
        return;
    }

    if (g_app != nullptr && g_app->bearing.has_value())
    {
        const auto reference = NearestAngleReference(*g_app->bearing);
        const auto guideFraction = CalculateAngleGuideFraction(*g_app->bearing);
        if (reference.has_value() && guideFraction.has_value())
        {
            const int guideX = std::clamp(
                static_cast<int>(std::lround(
                    *guideFraction * static_cast<double>(width))),
                0,
                width - 1);
            const int markerTop = std::clamp(
                static_cast<int>(std::lround(
                    height * kAngleMarkerTopFraction)),
                0,
                height - 1);
            const int markerBottom = std::clamp(
                static_cast<int>(std::lround(
                    height * kAngleMarkerBottomFraction)),
                markerTop,
                height - 1);

            HPEN anglePen = CreatePen(PS_SOLID, 3, RGB(255, 55, 55));
            HPEN previousPen = static_cast<HPEN>(SelectObject(dc, anglePen));
            MoveToEx(dc, guideX, markerTop, nullptr);
            LineTo(dc, guideX, markerBottom);
            SelectObject(dc, previousPen);
            DeleteObject(anglePen);

            const std::wstring label = FormatNumber(*reference, 0);
            HFONT font = CreateFontW(
                24,
                0,
                0,
                0,
                FW_SEMIBOLD,
                FALSE,
                FALSE,
                FALSE,
                DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS,
                CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY,
                DEFAULT_PITCH | FF_DONTCARE,
                L"Segoe UI");
            HFONT previousFont = static_cast<HFONT>(SelectObject(dc, font));
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, RGB(255, 55, 55));

            SIZE labelSize{};
            GetTextExtentPoint32W(
                dc,
                label.c_str(),
                static_cast<int>(label.size()),
                &labelSize);
            const int labelWidth = static_cast<int>(labelSize.cx);
            const int labelHeight = static_cast<int>(labelSize.cy);
            const int maxLabelX = std::max(0, width - labelWidth);
            const int labelX = std::clamp(
                guideX - (labelWidth / 2),
                0,
                maxLabelX);
            const int labelGap = std::max(
                1,
                static_cast<int>(std::lround(
                    height * kAngleMarkerLabelGapFraction)));
            const int labelY = std::min(
                height - std::max(1, labelHeight),
                markerBottom + labelGap);
            TextOutW(
                dc,
                labelX,
                std::max(0, labelY),
                label.c_str(),
                static_cast<int>(label.size()));

            SelectObject(dc, previousFont);
            DeleteObject(font);
        }
    }

    const auto target = CurrentRangeTargetMeters();
    if (!target.has_value())
    {
        return;
    }

    const auto reference = NearestRangeReference(*target);
    if (!reference.has_value())
    {
        return;
    }

    const auto guideFraction = CalculateRangeGuideFraction(*target);
    if (!guideFraction.has_value())
    {
        return;
    }

    const int guideY = std::clamp(
        static_cast<int>(std::lround(*guideFraction * static_cast<double>(height))),
        0,
        height - 1);
    const int barLeft = static_cast<int>(
        std::lround(width * kRangeMarkerBarLeftFraction));
    const int barRight = static_cast<int>(
        std::lround(width * kRangeMarkerBarRightFraction));
    const int markerLabelX = barRight + static_cast<int>(
        std::lround(width * kRangeMarkerLabelGapFraction));

    HPEN guidePen = CreatePen(PS_SOLID, 3, RGB(255, 55, 55));
    HPEN previousPen = static_cast<HPEN>(SelectObject(dc, guidePen));
    MoveToEx(dc, barLeft, guideY, nullptr);
    LineTo(dc, barRight, guideY);
    SelectObject(dc, previousPen);
    DeleteObject(guidePen);

    const std::wstring label = FormatNumber(*reference, 0);
    HFONT font = CreateFontW(
        24,
        0,
        0,
        0,
        FW_SEMIBOLD,
        FALSE,
        FALSE,
        FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        L"Segoe UI");
    HFONT previousFont = static_cast<HFONT>(SelectObject(dc, font));
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(255, 55, 55));
    TextOutW(
        dc,
        markerLabelX,
        // GDI's font cell has a small top inset; this offset centers the
        // visible glyphs on the guide instead of centering only the cell.
        std::max(0, guideY - 12),
        label.c_str(),
        static_cast<int>(label.size()));

    SelectObject(dc, previousFont);
    DeleteObject(font);
}

LRESULT CALLBACK RangeOverlayWndProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_PAINT:
    {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(window, &paint);
        PaintRangeOverlay(window, dc);
        EndPaint(window, &paint);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;

    case WM_NCHITTEST:
        // The guide must never steal game input, even when the information
        // panel's click-through setting is temporarily disabled.
        return HTTRANSPARENT;

    default:
        break;
    }

    return DefWindowProcW(window, message, wParam, lParam);
}

LRESULT CALLBACK MainWndProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
    {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(window, &paint);
        RECT rect{};
        GetClientRect(window, &rect);
        HDC buffer = CreateCompatibleDC(dc);
        HBITMAP bitmap = CreateCompatibleBitmap(dc, rect.right, rect.bottom);
        auto old = SelectObject(buffer, bitmap);
        HBRUSH background = CreateSolidBrush(kBackground);
        FillRect(buffer, &rect, background);
        DeleteObject(background);
        PaintMain(buffer);
        BitBlt(dc, 0, 0, rect.right, rect.bottom, buffer, 0, 0, SRCCOPY);
        SelectObject(buffer, old);
        DeleteObject(bitmap);
        DeleteDC(buffer);
        EndPaint(window, &paint);
        return 0;
    }
    case WM_CTLCOLOREDIT:
        SetTextColor(reinterpret_cast<HDC>(wParam), kText);
        SetBkColor(reinterpret_cast<HDC>(wParam), kSurface);
        SetDCBrushColor(reinterpret_cast<HDC>(wParam), kSurface);
        return reinterpret_cast<LRESULT>(GetStockObject(DC_BRUSH));
    case WM_DRAWITEM:
    {
        auto* item = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
        SetDCBrushColor(item->hDC, kBackground);
        FillRect(item->hDC, &item->rcItem, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
        bool primary = item->CtlID == kCommandCaptureFirst || item->CtlID == kCommandCaptureSecond;
        bool down = (item->itemState & ODS_SELECTED) != 0;
        Card(item->hDC, item->rcItem, primary ? (down ? RGB(81, 190, 154) : kMint) : kSurface,
            primary ? kMint : kBorder, 8);
        wchar_t label[128]{};
        GetWindowTextW(item->hwndItem, label, _countof(label));
        std::wstring text = label;
        if (item->CtlID == kCommandToggleOverlay) text += g_app->overlayEnabled ? L"  ON" : L"  OFF";
        if (item->CtlID == kCommandToggleClickThrough) text += g_app->clickThrough ? L"  ON" : L"  OFF";
        UiText(item->hDC, text, item->rcItem, 12, primary ? kBackground : kText,
            primary ? FW_SEMIBOLD : FW_NORMAL, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        if (item->itemState & ODS_FOCUS)
        {
            RECT focus = item->rcItem;
            InflateRect(&focus, -4, -4);
            DrawFocusRect(item->hDC, &focus);
        }
        return TRUE;
    }
    case kOcrResultMessage:
        HandleOcrResult(reinterpret_cast<OcrResult*>(lParam));
        return 0;

    case WM_CREATE:
        CreateMainControls(window);
        return 0;

    case WM_COMMAND:
        if (LOWORD(wParam) == kControlRangeScale &&
            HIWORD(wParam) == EN_CHANGE)
        {
            UpdateDisplay();
            return 0;
        }

        switch (LOWORD(wParam))
        {
        case kCommandCaptureFirst:
            StartOcrCapture(true);
            return 0;
        case kCommandCaptureSecond:
            StartOcrCapture(false);
            return 0;
        case kCommandToggleOverlay:
            ToggleOverlay();
            return 0;
        case kCommandCopyDistance:
            CopyDistance();
            return 0;
        case kCommandToggleClickThrough:
            ToggleClickThrough();
            return 0;
        case kCommandExit:
            if (g_app != nullptr)
            {
                g_app->closing = true;
            }
            DestroyWindow(window);
            return 0;
        default:
            break;
        }
        break;

    case WM_HOTKEY:
        switch (wParam)
        {
        case kHotkeyFirst:
            StartOcrCapture(true);
            return 0;
        case kHotkeySecond:
            StartOcrCapture(false);
            return 0;
        case kHotkeyToggleOverlay:
            ToggleOverlay();
            return 0;
        case kHotkeyCopyDistance:
            CopyDistance();
            return 0;
        case kHotkeyToggleClickThrough:
            ToggleClickThrough();
            return 0;
        default:
            break;
        }
        break;

    case kTrayMessage:
        if (lParam == WM_LBUTTONDBLCLK)
        {
            ShowMainWindow();
            return 0;
        }

        if (lParam == WM_RBUTTONUP)
        {
            ShowTrayMenu(window);
            return 0;
        }
        break;

    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
        {
            ShowWindow(window, SW_HIDE);
        }
        return 0;

    case WM_TIMER:
        if (wParam == kGamePollTimer)
        {
            RefreshTargetGame();
            return 0;
        }
        break;

    case WM_CLOSE:
        if (g_app != nullptr && !g_app->closing)
        {
            ShowWindow(window, SW_HIDE);
            SetStatus(L"Running in the system tray. Double-click the tray icon to reopen.");
            return 0;
        }
        break;

    case WM_DESTROY:
        KillTimer(window, kGamePollTimer);
        UnregisterGlobalHotkeys(window);
        RemoveTrayIcon();
        if (g_app != nullptr && g_app->rangeOverlayWindow != nullptr)
        {
            DestroyWindow(g_app->rangeOverlayWindow);
            g_app->rangeOverlayWindow = nullptr;
        }
        if (g_app != nullptr && g_app->overlayWindow != nullptr)
        {
            DestroyWindow(g_app->overlayWindow);
            g_app->overlayWindow = nullptr;
        }
        PostQuitMessage(0);
        return 0;

    default:
        break;
    }

    return DefWindowProcW(window, message, wParam, lParam);
}

bool RegisterWindowClasses(HINSTANCE instance)
{
    WNDCLASSEXW mainClass{};
    mainClass.cbSize = sizeof(mainClass);
    mainClass.hInstance = instance;
    mainClass.lpfnWndProc = MainWndProc;
    mainClass.lpszClassName = kMainClassName;
    mainClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    mainClass.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    mainClass.hIconSm = mainClass.hIcon;
    mainClass.hbrBackground = nullptr;

    WNDCLASSEXW overlayClass{};
    overlayClass.cbSize = sizeof(overlayClass);
    overlayClass.hInstance = instance;
    overlayClass.lpfnWndProc = OverlayWndProc;
    overlayClass.lpszClassName = kOverlayClassName;
    overlayClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);

    WNDCLASSEXW rangeOverlayClass{};
    rangeOverlayClass.cbSize = sizeof(rangeOverlayClass);
    rangeOverlayClass.hInstance = instance;
    rangeOverlayClass.lpfnWndProc = RangeOverlayWndProc;
    rangeOverlayClass.lpszClassName = kRangeOverlayClassName;
    rangeOverlayClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);

    return RegisterClassExW(&mainClass) != 0 &&
           RegisterClassExW(&overlayClass) != 0 &&
           RegisterClassExW(&rangeOverlayClass) != 0;
}

bool CreateOverlayWindow()
{
    const DWORD extendedStyle =
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_NOACTIVATE |
        (g_app->clickThrough ? WS_EX_TRANSPARENT : 0);

    g_app->overlayWindow = CreateWindowExW(
        extendedStyle,
        kOverlayClassName,
        L"War Dogs Artillery Overlay",
        WS_POPUP,
        30,
        30,
        410,
        175,
        nullptr,
        nullptr,
        g_app->instance,
        nullptr);

    if (g_app->overlayWindow == nullptr)
    {
        return false;
    }

    SetLayeredWindowAttributes(g_app->overlayWindow, 0, 225, LWA_ALPHA);
    ApplyOverlayHitTesting();
    return true;
}

bool CreateRangeOverlayWindow()
{
    if (g_app == nullptr)
    {
        return false;
    }

    const DWORD extendedStyle =
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_NOACTIVATE |
        WS_EX_TRANSPARENT;

    g_app->rangeOverlayWindow = CreateWindowExW(
        extendedStyle,
        kRangeOverlayClassName,
        L"War Dogs Artillery Range Overlay",
        WS_POPUP,
        0,
        0,
        100,
        100,
        nullptr,
        nullptr,
        g_app->instance,
        nullptr);

    if (g_app->rangeOverlayWindow == nullptr)
    {
        return false;
    }

    SetLayeredWindowAttributes(
        g_app->rangeOverlayWindow,
        RGB(0, 0, 0),
        0,
        LWA_COLORKEY);
    ApplyOverlayHitTesting();
    RefreshTargetGame();
    return true;
}
} // namespace

int APIENTRY wWinMain(
    HINSTANCE instance,
    HINSTANCE /*previousInstance*/,
    PWSTR /*commandLine*/,
    int /*showCommand*/)
{
    if (!SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2))
    {
        SetProcessDPIAware();
    }

    AppState state{};
    state.instance = instance;
    g_app = &state;

    if (!RegisterWindowClasses(instance))
    {
        MessageBoxW(nullptr, L"Could not register the application windows.", L"War Dogs Artillery", MB_ICONERROR);
        return 1;
    }

    state.mainWindow = CreateWindowExW(
        0,
        kMainClassName,
        L"War Dogs Artillery",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN,
        (GetSystemMetrics(SM_CXSCREEN) - 400) / 2,
        (GetSystemMetrics(SM_CYSCREEN) - 476) / 2,
        400,
        476,
        nullptr,
        nullptr,
        instance,
        nullptr);

    if (state.mainWindow == nullptr)
    {
        MessageBoxW(nullptr, L"Could not create the main window.", L"War Dogs Artillery", MB_ICONERROR);
        return 1;
    }

    if (!CreateOverlayWindow() || !CreateRangeOverlayWindow())
    {
        MessageBoxW(nullptr, L"Could not create the overlay window.", L"War Dogs Artillery", MB_ICONERROR);
        DestroyWindow(state.mainWindow);
        return 1;
    }

    AddTrayIcon();
    const bool hotkeysRegistered = RegisterGlobalHotkeys(state.mainWindow);
    SetTimer(state.mainWindow, kGamePollTimer, kGamePollIntervalMs, nullptr);
    if (!hotkeysRegistered)
    {
        SetStatus(L"One or more global hotkeys were already in use. The buttons still work.");
    }

    UpdateDisplay();
    ShowWindow(state.mainWindow, SW_SHOWNORMAL);
    UpdateWindow(state.mainWindow);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        if (IsDialogMessageW(state.mainWindow, &message)) continue;
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    DeleteObject(state.uiFont);
    g_app = nullptr;
    return static_cast<int>(message.wParam);
}
