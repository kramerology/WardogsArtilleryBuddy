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
#include <winhttp.h>

#include <winrt/base.h>
#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>

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
#pragma comment(lib, "Winhttp.lib")

namespace
{
constexpr wchar_t kMainClassName[] = L"WarDogsArtillery.MainWindow";
constexpr wchar_t kOverlayClassName[] = L"WarDogsArtillery.OverlayWindow";
constexpr wchar_t kRangeOverlayClassName[] = L"WarDogsArtillery.RangeOverlayWindow";

constexpr UINT kTrayMessage = WM_APP + 1;
constexpr UINT kTrayId = 1;
constexpr UINT_PTR kGamePollTimer = 1;
constexpr UINT kGamePollIntervalMs = 250;
constexpr UINT kUpdateCheckMessage = WM_APP + 3;
constexpr UINT kUpdateDownloadMessage = WM_APP + 4;
constexpr wchar_t kTargetProcessName[] = L"WardogsClient-Win64-Shipping.exe";
constexpr wchar_t kCurrentVersion[] = L"0.0.4";
constexpr wchar_t kLatestReleaseApiUrl[] =
    L"https://api.github.com/repos/kramerology/WardogsArtilleryBuddy/releases/latest";

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
constexpr int kCommandApplyPlayer = 1009;
constexpr int kCommandApplyTarget = 1010;
constexpr int kCommandPastePlayer = 1011;
constexpr int kCommandPasteTarget = 1012;
constexpr int kCommandToggleAutoUpdate = 1013;
constexpr int kCommandToggleExitOnClose = 1014;
constexpr int kCommandHotkeyBase = 1100;
constexpr int kControlPlayerX = 1201;
constexpr int kControlPlayerY = 1202;
constexpr int kControlTargetX = 1203;
constexpr int kControlTargetY = 1204;
constexpr size_t kHotkeyCount = 5;

constexpr wchar_t kSettingsRegistryPath[] = L"Software\\ArtyBuddy";
constexpr wchar_t kAutoUpdateRegistryName[] = L"AutoUpdateEnabled";
constexpr wchar_t kExitOnCloseRegistryName[] = L"ExitOnMainWindowClose";
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

constexpr double kDefaultMetersPerCoordinateUnit = 100.0;
constexpr double kMaximumRangeMeters = 700.0;
constexpr LONG kCoordinateMacroMouseOffset = 10;
constexpr DWORD kCoordinateMacroStepDelayMs = 40;
constexpr DWORD kCoordinateMacroClickSettleDelayMs = 300;
constexpr int kCoordinateMacroSelectAllAttempts = 3;
constexpr DWORD kCoordinateMacroClipboardTimeoutMs = 1000;
constexpr int kCoordinateMacroMaxAttempts = 2;
constexpr DWORD kCoordinateMacroRetryStepDelayMs = 100;
constexpr DWORD kCoordinateMacroRetryClickSettleDelayMs = 700;
constexpr DWORD kCoordinateMacroRetryClipboardTimeoutMs = 1500;

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

struct AppState
{
    HINSTANCE instance{};
    HWND mainWindow{};
    HWND overlayWindow{};
    HWND rangeOverlayWindow{};
    HWND targetWindow{};
    DWORD targetProcessId{};

    HFONT uiFont{};
    HBRUSH inputBrush{};
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
    bool autoUpdateEnabled{true};
    bool exitOnMainWindowClose{false};
    bool closing{false};
    bool settingsVisible{false};
    bool hotkeysRegistered{false};
    int activeHotkeyIndex{-1};
    HotkeyBinding hotkeyCaptureOriginal{};
    std::array<HotkeyBinding, kHotkeyCount> hotkeys{};
    std::array<HWND, kHotkeyCount> hotkeyControls{};
    HWND playerXEdit{};
    HWND playerYEdit{};
    HWND targetXEdit{};
    HWND targetYEdit{};
    HWND settingsButton{};
    HWND settingsBackButton{};
    HWND applyPlayerButton{};
    HWND applyTargetButton{};
    HWND pastePlayerButton{};
    HWND pasteTargetButton{};
    HWND autoUpdateButton{};
    HWND exitOnCloseButton{};
    std::atomic_bool updateCheckInProgress{false};
    std::atomic_bool updateDownloadInProgress{false};
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
    state.autoUpdateEnabled = true;
    state.exitOnMainWindowClose = false;

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

    DWORD autoUpdateEnabled = 1;
    DWORD autoUpdateEnabledSize = sizeof(autoUpdateEnabled);
    if (RegGetValueW(
            key,
            nullptr,
            kAutoUpdateRegistryName,
            RRF_RT_REG_DWORD,
            nullptr,
            &autoUpdateEnabled,
            &autoUpdateEnabledSize) == ERROR_SUCCESS)
    {
        state.autoUpdateEnabled = autoUpdateEnabled != 0;
    }

