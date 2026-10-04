/*
  SPDX-FileCopyrightText: 2003-2006, Sergey Zorin. All rights reserved.
  SPDX-FileCopyrightText:  2018-2020 Michael Reeves reeves.87@gmail.com
  SPDX-License-Identifier: BSD-2-Clause
*/

#include "diff_ext.h"

#include "translator.h"

#include <assert.h>
#include <stdio.h>
#include <wchar.h>

#include <map>
#include <vector>

DIFF_EXT::DIFF_EXT():
    m_nrOfSelectedFiles(0), _ref_count(0L),
    m_recentFiles(SERVER::instance()->recent_files())
{
    LOG();
    _resource = SERVER::instance()->handle();

    SERVER::instance()->lock();
}

DIFF_EXT::~DIFF_EXT()
{
    LOG();
    if(_resource != SERVER::instance()->handle())
    {
        FreeLibrary(_resource);
    }

    SERVER::instance()->release();
}

STDMETHODIMP
DIFF_EXT::QueryInterface(REFIID refiid, void** ppv)
{
    HRESULT ret = E_NOINTERFACE;
    *ppv = nullptr;

    if(IsEqualIID(refiid, IID_IShellExtInit) || IsEqualIID(refiid, IID_IUnknown))
    {
        *ppv = static_cast<IShellExtInit*>(this);
    }
    else if(IsEqualIID(refiid, IID_IContextMenu))
    {
        *ppv = static_cast<IContextMenu*>(this);
    }

    if(*ppv != nullptr)
    {
        AddRef();

        ret = NOERROR;
    }

    return ret;
}

STDMETHODIMP_(ULONG)
DIFF_EXT::AddRef()
{
    return InterlockedIncrement((LPLONG)&_ref_count);
}

STDMETHODIMP_(ULONG)
DIFF_EXT::Release()
{
    ULONG ret = 0L;

    if(InterlockedDecrement((LPLONG)&_ref_count) != 0)
    {
        ret = _ref_count;
    }
    else
    {
        delete this;
    }

    return ret;
}

STDMETHODIMP
DIFF_EXT::Initialize(LPCITEMIDLIST /*folder not used*/, IDataObject* data, HKEY /*key not used*/)
{
    LOG();

    FORMATETC format = {CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    STGMEDIUM medium;
    medium.tymed = TYMED_HGLOBAL;
    HRESULT ret = E_INVALIDARG;

    if(data->GetData(&format, &medium) == S_OK)
    {
        HDROP drop = (HDROP)medium.hGlobal;
        m_nrOfSelectedFiles = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);

        wchar_t tmp[MAX_PATH];

        if(m_nrOfSelectedFiles >= 1 && m_nrOfSelectedFiles <= 3)
        {
            DragQueryFileW(drop, 0, tmp, MAX_PATH);
            _file_name1 = tmp;

            if(m_nrOfSelectedFiles >= 2)
            {
                DragQueryFileW(drop, 1, tmp, MAX_PATH);
                _file_name2 = tmp;
            }

            if(m_nrOfSelectedFiles == 3)
            {
                DragQueryFileW(drop, 2, tmp, MAX_PATH);
                _file_name3 = tmp;
            }

            ret = S_OK;
        }
    }
    else
    {
        SYSERRORLOG(L"GetData");
    }

    return ret;
}

static int insertMenuItemHelper(HMENU menu, UINT id, UINT position, const std::wstring& text,
                                UINT fState = MFS_ENABLED, HMENU hSubMenu = nullptr)
{
    MENUITEMINFOW item_info;
    ZeroMemory(&item_info, sizeof(item_info));
    item_info.cbSize = sizeof(MENUITEMINFOW);
    item_info.wID = id;
    if(text.empty())
    { // Separator
        item_info.fMask = MIIM_TYPE;
        item_info.fType = MFT_SEPARATOR;
        item_info.dwTypeData = nullptr;
    }
    else
    {
        item_info.fMask = MIIM_ID | MIIM_TYPE | MIIM_STATE | (hSubMenu != nullptr ? MIIM_SUBMENU : 0);
        item_info.fType = MFT_STRING;
        item_info.fState = fState;
        item_info.dwTypeData = const_cast<wchar_t*>(text.c_str());
        item_info.hSubMenu = hSubMenu;
    }
    if(0 == InsertMenuItemW(menu, position, TRUE, &item_info))
        SYSERRORLOG(L"InsertMenuItem");
    return id;
}

