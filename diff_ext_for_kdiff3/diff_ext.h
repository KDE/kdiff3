/*
  SPDX-FileCopyrightText: 2003-2006, Sergey Zorin. All rights reserved.
  SPDX-FileCopyrightText:  2018-2020 Michael Reeves reeves.87@gmail.com
  SPDX-License-Identifier: BSD-2-Clause
*/


#ifndef diff_ext_h
#define diff_ext_h

//This option is not compatible with our shell extention.
#undef WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <windowsx.h>
#include <shlobj.h>

#include "server.h"


// this is the actual OLE Shell context menu handler
class DIFF_EXT : public IContextMenu, IShellExtInit {
  public:
    DIFF_EXT();
    virtual ~DIFF_EXT();

    //IUnknown members
    STDMETHODIMP QueryInterface(REFIID interface_id, void** result);
    STDMETHODIMP_(ULONG) AddRef();
    STDMETHODIMP_(ULONG) Release();

    //IShell members
    STDMETHODIMP QueryContextMenu(HMENU menu, UINT index, UINT cmd_first, UINT cmd_last, UINT flags);
    STDMETHODIMP InvokeCommand(LPCMINVOKECOMMANDINFO info);
    STDMETHODIMP GetCommandString(UINT_PTR cmd, UINT flags, UINT* reserved, LPSTR name, UINT name_length);

    //IShellExtInit methods
    STDMETHODIMP Initialize(LPCITEMIDLIST folder, IDataObject* subj, HKEY key);

  private:
    void diff( const tstring& arguments );
    void diff_with(unsigned int num, bool bMerge);
    tstring cut_to_length(const tstring&, size_t length = 64);

  private:
    UINT m_nrOfSelectedFiles;
    tstring _file_name1;
    tstring _file_name2;
    tstring _file_name3;
    HINSTANCE _resource;
    HWND _hwnd;

    ULONG  _ref_count;

    std::list< tstring >& m_recentFiles;
    UINT m_id_FirstCmd;
    UINT m_id_Diff;
    UINT m_id_DiffWith;
    UINT m_id_DiffLater;
    UINT m_id_MergeWith;
    UINT m_id_Merge3;
    UINT m_id_Diff3;
    UINT m_id_DiffWith_Base;
    UINT m_id_About;
    UINT m_id_ClearList;
};

#endif // __diff_ext_h__
