// clang-format off
/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
// clang-format on
#ifndef TYPEUTILS_H
#define TYPEUTILS_H

#include <QtGlobal>

#include <stdlib.h>
#include <type_traits>
#include <limits>

#include <boost/safe_numerics/automatic.hpp>
#include <boost/safe_numerics/safe_integer.hpp>

#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
using QtSizeType = qint32;
#else
using QtSizeType = qsizetype;
#endif
using FileOffset = quint64;
using QtNumberType = qint32;//Qt insists on one type for all but does not create a typedef for it.

using PtrDiffRef = size_t;

using KDiff3_exception_policy = boost::safe_numerics::exception_policy<
    boost::safe_numerics::throw_exception, // arithmetic error
    boost::safe_numerics::trap_exception,  // implementation defined behavior
    boost::safe_numerics::trap_exception,  // undefined behavior
    boost::safe_numerics::trap_exception   // uninitialized value
>;

template <typename T>
using SafeInt = boost::safe_numerics::safe<T, boost::safe_numerics::automatic, KDiff3_exception_policy>;

template <typename T>
using limits = std::numeric_limits<T>;

static_assert(sizeof(int) >= sizeof(qint32), "Legacy LP32 systems/compilers not supported"); // e.g. Windows 16-bit
static_assert(sizeof(FileOffset) >= sizeof(QtSizeType), "Size mis-match this configuration is not supported."); //Assumed in SourceData.

#endif
