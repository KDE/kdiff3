/*
  SPDX-FileCopyrightText: 2003-2006, Sergey Zorin. All rights reserved.
  SPDX-FileCopyrightText:  2018-2020 Michael Reeves reeves.87@gmail.com
  SPDX-License-Identifier: BSD-2-Clause
*/

#include "server.h"

#include <stdio.h>
#include <wchar.h>

#include <shlguid.h>
#include <olectl.h>
#include <objidl.h>

#include <objbase.h>
#include <initguid.h>

#include "class_factory.h"
#include "translator.h"

#define DllExport   __declspec( dllexport )

// registry key util struct
struct REGSTRUCT {
    const wchar_t* subkey;
    const wchar_t* name;
    const wchar_t* value;
};

SERVER* SERVER::_instance = nullptr;
static HINSTANCE server_instance; // Handle to this DLL itself.

//DEFINE_GUID(CLSID_DIFF_EXT, 0xA0482097, 0xC69D, 0x4DEC, 0x8A, 0xB6, 0xD3, 0xA2, 0x59, 0xAC, 0xC1, 0x51);
// New class id for DIFF_EXT for KDiff3
#if defined(_WIN64) || defined(_M_X64)
// {34471FFB-4002-438b-8952-E4588D0C0FE9}
DEFINE_GUID( CLSID_DIFF_EXT, 0x34471FFB, 0x4002, 0x438b, 0x89, 0x52, 0xE4, 0x58, 0x8D, 0x0C, 0x0F, 0xE9 );
#else
#error [KDiff3] Unsupported configuration.
#endif

std::wstring SERVER::getRegistryKeyString( const std::wstring& subKey, const std::wstring& value, bool isUserKey /*= true*/ )
{
    std::wstring keyName = m_registryBaseName;
   if (!subKey.empty())
       keyName += L"\\" + subKey;

   HKEY key;
   HKEY baseKey = isUserKey ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
    std::wstring result;
   for(;;)
   {
       if(RegOpenKeyExW(baseKey, keyName.c_str(), 0, KEY_READ | KEY_WOW64_64KEY, &key) == ERROR_SUCCESS)
       {
           DWORD neededSizeInBytes = 0;
           if(RegQueryValueExW(key, value.c_str(), nullptr, nullptr, nullptr, &neededSizeInBytes) == ERROR_SUCCESS)
           {
               DWORD length = neededSizeInBytes / sizeof(wchar_t);
               result.resize(length);
               if(RegQueryValueExW(key, value.c_str(), nullptr, nullptr, reinterpret_cast<LPBYTE>(&result[0]), &neededSizeInBytes) == ERROR_SUCCESS)
               {
                   //Everything is ok, but we want to cut off the terminating 0-character
                   result.resize(length - 1);
                   RegCloseKey(key);
                   return result;
               }
               else
               {
                   result.resize(0);
               }
           }

           RegCloseKey(key);
       }
      if (baseKey==HKEY_LOCAL_MACHINE)
         break;
      baseKey = HKEY_LOCAL_MACHINE;
   }

   // Error
   {
       wchar_t* message;
       FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, nullptr,
                      GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPWSTR>(&message), 0, nullptr);
    ERRORLOG((std::wstring(L"RegOpenKeyEx: " + keyName + L"->" + value) + L": ") + message);
       LocalFree(message);
   }
   return result;
}


STDAPI
DllCanUnloadNow(void) {
  HRESULT ret = S_FALSE;

  if(SERVER::instance()->reference_count() == 0) {
    ret = S_OK;
  }

  return ret;
}

extern "C" int APIENTRY
DllMain(HINSTANCE instance, DWORD reason, LPVOID /* reserved */) {
    //  char str[1024];
    //  char* reason_string[] = {"DLL_PROCESS_DETACH", "DLL_PROCESS_ATTACH", "DLL_THREAD_ATTACH", "DLL_THREAD_DETACH"};
    //  sprintf(str, "instance: %x; reason: '%s'", instance, reason_string[reason]);
    //  MessageBoxW(0, str, L"Info", MB_OK);
    switch(reason)
    {
        case DLL_PROCESS_ATTACH:
            server_instance = instance;
            SERVER::instance()->save_history();
            MESSAGELOG(L"DLL_PROCESS_ATTACH");
            break;

        case DLL_PROCESS_DETACH:
            MESSAGELOG(L"DLL_PROCESS_DETACH");
            SERVER::instance()->save_history();
            break;
    }

  return 1;
}

STDAPI
DllGetClassObject(REFCLSID rclsid, REFIID riid, void** class_object) {
  HRESULT ret = CLASS_E_CLASSNOTAVAILABLE;
  *class_object = nullptr;

  if (IsEqualIID(rclsid, CLSID_DIFF_EXT)) {
    CLASS_FACTORY* pcf = new CLASS_FACTORY();

    ret = pcf->QueryInterface(riid, class_object);
  }

  return ret;
}