STDMETHODIMP
DIFF_EXT::QueryContextMenu(HMENU menu, UINT position, UINT first_cmd, UINT /*last_cmd not used*/, UINT flags)
{
    LOG();

    SERVER::instance()->recent_files(); // updates recent files list (reads from registry)

    m_id_Diff = UINT(-1);
    m_id_DiffWith = UINT(-1);
    m_id_DiffLater = UINT(-1);
    m_id_MergeWith = UINT(-1);
    m_id_Merge3 = UINT(-1);
    m_id_Diff3 = UINT(-1);
    m_id_DiffWith_Base = UINT(-1);
    m_id_ClearList = UINT(-1);
    m_id_About = UINT(-1);

    HRESULT ret = MAKE_HRESULT(SEVERITY_SUCCESS, FACILITY_NULL, 0);

    if(!(flags & CMF_DEFAULTONLY))
    {
        /* Menu structure:
           KDiff3 -> (1 File selected):  Save 'selection' for later comparison (push onto history stack)
                                         Compare 'selection' with first file on history stack.
                                         Compare 'selection' with -> choice from history stack
                                         Merge 'selection' with first file on history stack.
                                         Merge 'selection' with last two files on history stack.
                     (2 Files selected): Compare 's1' with 's2'
                                         Merge 's1' with 's2'
                     (3 Files selected): Compare 's1', 's2' and 's3'
        */
        HMENU subMenu = CreateMenu();

        UINT id = first_cmd;
        m_id_FirstCmd = first_cmd;

        insertMenuItemHelper(menu, id++, position++, L""); // begin separator

        std::wstring menuString;
        UINT pos2 = 0;
        if(m_nrOfSelectedFiles == 1)
        {
            size_t nrOfRecentFiles = m_recentFiles.size();
            std::wstring menuStringCompare;
            std::wstring menuStringMerge;
            std::wstring firstFileName;
            if(nrOfRecentFiles >= 1)
            {
                firstFileName = L"'" + cut_to_length(m_recentFiles.front()) + L"'";
            }

            menuStringCompare = Translator::arg(Translator::translateContext("Contexualmenu option", "Compare with %1"), firstFileName);
            menuStringMerge = Translator::arg(Translator::translateContext("Contexualmenu option", "Merge with %1"), firstFileName);

            m_id_DiffWith = insertMenuItemHelper(subMenu, id++, pos2++, menuStringCompare, nrOfRecentFiles >= 1 ? MFS_ENABLED : MFS_DISABLED);
            m_id_MergeWith = insertMenuItemHelper(subMenu, id++, pos2++, menuStringMerge, nrOfRecentFiles >= 1 ? MFS_ENABLED : MFS_DISABLED);

            m_id_Merge3 = insertMenuItemHelper(subMenu, id++, pos2++, Translator::translateContext("Contexualmenu option", "3-way merge with base"),
                                               nrOfRecentFiles >= 2 ? MFS_ENABLED : MFS_DISABLED);

            menuString = Translator::arg(Translator::translateContext("Contexualmenu option", "Save '%1' for later"), _file_name1);
            m_id_DiffLater = insertMenuItemHelper(subMenu, id++, pos2++, menuString);

            HMENU file_list = CreateMenu();
            std::list<std::wstring>::iterator i;
            m_id_DiffWith_Base = id;
            int n = 0;
            for(i = m_recentFiles.begin(); i != m_recentFiles.end(); ++i)
            {
                std::wstring s = cut_to_length(*i);
                insertMenuItemHelper(file_list, id++, n, s);
                ++n;
            }

            insertMenuItemHelper(subMenu, id++, pos2++, Translator::translateContext("Contexualmenu option", "Compare with ..."),
                                 nrOfRecentFiles > 0 ? MFS_ENABLED : MFS_DISABLED, file_list);

            m_id_ClearList = insertMenuItemHelper(subMenu, id++, pos2++, Translator::translateContext("Contexualmenu option", "Clear list"), nrOfRecentFiles >= 1 ? MFS_ENABLED : MFS_DISABLED);
        }
        else if(m_nrOfSelectedFiles == 2)
        {
            //= "Diff " + cut_to_length(_file_name1, 20)+" and "+cut_to_length(_file_name2, 20);
            m_id_Diff = insertMenuItemHelper(subMenu, id++, pos2++, Translator::translateContext("Contexualmenu option", "Compare"));
        }
        else if(m_nrOfSelectedFiles == 3)
        {
            m_id_Diff3 = insertMenuItemHelper(subMenu, id++, pos2++, Translator::translateContext("Contexualmenu option", "3 way comparison"));
        }
        else
        {
            // More than 3 files selected?
        }
        m_id_About = insertMenuItemHelper(subMenu, id++, pos2++, Translator::translateContext("Contexualmenu option", "About Diff-Ext ..."));

        insertMenuItemHelper(menu, id++, position++, L"KDiff3", MFS_ENABLED, subMenu);

        insertMenuItemHelper(menu, id++, position++, L""); // final separator

        ret = MAKE_HRESULT(SEVERITY_SUCCESS, FACILITY_NULL, id - first_cmd);
    }

    return ret;
}

