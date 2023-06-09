/*
  SPDX-FileCopyrightText: 2003-2006, Sergey Zorin. All rights reserved.
  SPDX-FileCopyrightText:  2018-2020 Michael Reeves reeves.87@gmail.com
  SPDX-License-Identifier: BSD-2-Clause
*/

#include "diff_ext.h"

#include <KLocalizedString>

#include <QString>

#include <assert.h>
#include <stdio.h>
#include <tchar.h>

#include <map>
#include <vector>

DIFF_EXT::DIFF_EXT()
: m_nrOfSelectedFiles(0), _ref_count(0L),
  m_recentFiles( SERVER::instance()->recent_files() )
{
   LOG();
  _resource = SERVER::instance()->handle();

  SERVER::instance()->lock();
}

DIFF_EXT::~DIFF_EXT()
{
   LOG();
   if(_resource != SERVER::instance()->handle()) {
      FreeLibrary(_resource);
   }

   SERVER::instance()->release();
}

STDMETHODIMP
DIFF_EXT::QueryInterface(REFIID refiid, void** ppv)
{
  HRESULT ret = E_NOINTERFACE;
  *ppv = 0;

  if(IsEqualIID(refiid, IID_IShellExtInit) || IsEqualIID(refiid, IID_IUnknown)) {
    *ppv = static_cast<IShellExtInit*>(this);
  } else if (IsEqualIID(refiid, IID_IContextMenu)) {
    *ppv = static_cast<IContextMenu*>(this);
  }

  if(*ppv != 0) {
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

  if(InterlockedDecrement((LPLONG)&_ref_count) != 0) {
    ret = _ref_count;
  } else {
    delete this;
  }

  return ret;
}



STDMETHODIMP
DIFF_EXT::Initialize(LPCITEMIDLIST /*folder not used*/, IDataObject* data, HKEY /*key not used*/)
{
   LOG();

  FORMATETC format = {CF_HDROP, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
  STGMEDIUM medium;
  medium.tymed = TYMED_HGLOBAL;
  HRESULT ret = E_INVALIDARG;

  if(data->GetData(&format, &medium) == S_OK)
  {
    HDROP drop = (HDROP)medium.hGlobal;
    m_nrOfSelectedFiles = DragQueryFile(drop, 0xFFFFFFFF, 0, 0);

    TCHAR tmp[MAX_PATH];

    if (m_nrOfSelectedFiles >= 1 && m_nrOfSelectedFiles <= 3)
    {
       DragQueryFile(drop, 0, tmp, MAX_PATH);
       _file_name1 = tmp;

       if(m_nrOfSelectedFiles >= 2)
       {
         DragQueryFile(drop, 1, tmp, MAX_PATH);
         _file_name2 = tmp;
       }

       if( m_nrOfSelectedFiles == 3)
       {
         DragQueryFile(drop, 2, tmp, MAX_PATH);
         _file_name3 = tmp;
       }

       ret = S_OK;
    }
  }
  else
  {
     SYSERRORLOG(TEXT("GetData"));
  }

  return ret;
}

static int insertMenuItemHelper( HMENU menu, UINT id, UINT position, const tstring& text,
                                 UINT fState = MFS_ENABLED, HMENU hSubMenu=0 )
{
   MENUITEMINFO item_info;
   ZeroMemory(&item_info, sizeof(item_info));
   item_info.cbSize = sizeof(MENUITEMINFO);
   item_info.wID = id;
   if (text.empty())
   {   // Separator
       item_info.fMask = MIIM_TYPE;
       item_info.fType = MFT_SEPARATOR;
       item_info.dwTypeData = 0;
   }
   else
   {
      item_info.fMask = MIIM_ID | MIIM_TYPE | MIIM_STATE | (hSubMenu!=0 ? MIIM_SUBMENU : 0);
      item_info.fType = MFT_STRING;
      item_info.fState = fState;
      item_info.dwTypeData = (LPTSTR)text.c_str();
      item_info.hSubMenu = hSubMenu;
   }
   if ( 0 == InsertMenuItem(menu, position, TRUE, &item_info) )
      SYSERRORLOG(TEXT("InsertMenuItem"));
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

      insertMenuItemHelper( menu, id++, position++, TEXT("") );  // begin separator

      tstring menuString;
      UINT pos2=0;
      if(m_nrOfSelectedFiles == 1)
      {
         size_t nrOfRecentFiles = m_recentFiles.size();
         tstring menuStringCompare;
         tstring menuStringMerge;
         tstring firstFileName;
         if( nrOfRecentFiles>=1 )
         {
            firstFileName = TEXT("'") + cut_to_length( m_recentFiles.front() ) + TEXT("'");
         }

         menuStringCompare = fromQString(i18nc("Contexualmenu option", "Compare with %1", toQString(firstFileName)));
         menuStringMerge = fromQString(i18nc("Contexualmenu option", "Merge with %1", toQString(firstFileName)));

         m_id_DiffWith  = insertMenuItemHelper( subMenu, id++, pos2++, menuStringCompare, nrOfRecentFiles >=1 ? MFS_ENABLED : MFS_DISABLED );
         m_id_MergeWith = insertMenuItemHelper( subMenu, id++, pos2++, menuStringMerge, nrOfRecentFiles >=1 ? MFS_ENABLED : MFS_DISABLED );

         //if( nrOfRecentFiles>=2 )
         //{
         //   tstring firstFileName = cut_to_length( m_recentFiles.front() );
         //   tstring secondFileName = cut_to_length( *(++m_recentFiles.begin()) );
         //}
         m_id_Merge3 = insertMenuItemHelper(subMenu, id++, pos2++, fromQString(i18nc("Contexualmenu option", "3-way merge with base")),
                                            nrOfRecentFiles >= 2 ? MFS_ENABLED : MFS_DISABLED);

         menuString = fromQString(i18nc("Contexualmenu option", "Save '%1' for later", toQString(_file_name1)));
         m_id_DiffLater = insertMenuItemHelper( subMenu, id++, pos2++, menuString );

         HMENU file_list = CreateMenu();
         std::list<tstring>::iterator i;
         m_id_DiffWith_Base = id;
         int n = 0;
         for( i = m_recentFiles.begin(); i!=m_recentFiles.end(); ++i )
         {
            tstring s = cut_to_length( *i );
            insertMenuItemHelper( file_list, id++, n, s );
            ++n;
         }

         insertMenuItemHelper(subMenu, id++, pos2++, fromQString(i18nc("Contexualmenu option", "Compare with ...")),
                              nrOfRecentFiles > 0 ? MFS_ENABLED : MFS_DISABLED, file_list);

         m_id_ClearList = insertMenuItemHelper(subMenu, id++, pos2++, fromQString(i18nc("Contexualmenu option", "Clear list")), nrOfRecentFiles >= 1 ? MFS_ENABLED : MFS_DISABLED);
      }
      else if(m_nrOfSelectedFiles == 2)
      {
         //= "Diff " + cut_to_length(_file_name1, 20)+" and "+cut_to_length(_file_name2, 20);
         m_id_Diff = insertMenuItemHelper(subMenu, id++, pos2++, fromQString(i18nc("Contexualmenu option", "Compare")));
      }
      else if ( m_nrOfSelectedFiles == 3 )
      {
         m_id_Diff3 = insertMenuItemHelper(subMenu, id++, pos2++, fromQString(i18nc("Contexualmenu option", "3 way comparison")));
      }
      else
      {
         // More than 3 files selected?
      }
      m_id_About = insertMenuItemHelper(subMenu, id++, pos2++, fromQString(i18nc("Contexualmenu option", "About Diff-Ext ...")));

      insertMenuItemHelper( menu, id++, position++, TEXT("KDiff3"), MFS_ENABLED, subMenu );

      insertMenuItemHelper( menu, id++, position++, TEXT("") );  // final separator

      ret = MAKE_HRESULT(SEVERITY_SUCCESS, FACILITY_NULL, id-first_cmd);
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
         diff( TEXT("\"") + _file_name1 + TEXT("\" \"") + _file_name2 + TEXT("\"") );
      }
      else if(id == m_id_Diff3)
      {
         LOG();
         diff( TEXT("\"") + _file_name1 + TEXT("\" \"") + _file_name2 + TEXT("\" \"") + _file_name3 + TEXT("\"") );
      }
      else if(id == m_id_Merge3)
      {
         LOG();
         std::list< tstring >::iterator iFrom = m_recentFiles.begin();
         std::list< tstring >::iterator iBase = iFrom;
         ++iBase;
         diff( TEXT("-m \"") + *iBase + TEXT("\" \"") + *iFrom + TEXT("\" \"") + _file_name1 + TEXT("\"") );
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
         MESSAGELOG(TEXT("Diff Later: ")+_file_name1);
         m_recentFiles.remove( _file_name1 );
         m_recentFiles.push_front( _file_name1 );
         SERVER::instance()->save_history();
      }
      else if(id >= m_id_DiffWith_Base && id < m_id_DiffWith_Base+m_recentFiles.size())
      {
         LOG();
         diff_with(id-m_id_DiffWith_Base, false);
      }
      else if(id == m_id_About)
      {
         LOG();

         MessageBox( _hwnd, (fromQString(i18n("Diff-Ext Copyright (c) 2003-2006, Sergey Zorin. All rights reserved.\n")
            + i18n("This software is distributable under the BSD-2-Clause license.\n")
            + i18n("Some extensions for KDiff3 (c) 2006-2013 by Joachim Eibl.\n")
            + i18n("Ported to Qt5/Kf5 by Michael Reeves\n")
            + i18n("Homepage for Diff-Ext: http://diff-ext.sourceforge.net\n"))).c_str()
            , fromQString(i18n("About Diff-Ext for KDiff3 (64 Bit)")).c_str(), MB_OK );
      }
      else
      {
         ret = E_INVALIDARG;
         TCHAR verb[80];
         _sntprintf(verb, 79, TEXT("Command id: %d"), LOWORD(ici->lpVerb));
         verb[79]=0;
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
   // LOG(); // Gets called very often
   HRESULT ret = NOERROR;

   if(uFlags == GCS_HELPTEXT) {
      QString helpString;
      if( idCmd == m_id_Diff )
      {
         helpString = i18nc("Contexualmenu option", "Compare selected files");
      }
      else if( idCmd == m_id_DiffWith )
      {
         if(!m_recentFiles.empty())
         {
            helpString = i18nc("Contexualmenu option", "Compare '%1' with '%2'", toQString(_file_name1), toQString(m_recentFiles.front()));
         }
      }
      else if(idCmd == m_id_DiffLater)
      {
         helpString = i18nc("Contexualmenu option", "Save '%1' for later operation", toQString(_file_name1));
      }
      else if((idCmd >= m_id_DiffWith_Base) && (idCmd < m_id_DiffWith_Base+m_recentFiles.size()))
      {
         if( !m_recentFiles.empty() )
         {
            unsigned long long num = idCmd - m_id_DiffWith_Base;
            std::list<tstring>::iterator i = m_recentFiles.begin();
            for(unsigned long long j = 0; j < num && i != m_recentFiles.end(); j++)
               i++;

            if ( i!=m_recentFiles.end() )
            {
               helpString = i18nc("Contexualmenu option", "Compare '%1' with '%2'", toQString(_file_name1), toQString(*i));
            }
         }
      }
      lstrcpyn( (LPTSTR)pszName, fromQString(helpString).c_str(), cchMax );
   }
   else
   {
      ret = E_INVALIDARG;
   }

   return ret;
}

void
DIFF_EXT::diff( const tstring& arguments )
{
   LOG();
   STARTUPINFO si;
   PROCESS_INFORMATION pi;
   bool bError = true;
   tstring command = SERVER::instance()->getRegistryKeyString( TEXT(""), TEXT("diffcommand") );
   tstring commandLine = TEXT("\"") + command + TEXT("\" ") + arguments;
   if ( ! command.empty() )
   {
      ZeroMemory(&si, sizeof(si));
      si.cb = sizeof(si);
      if (CreateProcess(command.c_str(), (LPTSTR)commandLine.c_str(), 0, 0, FALSE, 0, 0, 0, &si, &pi) == 0)
      {
         SYSERRORLOG(TEXT("CreateProcess") + command);
      }
      else
      {
         bError = false;
         CloseHandle( pi.hProcess );
         CloseHandle( pi.hThread );
      }
   }

   if (bError)
   {
      tstring message = fromQString(i18n("Could not start KDiff3. Please rerun KDiff3 installation."));
      message += TEXT("\n") + fromQString(i18n("Command")) + TEXT(": ") + command;
      message += TEXT("\n") + fromQString(i18n("CommandLine")) + TEXT(": ") + commandLine;
      MessageBox(_hwnd, message.c_str(), fromQString(i18n("Diff-Ext For KDiff3")).c_str(), MB_OK);
   }
}

void
DIFF_EXT::diff_with(unsigned int num, bool bMerge)
{
   LOG();
   std::list<tstring>::iterator i = m_recentFiles.begin();
   for(unsigned int j = 0; j < num && i!=m_recentFiles.end(); j++) {
      i++;
   }

   if ( i!=m_recentFiles.end() )
      _file_name2 = *i;

   diff( (bMerge ? TEXT("-m \"") : TEXT("\"") ) + _file_name2 + TEXT("\" \"") + _file_name1 + TEXT("\"") );
}


tstring
DIFF_EXT::cut_to_length(const tstring& in, size_t max_len)
{
  tstring ret;
  if( in.length() > max_len)
  {
     ret = in.substr(0, (max_len-3)/2);
     ret += TEXT("...");
     ret += in.substr( in.length()-(max_len-3)/2 );
  }
  else
  {
     ret = in;
  }

  return ret;
}