/*extern "C" HRESULT STDAPICALLTYPE*/  STDAPI
DllRegisterServer() {
  return SERVER::instance()->do_register();
}

STDAPI
DllUnregisterServer() {
  return SERVER::instance()->do_unregister();
}

SERVER* SERVER::instance()
{
   if(_instance == nullptr)
   {
      _instance = new SERVER();
      _instance->initLogging();
      Translator::init();
      MESSAGELOG(L"New Server instance");
   }

   return _instance;
}

SERVER::SERVER()  : _reference_count(0)
{
    m_registryBaseName = L"Software\\KDE e.V.\\KDiff3\\diff-ext";
    m_pRecentFiles = nullptr;
    m_pLogFile = nullptr;
}

void SERVER::initLogging()
{
    std::wstring logFileName = getRegistryKeyString(L"", L"LogFile");
    if(!logFileName.empty())
    {
        m_pLogFile = _wfopen(logFileName.c_str(), L"a+, ccs=UTF-8");
        if(m_pLogFile)
        {
            fwprintf(m_pLogFile, L"\nSERVER::SERVER()\n");
        }
    }
}

SERVER::~SERVER()
{
   if ( m_pLogFile )
   {
       fwprintf(m_pLogFile, L"SERVER::~SERVER()\n\n");
       fclose(m_pLogFile);
   }

   delete m_pRecentFiles;
}

HINSTANCE
SERVER::handle() const
{
   return server_instance;
}

void
SERVER::lock() {
  InterlockedIncrement(&_reference_count);
}

void
SERVER::release() {
  InterlockedDecrement(&_reference_count);

  //if(InterlockedDecrement((LPLONG)&_reference_count) == 0)
  //   delete this;
}

void SERVER::logMessage( const char* function, const char* file, int line, const std::wstring& msg )
{
   SERVER* pServer = SERVER::instance();
   if ( pServer && pServer->m_pLogFile )
   {
      SYSTEMTIME st;
      GetSystemTime( &st );
      fwprintf(pServer->m_pLogFile, L"%04d/%02d/%02d %02d:%02d:%02d "
                                    L"%hs (%hs:%d) %ls\n",
               st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, function, file, line, msg.c_str());
      fflush(pServer->m_pLogFile);
   }
}

std::list<std::wstring>&
SERVER::recent_files()
{
   LOG();
   if ( m_pRecentFiles==nullptr )
   {
    m_pRecentFiles = new std::list<std::wstring>;
   }
   else
   {
      m_pRecentFiles->clear();
   }
   MESSAGELOG(L"Reading history from registry...");
   for( int i=0; i<32; ++i )  // Max history size
   {
       wchar_t numAsString[10];
       swprintf(numAsString, 10, L"%d", i);
    std::wstring historyItem = getRegistryKeyString(L"history", numAsString);
       if(!historyItem.empty())
           m_pRecentFiles->push_back(historyItem);
   }
   return *m_pRecentFiles;
}

void
SERVER::save_history() const
{
   if( m_pRecentFiles )
   {
      HKEY key;
      if(RegCreateKeyExW(HKEY_CURRENT_USER, (m_registryBaseName + L"\\history").c_str(), 0, nullptr,
                         REG_OPTION_NON_VOLATILE, KEY_WRITE | KEY_WOW64_64KEY, nullptr, &key, nullptr) == ERROR_SUCCESS)
      {
         LOG();
         //DWORD len = MAX_PATH;
         int n = 0;

         std::list<std::wstring>::const_iterator i;

         for(i = m_pRecentFiles->begin(); i!=m_pRecentFiles->end(); ++i, ++n )
         {
            std::wstring str = *i;
            wchar_t numAsString[10];
            swprintf(numAsString, 10, L"%d", n);
            if(RegSetValueExW(key, numAsString, 0, REG_SZ, reinterpret_cast<const BYTE*>(str.c_str()), (DWORD)(str.size() + 1) * sizeof(wchar_t)) != ERROR_SUCCESS)
            {
                wchar_t* message;
                FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, nullptr,
                               GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
                               reinterpret_cast<LPWSTR>(&message), 0, nullptr);
                MessageBoxW(nullptr, message, L"KDiff3-diff-ext: Save history failed", MB_OK | MB_ICONINFORMATION);
                LocalFree(message);
            }
         }
         for(; n<32; ++n )
         {
             wchar_t numAsString[10];
             swprintf(numAsString, 10, L"%d", n);
             RegDeleteValueW(key, numAsString);
         }

         RegCloseKey(key);
      }
      else
      {
          SYSERRORLOG(L"RegOpenKeyEx");
      }
   }
}

