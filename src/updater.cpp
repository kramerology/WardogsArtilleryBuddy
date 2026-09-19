#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <shellapi.h>

#include <cwchar>
#include <string>
#include <vector>

#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "User32.lib")

namespace
{
std::wstring QuoteCommandLineArgument(const std::wstring& value)
{
    return L"\"" + value + L"\"";
}

int ApplyUpdate(
    const std::wstring& tempPath,
    const std::wstring& targetPath,
    DWORD parentProcessId)
{
    if (parentProcessId != 0)
    {
        HANDLE parent = OpenProcess(SYNCHRONIZE, FALSE, parentProcessId);
        if (parent != nullptr)
        {
            WaitForSingleObject(parent, 30000);
            CloseHandle(parent);
        }
    }

    bool replaced = false;
    for (int attempt = 0; attempt < 120; ++attempt)
    {
        if (MoveFileExW(
                tempPath.c_str(),
                targetPath.c_str(),
                MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED))
        {
            replaced = true;
            break;
        }
        Sleep(250);
    }

    if (!replaced)
    {
        DeleteFileW(tempPath.c_str());
        MessageBoxW(
            nullptr,
            L"Arty Buddy could not replace the current executable with the update.",
            L"Arty Buddy update",
            MB_OK | MB_ICONERROR);
        return 1;
    }

    std::wstring commandLine = QuoteCommandLineArgument(targetPath);
    std::vector<wchar_t> mutableCommandLine(
        commandLine.begin(),
        commandLine.end());
    mutableCommandLine.push_back(L'\0');

    const size_t separator = targetPath.find_last_of(L"\\/");
    const std::wstring directory = separator == std::wstring::npos
        ? L""
        : targetPath.substr(0, separator);

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(
            targetPath.c_str(),
            mutableCommandLine.data(),
            nullptr,
            nullptr,
            FALSE,
            0,
            nullptr,
            directory.empty() ? nullptr : directory.c_str(),
            &startup,
            &process))
    {
        MessageBoxW(
            nullptr,
            L"Arty Buddy was updated, but could not be restarted automatically.",
            L"Arty Buddy update",
            MB_OK | MB_ICONWARNING);
        return 1;
    }

    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return 0;
}
} // namespace

int APIENTRY wWinMain(
    HINSTANCE /*instance*/,
    HINSTANCE /*previousInstance*/,
    PWSTR /*commandLine*/,
    int /*showCommand*/)
{
    int argumentCount = 0;
    LPWSTR* arguments = CommandLineToArgvW(
        GetCommandLineW(),
        &argumentCount);
    if (arguments == nullptr)
    {
        return 1;
    }

    int result = 1;
    if (argumentCount >= 5 &&
        _wcsicmp(arguments[1], L"--apply-update") == 0)
    {
        wchar_t* end = nullptr;
        const unsigned long parentProcessId = std::wcstoul(
            arguments[4],
            &end,
            10);
        if (end != arguments[4] && *end == L'\0')
        {
            result = ApplyUpdate(
                arguments[2],
                arguments[3],
                static_cast<DWORD>(parentProcessId));
        }
    }

    LocalFree(arguments);
    return result;
}
