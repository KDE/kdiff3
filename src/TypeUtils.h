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

#include <QtGlobal>

#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
using QtSizeType = qint32;
#else
using QtSizeType = qsizetype;
#endif
using FileOffset = quint64;
using QtNumberType = qint32;//Qt insists on one type for all but does not create a typedef for it.

using PtrDiffRef = size_t;

#define TYPE_MAX(x) std::numeric_limits<x>::max()
#define TYPE_MIN(x) std::numeric_limits<x>::min()

static_assert(sizeof(int) >= sizeof(qint32), "Legacy LP32 systems/compilers not supported"); // e.g. Windows 16-bit
static_assert(sizeof(FileOffset) >= sizeof(QtSizeType), "Size mis-match this configuration is not supported."); //Assumed in SourceData.

#endif