HRESULT
SERVER::do_register() {
   LOG();
   wchar_t class_id[MAX_PATH];
   wchar_t* tmp_guid;
   HRESULT ret = SELFREG_E_CLASS;

   if(StringFromIID(CLSID_DIFF_EXT, &tmp_guid) == S_OK)
   {
       wcsncpy(class_id, tmp_guid, MAX_PATH);

       CoTaskMemFree((void*)tmp_guid);

       wchar_t subkey[MAX_PATH];
       wchar_t server_path[MAX_PATH];
       HKEY key;
       LRESULT result = NOERROR;
       DWORD dwDisp;

       GetModuleFileNameW(SERVER::instance()->handle(), server_path, MAX_PATH);

       REGSTRUCT entry[] = {
           {L"Software\\Classes\\CLSID\\%s", nullptr, L"kdiff3ext"},
           {L"Software\\Classes\\CLSID\\%s\\InProcServer32", nullptr, L"%s"},
           {L"Software\\Classes\\CLSID\\%s\\InProcServer32", L"ThreadingModel", L"Apartment"}};

       for(unsigned int i = 0; (i < sizeof(entry) / sizeof(entry[0])) && (result == NOERROR); i++)
       {
           swprintf(subkey, MAX_PATH, entry[i].subkey, class_id);
           result = RegCreateKeyExW(HKEY_CURRENT_USER, subkey, 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &key, &dwDisp);

           if(result == NOERROR)
           {
               wchar_t szData[MAX_PATH];

               swprintf(szData, MAX_PATH, entry[i].value, server_path);
               szData[MAX_PATH - 1] = 0;

               result = RegSetValueExW(key, entry[i].name, 0, REG_SZ, reinterpret_cast<LPBYTE>(szData), DWORD(wcslen(szData) * sizeof(wchar_t)));
           }

           RegCloseKey(key);
       }

       if(result == NOERROR)
       {
           result = RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\*\\shellex\\ContextMenuHandlers\\kdiff3ext", 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &key, &dwDisp);

           if(result == NOERROR)
           {

               result = RegSetValueExW(key, nullptr, 0, REG_SZ, reinterpret_cast<LPBYTE>(class_id), DWORD(wcslen(class_id) * sizeof(wchar_t)));

               RegCloseKey(key);

               // NT needs to have shell extensions "approved".
               result = RegCreateKeyExW(HKEY_CURRENT_USER,
                                        L"Software\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved",
                                        0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &key, &dwDisp);

               if(result == NOERROR)
               {
                   wchar_t szData[MAX_PATH];

                   wcscpy(szData, L"diff-ext");

                   result = RegSetValueExW(key, class_id, 0, REG_SZ, reinterpret_cast<LPBYTE>(szData), DWORD(wcslen(szData) * sizeof(wchar_t)));

                   RegCloseKey(key);

                   ret = S_OK;
               }
               else if(result == ERROR_ACCESS_DENIED)
               {
                   wchar_t msg[] = L"Warning! You have unsufficient rights to write to a specific registry key.\n"
                                   L"The application may work anyway, but it is advised to register this module "
                                   L"again while having administrator rights.";

                   MessageBoxW(nullptr, msg, L"Warning", MB_ICONEXCLAMATION);

                   ret = S_OK;
               }
           }
       }
   }

  return ret;
}

HRESULT
SERVER::do_unregister() {
   LOG();
   wchar_t class_id[MAX_PATH];
   wchar_t* tmp_guid;
   HRESULT ret = SELFREG_E_CLASS;

   if(StringFromIID(CLSID_DIFF_EXT, &tmp_guid) == S_OK)
   {
       wcsncpy(class_id, tmp_guid, MAX_PATH);

       CoTaskMemFree((void*)tmp_guid);

       LRESULT result = NOERROR;
       wchar_t subkey[MAX_PATH];

       REGSTRUCT entry[] = {
           {L"Software\\Classes\\CLSID\\%s\\InProcServer32", nullptr, nullptr},
           {L"Software\\Classes\\CLSID\\%s", nullptr, nullptr}};

       for(unsigned int i = 0; (i < sizeof(entry) / sizeof(entry[0])) && (result == NOERROR); i++)
       {
           swprintf(subkey, MAX_PATH, entry[i].subkey, class_id);
           result = RegDeleteKeyW(HKEY_CURRENT_USER, subkey);
       }

       if(result == NOERROR)
       {
           result = RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\*\\shellex\\ContextMenuHandlers\\kdiff3ext");

           if(result == NOERROR)
           {
               // NT needs to have shell extensions "approved".
               HKEY key;

               RegOpenKeyExW(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved", 0, KEY_ALL_ACCESS, &key);

               result = RegDeleteValueW(key, class_id);

               RegCloseKey(key);

               if(result == ERROR_SUCCESS)
               {
                   ret = S_OK;
               }
           }
       }
   }

  return ret;
}
