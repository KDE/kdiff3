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
#include <boost/safe_numerics/safe_integer_range.hpp>

using FileOffset = quint64;
using PtrDiffRef = size_t;

template <typename T>
using limits = std::numeric_limits<T>;

using KDiff3_exception_policy = boost::safe_numerics::exception_policy<
    boost::safe_numerics::throw_exception, // arithmetic error
    boost::safe_numerics::trap_exception,  // implementation defined behavior
    boost::safe_numerics::trap_exception,  // undefined behavior
    boost::safe_numerics::trap_exception   // uninitialized value
>;

template <typename T, T MIN = limits<T>::min(), T MAX = limits<T>::max()>
using SafeSignedRange =
    boost::safe_numerics::safe_signed_range<MIN, MAX>;

template <typename T>
using SafeInt = boost::safe_numerics::safe<T, boost::safe_numerics::automatic, KDiff3_exception_policy>;

constexpr static qint32 maxNofRecentFiles = 10;
constexpr static qint32 maxNofRecentCodecs = 5;

static_assert(sizeof(FileOffset) >= sizeof(qsizetype), "Size mis-match this configuration is not supported."); //Assumed in SourceData.

#endif
