/**
 *
 * Copyright (C) 2018 Michael Reeves <reeves.87@gmail.com>
 *
 * This file is part of Kdiff3.
 *
 * Kdiff3 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * Kdiff3 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Kdiff3.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#ifndef TYPEUTILS_H
#define TYPEUTILS_H

#include <stdlib.h>
#include <type_traits>

#include <QtGlobal>

#define TYPE_MAX(x) std::numeric_limits<x>::max()
#define TYPE_MIN(x) std::numeric_limits<x>::min()

typedef size_t PtrDiffRef;
typedef qint32 QtNumberType;//Qt insists on one type for all but does not create a typedef for it.

static_assert(sizeof(int) >= sizeof(qint32), "Legacy LP32 systems/compilers not supported"); // e.g. Windows 16-bit

#endif
