/*
  SPDX-FileCopyrightText: 2026 Michael Reeves reeves.87@gmail.com
  SPDX-License-Identifier: BSD-2-Clause
*/
#include "translator.h"
#include "server.h"

#include <libintl.h>

#include <mutex>
#include <string>

constexpr char TRDOMAIN[] = "diff_ext";
static std::once_flag g_initFlag;

static std::wstring toWide(const char* utf8)
{
    std::wstring result;

    if(utf8 == nullptr || *utf8 == '\0')
        return {};

    const int length = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
    if(length <= 0)
        return {};

    result = std::wstring(length, L'\0');
    if(MultiByteToWideChar(CP_UTF8, 0, utf8, -1, result.data(), length) == 0)
        return {};

    result.pop_back();
    return result;
}

static std::string toUtf8(const std::wstring& wide)
{
    if(wide.empty())
        return {};

    const int length = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if(length <= 0)
        return {};

    std::string result(length, '\0');
    if(WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, result.data(), length, nullptr, nullptr) == 0)
        return {};

    result.pop_back();
    return result;
}

// The shell extension's own module directory rarely matches applicationDirPath()
// of the host process (explorer.exe), so it must be derived from this DLL's handle.
static std::wstring findLocaleDir()
{
    static const wchar_t* candidates[] = {L"\\share\\locale", L"\\..\\share\\locale", L"\\locale"};

    std::wstring dir;
    std::wstring path;
    std::wstring overrideDir = SERVER::instance()->getRegistryKeyString(L"", L"LocaleDir");
    wchar_t modulePath[MAX_PATH];

    if(!overrideDir.empty())
        return overrideDir;

    GetModuleFileNameW(SERVER::instance()->handle(), modulePath, MAX_PATH);

    path = modulePath;
    const auto pos = path.find_last_of(L"\\/");
    dir = (pos == std::wstring::npos) ? std::wstring() : path.substr(0, pos);

    for(const wchar_t* candidate: candidates)
    {
        const std::wstring full = dir + candidate;
        if(GetFileAttributesW(full.c_str()) != INVALID_FILE_ATTRIBUTES)
            return full;
    }
    return dir + L"\\share\\locale";
}

void Translator::init()
{
    // std::call_once guarantees the global gettext state (bindtextdomain/textdomain)
    // is written exactly once, no matter how many COM apartments/threads call in concurrently.
    std::call_once(g_initFlag, []() {
        const std::wstring localeDir = findLocaleDir();
        const std::string narrowDir = toUtf8(localeDir);
        bindtextdomain(TRDOMAIN, narrowDir.c_str());
        bind_textdomain_codeset(TRDOMAIN, "UTF-8");
        textdomain(TRDOMAIN);
    });
}

std::wstring Translator::translate(const char* msgid)
{
    init();
    return toWide(dgettext(TRDOMAIN, msgid));
}

std::wstring Translator::translateContext(const char* context, const char* msgid)
{
    init();
    {
        const std::string key = std::string(context) + '\x04' + msgid;
        const char* translated = dgettext(TRDOMAIN, key.c_str());
        // gettext returns the exact same pointer it was given when no translation exists.
        if(translated == key.c_str())
            return toWide(msgid);
        return toWide(translated);
    }
}

std::wstring Translator::arg(const std::wstring& text, const std::wstring& a1)
{
    std::wstring result = text;
    const size_t pos = result.find(L"%1");
    if(pos != std::wstring::npos)
        result.replace(pos, 2, a1);
    return result;
}

std::wstring Translator::arg(const std::wstring& text, const std::wstring& a1, const std::wstring& a2)
{
    return arg(arg(text, a1), a2);
}
