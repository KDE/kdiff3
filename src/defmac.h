/* clang-format off */
/*****************************************************************************
  The MIT License

  Copyright © 2010 Pavel Karelin (hkarel), <hkarel@yandex.ru>

  Permission is hereby granted, free of charge, to any person obtaining
  a copy of this software and associated documentation files (the
  "Software"), to deal in the Software without restriction, including
  without limitation the rights to use, copy, modify, merge, publish,
  distribute, sublicense, and/or sell copies of the Software, and to
  permit persons to whom the Software is furnished to do so, subject to
  the following conditions:

  The above copyright notice and this permission notice shall be included
  in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
  CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
  TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
  SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
  ---

  This header is defined macros of general purpose.
  
*****************************************************************************/

#pragma once

#define DISABLE_DEFAULT_CONSTRUCT( ClassName ) \
    ClassName () = delete;                     \
    ClassName ( ClassName && ) = delete;       \
    ClassName ( const ClassName & ) = delete;

#define DISABLE_DEFAULT_COPY( ClassName )      \
    ClassName ( ClassName && ) = delete;       \
    ClassName ( const ClassName & ) = delete;  \
    ClassName & operator = ( ClassName && ) = delete; \
    ClassName & operator = ( const ClassName & ) = delete;

#define DISABLE_DEFAULT_FUNC( ClassName )      \
    ClassName () = delete;                     \
    ClassName ( ClassName && ) = delete;       \
    ClassName ( const ClassName & ) = delete;  \
    ClassName & operator = ( ClassName && ) = delete; \
    ClassName & operator = ( const ClassName & ) = delete;


#ifndef NDEBUG
#define QCONNECT_ASSERT(COND_) Q_ASSERT(COND_)
#else
#define QCONNECT_ASSERT(COND_) COND_
#endif

/**
  The chk_connect macro is used to check result returned by the function
  QObject::connect() in debug mode,  it looks like on assert() function.
  However, in the release mode, unlike assert() function, test expression
  is not removed.
*/
#define chk_connect(SOURCE_, SIGNAL_, DEST_, SLOT_, CONNECT_TYPE_) \
    QCONNECT_ASSERT(QObject::connect(SOURCE_, SIGNAL_, DEST_, SLOT_, CONNECT_TYPE_));

#define chk_connect_a(SOURCE_, SIGNAL_, DEST_, SLOT_) \
    QCONNECT_ASSERT(QObject::connect(SOURCE_, SIGNAL_, DEST_, SLOT_, \
    Qt::ConnectionType(Qt::AutoConnection | Qt::UniqueConnection)));

#define chk_connect_d(SOURCE_, SIGNAL_, DEST_, SLOT_) \
    QCONNECT_ASSERT(QObject::connect(SOURCE_, SIGNAL_, DEST_, SLOT_, \
    Qt::ConnectionType(Qt::DirectConnection | Qt::UniqueConnection)));

#define chk_connect_q(SOURCE_, SIGNAL_, DEST_, SLOT_) \
    QCONNECT_ASSERT(QObject::connect(SOURCE_, SIGNAL_, DEST_, SLOT_, \
    Qt::ConnectionType(Qt::QueuedConnection | Qt::UniqueConnection)));

#define chk_connect_bq(SOURCE_, SIGNAL_, DEST_, SLOT_) \
    QCONNECT_ASSERT(QObject::connect(SOURCE_, SIGNAL_, DEST_, SLOT_, \
    Qt::ConnectionType(Qt::BlockingQueuedConnection | Qt::UniqueConnection)));

#if defined(__MINGW32__) || defined(__MINGW64__)
#define MINGW
#endif
