/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
// clang-format on
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

    static constexpr LineType invalid = -1;
    constexpr inline LineRef() = default;
    //cppcheck-suppress noExplicitConstructor
    constexpr inline LineRef(const LineType i) { mLineNumber = i; }
    //cppcheck-suppress noExplicitConstructor
    inline LineRef(const qint64 i)
    {
        if(i <= TYPE_MAX(LineType) && i >= 0)
            mLineNumber = (LineType)i;
        else
            mLineNumber = invalid;
    }

    inline operator LineType() const noexcept { return mLineNumber; }

    inline LineRef& operator=(const LineType lineIn) noexcept
    {
        mLineNumber = lineIn;
        return *this;
    }

    inline LineRef& operator+=(const LineType& inLine) noexcept
    {
        mLineNumber += inLine;
        return *this;
    };

    LineRef& operator++() noexcept
    {
        ++mLineNumber;
        return *this;
    };

    const LineRef operator++(qint32) noexcept
    {
        LineRef line(*this);
        ++mLineNumber;
        return line;
    };

    LineRef& operator--() noexcept
    {
        --mLineNumber;
        return *this;
    };

    const LineRef operator--(qint32) noexcept
    {
        LineRef line(*this);
        --mLineNumber;
        return line;
    };
    inline void invalidate() noexcept { mLineNumber = invalid; }
    [[nodiscard]] inline bool isValid() const noexcept { return mLineNumber != invalid; }

  private:
    SafeInt<LineType> mLineNumber = invalid;
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
static_assert(std::is_convertible<LineRef, qint32>::value, "Can not convert LineRef to qint32.");
static_assert(std::is_convertible<qint32, LineRef>::value, "Can not convert qint32 to LineRef.");

typedef LineRef::LineType LineCount;
typedef LineRef::LineType LineIndex;

//Break in an obvious way if way cann't get LineCounts from Qt supplied ints without overflow issues.
static_assert(sizeof(LineCount) >= sizeof(qint32)); //Generally assumed by KDiff3
#endif
