/*
 * Copyright (c) 2003-2006, Sergey Zorin. All rights reserved.
 *
 * This software is distributable under the BSD license. See the terms
 * of the BSD license in the LICENSE file provided with this software.
 *
 */
#define _CRT_SECURE_NO_DEPRECATE

#include <assert.h>
#include <stdio.h>
#include <tchar.h>

#include "diff_ext.h"
#include <map>
#include <vector>


#ifdef UNICODE

static void parseString( const std::wstring& s, size_t& i /*pos*/, std::wstring& r /*result*/ )
{
   size_t size = s.size();
   ++i; // Skip initial '"'
   for( ; i<size; ++i )
   {
      if ( s[i]=='"' )
      {
         ++i;
         break;
      }
      else if ( s[i]==L'\\' && i+1<size )
      {
         ++i;
         switch( s[i] ) {
            case L'n':  r+=L'\n'; break;
            case L'r':  r+=L'\r'; break;
            case L'\\': r+=L'\\'; break;
            case L'"':  r+=L'"';  break;
            case L't':  r+=L'\t'; break;
            default:    r+=L'\\'; r+=s[i]; break;
         }
      }
      else
         r+=s[i];
   }
}

static std::map< std::wstring, std::wstring > s_translationMap;
static tstring s_translationFileName;

void readTranslationFile()
{
   s_translationMap.clear();
   FILE* pFile = _tfopen( s_translationFileName.c_str(), TEXT("rb") );
   if ( pFile )
   {
      MESSAGELOG( TEXT( "Reading translations: " ) + s_translationFileName );
      std::vector<char> buffer;
      try {
         if ( fseek(pFile, 0, SEEK_END)==0 )
         {
            size_t length = ftell(pFile); // Get the file length
            buffer.resize(length);
            fseek(pFile, 0, SEEK_SET );
            fread(&buffer[0], 1, length, pFile );
         }
      } 
      catch(...) 
      {
      }
      fclose(pFile);

      if (buffer.size()>0)
      {
         size_t bufferSize = buffer.size();
         int offset = 0;
         if ( buffer[0]=='\xEF' && buffer[1]=='\xBB' && buffer[2]=='\xBF' )
         {
            offset += 3;
            bufferSize -= 3;
         }

         size_t sLength = MultiByteToWideChar(CP_UTF8,0,&buffer[offset], (int)bufferSize, 0, 0 );
         std::wstring s( sLength, L' ' );         
         MultiByteToWideChar(CP_UTF8,0,&buffer[offset], (int)bufferSize, &s[0], (int)s.size() );

         // Now analyse the file and extract translation strings
         std::wstring msgid;
         std::wstring msgstr;
         msgid.reserve( 1000 );
         msgstr.reserve( 1000 );
         bool bExpectingId = true;
         for( size_t i=0; i<sLength; ++i )
         {
            wchar_t c = s[i];
            if( c == L'\n' || c == L'\r' || c==L' ' || c==L'\t' )
               continue;
            else if ( s[i]==L'#' ) // Comment
               while( s[i]!='\n' && s[i]!=L'\r' && i<sLength )
                  ++i;
            else if ( s[i]==L'"' )
            {
               if ( bExpectingId ) parseString(s,i,msgid);
               else                parseString(s,i,msgstr);
            }
            else if ( sLength-i>5 && wcsncmp( &s[i], L"msgid", 5 )==0 )
            {
               if ( !msgid.empty() && !msgstr.empty() )
               {
                  s_translationMap[msgid] = msgstr;
               }
               bExpectingId = true;
               msgid.clear();
               i+=4;
            }
            else if ( sLength-i>6 && wcsncmp( &s[i], L"msgstr", 6 )==0 )
            {
               bExpectingId = false;
               msgstr.clear();
               i+=5;
            }
            else
            {
               // Unexpected ?
            }
         }
      }
   }
   else
   {
      ERRORLOG( TEXT( "Reading translations failed: " ) + s_translationFileName );
   }
}

