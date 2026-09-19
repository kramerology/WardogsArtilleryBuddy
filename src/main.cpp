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
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <cstdlib>
#include <memory>
#include <iomanip>
#include <limits>
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
#pragma comment(lib, "Advapi32.lib")

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
constexpr int kCommandSettings = 1007;
constexpr int kCommandSettingsBack = 1008;
constexpr int kCommandHotkeyBase = 1100;
constexpr size_t kHotkeyCount = 5;

constexpr wchar_t kSettingsRegistryPath[] = L"Software\\ArtyBuddy";
constexpr const wchar_t* kHotkeyRegistryNames[] = {
    L"CapturePlayerModifiers",
    L"CapturePlayerKey",
    L"CaptureTargetModifiers",
    L"CaptureTargetKey",
    L"ToggleOverlayModifiers",
    L"ToggleOverlayKey",
    L"CopyDistanceModifiers",
    L"CopyDistanceKey",
    L"ToggleClickThroughModifiers",
    L"ToggleClickThroughKey",
};

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
// Keep the marker inside the left side of the reticle. Its length matches the
// short horizontal range ticks in the supplied 1920x1080 scope reference.
constexpr double kRangeMarkerBarLeftFraction = 0.369;
constexpr double kRangeMarkerBarRightFraction = 0.388;
constexpr double kRangeMarkerLabelGapFraction = 0.011;
constexpr int kOverlayGuidePenWidth = 3;

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
constexpr double kAngleMarkerTopFraction = 0.300;
constexpr double kAngleMarkerBottomFraction = 0.344;
constexpr double kAngleMarkerLabelGapFraction = 0.005;

struct Coordinate
{
    double x{};
    double y{};
};

