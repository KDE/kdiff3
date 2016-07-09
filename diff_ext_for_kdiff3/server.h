/*
 * Copyright (c) 2003-2005, Sergey Zorin. All rights reserved.
 *
 * This software is distributable under the BSD license. See the terms
 * of the BSD license in the LICENSE file provided with this software.
 *
 */
#ifndef __server_h__
#define __server_h__
#include <list>   // std::list
//#include <log/file_sink.h>
#include <windows.h>

#if 1
#include <string> // std::wstring
#ifdef UNICODE
typedef std::wstring tstring;
#else
typedef std::string tstring;
#endif
#define i18n(x) getTranslation( TEXT(x) )
#else
#include "diffextstring.h"
typedef STRING tstring;
#define i18n(x) TEXT(x) 
#endif

#define MESSAGELOG( msg ) SERVER::logMessage( __FUNCTION__, __FILE__, __LINE__, msg )
#define LOG()             MESSAGELOG( TEXT("") )
#define ERRORLOG( msg )   MESSAGELOG( TEXT("Error: ")+tstring(msg) )
#define SYSERRORLOG( msg )                                                                    \
   {                                                                                          \
      LPTSTR message;                                                                         \
      FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, 0,           \
         GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR) &message, 0, 0); \
      ERRORLOG( (tstring(msg) + TEXT(": ")) + message );                                        \
      LocalFree(message);                                                                     \
   }


class SERVER {
  public:
    static SERVER* instance();
    void initLogging();
  
  public:
    virtual ~SERVER();
    
    tstring getRegistryKeyString( const tstring& subKey, const tstring& value );
  
    HINSTANCE handle() const;
  
    HRESULT do_register();
    HRESULT do_unregister();
  
    void lock();
    void release();
  
    ULONG reference_count() const {
      return _reference_count;
    }
    
    std::list< tstring >& recent_files();
    
    void save_history() const;

    static void logMessage( const char* function, const char* file, int line, const tstring& msg );
  
  private:
    SERVER();
    SERVER(const SERVER&) {}
      
  private:
    LONG _reference_count;
    std::list<tstring>* m_pRecentFiles;
    static SERVER* _instance;
    tstring m_registryBaseName;
    FILE* m_pLogFile;
};

#endif // __server_h__
