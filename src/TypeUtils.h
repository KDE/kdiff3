/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef TYPEUTILS_H
#define TYPEUTILS_H

#include <limits>
#include <stdlib.h>
#include <type_traits>

#include <QtGlobal>

#define TYPE_MAX(x) std::numeric_limits<x>::max()
#define TYPE_MIN(x) std::numeric_limits<x>::min()

typedef size_t PtrDiffRef;
typedef qint32 QtNumberType;//Qt insists on one type for all but does not create a typedef for it.

static_assert(sizeof(int) >= sizeof(qint32), "Legacy LP32 systems/compilers not supported"); // e.g. Windows 16-bit

#endif