    DWORD exitOnMainWindowClose = 0;
    DWORD exitOnMainWindowCloseSize = sizeof(exitOnMainWindowClose);
    if (RegGetValueW(
            key,
            nullptr,
            kExitOnCloseRegistryName,
            RRF_RT_REG_DWORD,
            nullptr,
            &exitOnMainWindowClose,
            &exitOnMainWindowCloseSize) == ERROR_SUCCESS)
    {
        state.exitOnMainWindowClose = exitOnMainWindowClose != 0;
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

    const DWORD autoUpdateEnabled = state.autoUpdateEnabled ? 1u : 0u;
    RegSetValueExW(
        key,
        kAutoUpdateRegistryName,
        0,
        REG_DWORD,
        reinterpret_cast<const BYTE*>(&autoUpdateEnabled),
        sizeof(autoUpdateEnabled));

    const DWORD exitOnMainWindowClose = state.exitOnMainWindowClose ? 1u : 0u;
    RegSetValueExW(
        key,
        kExitOnCloseRegistryName,
        0,
        REG_DWORD,
        reinterpret_cast<const BYTE*>(&exitOnMainWindowClose),
        sizeof(exitOnMainWindowClose));

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

std::wstring DisplayCoordinate(const std::optional<Coordinate>& coordinate)
{
    return FormatCoordinate(coordinate.value_or(Coordinate{}));
}

void SetCoordinateEditValue(HWND edit, const std::optional<double>& value)
{
    if (edit == nullptr)
    {
        return;
    }

    const std::wstring text = FormatNumber(value.value_or(0.0), 3);
    SetWindowTextW(edit, text.c_str());
}

void UpdateCoordinateInputControls()
{
    if (g_app == nullptr)
    {
        return;
    }

    SetCoordinateEditValue(
        g_app->playerXEdit,
        g_app->first.has_value()
            ? std::optional<double>(g_app->first->x)
            : std::nullopt);
    SetCoordinateEditValue(
        g_app->playerYEdit,
        g_app->first.has_value()
            ? std::optional<double>(g_app->first->y)
            : std::nullopt);
    SetCoordinateEditValue(
        g_app->targetXEdit,
        g_app->second.has_value()
            ? std::optional<double>(g_app->second->x)
            : std::nullopt);
    SetCoordinateEditValue(
        g_app->targetYEdit,
        g_app->second.has_value()
            ? std::optional<double>(g_app->second->y)
            : std::nullopt);
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

std::optional<std::wstring> GetClipboardText()
{
    if (!OpenClipboard(g_app != nullptr ? g_app->mainWindow : nullptr))
    {
        return std::nullopt;
    }

    std::optional<std::wstring> result;
    if (const HANDLE unicodeData = GetClipboardData(CF_UNICODETEXT);
        unicodeData != nullptr)
    {
        const auto* text = static_cast<const wchar_t*>(GlobalLock(unicodeData));
        if (text != nullptr)
        {
            result = std::wstring(text);
            GlobalUnlock(unicodeData);
        }
    }
    else if (const HANDLE ansiData = GetClipboardData(CF_TEXT); ansiData != nullptr)
    {
        const auto* text = static_cast<const char*>(GlobalLock(ansiData));
        if (text != nullptr)
        {
            const int length = MultiByteToWideChar(
                CP_ACP,
                MB_PRECOMPOSED,
                text,
                -1,
                nullptr,
                0);
            if (length > 0)
            {
                std::wstring converted(static_cast<size_t>(length), L'\0');
                MultiByteToWideChar(
                    CP_ACP,
                    MB_PRECOMPOSED,
                    text,
                    -1,
                    converted.data(),
                    length);
                converted.resize(static_cast<size_t>(length - 1));
                result = std::move(converted);
            }
            GlobalUnlock(ansiData);
        }
    }

    CloseClipboard();
    return result;
}

struct CoordinateNumberToken
{
    double value{};
    std::size_t position{};
    std::size_t length{};
    int fractionalDigits{};
};

struct CoordinateLabeledValue
{
    CoordinateNumberToken value;
    std::size_t labelPosition{};
};

struct ParsedCoordinateText
{
    Coordinate coordinate{};
    int quality{};
};

const std::wstring& CoordinateNumberPattern()
{
    static const std::wstring pattern =
        LR"([-+]?(?:\d+(?:\.\d*)?|\.\d+))";
    return pattern;
}

const std::wstring& CoordinateNumberLikePattern()
{
    // Accept common text-copy glyph variants while keeping substitutions
    // restricted to values immediately following an x/y label.
    static const std::wstring pattern =
        LR"([-+]?(?:[0-9OoGgSsBbIiLl|]+(?:\.[0-9OoGgSsBbIiLl|]*)?|\.[0-9OoGgSsBbIiLl|]+))";
    return pattern;
}

std::optional<CoordinateNumberToken> ParseCoordinateNumberToken(
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
        return CoordinateNumberToken{
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

std::vector<CoordinateNumberToken> FindCoordinateNumberTokens(const std::wstring& text)
{
    std::vector<CoordinateNumberToken> numbers;

    try
    {
        const std::wregex numberRegex(CoordinateNumberPattern());
        for (std::wsregex_iterator iterator(text.begin(), text.end(), numberRegex);
             iterator != std::wsregex_iterator();
             ++iterator)
        {
            const auto match = *iterator;
            const auto token = ParseCoordinateNumberToken(
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
        // Invalid coordinate text is handled as a parse failure below.
    }

    return numbers;
}

std::vector<CoordinateLabeledValue> FindCoordinateLabeledValues(
    const std::wstring& text,
    wchar_t label)
{
    std::vector<CoordinateLabeledValue> values;

    try
    {
        const std::wstring labelPattern = label == L'x'
            ? L"[xX]"
            : L"[yY]";
        const std::wregex pattern(
            labelPattern + L"\\s*[:=]?\\s*(" + CoordinateNumberLikePattern() + L")");

        for (std::wsregex_iterator iterator(text.begin(), text.end(), pattern);
             iterator != std::wsregex_iterator();
             ++iterator)
        {
            const auto match = *iterator;
            if (match.size() < 2)
            {
                continue;
            }

            const auto token = ParseCoordinateNumberToken(
                match.str(1),
                static_cast<std::size_t>(match.position(1)));
            if (token.has_value())
            {
                values.push_back(CoordinateLabeledValue{
                    *token,
                    static_cast<std::size_t>(match.position())});
            }
        }
    }
    catch (const std::exception&)
    {
        // Invalid coordinate text is handled as a parse failure below.
    }

    return values;
}

int CoordinatePrecisionQuality(const CoordinateNumberToken& token)
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

const CoordinateNumberToken* FindNearestDecimalToken(
    const std::vector<CoordinateNumberToken>& numbers,
    std::size_t anchor,
    bool preferBefore)
{
    const CoordinateNumberToken* nearest = nullptr;
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

std::optional<ParsedCoordinateText> ParseSingleCoordinateText(
    const std::wstring& text)
{
    const auto numbers = FindCoordinateNumberTokens(text);
    const auto xValues = FindCoordinateLabeledValues(text, L'x');
    const auto yValues = FindCoordinateLabeledValues(text, L'y');

    if (!xValues.empty() && !yValues.empty())
    {
        const CoordinateLabeledValue* bestX = nullptr;
        const CoordinateLabeledValue* bestY = nullptr;
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
            return ParsedCoordinateText{
                Coordinate{bestX->value.value, bestY->value.value},
                60 + CoordinatePrecisionQuality(bestX->value) +
                    CoordinatePrecisionQuality(bestY->value)};
        }
    }

    // If one axis label is missing, associate the nearest decimal token with
    // the surviving axis label.
    if (!xValues.empty())
    {
        const auto& x = xValues.front();
        const auto* y = FindNearestDecimalToken(numbers, x.value.position, true);
        if (y != nullptr)
        {
            return ParsedCoordinateText{
                Coordinate{x.value.value, y->value},
                35 + CoordinatePrecisionQuality(x.value) +
                    CoordinatePrecisionQuality(*y)};
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
            return ParsedCoordinateText{
                Coordinate{x->value, y.value.value},
                35 + CoordinatePrecisionQuality(*x) +
                    CoordinatePrecisionQuality(y.value)};
        }
    }

    // Keep support for a clean, unlabeled "x, y" coordinate result. This fallback
    // is deliberately only used when there are exactly two decimal values,
    // so axis/range integers in the broad capture cannot become coordinates.
    std::vector<const CoordinateNumberToken*> decimalNumbers;
    for (const auto& number : numbers)
    {
        if (number.fractionalDigits > 0)
        {
            decimalNumbers.push_back(&number);
        }
    }
    if (decimalNumbers.size() == 2)
    {
        return ParsedCoordinateText{
            Coordinate{decimalNumbers[0]->value, decimalNumbers[1]->value},
            20 + CoordinatePrecisionQuality(*decimalNumbers[0]) +
                CoordinatePrecisionQuality(*decimalNumbers[1])};
    }

    return std::nullopt;
}

std::optional<Coordinate> ParseCoordinateText(const std::wstring& text)
{
    const auto parsed = ParseSingleCoordinateText(text);
    return parsed.has_value()
        ? std::optional<Coordinate>(parsed->coordinate)
        : std::nullopt;
}

struct WinrtApartmentGuard
{
    bool initialized{false};

    WinrtApartmentGuard()
    {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        initialized = true;
    }

    ~WinrtApartmentGuard()
    {
        if (initialized)
        {
            winrt::uninit_apartment();
        }
    }
};

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
    if (g_app->autoUpdateButton != nullptr)
    {
        InvalidateRect(g_app->autoUpdateButton, nullptr, TRUE);
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

void ClearCapturedCoordinates();

void RefreshTargetGame()
{
    if (g_app == nullptr || g_app->overlayWindow == nullptr)
    {
        return;
    }

    const DWORD processId = FindTargetProcessId();
    const HWND targetWindow = FindTargetMainWindow(processId);
    const bool gameWasRunning = g_app->targetProcessId != 0;

    const bool oldVisible = g_app->overlayVisible;
    const HWND oldTargetWindow = g_app->targetWindow;
    g_app->targetProcessId = processId;
    g_app->targetWindow = targetWindow;

    if (gameWasRunning && processId == 0)
    {
        ClearCapturedCoordinates();
        SetStatus(L"Game client closed. Player and target positions cleared.");
        UpdateDisplay();
    }

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

void RecalculateSolution()
{
    if (g_app == nullptr || !g_app->first.has_value() ||
        !g_app->second.has_value())
    {
        if (g_app != nullptr)
        {
            g_app->distance.reset();
            g_app->bearing.reset();
            g_app->compassDirection.clear();
        }
        return;
    }

    const double dx = g_app->second->x - g_app->first->x;
    const double dy = g_app->second->y - g_app->first->y;
    g_app->distance = std::hypot(dx, dy);
    const DirectionResult direction = CalculateDirection(*g_app->first, *g_app->second);
    g_app->bearing = direction.bearing;
    g_app->compassDirection = direction.compass;
}

void ClearCapturedCoordinates()
{
    if (g_app == nullptr)
    {
        return;
    }

    g_app->first.reset();
    g_app->second.reset();
    RecalculateSolution();
    UpdateCoordinateInputControls();
}

void ClearCoordinateForCapture(bool firstPoint)
{
    if (g_app == nullptr)
    {
        return;
    }

    if (firstPoint)
    {
        g_app->first.reset();
    }
    else
    {
        g_app->second.reset();
    }
    RecalculateSolution();
    UpdateCoordinateInputControls();
    UpdateDisplay();
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
    }
    else
    {
        g_app->second = coordinate;
    }

    RecalculateSolution();
    if (g_app->distance.has_value() && g_app->bearing.has_value())
    {
        SetStatus(
            L"Distance: " + FormatNumber(*g_app->distance, 2) + L" | Direction: " +
            FormatNumber(*g_app->bearing, 0) + L"\u00b0 " + g_app->compassDirection);
    }
    else if (firstPoint)
    {
        SetStatus(L"Captured the player's position. Capture or enter the target position.");
    }
    else
    {
        SetStatus(L"Captured the target's position. Capture or enter the player's position.");
    }
    UpdateCoordinateInputControls();
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

bool SendMouseInput(DWORD flags, LONG dx = 0, LONG dy = 0)
{
    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dx = dx;
    input.mi.dy = dy;
    input.mi.dwFlags = flags;
    return SendInput(1, &input, sizeof(input)) == 1;
}

bool SendMouseClick(DWORD buttonDown, DWORD buttonUp)
{
    INPUT inputs[2]{};
    inputs[0].type = INPUT_MOUSE;
    inputs[0].mi.dwFlags = buttonDown;
    inputs[1].type = INPUT_MOUSE;
    inputs[1].mi.dwFlags = buttonUp;
    return SendInput(_countof(inputs), inputs, sizeof(INPUT)) == _countof(inputs);
}

bool SendKeyInput(WORD virtualKey, bool down)
{
    INPUT input{};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = virtualKey;
    input.ki.dwFlags = down ? 0 : KEYEVENTF_KEYUP;
    return SendInput(1, &input, sizeof(input)) == 1;
}

bool SendKeyPress(WORD virtualKey)
{
    return SendKeyInput(virtualKey, true) && SendKeyInput(virtualKey, false);
}

bool SendCtrlShortcut(WORD virtualKey)
{
    const bool controlDown = SendKeyInput(VK_LCONTROL, true);
    const bool keyPress = SendKeyPress(virtualKey);
    const bool controlUp = SendKeyInput(VK_LCONTROL, false);
    return controlDown && keyPress && controlUp;
}

struct CursorPositionRestorer
{
    POINT position{};

    ~CursorPositionRestorer()
    {
        SetCursorPos(position.x, position.y);
    }
};

std::optional<std::wstring> RunCoordinateClipboardMacro(
    std::wstring& error,
    DWORD stepDelayMs,
    DWORD clickSettleDelayMs,
    DWORD clipboardTimeoutMs)
{
    POINT initialCursor{};
    if (!GetCursorPos(&initialCursor))
    {
        error = L"Could not read the current mouse position.";
        return std::nullopt;
    }
    CursorPositionRestorer restoreCursor{initialCursor};

    const DWORD initialClipboardSequence = GetClipboardSequenceNumber();

    if (!SendMouseClick(MOUSEEVENTF_RIGHTDOWN, MOUSEEVENTF_RIGHTUP))
    {
        error = L"Could not send the right-click input to the game.";
        return std::nullopt;
    }
    Sleep(stepDelayMs);

    if (!SendMouseInput(
            MOUSEEVENTF_MOVE,
            kCoordinateMacroMouseOffset,
            kCoordinateMacroMouseOffset))
    {
        error = L"Could not move the mouse for the coordinate capture macro.";
        return std::nullopt;
    }
    Sleep(stepDelayMs);

    if (!SendMouseClick(MOUSEEVENTF_LEFTDOWN, MOUSEEVENTF_LEFTUP))
    {
        error = L"Could not send the left-click input to the game.";
        return std::nullopt;
    }
    Sleep(clickSettleDelayMs);

    for (int attempt = 0; attempt < kCoordinateMacroSelectAllAttempts; ++attempt)
    {
        if (!SendCtrlShortcut('A'))
        {
            error = L"Could not send the coordinate copy keyboard input to the game.";
            return std::nullopt;
        }
        Sleep(stepDelayMs);
    }
    if (!SendCtrlShortcut('C'))
    {
        error = L"Could not send the coordinate copy keyboard input to the game.";
        return std::nullopt;
    }
    Sleep(stepDelayMs);

    const ULONGLONG deadline =
        GetTickCount64() + clipboardTimeoutMs;
    bool clipboardChanged = false;
    if (initialClipboardSequence == 0)
    {
        Sleep(stepDelayMs * 2);
        clipboardChanged = true;
    }
    while (!clipboardChanged && GetTickCount64() < deadline)
    {
        clipboardChanged = GetClipboardSequenceNumber() != initialClipboardSequence;
        if (!clipboardChanged)
        {
            Sleep(10);
        }
    }

    if (!SendKeyPress(VK_BACK))
    {
        error = L"Could not finish the coordinate capture macro.";
        return std::nullopt;
    }
    Sleep(stepDelayMs);

    // This is the final input event in the macro; cursor restoration happens
    // afterward as cleanup.
    if (!SendMouseClick(MOUSEEVENTF_LEFTDOWN, MOUSEEVENTF_LEFTUP))
    {
        error = L"Could not finish the coordinate capture macro.";
        return std::nullopt;
    }
    Sleep(stepDelayMs);

    if (!clipboardChanged)
    {
        error = L"The game did not copy coordinates to the clipboard.";
        return std::nullopt;
    }

    const auto text = GetClipboardText();
    if (!text.has_value())
    {
        error = L"The coordinate clipboard text could not be read.";
        return std::nullopt;
    }

    return text;
}

void StartCoordinateCapture(bool firstPoint)
{
    if (g_app == nullptr)
    {
        return;
    }

    ClearCoordinateForCapture(firstPoint);
    RefreshTargetGame();
    if (g_app->targetWindow == nullptr || !IsWindow(g_app->targetWindow))
    {
        SetStatus(L"WardogsClient-Win64-Shipping.exe is not running or has no visible window.");
        MessageBeep(MB_ICONWARNING);
        return;
    }

    std::wstring focusError;
    if (!BringTargetGameToForeground(focusError))
    {
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

    DwmFlush();
    Sleep(kCoordinateMacroStepDelayMs);

    std::wstring macroError;
    std::optional<Coordinate> coordinate;
    for (int attempt = 0; attempt < kCoordinateMacroMaxAttempts; ++attempt)
    {
        const bool retry = attempt > 0;
        if (retry)
        {
            SetStatus(
                firstPoint
                    ? L"Player coordinates were not detected. Retrying with longer waits..."
                    : L"Target coordinates were not detected. Retrying with longer waits...");
        }
        else
        {
            SetStatus(firstPoint ? L"Running the player coordinate macro..."
                                 : L"Running the target coordinate macro...");
        }

        const auto clipboardText = RunCoordinateClipboardMacro(
            macroError,
            retry ? kCoordinateMacroRetryStepDelayMs
                  : kCoordinateMacroStepDelayMs,
            retry ? kCoordinateMacroRetryClickSettleDelayMs
                  : kCoordinateMacroClickSettleDelayMs,
            retry ? kCoordinateMacroRetryClipboardTimeoutMs
                  : kCoordinateMacroClipboardTimeoutMs);
        if (!clipboardText.has_value())
        {
            continue;
        }

        const auto parsedCoordinate = ParseCoordinateText(*clipboardText);
        if (parsedCoordinate.has_value())
        {
            coordinate = parsedCoordinate;
            break;
        }

        macroError =
            L"The game copied text, but it did not contain a valid X/Y coordinate pair.";
    }

    if (infoOverlayWasVisible)
    {
        ShowWindow(g_app->overlayWindow, SW_SHOWNOACTIVATE);
    }
    if (rangeOverlayWasVisible)
    {
        ShowWindow(g_app->rangeOverlayWindow, SW_SHOWNOACTIVATE);
    }
    UpdateOverlay();

    if (!coordinate.has_value())
    {
        if (macroError.empty())
        {
            macroError = L"The coordinate macro did not return valid coordinates.";
        }
        SetStatus(macroError);
        MessageBeep(MB_ICONWARNING);
        return;
    }

    ApplyCapturedCoordinate(firstPoint, *coordinate);
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

std::wstring ReadEditText(HWND edit)
{
    if (edit == nullptr)
    {
        return {};
    }

    const int length = GetWindowTextLengthW(edit);
    std::vector<wchar_t> buffer(static_cast<size_t>(length) + 1, L'\0');
    GetWindowTextW(edit, buffer.data(), static_cast<int>(buffer.size()));
    return std::wstring(buffer.data());
}

std::optional<double> ParseManualCoordinateValue(const std::wstring& text)
{
    try
    {
        size_t consumed = 0;
        const double value = std::stod(text, &consumed);
        if (!std::isfinite(value))
        {
            return std::nullopt;
        }

        if (text.find_first_not_of(L" \t\r\n", consumed) != std::wstring::npos)
        {
            return std::nullopt;
        }

        return value;
    }
    catch (const std::exception&)
    {
        return std::nullopt;
    }
}

void ApplyManualCoordinate(bool firstPoint)
{
    if (g_app == nullptr)
    {
        return;
    }

    const HWND xEdit = firstPoint ? g_app->playerXEdit : g_app->targetXEdit;
    const HWND yEdit = firstPoint ? g_app->playerYEdit : g_app->targetYEdit;
    const auto x = ParseManualCoordinateValue(ReadEditText(xEdit));
    const auto y = ParseManualCoordinateValue(ReadEditText(yEdit));
    if (!x.has_value() || !y.has_value())
    {
        SetStatus(L"Enter valid numeric X and Y coordinates.");
        MessageBeep(MB_ICONWARNING);
        return;
    }

    ApplyCapturedCoordinate(firstPoint, Coordinate{*x, *y});
}

void ApplyClipboardCoordinate(bool firstPoint)
{
    const auto text = GetClipboardText();
    if (!text.has_value())
    {
        SetStatus(L"Could not read text from the clipboard.");
        MessageBeep(MB_ICONWARNING);
        return;
    }

    const auto coordinate = ParseCoordinateText(*text);
    if (!coordinate.has_value())
    {
        SetStatus(L"Clipboard text did not contain a valid X/Y coordinate pair.");
        MessageBeep(MB_ICONWARNING);
        return;
    }

    ApplyCapturedCoordinate(firstPoint, *coordinate);
}

struct SemanticVersion
{
    int major{};
    int minor{};
    int patch{};
};

bool IsNewerVersion(const SemanticVersion& candidate, const SemanticVersion& current)
{
    if (candidate.major != current.major)
    {
        return candidate.major > current.major;
    }
    if (candidate.minor != current.minor)
    {
        return candidate.minor > current.minor;
    }
    return candidate.patch > current.patch;
}

std::optional<SemanticVersion> ParseSemanticVersion(const std::wstring& value)
{
    static const std::wregex pattern(
        LR"(^[vV]?([0-9]+)\.([0-9]+)\.([0-9]+)(?:[-+].*)?$)");
    std::wsmatch match;
    if (!std::regex_match(value, match, pattern))
    {
        return std::nullopt;
    }

    try
    {
        const long long major = std::stoll(match[1].str());
        const long long minor = std::stoll(match[2].str());
        const long long patch = std::stoll(match[3].str());
        const long long maximum = std::numeric_limits<int>::max();
        if (major > maximum || minor > maximum || patch > maximum)
        {
            return std::nullopt;
        }

        return SemanticVersion{
            static_cast<int>(major),
            static_cast<int>(minor),
            static_cast<int>(patch)};
    }
    catch (const std::exception&)
    {
        return std::nullopt;
    }
}

struct WinHttpHandle
{
    HINTERNET value{};

    ~WinHttpHandle()
    {
        if (value != nullptr)
        {
            WinHttpCloseHandle(value);
        }
    }

    operator HINTERNET() const
    {
        return value;
    }
};

constexpr size_t kMaximumUpdateDownloadBytes = 64u * 1024u * 1024u;

bool HttpGetBytes(
    const std::wstring& url,
    std::vector<std::uint8_t>& response,
    std::wstring& error)
{
    std::wstring mutableUrl = url;
    URL_COMPONENTS components{};
    components.dwStructSize = sizeof(components);
    components.dwSchemeLength = static_cast<DWORD>(-1);
    components.dwHostNameLength = static_cast<DWORD>(-1);
    components.dwUrlPathLength = static_cast<DWORD>(-1);
    components.dwExtraInfoLength = static_cast<DWORD>(-1);
    if (!WinHttpCrackUrl(
            mutableUrl.data(),
            static_cast<DWORD>(mutableUrl.size()),
            0,
            &components))
    {
        error = L"Could not parse the GitHub update URL.";
        return false;
    }

    if (components.nScheme != INTERNET_SCHEME_HTTPS)
    {
        error = L"The GitHub update URL was not HTTPS.";
        return false;
    }

    const std::wstring host(
        components.lpszHostName,
        components.dwHostNameLength);
    std::wstring path(
        components.lpszUrlPath,
        components.dwUrlPathLength);
    if (components.dwExtraInfoLength > 0)
    {
        path.append(components.lpszExtraInfo, components.dwExtraInfoLength);
    }
    if (path.empty())
    {
        path = L"/";
    }

    const std::wstring userAgent = L"ArtyBuddy/" + std::wstring(kCurrentVersion);
    WinHttpHandle session{
        WinHttpOpen(
            userAgent.c_str(),
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0)};
    if (session.value == nullptr)
    {
        error = L"Could not start the Windows HTTP client.";
        return false;
    }

    WinHttpHandle connection{
        WinHttpConnect(
            session,
            host.c_str(),
            components.nPort,
            0)};
    if (connection.value == nullptr)
    {
        error = L"Could not connect to GitHub.";
        return false;
    }

    WinHttpHandle request{
        WinHttpOpenRequest(
            connection,
            L"GET",
            path.c_str(),
            nullptr,
            WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            WINHTTP_FLAG_SECURE)};
    if (request.value == nullptr)
    {
        error = L"Could not create the GitHub request.";
        return false;
    }

    const std::wstring headers =
        L"Accept: application/vnd.github+json\r\nUser-Agent: " + userAgent + L"\r\n";
    if (!WinHttpSendRequest(
            request,
            headers.c_str(),
            static_cast<DWORD>(-1),
            WINHTTP_NO_REQUEST_DATA,
            0,
            0,
            0) ||
        !WinHttpReceiveResponse(request, nullptr))
    {
        error = L"The GitHub request could not be completed.";
        return false;
    }

    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    if (!WinHttpQueryHeaders(
            request,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &statusCode,
            &statusSize,
            WINHTTP_NO_HEADER_INDEX) ||
        statusCode != HTTP_STATUS_OK)
    {
        error = L"GitHub returned HTTP " + std::to_wstring(statusCode) + L".";
        return false;
    }

    DWORD contentLength = 0;
    DWORD contentLengthSize = sizeof(contentLength);
    if (WinHttpQueryHeaders(
            request,
            WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &contentLength,
            &contentLengthSize,
            WINHTTP_NO_HEADER_INDEX) &&
        contentLength > kMaximumUpdateDownloadBytes)
    {
        error = L"The GitHub update is unexpectedly large.";
        return false;
    }

    response.clear();
    if (contentLength > 0)
    {
        response.reserve(contentLength);
    }

    for (;;)
    {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(request, &available))
        {
            error = L"Could not read the GitHub response.";
            return false;
        }
        if (available == 0)
        {
            break;
        }
        if (response.size() + available > kMaximumUpdateDownloadBytes)
        {
            error = L"The GitHub update is unexpectedly large.";
            return false;
        }

        const size_t previousSize = response.size();
        response.resize(previousSize + available);
        DWORD bytesRead = 0;
        if (!WinHttpReadData(
                request,
                response.data() + previousSize,
                available,
                &bytesRead))
        {
            error = L"Could not download the GitHub update.";
            return false;
        }
        response.resize(previousSize + bytesRead);
        if (bytesRead == 0)
        {
            break;
        }
    }

    return true;
}

std::optional<std::wstring> Utf8ToWide(const std::vector<std::uint8_t>& bytes)
{
    if (bytes.empty())
    {
        return std::nullopt;
    }

    if (bytes.size() > static_cast<size_t>(std::numeric_limits<int>::max()))
    {
        return std::nullopt;
    }

    const int byteCount = static_cast<int>(bytes.size());
    const int characterCount = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        reinterpret_cast<const char*>(bytes.data()),
        byteCount,
        nullptr,
        0);
    if (characterCount <= 0)
    {
        return std::nullopt;
    }

    std::wstring result(static_cast<size_t>(characterCount), L'\0');
    if (MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            reinterpret_cast<const char*>(bytes.data()),
            byteCount,
            result.data(),
            characterCount) != characterCount)
    {
        return std::nullopt;
    }

    return result;
}

bool EndsWithCaseInsensitive(const std::wstring& value, const wchar_t* suffix)
{
    const size_t suffixLength = wcslen(suffix);
    return value.size() >= suffixLength &&
        _wcsicmp(
            value.c_str() + value.size() - suffixLength,
            suffix) == 0;
}

struct LatestReleaseInfo
{
    SemanticVersion version{};
    std::wstring tag;
    std::wstring assetName;
    std::wstring assetUrl;
};

bool FetchLatestRelease(LatestReleaseInfo& release, std::wstring& error)
{
    std::vector<std::uint8_t> response;
    if (!HttpGetBytes(kLatestReleaseApiUrl, response, error))
    {
        return false;
    }

    const auto jsonText = Utf8ToWide(response);
    if (!jsonText.has_value())
    {
        error = L"GitHub returned invalid UTF-8 JSON.";
        return false;
    }

    try
    {
        const auto json = winrt::Windows::Data::Json::JsonObject::Parse(
            winrt::hstring(*jsonText));
        if (json.GetNamedBoolean(L"draft", false) ||
            json.GetNamedBoolean(L"prerelease", false))
        {
            error = L"GitHub's latest release is not a stable release.";
            return false;
        }

        release.tag = json.GetNamedString(L"tag_name", L"").c_str();
        const auto version = ParseSemanticVersion(release.tag);
        if (!version.has_value())
        {
            error = L"GitHub's latest release has an invalid version tag.";
            return false;
        }
        release.version = *version;

        const auto assets = json.GetNamedArray(L"assets");
        std::wstring fallbackName;
        std::wstring fallbackUrl;
        for (const auto& value : assets)
        {
            const auto asset = value.GetObject();
            const std::wstring name = asset.GetNamedString(L"name", L"").c_str();
            const std::wstring url = asset.GetNamedString(
                L"browser_download_url",
                L"").c_str();
            if (!EndsWithCaseInsensitive(name, L".exe") || url.empty())
            {
                continue;
            }

            if (fallbackUrl.empty())
            {
                fallbackName = name;
                fallbackUrl = url;
            }
            if (name.find(L"ArtyBuddy") != std::wstring::npos ||
                name.find(L"WarDogsArtillery") != std::wstring::npos)
            {
                release.assetName = name;
                release.assetUrl = url;
                break;
            }
        }

        if (release.assetUrl.empty() && !fallbackUrl.empty())
        {
            release.assetName = fallbackName;
            release.assetUrl = fallbackUrl;
        }
        if (release.assetUrl.empty() ||
            release.assetUrl.rfind(L"https://", 0) != 0)
        {
            error = L"GitHub's latest release has no HTTPS Windows executable asset.";
            return false;
        }
    }
    catch (const winrt::hresult_error& exception)
    {
        error = L"Could not parse GitHub's latest release: ";
        error += exception.message().c_str();
        return false;
    }

    return true;
}

struct UpdateCheckResult
{
    bool updateAvailable{};
    std::wstring version;
    std::wstring downloadUrl;
    std::wstring error;
};

struct UpdateDownloadResult
{
    bool succeeded{};
    std::wstring version;
    std::wstring tempPath;
    std::wstring error;
};

std::optional<std::wstring> CreateUpdateTempPath()
{
    wchar_t tempDirectory[MAX_PATH]{};
    const DWORD directoryLength = GetTempPathW(
        _countof(tempDirectory),
        tempDirectory);
    if (directoryLength == 0 || directoryLength >= _countof(tempDirectory))
    {
        return std::nullopt;
    }

    wchar_t tempFile[MAX_PATH]{};
    if (GetTempFileNameW(
            tempDirectory,
            L"art",
            0,
            tempFile) == 0)
    {
        return std::nullopt;
    }

    DeleteFileW(tempFile);
    return std::wstring(tempFile);
}

bool WriteBinaryFile(
    const std::wstring& path,
    const std::vector<std::uint8_t>& bytes,
    std::wstring& error)
{
    HANDLE file = CreateFileW(
        path.c_str(),
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_TEMPORARY,
        nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        error = L"Could not create the temporary update file.";
        return false;
    }

    bool succeeded = true;
    size_t offset = 0;
    while (offset < bytes.size())
    {
        const DWORD chunkSize = static_cast<DWORD>(std::min<size_t>(
            bytes.size() - offset,
            std::numeric_limits<DWORD>::max()));
        DWORD written = 0;
        if (!WriteFile(
                file,
                bytes.data() + offset,
                chunkSize,
                &written,
                nullptr) ||
            written != chunkSize)
        {
            succeeded = false;
            break;
        }
        offset += written;
    }

    if (succeeded && !FlushFileBuffers(file))
    {
        succeeded = false;
    }
    CloseHandle(file);

    if (!succeeded)
    {
        DeleteFileW(path.c_str());
        error = L"Could not save the downloaded update.";
    }
    return succeeded;
}

bool DownloadUpdate(
    const std::wstring& url,
    const std::wstring& path,
    std::wstring& error)
{
    std::vector<std::uint8_t> bytes;
    if (!HttpGetBytes(url, bytes, error))
    {
        return false;
    }
    if (bytes.size() < 2 || bytes[0] != 'M' || bytes[1] != 'Z')
    {
        error = L"GitHub did not return a Windows executable.";
        return false;
    }

    return WriteBinaryFile(path, bytes, error);
}

void StartUpdateCheck()
{
    if (g_app == nullptr || !g_app->autoUpdateEnabled ||
        g_app->updateCheckInProgress.exchange(true))
    {
        return;
    }

    const HWND mainWindow = g_app->mainWindow;
    std::thread([mainWindow]()
    {
        auto result = std::make_unique<UpdateCheckResult>();
        try
        {
            WinrtApartmentGuard apartment;
            LatestReleaseInfo latest;
            if (!FetchLatestRelease(latest, result->error))
            {
                // Update checks are intentionally quiet when GitHub is
                // unavailable; the app remains fully usable offline.
            }
            else
            {
                const auto current = ParseSemanticVersion(kCurrentVersion);
                if (current.has_value() && IsNewerVersion(latest.version, *current))
                {
                    result->updateAvailable = true;
                    result->version = latest.tag;
                    result->downloadUrl = latest.assetUrl;
                }
            }
        }
        catch (const winrt::hresult_error& exception)
        {
            result->error = L"Update check failed: ";
            result->error += exception.message().c_str();
        }
        catch (const std::exception& exception)
        {
            result->error = L"Update check failed: ";
            const std::string message = exception.what();
            result->error += std::wstring(message.begin(), message.end());
        }

        UpdateCheckResult* rawResult = result.release();
        if (!PostMessageW(
                mainWindow,
                kUpdateCheckMessage,
                0,
                reinterpret_cast<LPARAM>(rawResult)))
        {
            // The window may have been closed while the request was running.
            // In that case there is no UI owner left for the result.
            delete rawResult;
        }
    }).detach();
}

void StartUpdateDownload(
    const std::wstring& version,
    const std::wstring& downloadUrl)
{
    if (g_app == nullptr || g_app->updateDownloadInProgress.exchange(true))
    {
        return;
    }

    const HWND mainWindow = g_app->mainWindow;
    std::thread([mainWindow, version, downloadUrl]()
    {
        auto result = std::make_unique<UpdateDownloadResult>();
        result->version = version;
        const auto tempPath = CreateUpdateTempPath();
        if (!tempPath.has_value())
        {
            result->error = L"Could not create a temporary update path.";
        }
        else
        {
            result->tempPath = *tempPath;
            result->succeeded = DownloadUpdate(
                downloadUrl,
                result->tempPath,
                result->error);
        }

        UpdateDownloadResult* rawResult = result.release();
        if (!PostMessageW(
                mainWindow,
                kUpdateDownloadMessage,
                0,
                reinterpret_cast<LPARAM>(rawResult)))
        {
            // The window may have been closed while the download was running.
            if (!rawResult->tempPath.empty())
            {
                DeleteFileW(rawResult->tempPath.c_str());
            }
            delete rawResult;
        }
    }).detach();
}

std::wstring QuoteCommandLineArgument(const std::wstring& value)
{
    return L"\"" + value + L"\"";
}

std::optional<std::wstring> CurrentExecutablePath()
{
    std::vector<wchar_t> buffer(MAX_PATH);
    for (;;)
    {
        const DWORD length = GetModuleFileNameW(
            nullptr,
            buffer.data(),
            static_cast<DWORD>(buffer.size()));
        if (length == 0)
        {
            return std::nullopt;
        }
        if (length < buffer.size() - 1)
        {
            return std::wstring(buffer.data(), length);
        }
        buffer.resize(buffer.size() * 2);
    }
}

bool LaunchPendingUpdate(const std::wstring& tempPath)
{
    const auto executablePath = CurrentExecutablePath();
    if (!executablePath.has_value())
    {
        return false;
    }

    const size_t separator = executablePath->find_last_of(L"\\/");
    if (separator == std::wstring::npos)
    {
        return false;
    }
    const std::wstring directory = executablePath->substr(0, separator);
    const std::wstring updaterPath = directory + L"\\ArtyBuddyUpdater.exe";
    if (GetFileAttributesW(updaterPath.c_str()) == INVALID_FILE_ATTRIBUTES)
    {
        return false;
    }

    std::wstring commandLine =
        QuoteCommandLineArgument(updaterPath) +
        L" --apply-update " +
        QuoteCommandLineArgument(tempPath) +
        L" " +
        QuoteCommandLineArgument(*executablePath) +
        L" " +
        std::to_wstring(GetCurrentProcessId());
    std::vector<wchar_t> mutableCommandLine(
        commandLine.begin(),
        commandLine.end());
    mutableCommandLine.push_back(L'\0');

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(
            updaterPath.c_str(),
            mutableCommandLine.data(),
            nullptr,
            nullptr,
            FALSE,
            CREATE_NO_WINDOW,
            nullptr,
            directory.c_str(),
            &startup,
            &process))
    {
        return false;
    }

    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return true;
}

void HandleUpdateCheckResult(UpdateCheckResult* result)
{
    std::unique_ptr<UpdateCheckResult> ownedResult(result);
    if (g_app == nullptr || result == nullptr)
    {
        return;
    }

    g_app->updateCheckInProgress = false;
    if (!result->updateAvailable || !g_app->autoUpdateEnabled)
    {
        return;
    }

    const std::wstring message =
        L"Arty Buddy " + result->version +
        L" is available. Download and install it now?";
    if (MessageBoxW(
            g_app->mainWindow,
            message.c_str(),
            L"Arty Buddy update",
            MB_YESNO | MB_ICONINFORMATION) == IDYES)
    {
        SetStatus(L"Downloading the Arty Buddy update...");
        StartUpdateDownload(result->version, result->downloadUrl);
    }
}

void HandleUpdateDownloadResult(UpdateDownloadResult* result)
{
    std::unique_ptr<UpdateDownloadResult> ownedResult(result);
    if (g_app == nullptr || result == nullptr)
    {
        return;
    }

    g_app->updateDownloadInProgress = false;
    if (!result->succeeded)
    {
        if (!result->tempPath.empty())
        {
            DeleteFileW(result->tempPath.c_str());
        }
        const std::wstring message = result->error.empty()
            ? L"The Arty Buddy update could not be downloaded."
            : result->error;
        SetStatus(message);
        MessageBoxW(
            g_app->mainWindow,
            message.c_str(),
            L"Arty Buddy update",
            MB_OK | MB_ICONWARNING);
        return;
    }

    if (!LaunchPendingUpdate(result->tempPath))
    {
        DeleteFileW(result->tempPath.c_str());
        const wchar_t message[] =
            L"The Arty Buddy update could not be started.";
        SetStatus(message);
        MessageBoxW(
            g_app->mainWindow,
            message,
            L"Arty Buddy update",
            MB_OK | MB_ICONWARNING);
        return;
    }

    g_app->closing = true;
    DestroyWindow(g_app->mainWindow);
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

void ToggleAutoUpdate()
{
    if (g_app == nullptr)
    {
        return;
    }

    g_app->autoUpdateEnabled = !g_app->autoUpdateEnabled;
    SaveHotkeyBindings(*g_app);
    SetStatus(
        g_app->autoUpdateEnabled
            ? L"Automatic update checks enabled."
            : L"Automatic update checks disabled.");
    UpdateDisplay();

    if (g_app->autoUpdateEnabled)
    {
        StartUpdateCheck();
    }
}

void ToggleExitOnMainWindowClose()
{
    if (g_app == nullptr)
    {
        return;
    }

    g_app->exitOnMainWindowClose = !g_app->exitOnMainWindowClose;
    SaveHotkeyBindings(*g_app);
    SetStatus(
        g_app->exitOnMainWindowClose
            ? L"Closing the main window will exit Arty Buddy."
            : L"Closing the main window will keep Arty Buddy in the tray.");
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
        kCommandApplyPlayer,
        kCommandApplyTarget,
        kCommandPastePlayer,
        kCommandPasteTarget,
    };
    for (const int id : mainControlIds)
    {
        ShowWindow(
            GetDlgItem(g_app->mainWindow, id),
            show ? SW_HIDE : SW_SHOW);
    }

    const HWND coordinateEdits[] = {
        g_app->playerXEdit,
        g_app->playerYEdit,
        g_app->targetXEdit,
        g_app->targetYEdit,
    };
    for (const HWND edit : coordinateEdits)
    {
        if (edit != nullptr)
        {
            ShowWindow(edit, show ? SW_HIDE : SW_SHOW);
        }
    }

    ShowWindow(g_app->settingsButton, show ? SW_HIDE : SW_SHOW);
    ShowWindow(g_app->settingsBackButton, show ? SW_SHOW : SW_HIDE);
    ShowWindow(g_app->autoUpdateButton, show ? SW_SHOW : SW_HIDE);
    ShowWindow(g_app->exitOnCloseButton, show ? SW_SHOW : SW_HIDE);
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

bool IsRangeTooFar()
{
    const auto value = CurrentRangeTargetMeters();
    return value.has_value() && *value > kMaximumRangeMeters;
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
    g_app->inputBrush = CreateSolidBrush(kSurface);
    auto button = [&](int id, const wchar_t* label, int x, int y, int w, int h) -> HWND {
        HWND control = CreateWindowW(L"BUTTON", label, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
            x, y, w, h, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), g_app->instance, nullptr);
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(g_app->uiFont), TRUE);
        return control;
    };
    auto edit = [&](int id, int x, int y, int w, int h) -> HWND {
        HWND control = CreateWindowExW(
            WS_EX_CLIENTEDGE,
            L"EDIT",
            L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
            x,
            y,
            w,
            h,
            window,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
            g_app->instance,
            nullptr);
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(g_app->uiFont), TRUE);
        return control;
    };
    g_app->settingsButton = button(kCommandSettings, L"\u2699", 344, 8, 40, 34);
    g_app->settingsBackButton = button(kCommandSettingsBack, L"Back", 16, 8, 52, 34);

    g_app->playerXEdit = edit(kControlPlayerX, 30, 258, 70, 28);
    g_app->playerYEdit = edit(kControlPlayerY, 126, 258, 70, 28);
    g_app->targetXEdit = edit(kControlTargetX, 30, 322, 70, 28);
    g_app->targetYEdit = edit(kControlTargetY, 126, 322, 70, 28);
    g_app->pastePlayerButton = button(kCommandPastePlayer, L"Paste", 206, 256, 80, 32);
    g_app->applyPlayerButton = button(kCommandApplyPlayer, L"Apply", 294, 256, 84, 32);
    g_app->pasteTargetButton = button(kCommandPasteTarget, L"Paste", 206, 320, 80, 32);
    g_app->applyTargetButton = button(kCommandApplyTarget, L"Apply", 294, 320, 84, 32);

    button(kCommandCaptureFirst, L"Capture player", 16, 370, 170, 36);
    button(kCommandCaptureSecond, L"Capture target", 198, 370, 170, 36);
    button(kCommandToggleOverlay, L"Overlay", 16, 428, 100, 32);
    button(kCommandToggleClickThrough, L"Click-through", 124, 428, 142, 32);
    button(kCommandCopyDistance, L"Copy distance", 16, 468, 352, 32);

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
    g_app->autoUpdateButton = button(
        kCommandToggleAutoUpdate,
        L"Auto-update",
        190,
        316,
        178,
        32);
    g_app->exitOnCloseButton = button(
        kCommandToggleExitOnClose,
        L"Exit on close",
        190,
        364,
        178,
        32);

    UpdateCoordinateInputControls();

    BOOL dark = TRUE;
    DwmSetWindowAttribute(window, 20, &dark, sizeof(dark));
    COLORREF caption = kBackground;
    DwmSetWindowAttribute(window, 35, &caption, sizeof(caption));
}

void PaintSettings(HDC dc)
{
    UiText(dc, L"Settings", {82, 12, 330, 43}, 21, kText, FW_SEMIBOLD);
    UiText(dc, L"Keyboard shortcuts", {18, 55, 368, 76}, 11, kMuted, FW_SEMIBOLD);
    UiText(dc, L"Automatic updates", {18, 316, 180, 348}, 12, kText);
    UiText(dc, L"Exit when window closes", {18, 364, 180, 396}, 12, kText);
    UiText(dc, L"Click a shortcut, then press the key combination to assign it.",
        {18, 412, 382, 436}, 11, kMuted);
    UiText(dc, L"Automatic updates check GitHub for a newer stable release at startup.",
        {18, 438, 382, 462}, 11, kMuted);
    UiText(dc, L"When exit-on-close is off, closing this window keeps Arty Buddy in the tray.",
        {18, 464, 382, 488}, 11, kMuted);
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
    if (IsRangeTooFar())
    {
        UiText(dc, L"Too Far", {16, 130, 190, 153}, 13, kCoral, FW_SEMIBOLD);
    }
    UiText(dc, BearingValue(), {198, 83, 368, 131}, 34, kText, FW_SEMIBOLD);
    for (int i = 0; i < 2; ++i)
    {
        int x = 16 + i * 182;
        const auto point = i == 1 ? g_app->second : g_app->first;
        UiText(dc, i == 1 ? L"TARGET" : L"PLAYER", {x, 157, x+170, 175}, 10, kMuted);
        UiText(dc, DisplayCoordinate(point), {x, 178, x+170, 204}, 14);
    }

    UiText(dc, L"MANUAL / CLIPBOARD", {18, 214, 382, 233}, 10, kMuted, FW_SEMIBOLD);
    UiText(dc, L"PLAYER", {16, 236, 100, 254}, 10, kMuted);
    UiText(dc, L"X", {16, 258, 29, 286}, 11, kMuted, FW_SEMIBOLD,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    UiText(dc, L"Y", {112, 258, 125, 286}, 11, kMuted, FW_SEMIBOLD,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    UiText(dc, L"TARGET", {16, 300, 100, 318}, 10, kMuted);
    UiText(dc, L"X", {16, 322, 29, 350}, 11, kMuted, FW_SEMIBOLD,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    UiText(dc, L"Y", {112, 322, 125, 350}, 11, kMuted, FW_SEMIBOLD,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

void PaintOverlay(HWND window, HDC dc)
{
    RECT client{};
    GetClientRect(window, &client);
    Card(dc, client, kBackground);
    const int w = client.right;
    UiText(dc, L"Arty Buddy", {16, 10, w-16, 32}, 13, kText, FW_SEMIBOLD);
    UiText(dc, RangeValue(), {16, 37, w/2, 82}, 30, kText, FW_SEMIBOLD);
    if (IsRangeTooFar())
    {
        UiText(dc, L"Too Far!", {16, 76, w/2, 92}, 11, kCoral, FW_SEMIBOLD);
    }
    UiText(dc, BearingValue(), {w/2+8, 37, w-16, 82}, 30, kText, FW_SEMIBOLD);
    UiText(dc, L"PLAYER", {16, 91, w/2, 107}, 10, kMuted);
    UiText(dc, L"TARGET", {w/2+8, 91, w-16, 107}, 10, kMuted);
    UiText(dc, DisplayCoordinate(g_app->first), {16, 108, w/2, 130}, 12);
    UiText(dc, DisplayCoordinate(g_app->second), {w/2+8, 108, w-16, 130}, 12);
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

            const bool rangeTooFar = IsRangeTooFar();
            if (!rangeTooFar)
            {
                HPEN anglePen = CreatePen(
                    PS_SOLID,
                    kOverlayGuidePenWidth,
                    RGB(255, 55, 55));
                HPEN previousPen = static_cast<HPEN>(SelectObject(dc, anglePen));
                MoveToEx(dc, guideX, markerTop, nullptr);
                LineTo(dc, guideX, markerBottom);
                SelectObject(dc, previousPen);
                DeleteObject(anglePen);
            }

            const std::wstring label = rangeTooFar
                ? L"TOO FAR!"
                : FormatNumber(*reference, 0);
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
        bool primary = item->CtlID == kCommandCaptureFirst ||
            item->CtlID == kCommandCaptureSecond ||
            item->CtlID == kCommandApplyPlayer ||
            item->CtlID == kCommandApplyTarget;
        const bool settingsControl =
            item->CtlID == kCommandSettings ||
            item->CtlID == kCommandSettingsBack ||
            item->CtlID == kCommandToggleAutoUpdate ||
            item->CtlID == kCommandToggleExitOnClose ||
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
        if (item->CtlID == kCommandToggleAutoUpdate) text += g_app->autoUpdateEnabled ? L"  ON" : L"  OFF";
        if (item->CtlID == kCommandToggleExitOnClose) text += g_app->exitOnMainWindowClose ? L"  ON" : L"  OFF";
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
    case WM_CTLCOLOREDIT:
        if (g_app != nullptr && g_app->inputBrush != nullptr)
        {
            const HWND edit = reinterpret_cast<HWND>(lParam);
            if (edit == g_app->playerXEdit || edit == g_app->playerYEdit ||
                edit == g_app->targetXEdit || edit == g_app->targetYEdit)
            {
                SetTextColor(reinterpret_cast<HDC>(wParam), kText);
                SetBkColor(reinterpret_cast<HDC>(wParam), kSurface);
                return reinterpret_cast<LRESULT>(g_app->inputBrush);
            }
        }
        break;
    case kUpdateCheckMessage:
        HandleUpdateCheckResult(reinterpret_cast<UpdateCheckResult*>(lParam));
        return 0;
    case kUpdateDownloadMessage:
        HandleUpdateDownloadResult(reinterpret_cast<UpdateDownloadResult*>(lParam));
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
            StartCoordinateCapture(true);
            return 0;
        case kCommandCaptureSecond:
            StartCoordinateCapture(false);
            return 0;
        case kCommandApplyPlayer:
            ApplyManualCoordinate(true);
            return 0;
        case kCommandApplyTarget:
            ApplyManualCoordinate(false);
            return 0;
        case kCommandPastePlayer:
            ApplyClipboardCoordinate(true);
            return 0;
        case kCommandPasteTarget:
            ApplyClipboardCoordinate(false);
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
        case kCommandToggleAutoUpdate:
            ToggleAutoUpdate();
            return 0;
        case kCommandToggleExitOnClose:
            ToggleExitOnMainWindowClose();
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
            StartCoordinateCapture(true);
            return 0;
        case kHotkeySecond:
            StartCoordinateCapture(false);
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
    {
        const UINT trayEvent = static_cast<UINT>(lParam);
        const UINT lowTrayEvent = LOWORD(lParam);
        const UINT highTrayEvent = HIWORD(lParam);
        const auto isTrayEvent = [&](UINT expected) {
            return trayEvent == expected || lowTrayEvent == expected ||
                highTrayEvent == expected;
        };

        if (isTrayEvent(WM_LBUTTONDBLCLK))
        {
            ShowMainWindow();
            return 0;
        }

        if (isTrayEvent(WM_RBUTTONUP) || isTrayEvent(WM_CONTEXTMENU))
        {
            ShowTrayMenu(window);
            return 0;
        }
        break;
    }

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
        if (g_app != nullptr && !g_app->closing && !g_app->exitOnMainWindowClose)
        {
            ShowWindow(window, SW_HIDE);
            SetStatus(L"Running in the system tray. Double-click the tray icon to reopen.");
            return 0;
        }
        if (g_app != nullptr)
        {
            g_app->closing = true;
        }
        DestroyWindow(window);
        return 0;

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
        (GetSystemMetrics(SM_CYSCREEN) - 560) / 2,
        400,
        560,
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
    StartUpdateCheck();

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        if (IsDialogMessageW(state.mainWindow, &message)) continue;
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    DeleteObject(state.uiFont);
    DeleteObject(state.inputBrush);
    g_app = nullptr;
    return static_cast<int>(message.wParam);
}
