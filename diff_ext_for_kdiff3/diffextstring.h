/*
 * Copyright (c) 2003, Sergey Zorin. All rights reserved.
 *
 * This software is distributable under the BSD license. See the terms of the
 * BSD license in the LICENSE file provided with this software.
 *
 */

#ifndef __string_h__
#define __string_h__

#include <windows.h>
#include <tchar.h>

#include <string.h>
#include <stdlib.h>

class STRING;
inline STRING operator+( const STRING& s1, const STRING& s2);

class STRING {
  public:
    static const int begin = 0;
    static const int end = -1;
  
  public:
    STRING(const STRING& s) {
      _str = new TCHAR[s.length()+1];
      lstrcpy(_str, s);
    }
    
    STRING(const TCHAR* str = TEXT("")) {
      _str = new TCHAR[lstrlen(str)+1];
      lstrcpy(_str, str);
    }
    
    ~STRING() {
      delete[] _str;
    }

    void resize( size_t newLength )
    {
       size_t oldLength = length();
       if ( newLength < oldLength ) {
          _str[newLength] = 0; // Just truncate the string
       } else if( newLength>oldLength) {
          TCHAR* p = new TCHAR[ newLength + 1 ];
          lstrcpy(p, _str);
          for( size_t i=oldLength; i<newLength; ++i)
             p[i]=TEXT(' ');
          p[newLength]=0;
       }
    }
    
    STRING& operator=(const STRING& s) {
      delete[] _str;
      _str = new TCHAR[s.length()+1];
      lstrcpy(_str, s);
      return *this;
    }
    
    operator TCHAR*() {
      return _str;
    }
    
    operator const TCHAR*() const {
      return _str;
    }

    const TCHAR* c_str() const {
       return _str;
    }
    
    size_t length() const {
      return _tcslen(_str);
    }
    
    // Also returns the length. Behaviour like std::basic_string::size.
    // See also sizeInBytes() below.
    size_t size() const {
      return length();
    }

    // String length in bytes. May differ from length() for Unicode or MBCS
    size_t sizeInBytes() const {
      return length()*sizeof(TCHAR);
    }

    bool empty() const
    {
       return length()==0;
    }
    
    STRING substr(size_t from, size_t len=size_t(-1)) const {
      STRING tmp;
      size_t to = len==size_t(-1) ? length() : from + len;
            
      if(from < to && (to <= length())) {
        size_t new_len = to - from + 1;
        TCHAR* new_str = new TCHAR[new_len+1];
        lstrcpyn(new_str, &_str[from], int(new_len) );
        new_str[new_len] = 0;
        
        tmp = new_str;
        delete[] new_str;
      }
      
      return tmp;
    }

    STRING& replace( size_t pos, size_t num, const STRING& s )
    {
       *this = substr( 0, pos ) + s + substr( pos+num );
       return *this;
    }
    
    bool operator ==(const STRING& s) const {
      return (lstrcmp(_str, s) == 0);
    }

    size_t find(const STRING& s) const
    {
       const TCHAR* p = _tcsstr( _str, s._str );
       if (p)
          return p - _str;
       else
          return size_t(-1);
    }
    
    STRING& operator +=(const STRING& s) {
      TCHAR* str = new TCHAR[lstrlen(_str)+s.length()+1];

      lstrcpy(str, _str);
      lstrcat(str, s);
      
      delete[] _str;
      
      _str = str;
      
      return *this;
    }

  private:
    TCHAR* _str;
};

inline STRING operator+( const STRING& s1, const STRING& s2) {
  STRING tmp(s1);
  
  tmp+=s2;
  
  return tmp;
}



#endif // __string_h__