struct HotkeyBinding
{
    UINT modifiers{};
    UINT virtualKey{};
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

struct OcrTextPass
{
    std::wstring text;
    bool focused{};
};

struct OcrRecognition
{
    std::wstring combinedText;
    std::vector<OcrTextPass> passes;
};

struct OcrResult
{
    bool firstPoint{};
    bool coordinateFound{};
    Coordinate coordinate{};
    std::wstring recognizedText;
    std::vector<OcrTextPass> ocrPasses;
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
    bool settingsVisible{false};
    bool hotkeysRegistered{false};
    int activeHotkeyIndex{-1};
    HotkeyBinding hotkeyCaptureOriginal{};
    std::array<HotkeyBinding, kHotkeyCount> hotkeys{};
    std::array<HWND, kHotkeyCount> hotkeyControls{};
    HWND settingsButton{};
    HWND settingsBackButton{};
    std::atomic_bool ocrInProgress{false};
};

AppState* g_app = nullptr;

constexpr UINT kSupportedHotkeyModifiers =
    MOD_CONTROL | MOD_ALT | MOD_SHIFT | MOD_WIN;

std::array<HotkeyBinding, kHotkeyCount> DefaultHotkeyBindings()
{
    return {
        HotkeyBinding{MOD_CONTROL | MOD_ALT, '1'},
        HotkeyBinding{MOD_CONTROL | MOD_ALT, '2'},
        HotkeyBinding{MOD_CONTROL | MOD_ALT, 'O'},
        HotkeyBinding{MOD_CONTROL | MOD_ALT, 'C'},
        HotkeyBinding{MOD_CONTROL | MOD_ALT, 'T'},
    };
}

bool IsValidHotkeyBinding(const HotkeyBinding& binding)
{
    return binding.virtualKey != 0 &&
           (binding.modifiers & ~kSupportedHotkeyModifiers) == 0;
}

void LoadHotkeyBindings(AppState& state)
{
    state.hotkeys = DefaultHotkeyBindings();

    HKEY key = nullptr;
    if (RegOpenKeyExW(
            HKEY_CURRENT_USER,
            kSettingsRegistryPath,
            0,
            KEY_QUERY_VALUE,
            &key) != ERROR_SUCCESS)
    {
        return;
    }

    for (size_t index = 0; index < kHotkeyCount; ++index)
    {
        DWORD modifiers = 0;
        DWORD virtualKey = 0;
        DWORD modifiersSize = sizeof(modifiers);
        DWORD virtualKeySize = sizeof(virtualKey);
        const auto modifiersResult = RegGetValueW(
            key,
            nullptr,
            kHotkeyRegistryNames[index * 2],
            RRF_RT_REG_DWORD,
            nullptr,
            &modifiers,
            &modifiersSize);
        const auto virtualKeyResult = RegGetValueW(
            key,
            nullptr,
            kHotkeyRegistryNames[index * 2 + 1],
            RRF_RT_REG_DWORD,
            nullptr,
            &virtualKey,
            &virtualKeySize);

        const HotkeyBinding loaded{
            static_cast<UINT>(modifiers),
            static_cast<UINT>(virtualKey)};
        if (modifiersResult == ERROR_SUCCESS &&
            virtualKeyResult == ERROR_SUCCESS &&
            IsValidHotkeyBinding(loaded))
        {
            state.hotkeys[index] = loaded;
        }
    }

    RegCloseKey(key);
}

void SaveHotkeyBindings(const AppState& state)
{
    HKEY key = nullptr;
    DWORD disposition = 0;
    if (RegCreateKeyExW(
            HKEY_CURRENT_USER,
            kSettingsRegistryPath,
            0,
            nullptr,
            0,
            KEY_SET_VALUE,
            nullptr,
            &key,
            &disposition) != ERROR_SUCCESS)
    {
        return;
    }

    for (size_t index = 0; index < kHotkeyCount; ++index)
    {
        const DWORD modifiers = state.hotkeys[index].modifiers;
        const DWORD virtualKey = state.hotkeys[index].virtualKey;
        RegSetValueExW(
            key,
            kHotkeyRegistryNames[index * 2],
            0,
            REG_DWORD,
            reinterpret_cast<const BYTE*>(&modifiers),
            sizeof(modifiers));
        RegSetValueExW(
            key,
            kHotkeyRegistryNames[index * 2 + 1],
            0,
            REG_DWORD,
            reinterpret_cast<const BYTE*>(&virtualKey),
            sizeof(virtualKey));
    }

    RegCloseKey(key);
}

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

std::wstring HotkeyKeyName(UINT virtualKey)
{
    if (virtualKey == VK_SPACE)
    {
        return L"Space";
    }

    UINT scanCode = MapVirtualKeyW(virtualKey, MAPVK_VK_TO_VSC);
    LONG keyNameCode = static_cast<LONG>(scanCode << 16);
    if (virtualKey == VK_LEFT || virtualKey == VK_RIGHT ||
        virtualKey == VK_UP || virtualKey == VK_DOWN ||
        virtualKey == VK_PRIOR || virtualKey == VK_NEXT ||
        virtualKey == VK_END || virtualKey == VK_HOME ||
        virtualKey == VK_INSERT || virtualKey == VK_DELETE ||
        virtualKey == VK_DIVIDE)
    {
        keyNameCode |= (1L << 24);
    }

    wchar_t keyName[64]{};
    const int length = GetKeyNameTextW(keyNameCode, keyName, _countof(keyName));
    if (length > 0)
    {
        return std::wstring(keyName, static_cast<size_t>(length));
    }

    if (virtualKey >= VK_F1 && virtualKey <= VK_F24)
    {
        return L"F" + std::to_wstring(virtualKey - VK_F1 + 1);
    }

    return L"Key " + FormatNumber(static_cast<double>(virtualKey), 0);
}

std::wstring FormatHotkey(const HotkeyBinding& binding)
{
    if (!IsValidHotkeyBinding(binding))
    {
        return L"Unassigned";
    }

    std::wstring result;
    if ((binding.modifiers & MOD_CONTROL) != 0)
    {
        result += L"Ctrl+";
    }
    if ((binding.modifiers & MOD_ALT) != 0)
    {
        result += L"Alt+";
    }
    if ((binding.modifiers & MOD_SHIFT) != 0)
    {
        result += L"Shift+";
    }
    if ((binding.modifiers & MOD_WIN) != 0)
    {
        result += L"Win+";
    }
    return result + HotkeyKeyName(binding.virtualKey);
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

struct OcrNumberToken
{
    double value{};
    std::size_t position{};
    std::size_t length{};
    int fractionalDigits{};
};

struct OcrLabeledValue
{
    OcrNumberToken value;
    std::size_t labelPosition{};
};

struct ParsedOcrCoordinate
{
    Coordinate coordinate{};
    int quality{};
};

const std::wstring& OcrNumberPattern()
{
    static const std::wstring pattern =
        LR"([-+]?(?:\d+(?:\.\d*)?|\.\d+))";
    return pattern;
}

const std::wstring& OcrNumberLikePattern()
{
    // Windows OCR commonly returns zeroes as 'o' and nines as 'g' when the
    // coordinate text is over a textured map. Restrict these substitutions
    // to values immediately following an x/y label.
    static const std::wstring pattern =
        LR"([-+]?(?:[0-9OoGgSsBbIiLl|]+(?:\.[0-9OoGgSsBbIiLl|]*)?|\.[0-9OoGgSsBbIiLl|]+))";
    return pattern;
}

std::optional<OcrNumberToken> ParseOcrNumberToken(
    const std::wstring& value,
    std::size_t position)
{
    try
    {
        std::wstring normalized;
        normalized.reserve(value.size());
        for (const wchar_t character : value)
        {
            switch (character)
            {
            case L'o':
            case L'O':
                normalized += L'0';
                break;
            case L'g':
            case L'G':
                normalized += L'9';
                break;
            case L's':
            case L'S':
                normalized += L'5';
                break;
            case L'b':
            case L'B':
                normalized += L'8';
                break;
            case L'i':
            case L'I':
            case L'l':
            case L'L':
            case L'|':
                normalized += L'1';
                break;
            default:
                normalized += character;
                break;
            }
        }

        const double parsed = std::stod(normalized);
        if (!std::isfinite(parsed))
        {
            return std::nullopt;
        }

        const std::size_t decimal = normalized.find(L'.');
        const int fractionalDigits = decimal == std::wstring::npos
            ? 0
            : static_cast<int>(normalized.size() - decimal - 1);
        return OcrNumberToken{
            parsed,
            position,
            value.size(),
            fractionalDigits};
    }
    catch (const std::exception&)
    {
        return std::nullopt;
    }
}

std::vector<OcrNumberToken> FindOcrNumberTokens(const std::wstring& text)
{
    std::vector<OcrNumberToken> numbers;

    try
    {
        const std::wregex numberRegex(OcrNumberPattern());
        for (std::wsregex_iterator iterator(text.begin(), text.end(), numberRegex);
             iterator != std::wsregex_iterator();
             ++iterator)
        {
            const auto match = *iterator;
            const auto token = ParseOcrNumberToken(
                match.str(),
                static_cast<std::size_t>(match.position()));
            if (token.has_value())
            {
                numbers.push_back(*token);
            }
        }
    }
    catch (const std::exception&)
    {
        // Invalid OCR text is handled as a parse failure below.
    }

    return numbers;
}

std::vector<OcrLabeledValue> FindOcrLabeledValues(
    const std::wstring& text,
    wchar_t label)
{
    std::vector<OcrLabeledValue> values;

    try
    {
        const std::wstring labelPattern = label == L'x'
            ? L"[xX]"
            : L"[yY]";
        const std::wregex pattern(
            labelPattern + L"\\s*[:=]?\\s*(" + OcrNumberLikePattern() + L")");

        for (std::wsregex_iterator iterator(text.begin(), text.end(), pattern);
             iterator != std::wsregex_iterator();
             ++iterator)
        {
            const auto match = *iterator;
            if (match.size() < 2)
            {
                continue;
            }

            const auto token = ParseOcrNumberToken(
                match.str(1),
                static_cast<std::size_t>(match.position(1)));
            if (token.has_value())
            {
                values.push_back(OcrLabeledValue{
                    *token,
                    static_cast<std::size_t>(match.position())});
            }
        }
    }
    catch (const std::exception&)
    {
        // Invalid OCR text is handled as a parse failure below.
    }

    return values;
}

int OcrPrecisionQuality(const OcrNumberToken& token)
{
    // The game prints two fractional digits. Prefer that stable format over
    // a noisy pass that invents or drops a decimal digit.
    if (token.fractionalDigits == 2)
    {
        return 15;
    }
    if (token.fractionalDigits == 1)
    {
        return 5;
    }
    if (token.fractionalDigits == 3)
    {
        return 2;
    }
    return 0;
}

const OcrNumberToken* FindNearestDecimalToken(
    const std::vector<OcrNumberToken>& numbers,
    std::size_t anchor,
    bool preferBefore)
{
    const OcrNumberToken* nearest = nullptr;
    long long nearestScore = std::numeric_limits<long long>::max();

    for (const auto& number : numbers)
    {
        if (number.fractionalDigits == 0 || number.position == anchor)
        {
            continue;
        }

        const bool before = number.position < anchor;
        const auto distance = std::llabs(
            static_cast<long long>(number.position) -
            static_cast<long long>(anchor));
        if (distance > 240)
        {
            continue;
        }

        const long long directionPenalty = before == preferBefore ? 0 : 60;
        const long long score = distance + directionPenalty;
        if (score < nearestScore)
        {
            nearest = &number;
            nearestScore = score;
        }
    }

    return nearest;
}

std::optional<ParsedOcrCoordinate> ParseSingleOcrCoordinate(
    const std::wstring& text)
{
    const auto numbers = FindOcrNumberTokens(text);
    const auto xValues = FindOcrLabeledValues(text, L'x');
    const auto yValues = FindOcrLabeledValues(text, L'y');

    if (!xValues.empty() && !yValues.empty())
    {
        const OcrLabeledValue* bestX = nullptr;
        const OcrLabeledValue* bestY = nullptr;
        long long bestDistance = std::numeric_limits<long long>::max();

        for (const auto& x : xValues)
        {
            for (const auto& y : yValues)
            {
                const auto distance = std::llabs(
                    static_cast<long long>(x.labelPosition) -
                    static_cast<long long>(y.labelPosition));
                if (distance < bestDistance)
                {
                    bestX = &x;
                    bestY = &y;
                    bestDistance = distance;
                }
            }
        }

        if (bestX != nullptr && bestY != nullptr)
        {
            return ParsedOcrCoordinate{
                Coordinate{bestX->value.value, bestY->value.value},
                60 + OcrPrecisionQuality(bestX->value) +
                    OcrPrecisionQuality(bestY->value)};
        }
    }

    // Windows OCR sometimes drops the small 'y' glyph, while retaining the
    // decimal number. In the map UI the y label is immediately above the x
    // label, so associate the nearest decimal token with a surviving x label.
    if (!xValues.empty())
    {
        const auto& x = xValues.front();
        const auto* y = FindNearestDecimalToken(numbers, x.value.position, true);
        if (y != nullptr)
        {
            return ParsedOcrCoordinate{
                Coordinate{x.value.value, y->value},
                35 + OcrPrecisionQuality(x.value) +
                    OcrPrecisionQuality(*y)};
        }
    }

    // Conversely, if the x glyph is missing, pair a surviving y label with
    // the nearest decimal number that follows it.
    if (!yValues.empty())
    {
        const auto& y = yValues.front();
        const auto* x = FindNearestDecimalToken(numbers, y.value.position, false);
        if (x != nullptr)
        {
            return ParsedOcrCoordinate{
                Coordinate{x->value, y.value.value},
                35 + OcrPrecisionQuality(*x) +
                    OcrPrecisionQuality(y.value)};
        }
    }

    // Keep support for a clean, unlabeled "x, y" OCR result. This fallback
    // is deliberately only used when there are exactly two decimal values,
    // so axis/range integers in the broad capture cannot become coordinates.
    std::vector<const OcrNumberToken*> decimalNumbers;
    for (const auto& number : numbers)
    {
        if (number.fractionalDigits > 0)
        {
            decimalNumbers.push_back(&number);
        }
    }
    if (decimalNumbers.size() == 2)
    {
        return ParsedOcrCoordinate{
            Coordinate{decimalNumbers[0]->value, decimalNumbers[1]->value},
            20 + OcrPrecisionQuality(*decimalNumbers[0]) +
                OcrPrecisionQuality(*decimalNumbers[1])};
    }

    return std::nullopt;
}

std::optional<Coordinate> ParseOcrCoordinate(const std::wstring& text)
{
    const auto parsed = ParseSingleOcrCoordinate(text);
    return parsed.has_value()
        ? std::optional<Coordinate>(parsed->coordinate)
        : std::nullopt;
}

std::optional<Coordinate> SelectOcrCoordinate(
    const std::vector<OcrTextPass>& passes)
{
    std::vector<ParsedOcrCoordinate> candidates;
    candidates.reserve(passes.size());

    for (const auto& pass : passes)
    {
        const auto parsed = ParseSingleOcrCoordinate(pass.text);
        if (!parsed.has_value())
        {
            continue;
        }

        ParsedOcrCoordinate candidate = *parsed;
        if (pass.focused)
        {
            // A focused map-label pass has fewer axis and HUD numbers to
            // confuse the parser, so let it win when OCR quality is close.
            candidate.quality += 12;
        }
        candidates.push_back(candidate);
    }

    if (candidates.empty())
    {
        return std::nullopt;
    }

    std::size_t bestIndex = 0;
    int bestScore = std::numeric_limits<int>::min();
    for (std::size_t index = 0; index < candidates.size(); ++index)
    {
        int score = candidates[index].quality;
        for (std::size_t other = 0; other < candidates.size(); ++other)
        {
            if (index == other)
            {
                continue;
            }

            if (std::abs(candidates[index].coordinate.x -
                         candidates[other].coordinate.x) < 0.005 &&
                std::abs(candidates[index].coordinate.y -
                         candidates[other].coordinate.y) < 0.005)
            {
                score += 6;
            }
        }

        if (score > bestScore)
        {
            bestIndex = index;
            bestScore = score;
        }
    }

    return candidates[bestIndex].coordinate;
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

std::optional<double> CurrentRangeTargetMeters()
{
    if (g_app == nullptr || !g_app->distance.has_value())
    {
        return std::nullopt;
    }

    const double target = *g_app->distance * kDefaultMetersPerCoordinateUnit;
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
    if (g_app->settingsButton != nullptr)
    {
        InvalidateRect(g_app->settingsButton, nullptr, TRUE);
    }
    if (g_app->settingsBackButton != nullptr)
    {
        InvalidateRect(g_app->settingsBackButton, nullptr, TRUE);
    }
    for (const HWND control : g_app->hotkeyControls)
    {
        if (control != nullptr)
        {
            InvalidateRect(control, nullptr, TRUE);
        }
    }
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

bool IsTargetGameForeground(HWND targetWindow, DWORD targetProcessId)
{
    if (targetWindow == nullptr || targetProcessId == 0 ||
        !IsWindow(targetWindow))
    {
        return false;
    }

    const HWND foregroundWindow = GetForegroundWindow();
    if (foregroundWindow == nullptr)
    {
        return false;
    }

    DWORD foregroundProcessId = 0;
    if (GetWindowThreadProcessId(foregroundWindow, &foregroundProcessId) == 0 ||
        foregroundProcessId != targetProcessId)
    {
        return false;
    }

    // The main game window is normally foreground. Accept a dialog owned by
    // that window as well, while rejecting unrelated windows in the same
    // process.
    return foregroundWindow == targetWindow ||
           GetAncestor(foregroundWindow, GA_ROOTOWNER) == targetWindow;
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

    const bool targetIsForeground =
        IsTargetGameForeground(targetWindow, processId);
    bool shouldShow = g_app->overlayEnabled && targetWindow != nullptr &&
                      IsWindowVisible(targetWindow) && !IsIconic(targetWindow) &&
                      targetIsForeground;

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

CapturedImage CropCapturedImage(
    const CapturedImage& source,
    double leftFraction,
    double topFraction,
    double rightFraction,
    double bottomFraction)
{
    const int left = std::clamp(
        static_cast<int>(std::lround(source.width * leftFraction)),
        0,
        std::max(0, source.width - 1));
    const int top = std::clamp(
        static_cast<int>(std::lround(source.height * topFraction)),
        0,
        std::max(0, source.height - 1));
    const int right = std::clamp(
        static_cast<int>(std::lround(source.width * rightFraction)),
        left + 1,
        source.width);
    const int bottom = std::clamp(
        static_cast<int>(std::lround(source.height * bottomFraction)),
        top + 1,
        source.height);

    CapturedImage cropped;
    cropped.width = right - left;
    cropped.height = bottom - top;
    cropped.stride = cropped.width * 4;
    cropped.pixels.resize(static_cast<size_t>(cropped.stride) * cropped.height);

    for (int y = 0; y < cropped.height; ++y)
    {
        const auto* sourceRow = source.pixels.data() +
            (static_cast<size_t>(top + y) * source.stride) +
            (static_cast<size_t>(left) * 4);
        auto* destinationRow = cropped.pixels.data() +
            (static_cast<size_t>(y) * cropped.stride);
        std::memcpy(destinationRow, sourceRow, static_cast<size_t>(cropped.stride));
    }

    return cropped;
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

OcrRecognition RecognizeImageText(
    const CapturedImage& image,
    std::wstring& error)
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

        OcrRecognition recognition;
        const auto focusedImage = CropCapturedImage(
            image,
            0.30,
            0.26,
            0.72,
            0.70);

        const auto recognizePass =
            [&engine, &recognition](
                const CapturedImage& source,
                OcrImageMode mode,
                bool focused)
        {
            const CapturedImage prepared = PrepareOcrImage(
                source,
                mode,
                static_cast<int>(kOcrScale),
                false);
            const auto bitmap = MakeSoftwareBitmap(prepared);
            const auto result = engine.RecognizeAsync(bitmap).get();
            const std::wstring recognized = result.Text().c_str();

            if (!recognition.combinedText.empty() && !recognized.empty())
            {
                recognition.combinedText += L"\n";
            }
            recognition.combinedText += recognized;
            recognition.passes.push_back(OcrTextPass{recognized, focused});
        };

        // The coordinate labels are small and often sit over textured map
        // imagery. A center-focused pass gives Windows OCR enough scale and
        // context to retain both decimal digits. Keep the broad passes too,
        // because a marked point may be away from the exact map center.
        for (const OcrImageMode mode : {
                 OcrImageMode::Original,
                 OcrImageMode::BrightTextThreshold,
                 OcrImageMode::DarkTextThreshold,
                 OcrImageMode::LocalDarkTextThreshold})
        {
            recognizePass(focusedImage, mode, true);
        }

        for (const OcrImageMode mode : {
                 // Keep the unmodified map first within the broad passes.
                 // Thresholded passes are useful fallbacks, but can distort
                 // small decimal glyphs.
                 OcrImageMode::Original,
                 OcrImageMode::BrightTextThreshold,
                 OcrImageMode::DarkTextThreshold,
                 OcrImageMode::LocalDarkTextThreshold})
        {
            recognizePass(image, mode, false);
        }

        error = L"OCR completed, but no x###.## and y###.## coordinate pair was found.";
        return recognition;
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

    // The guides are layered windows. Give DWM a chance to commit their
    // hidden state before BitBlt reads the desktop, otherwise one frame of a
    // red guide can remain over a coordinate glyph and change its OCR.
    DwmFlush();
    Sleep(50);

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
            const auto recognition = RecognizeImageText(image, result->error);
            result->recognizedText = recognition.combinedText;
            result->ocrPasses = recognition.passes;

            if (!result->ocrPasses.empty())
            {
                const auto coordinate = SelectOcrCoordinate(result->ocrPasses);
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
        L"Arty Buddy",
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
    if (g_app == nullptr)
    {
        return false;
    }

    bool allRegistered = true;
    for (size_t index = 0; index < kHotkeyCount; ++index)
    {
        const auto& binding = g_app->hotkeys[index];
        const int id = kHotkeyFirst + static_cast<int>(index);
        allRegistered &= IsValidHotkeyBinding(binding) &&
            RegisterHotKey(
                window,
                id,
                binding.modifiers | MOD_NOREPEAT,
                binding.virtualKey) != FALSE;
    }

    g_app->hotkeysRegistered = allRegistered;
    return allRegistered;
}

void UnregisterGlobalHotkeys(HWND window)
{
    UnregisterHotKey(window, kHotkeyFirst);
    UnregisterHotKey(window, kHotkeySecond);
    UnregisterHotKey(window, kHotkeyToggleOverlay);
    UnregisterHotKey(window, kHotkeyCopyDistance);
    UnregisterHotKey(window, kHotkeyToggleClickThrough);
    if (g_app != nullptr)
    {
        g_app->hotkeysRegistered = false;
    }
}

constexpr const wchar_t* kHotkeyActionLabels[] = {
    L"Capture player",
    L"Capture target",
    L"Toggle overlay",
    L"Copy distance",
    L"Click-through",
};

bool IsHotkeyModifierKey(UINT virtualKey)
{
    return virtualKey == VK_SHIFT || virtualKey == VK_LSHIFT ||
           virtualKey == VK_RSHIFT || virtualKey == VK_CONTROL ||
           virtualKey == VK_LCONTROL || virtualKey == VK_RCONTROL ||
           virtualKey == VK_MENU || virtualKey == VK_LMENU ||
           virtualKey == VK_RMENU || virtualKey == VK_LWIN ||
           virtualKey == VK_RWIN;
}

UINT CurrentHotkeyModifiers()
{
    UINT modifiers = 0;
    if ((GetKeyState(VK_CONTROL) & 0x8000) != 0)
    {
        modifiers |= MOD_CONTROL;
    }
    if ((GetKeyState(VK_MENU) & 0x8000) != 0)
    {
        modifiers |= MOD_ALT;
    }
    if ((GetKeyState(VK_SHIFT) & 0x8000) != 0)
    {
        modifiers |= MOD_SHIFT;
    }
    if ((GetKeyState(VK_LWIN) & 0x8000) != 0 ||
        (GetKeyState(VK_RWIN) & 0x8000) != 0)
    {
        modifiers |= MOD_WIN;
    }
    return modifiers;
}

void RefreshHotkeyControlLabels()
{
    if (g_app == nullptr)
    {
        return;
    }

    for (size_t index = 0; index < kHotkeyCount; ++index)
    {
        if (g_app->hotkeyControls[index] == nullptr)
        {
            continue;
        }

        const std::wstring label = g_app->activeHotkeyIndex ==
                static_cast<int>(index)
            ? L"Press a key..."
            : FormatHotkey(g_app->hotkeys[index]);
        SetWindowTextW(g_app->hotkeyControls[index], label.c_str());
        InvalidateRect(g_app->hotkeyControls[index], nullptr, TRUE);
    }
}

void CancelHotkeyCapture(HWND window)
{
    if (g_app == nullptr || g_app->activeHotkeyIndex < 0)
    {
        return;
    }

    g_app->activeHotkeyIndex = -1;
    RefreshHotkeyControlLabels();
    if (!g_app->hotkeysRegistered)
    {
        RegisterGlobalHotkeys(window);
    }
    SetFocus(window);
}

void BeginHotkeyCapture(HWND window, size_t index)
{
    if (g_app == nullptr || index >= kHotkeyCount ||
        g_app->settingsVisible == false)
    {
        return;
    }

    if (g_app->activeHotkeyIndex >= 0)
    {
        CancelHotkeyCapture(window);
    }

    UnregisterGlobalHotkeys(window);
    g_app->activeHotkeyIndex = static_cast<int>(index);
    g_app->hotkeyCaptureOriginal = g_app->hotkeys[index];
    RefreshHotkeyControlLabels();

    // The top-level window receives the next key message, including
    // WM_SYSKEYDOWN for Alt combinations, while the button remains visibly
    // armed as the capture target.
    SetFocus(window);
    UpdateDisplay();
}

void CompleteHotkeyCapture(HWND window, UINT virtualKey)
{
    if (g_app == nullptr || g_app->activeHotkeyIndex < 0 ||
        static_cast<size_t>(g_app->activeHotkeyIndex) >= kHotkeyCount)
    {
        return;
    }

    if (virtualKey == VK_ESCAPE)
    {
        CancelHotkeyCapture(window);
        return;
    }
    if (IsHotkeyModifierKey(virtualKey))
    {
        return;
    }

    const size_t index = static_cast<size_t>(g_app->activeHotkeyIndex);
    const HotkeyBinding candidate{CurrentHotkeyModifiers(), virtualKey};
    const HotkeyBinding original = g_app->hotkeyCaptureOriginal;

    UnregisterGlobalHotkeys(window);
    g_app->hotkeys[index] = candidate;
    if (!RegisterGlobalHotkeys(window))
    {
        // A system-reserved or duplicate binding must not leave the app with
        // a partially registered shortcut set.
        UnregisterGlobalHotkeys(window);
        g_app->hotkeys[index] = original;
        RegisterGlobalHotkeys(window);
        MessageBeep(MB_ICONWARNING);
    }
    else
    {
        SaveHotkeyBindings(*g_app);
    }

    g_app->activeHotkeyIndex = -1;
    RefreshHotkeyControlLabels();
    SetFocus(window);
    UpdateDisplay();
}

void ShowSettingsPage(bool show)
{
    if (g_app == nullptr || g_app->mainWindow == nullptr)
    {
        return;
    }

    if (!show && g_app->activeHotkeyIndex >= 0)
    {
        CancelHotkeyCapture(g_app->mainWindow);
    }

    g_app->settingsVisible = show;

    const int mainControlIds[] = {
        kCommandCaptureFirst,
        kCommandCaptureSecond,
        kCommandToggleOverlay,
        kCommandToggleClickThrough,
        kCommandCopyDistance,
    };
    for (const int id : mainControlIds)
    {
        ShowWindow(
            GetDlgItem(g_app->mainWindow, id),
            show ? SW_HIDE : SW_SHOW);
    }

    ShowWindow(g_app->settingsButton, show ? SW_HIDE : SW_SHOW);
    ShowWindow(g_app->settingsBackButton, show ? SW_SHOW : SW_HIDE);
    for (const HWND control : g_app->hotkeyControls)
    {
        ShowWindow(control, show ? SW_SHOW : SW_HIDE);
    }

    InvalidateRect(g_app->mainWindow, nullptr, TRUE);
    if (show)
    {
        SetFocus(g_app->mainWindow);
    }
}

bool HandleHotkeyCaptureKey(HWND window, UINT virtualKey)
{
    if (g_app == nullptr || !g_app->settingsVisible ||
        g_app->activeHotkeyIndex < 0)
    {
        return false;
    }

    CompleteHotkeyCapture(window, virtualKey);
    return true;
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

void CreateMainControls(HWND window)
{
    g_app->uiFont = CreateFontW(-15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    auto button = [&](int id, const wchar_t* label, int x, int y, int w, int h) -> HWND {
        HWND control = CreateWindowW(L"BUTTON", label, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
            x, y, w, h, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), g_app->instance, nullptr);
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(g_app->uiFont), TRUE);
        return control;
    };
    g_app->settingsButton = button(kCommandSettings, L"\u2699", 344, 8, 40, 34);
    g_app->settingsBackButton = button(kCommandSettingsBack, L"Back", 16, 8, 52, 34);

    button(kCommandCaptureFirst, L"Capture player", 16, 220, 170, 36);
    button(kCommandCaptureSecond, L"Capture target", 198, 220, 170, 36);
    button(kCommandToggleOverlay, L"Overlay", 16, 278, 100, 32);
    button(kCommandToggleClickThrough, L"Click-through", 124, 278, 142, 32);
    button(kCommandCopyDistance, L"Copy distance", 16, 326, 352, 32);

    for (size_t index = 0; index < kHotkeyCount; ++index)
    {
        const std::wstring label = FormatHotkey(g_app->hotkeys[index]);
        g_app->hotkeyControls[index] = button(
            kCommandHotkeyBase + static_cast<int>(index),
            label.c_str(),
            190,
            76 + static_cast<int>(index) * 48,
            178,
            32);
    }

    BOOL dark = TRUE;
    DwmSetWindowAttribute(window, 20, &dark, sizeof(dark));
    COLORREF caption = kBackground;
    DwmSetWindowAttribute(window, 35, &caption, sizeof(caption));
}

void PaintSettings(HDC dc)
{
    UiText(dc, L"Settings", {82, 12, 330, 43}, 21, kText, FW_SEMIBOLD);
    UiText(dc, L"Keyboard shortcuts", {18, 55, 368, 76}, 11, kMuted, FW_SEMIBOLD);
    UiText(dc, L"Click a shortcut, then press the key combination to assign it.",
        {18, 350, 382, 374}, 11, kMuted);
    for (size_t index = 0; index < kHotkeyCount; ++index)
    {
        const int top = 76 + static_cast<int>(index) * 48;
        UiText(
            dc,
            kHotkeyActionLabels[index],
            {18, top, 180, top + 32},
            12,
            kText,
            FW_NORMAL,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }
}

void PaintMain(HDC dc)
{
    if (g_app != nullptr && g_app->settingsVisible)
    {
        PaintSettings(dc);
        return;
    }

    UiText(dc, L"\u2295", {16, 13, 47, 48}, 30, kMint);
    UiText(dc, L"Arty Buddy", {55, 14, 300, 47}, 21, kText, FW_SEMIBOLD);
    UiText(dc, L"RANGE", {18, 63, 182, 82}, 10, kMuted, FW_SEMIBOLD);
    UiText(dc, L"BEARING", {200, 63, 366, 82}, 10, kMuted, FW_SEMIBOLD);
    UiText(dc, RangeValue(), {16, 83, 190, 131}, 34, kText, FW_SEMIBOLD);
    UiText(dc, BearingValue(), {198, 83, 368, 131}, 34, kText, FW_SEMIBOLD);
    for (int i = 0; i < 2; ++i)
    {
        int x = 16 + i * 182;
        const auto point = i == 1 ? g_app->second : g_app->first;
        UiText(dc, i == 1 ? L"TARGET" : L"PLAYER", {x, 157, x+170, 175}, 10, kMuted);
        UiText(dc, point ? FormatCoordinate(*point) : L"Not captured", {x, 178, x+170, 204}, 14);
    }
}

void PaintOverlay(HWND window, HDC dc)
{
    RECT client{};
    GetClientRect(window, &client);
    Card(dc, client, kBackground);
    const int w = client.right;
    UiText(dc, L"Arty Buddy", {16, 10, w-16, 32}, 13, kText, FW_SEMIBOLD);
    UiText(dc, RangeValue(), {16, 37, w/2, 82}, 30, kText, FW_SEMIBOLD);
    UiText(dc, BearingValue(), {w/2+8, 37, w-16, 82}, 30, kText, FW_SEMIBOLD);
    UiText(dc, L"PLAYER", {16, 91, w/2, 107}, 10, kMuted);
    UiText(dc, L"TARGET", {w/2+8, 91, w-16, 107}, 10, kMuted);
    UiText(dc, g_app->first ? FormatCoordinate(*g_app->first) : L"Not captured", {16, 108, w/2, 130}, 12);
    UiText(dc, g_app->second ? FormatCoordinate(*g_app->second) : L"Not captured", {w/2+8, 108, w-16, 130}, 12);
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

            HPEN anglePen = CreatePen(
                PS_SOLID,
                kOverlayGuidePenWidth,
                RGB(255, 55, 55));
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

    HPEN guidePen = CreatePen(
        PS_SOLID,
        kOverlayGuidePenWidth,
        RGB(255, 55, 55));
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
    case WM_DRAWITEM:
    {
        auto* item = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
        SetDCBrushColor(item->hDC, kBackground);
        FillRect(item->hDC, &item->rcItem, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
        bool primary = item->CtlID == kCommandCaptureFirst || item->CtlID == kCommandCaptureSecond;
        const bool settingsControl =
            item->CtlID == kCommandSettings ||
            item->CtlID == kCommandSettingsBack ||
            (item->CtlID >= kCommandHotkeyBase &&
             item->CtlID < kCommandHotkeyBase + static_cast<int>(kHotkeyCount));
        bool down = (item->itemState & ODS_SELECTED) != 0;
        Card(
            item->hDC,
            item->rcItem,
            primary ? (down ? RGB(81, 190, 154) : kMint) :
                (down ? RGB(34, 46, 58) : kSurface),
            primary ? kMint : (settingsControl ? kMint : kBorder),
            8);
        wchar_t label[128]{};
        GetWindowTextW(item->hwndItem, label, _countof(label));
        std::wstring text = label;
        if (item->CtlID == kCommandToggleOverlay) text += g_app->overlayEnabled ? L"  ON" : L"  OFF";
        if (item->CtlID == kCommandToggleClickThrough) text += g_app->clickThrough ? L"  ON" : L"  OFF";
        const int textSize = item->CtlID == kCommandSettings ? 20 : 12;
        UiText(item->hDC, text, item->rcItem, textSize, primary ? kBackground : kText,
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
        if (LOWORD(wParam) >= kCommandHotkeyBase &&
            LOWORD(wParam) < kCommandHotkeyBase + static_cast<int>(kHotkeyCount) &&
            HIWORD(wParam) == BN_CLICKED)
        {
            BeginHotkeyCapture(
                window,
                static_cast<size_t>(LOWORD(wParam) - kCommandHotkeyBase));
            return 0;
        }

        switch (LOWORD(wParam))
        {
        case kCommandSettings:
            ShowSettingsPage(true);
            return 0;
        case kCommandSettingsBack:
            ShowSettingsPage(false);
            return 0;
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

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (HandleHotkeyCaptureKey(window, static_cast<UINT>(wParam)))
        {
            return 0;
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
        L"Arty Buddy Overlay",
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
        L"Arty Buddy Range Overlay",
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
    LoadHotkeyBindings(state);
    g_app = &state;

    if (!RegisterWindowClasses(instance))
    {
        MessageBoxW(nullptr, L"Could not register the application windows.", L"Arty Buddy", MB_ICONERROR);
        return 1;
    }

    state.mainWindow = CreateWindowExW(
        0,
        kMainClassName,
        L"Arty Buddy",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN,
        (GetSystemMetrics(SM_CXSCREEN) - 400) / 2,
        (GetSystemMetrics(SM_CYSCREEN) - 476) / 2,
        400,
        420,
        nullptr,
        nullptr,
        instance,
        nullptr);

    if (state.mainWindow == nullptr)
    {
        MessageBoxW(nullptr, L"Could not create the main window.", L"Arty Buddy", MB_ICONERROR);
        return 1;
    }

    ShowSettingsPage(false);

    if (!CreateOverlayWindow() || !CreateRangeOverlayWindow())
    {
        MessageBoxW(nullptr, L"Could not create the overlay window.", L"Arty Buddy", MB_ICONERROR);
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
