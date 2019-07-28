/*
 * Copyright (c) 2003-2005, Sergey Zorin. All rights reserved.
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
#ifndef server_h
#define server_h
#include <list>   // std::list
//#include <log/file_sink.h>
#include <windows.h>
#include <string> // std::wstring

#ifdef UNICODE
typedef std::wstring tstring;
#else
typedef std::string tstring;
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