STDMETHODIMP
DIFF_EXT::InvokeCommand(LPCMINVOKECOMMANDINFO ici)
{
    HRESULT ret = NOERROR;

    _hwnd = ici->hwnd;

    if(HIWORD(ici->lpVerb) == 0)
    {
        UINT id = m_id_FirstCmd + LOWORD(ici->lpVerb);
        if(id == m_id_Diff)
        {
            LOG();
            diff(L"\"" + _file_name1 + L"\" \"" + _file_name2 + L"\"");
        }
        else if(id == m_id_Diff3)
        {
            LOG();
            diff(L"\"" + _file_name1 + L"\" \"" + _file_name2 + L"\" \"" + _file_name3 + L"\"");
        }
        else if(id == m_id_Merge3)
        {
            LOG();
            std::list<std::wstring>::iterator iFrom = m_recentFiles.begin();
            std::list<std::wstring>::iterator iBase = iFrom;
            ++iBase;
            diff(L"-m \"" + *iBase + L"\" \"" + *iFrom + L"\" \"" + _file_name1 + L"\"");
        }
        else if(id == m_id_DiffWith)
        {
            LOG();
            diff_with(0, false);
        }
        else if(id == m_id_MergeWith)
        {
            LOG();
            diff_with(0, true);
        }
        else if(id == m_id_ClearList)
        {
            LOG();
            m_recentFiles.clear();
            SERVER::instance()->save_history();
        }
        else if(id == m_id_DiffLater)
        {
            MESSAGELOG(L"Diff Later: " + _file_name1);
            m_recentFiles.remove(_file_name1);
            m_recentFiles.push_front(_file_name1);
            SERVER::instance()->save_history();
        }
        else if(id >= m_id_DiffWith_Base && id < m_id_DiffWith_Base + m_recentFiles.size())
        {
            LOG();
            diff_with(id - m_id_DiffWith_Base, false);
        }
        else if(id == m_id_About)
        {
            LOG();

            std::wstring aboutText = Translator::translate(u8"Diff-Ext Copyright \u00A92003-2006, Sergey Zorin. All rights reserved.\n") +
                                Translator::translate("This software is distributable under the BSD-2-Clause license.\n") +
                                Translator::translate(u8"Some extensions for KDiff3 \u00A92006-2013 by Joachim Eibl.\n") +
                                Translator::translate("Homepage for Diff-Ext: http://diff-ext.sourceforge.net\n");
            MessageBoxW(_hwnd, aboutText.c_str(), Translator::translate("About Diff-Ext for KDiff3 (64 Bit)").c_str(), MB_OK);
        }
        else
        {
            ret = E_INVALIDARG;
            wchar_t verb[80];
            swprintf(verb, 80, L"Command id: %d", LOWORD(ici->lpVerb));
            verb[79] = 0;
            ERRORLOG(verb);
        }
    }
    else
    {
        ret = E_INVALIDARG;
    }

    return ret;
}