static tstring getTranslation( const tstring& fallback )
{
   std::map< std::wstring, std::wstring >::iterator i = s_translationMap.find( fallback );
   if (i!=s_translationMap.end())
      return i->second;
   return fallback;
}
#else

static tstring getTranslation( const tstring& fallback )
{
   return fallback;
}

#endif


static void replaceArgs( tstring& s, const tstring& r1, const tstring& r2=TEXT(""), const tstring& r3=TEXT("") )
{
   tstring arg1 = TEXT("%1");
   size_t pos1 = s.find( arg1 );
   tstring arg2 = TEXT("%2");
   size_t pos2 = s.find( arg2 );
   tstring arg3 = TEXT("%3");
   size_t pos3 = s.find( arg3 );
   if ( pos1 != size_t(-1) )
   {
      s.replace( pos1, arg1.length(), r1 );
      if ( pos2 != size_t(-1) && pos1<pos2 )
         pos2 += r1.length() - arg1.length();
      if ( pos3 != size_t(-1) && pos1<pos3 )
         pos3 += r1.length() - arg1.length();
   }
   if ( pos2 != size_t(-1) )
   {
      s.replace( pos2, arg2.length(), r2 );
      if ( pos3 != size_t(-1) && pos2<pos3 )
         pos3 += r2.length() - arg2.length();
   }
   if ( pos3 != size_t(-1) )
   {
      s.replace( pos3, arg3.length(), r3 );
   }
}

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

#ifdef UNICODE
   tstring installDir = SERVER::instance()->getRegistryKeyString( TEXT(""), TEXT("InstallDir") );
   tstring language = SERVER::instance()->getRegistryKeyString( TEXT(""), TEXT("Language") );
   tstring translationFileName = installDir + TEXT("\\translations\\diff_ext_") + language + TEXT(".po");
   if ( s_translationFileName != translationFileName )
   {
      s_translationFileName = translationFileName;
      readTranslationFile();
   }
#endif

  FORMATETC format = {CF_HDROP, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
  STGMEDIUM medium;
  medium.tymed = TYMED_HGLOBAL;
  HRESULT ret = E_INVALIDARG;

  if(data->GetData(&format, &medium) == S_OK) 
  {
    HDROP drop = (HDROP)medium.hGlobal;
    m_nrOfSelectedFiles = DragQueryFile(drop, 0xFFFFFFFF, 0, 0);

    TCHAR tmp[MAX_PATH];
    
    //initialize_language();
    
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
         tstring menuStringCompare = i18n("Compare with %1");
         tstring menuStringMerge   = i18n("Merge with %1");
         tstring firstFileName;
         if( nrOfRecentFiles>=1 )
         {
            tstring firstFileName = TEXT("'") + cut_to_length( m_recentFiles.front() ) + TEXT("'");
         } 
         replaceArgs( menuStringCompare, firstFileName );
         replaceArgs( menuStringMerge,   firstFileName );
         m_id_DiffWith  = insertMenuItemHelper( subMenu, id++, pos2++, menuStringCompare, nrOfRecentFiles >=1 ? MFS_ENABLED : MFS_DISABLED );
         m_id_MergeWith = insertMenuItemHelper( subMenu, id++, pos2++, menuStringMerge, nrOfRecentFiles >=1 ? MFS_ENABLED : MFS_DISABLED );

         //if( nrOfRecentFiles>=2 )
         //{
         //   tstring firstFileName = cut_to_length( m_recentFiles.front() );        
         //   tstring secondFileName = cut_to_length( *(++m_recentFiles.begin()) );        
         //} 
         m_id_Merge3 = insertMenuItemHelper( subMenu, id++, pos2++, i18n("3-way merge with base"), 
            nrOfRecentFiles >=2 ? MFS_ENABLED : MFS_DISABLED );

         menuString = i18n("Save '%1' for later");
         replaceArgs( menuString, _file_name1 );
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

         insertMenuItemHelper( subMenu, id++, pos2++, i18n("Compare with ..."), 
            nrOfRecentFiles > 0 ? MFS_ENABLED : MFS_DISABLED, file_list );

         m_id_ClearList = insertMenuItemHelper( subMenu, id++, pos2++, i18n("Clear list"), nrOfRecentFiles >=1 ? MFS_ENABLED : MFS_DISABLED );
      }
      else if(m_nrOfSelectedFiles == 2) 
      {      
         //= "Diff " + cut_to_length(_file_name1, 20)+" and "+cut_to_length(_file_name2, 20);
         m_id_Diff = insertMenuItemHelper( subMenu, id++, pos2++, i18n("Compare") );
      } 
      else if ( m_nrOfSelectedFiles == 3 ) 
      {      
         m_id_Diff3 = insertMenuItemHelper( subMenu, id++, pos2++, i18n("3 way comparison") );
      } 
      else 
      {
         // More than 3 files selected?
      }
      m_id_About = insertMenuItemHelper( subMenu, id++, pos2++, i18n("About Diff-Ext ...") );

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
      } 
      else if(id == m_id_DiffLater) 
      {
         MESSAGELOG(TEXT("Diff Later: ")+_file_name1);
         m_recentFiles.remove( _file_name1 );
         m_recentFiles.push_front( _file_name1 );
      } 
      else if(id >= m_id_DiffWith_Base && id < m_id_DiffWith_Base+m_recentFiles.size()) 
      {
         LOG();
         diff_with(id-m_id_DiffWith_Base, false);
      } 
      else if(id == m_id_About) 
      {
         LOG();
         MessageBox( _hwnd, (i18n("Diff-Ext Copyright (c) 2003-2006, Sergey Zorin. All rights reserved.\n")
            + i18n("This software is distributable under the BSD license.\n")
            + i18n("Some extensions for KDiff3 by Joachim Eibl.\n")
            + i18n("Homepage for Diff-Ext: http://diff-ext.sourceforge.net\n")
            + i18n("Homepage for KDiff3: http://kdiff3.sourceforge.net")).c_str()
            , i18n("About Diff-Ext for KDiff3").c_str(), MB_OK );
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

   return ret;
}

