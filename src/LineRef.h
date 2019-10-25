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
#ifndef LINEREF_H
#define LINEREF_H

#include "TypeUtils.h"

#include <stdlib.h>
#include <type_traits>

#include <QtGlobal>

class LineRef
{
  public:
    typedef qint32 LineType;
    inline LineRef() = default;
    inline LineRef(const LineType i) { mLineNumber = i; }
    inline LineRef(const qint64 i)
    {
        if(i <= TYPE_MAX(LineType))
            mLineNumber = (LineType)i;
        else
            mLineNumber = -1;
    }
    inline operator LineType() const { return mLineNumber; }
    inline LineRef& operator=(const LineType lineIn) {
        mLineNumber = lineIn;
        return *this;
    }
    inline LineRef& operator+=(const LineType& inLine)
    {
        mLineNumber += inLine;
        return *this;
    };

    LineRef& operator++()
    {
        ++mLineNumber;
        return *this;
    };

    const LineRef operator++(int)
    {
        LineRef line(*this);
        ++mLineNumber;
        return line;
    };

    LineRef& operator--()
    {
        --mLineNumber;
        return *this;
    };

    const LineRef operator--(int)
    {
        LineRef line(*this);
        --mLineNumber;
        return line;
    };
    inline void invalidate() { mLineNumber = -1; }
    inline bool isValid() const { return mLineNumber != -1; }

  private:
    LineType mLineNumber = -1;
};

static_assert(std::is_copy_constructible<LineRef>::value, "LineRef must be copt constuctible.");
static_assert(std::is_copy_assignable<LineRef>::value, "LineRef must copy assignable.");
static_assert(std::is_move_constructible<LineRef>::value, "LineRef must be move constructible.");
static_assert(std::is_move_assignable<LineRef>::value, "LineRef must be move assignable.");
static_assert(std::is_convertible<LineRef, int>::value, "Can not convert LineRef to int.");
static_assert(std::is_convertible<int, LineRef>::value, "Can not convert int to LineRef.");

typedef LineRef::LineType LineCount;
typedef LineRef::LineType LineIndex;

#endif
