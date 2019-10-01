/*
 * Copyright (c) 2003-2004, Sergey Zorin. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:

 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */


#ifndef __diff_ext_h__
#define __diff_ext_h__

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
