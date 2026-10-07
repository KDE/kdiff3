/*
  SPDX-FileCopyrightText: 2026 Michael Reeves reeves.87@gmail.com
  SPDX-License-Identifier: BSD-2-Clause
*/
#ifndef translator_h
#define translator_h

#include <string>

// Thin wrapper around GNU gettext. Avoids KLocalizedString/QCoreApplication,
// neither of which is available inside a shell-extension DLL hosted by explorer.exe.
namespace Translator
{
    // Idempotent and safe to call from any thread/apartment; the one-time
    // bindtextdomain()/textdomain() setup runs exactly once (std::call_once).
    void init();

    std::wstring translate(const char* msgid);
    std::wstring translateContext(const char* context, const char* msgid);

    // Replaces the first occurrence of %1 (and %2) with the given argument, KDE-i18n style.
    std::wstring arg(const std::wstring& text, const std::wstring& a1);
    std::wstring arg(const std::wstring& text, const std::wstring& a1, const std::wstring& a2);
}

// Wrapper functions for xgettext extraction (without namespace qualification)
inline std::wstring tr(const char* msgid)
{
    return Translator::translate(msgid);
}

inline std::wstring trContext(const char* context, const char* msgid)
{
    return Translator::translateContext(context, msgid);
}

#endif
