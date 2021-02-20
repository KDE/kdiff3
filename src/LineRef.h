/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
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
    constexpr inline LineRef() noexcept = default;
    constexpr inline LineRef(const LineType i) noexcept { mLineNumber = i; }
    inline LineRef(const qint64 i) noexcept
    {
        if(i <= TYPE_MAX(LineType))
            mLineNumber = (LineType)i;
        else
            mLineNumber = -1;
    }
    inline operator LineType() const noexcept { return mLineNumber; }
    inline LineRef& operator=(const LineType lineIn) noexcept
    {
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
    inline void invalidate() noexcept { mLineNumber = -1; }
    [[nodiscard]] inline bool isValid() const noexcept { return mLineNumber != -1; }

  private:
    LineType mLineNumber = -1;
};

/*
    This is here because its easy to unknowingly break these conditions. The resulting
    compiler errors are numerous and may be a bit cryptic if you aren't famialar with the C++ language.
    Also some IDEs with clangd or ccls integration can automatically check static_asserts
    without doing a full compile.
*/
static_assert(std::is_copy_constructible<LineRef>::value, "LineRef must be copy constuctible.");
static_assert(std::is_copy_assignable<LineRef>::value, "LineRef must copy assignable.");
static_assert(std::is_move_constructible<LineRef>::value, "LineRef must be move constructible.");
static_assert(std::is_move_assignable<LineRef>::value, "LineRef must be move assignable.");
static_assert(std::is_convertible<LineRef, int>::value, "Can not convert LineRef to int.");
static_assert(std::is_convertible<int, LineRef>::value, "Can not convert int to LineRef.");

typedef LineRef::LineType LineCount;
typedef LineRef::LineType LineIndex;

#endif