STDMETHODIMP
DIFF_EXT::GetCommandString(UINT_PTR idCmd, UINT uFlags, UINT*, LPSTR pszName, UINT cchMax)
{
    HRESULT ret = NOERROR;

    if(uFlags == GCS_HELPTEXTW)
    {
        std::wstring helpString;
        if(idCmd == m_id_Diff)
        {
            helpString = Translator::translateContext("Contexualmenu option", "Compare selected files");
        }
        else if(idCmd == m_id_DiffWith)
        {
            if(!m_recentFiles.empty())
            {
                helpString = Translator::arg(Translator::translateContext("Contexualmenu option", "Compare '%1' with '%2'"), _file_name1, m_recentFiles.front());
            }
        }
        else if(idCmd == m_id_DiffLater)
        {
            helpString = Translator::arg(Translator::translateContext("Contexualmenu option", "Save '%1' for later operation"), _file_name1);
        }
        else if((idCmd >= m_id_DiffWith_Base) && (idCmd < m_id_DiffWith_Base + m_recentFiles.size()))
        {
            if(!m_recentFiles.empty())
            {
                unsigned long long num = idCmd - m_id_DiffWith_Base;
                std::list<std::wstring>::iterator i = m_recentFiles.begin();
                for(unsigned long long j = 0; j < num && i != m_recentFiles.end(); j++)
                    i++;

                if(i != m_recentFiles.end())
                {
                    helpString = Translator::arg(Translator::translateContext("Contexualmenu option", "Compare '%1' with '%2'"), _file_name1, *i);
                }
            }
        }
        if(cchMax > 0)
        {
            wchar_t* wideName = reinterpret_cast<wchar_t*>(pszName);
            wcsncpy(wideName, helpString.c_str(), cchMax - 1);
            wideName[cchMax - 1] = L'\0';
        }
    }
    else
    {
        ret = E_INVALIDARG;
    }

    return ret;
}

void DIFF_EXT::diff(const std::wstring& arguments)
{
    LOG();
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    bool bError = true;
    std::wstring command = SERVER::instance()->getRegistryKeyString(L"", L"diffcommand", true); //look in user registry first so it can be overridden
    if(command.empty()) command = SERVER::instance()->getRegistryKeyString(L"", L"diffcommand", false);
    std::wstring commandLine = L"\"" + command + L"\" " + arguments;
    if(!command.empty())
    {
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        if(CreateProcessW(command.c_str(), const_cast<wchar_t*>(commandLine.c_str()), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi) == 0)
        {
            SYSERRORLOG(L"CreateProcess" + command);
        }
        else
        {
            bError = false;
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
    }

    if(bError)
    {
        std::wstring message = Translator::translate("Could not start KDiff3. Please rerun KDiff3 installation.");
        message += L"\n" + Translator::translate("Command") + L": " + command;
        message += L"\n" + Translator::translate("CommandLine") + L": " + commandLine;
        MessageBoxW(_hwnd, message.c_str(), Translator::translate("Diff-Ext For KDiff3").c_str(), MB_OK);
    }
}

void DIFF_EXT::diff_with(unsigned int num, bool bMerge)
{
    LOG();
    std::list<std::wstring>::iterator i = m_recentFiles.begin();
    for(unsigned int j = 0; j < num && i != m_recentFiles.end(); j++)
    {
        i++;
    }

    if(i != m_recentFiles.end())
        _file_name2 = *i;

    diff((bMerge ? L"-m \"" : L"\"") + _file_name2 + L"\" \"" + _file_name1 + L"\"");
}

std::wstring
DIFF_EXT::cut_to_length(const std::wstring& in, size_t max_len)
{
    std::wstring ret;
    if(in.length() > max_len)
    {
        ret = in.substr(0, (max_len - 3) / 2);
        ret += L"...";
        ret += in.substr(in.length() - (max_len - 3) / 2);
    }
    else
    {
        ret = in;
    }

    return ret;
}
