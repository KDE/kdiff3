/**
 *
 * Copyright (C) 2018 Michael Reeves
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
#ifndef LINEREF_H
#define LINEREF_H

#include <stdlib.h>
#include <type_traits>
#include <QtGlobal>

class LineRef
{
    public:
      typedef qint32 LineType;
      LineRef() = default;
      LineRef(const LineType i) { mLineNumber = i; }
      operator LineType() const { return mLineNumber; }
      inline void operator= (const LineType lineIn) { mLineNumber = lineIn; }
      inline LineRef& operator+=(const LineType& inLine)
      {
          mLineNumber += inLine;
          return *this;
      };

      void invalidate() { mLineNumber = -1; }
      bool isValid() const { return mLineNumber != -1; }

    private:
        LineType mLineNumber = -1;
};

static_assert(std::is_copy_constructible<LineRef>::value, "");
static_assert(std::is_copy_assignable<LineRef>::value, "");
static_assert(std::is_move_constructible<LineRef>::value, "");
static_assert(std::is_move_assignable<LineRef>::value, "");
static_assert(std::is_convertible<LineRef, int>::value, "");
static_assert(std::is_convertible<int, LineRef>::value, "");

typedef LineRef::LineType LineCount;
typedef size_t PtrDiffRef;
typedef LineRef::LineType LineIndex;

#endif