STDMETHODIMP
DIFF_EXT::GetCommandString(UINT idCmd, UINT uFlags, UINT*, LPSTR pszName, UINT cchMax) 
{
   // LOG(); // Gets called very often
   HRESULT ret = NOERROR;

   if(uFlags == GCS_HELPTEXT) {
      tstring helpString;
      if( idCmd == m_id_Diff ) 
      {
         helpString = i18n("Compare selected files");
      } 
      else if( idCmd == m_id_DiffWith ) 
      {
         if(!m_recentFiles.empty()) 
         {
            helpString = i18n("Compare '%1' with '%2'");
            replaceArgs( helpString, _file_name1, m_recentFiles.front() );
         }
      } 
      else if(idCmd == m_id_DiffLater) 
      {
         helpString = i18n("Save '%1' for later operation");
         replaceArgs( helpString, _file_name1 );
      } 
      else if((idCmd >= m_id_DiffWith_Base) && (idCmd < m_id_DiffWith_Base+m_recentFiles.size())) 
      {
         if( !m_recentFiles.empty() ) 
         {
            unsigned int num = idCmd - m_id_DiffWith_Base;
            std::list<tstring>::iterator i = m_recentFiles.begin();
            for(unsigned int j = 0; j < num && i != m_recentFiles.end(); j++)
               i++;

            if ( i!=m_recentFiles.end() )
            {
               helpString = i18n("Compare '%1' with '%2'");
               replaceArgs( helpString, _file_name1, *i );
            }
         }
      }
      lstrcpyn( (LPTSTR)pszName, helpString.c_str(), cchMax );
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
      tstring message = i18n("Could not start KDiff3. Please rerun KDiff3 installation.");
      message += TEXT("\n") + i18n("Command") + TEXT(": ") + command;
      message += TEXT("\n") + i18n("CommandLine") + TEXT(": ") + commandLine;
      MessageBox(_hwnd, message.c_str(), i18n("Diff-Ext For KDiff3").c_str(), MB_OK);
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
