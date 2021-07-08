/* clang-format off */
/*****************************************************************************

  SPDX-FileCopyrightText: Copyright © 2010 Pavel Karelin (hkarel), <hkarel@yandex.ru>
  SPDX-License-Identifier: MIT
  ---

  This header is defined macros of general purpose.

*****************************************************************************/


#ifndef DEFMAC_H
#define DEFMAC_H

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
#define QCONNECT_ASSERT(COND_) assert(COND_)
#else
#define QCONNECT_ASSERT(COND_) COND_
#endif

/**
  The chk_connect macro is used to check result returned by the function
  QObject::connect() in debug mode,  it looks like on assert() function.
  However, in the release mode, unlike assert() function, test expression
  is not removed.
*/

#define chk_connect(SOURCE_, SIGNAL_, DEST_, SLOT_) \
    QCONNECT_ASSERT(QObject::connect(SOURCE_, SIGNAL_, DEST_, SLOT_));

#define chk_connect_unique(SOURCE_, SIGNAL_, DEST_, SLOT_, CONNECT_TYPE_) \
    QCONNECT_ASSERT(QObject::connect(SOURCE_, SIGNAL_, DEST_, SLOT_, CONNECT_TYPE_ | Qt::UniqueConnection));


#define chk_connect_custom(SOURCE_, SIGNAL_, DEST_, SLOT_, CONNECT_TYPE_) \
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

#endif /* DEFMAC_H */
