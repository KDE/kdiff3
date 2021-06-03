/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef TYPEUTILS_H
#define TYPEUTILS_H

#include <stdlib.h>
#include <type_traits>
#include <limits>

/*
    MSVC is not compatiable with boost::safe_numerics it creates duplicate symbols as this is specfic to MSCV blacklist it
    I don't have time to deal with wierd link-time wonkyness.
*/
#ifndef Q_CC_MSVC
#include <boost/safe_numerics/safe_integer.hpp>
#endif

#include <QtGlobal>

#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
using QtSizeType = qint32;
#else
using QtSizeType = qsizetype;
#endif
using FileOffset = quint64;
using QtNumberType = qint32;//Qt insists on one type for all but does not create a typedef for it.

using PtrDiffRef = size_t;
// using SafeInt32 = boost::safe_numerics::safe<int>;

#ifndef Q_CC_MSVC
template<typename T> using SafeInt32 = boost::safe_numerics::safe<T, boost::safe_numerics::native>;
#else
template<typename T> using SafeInt32 = T;
#endif

#define TYPE_MAX(x) std::numeric_limits<x>::max()
#define TYPE_MIN(x) std::numeric_limits<x>::min()

static_assert(sizeof(int) >= sizeof(qint32), "Legacy LP32 systems/compilers not supported"); // e.g. Windows 16-bit
static_assert(sizeof(FileOffset) >= sizeof(QtSizeType), "Size mis-match this configuration is not supported."); //Assumed in SourceData.

#endif
