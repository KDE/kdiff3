// clang-format off
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
    constexpr LineRef() = default;
    //cppcheck-suppress noExplicitConstructor
    constexpr LineRef(const qint64 i)
    {
        mLineNumber = i;
    }

    operator LineType() const noexcept { return mLineNumber; }

    operator SafeInt<LineType>() const noexcept { return mLineNumber; }

    LineRef& operator=(const LineType lineIn)
    {
        mLineNumber = lineIn;
        return *this;
    }

    LineRef& operator+=(const LineType& inLine)
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
    void invalidate() { mLineNumber = invalid; }
    [[nodiscard]] bool isValid() const { return mLineNumber != invalid; }

  private:
    SafeSignedRange<LineType, invalid> mLineNumber = invalid;
};

/*
    This is here because its easy to unknowingly break these conditions. The resulting
    compiler errors are numerous and may be a bit cryptic if you aren't familiar with the C++ language.
    Also some IDEs with clangd or ccls integration can automatically check static_asserts
    without doing a full compile.
*/
static_assert(std::is_copy_constructible<LineRef>::value, "LineRef must be copy constructible.");
static_assert(std::is_copy_assignable<LineRef>::value, "LineRef must copy assignable.");
static_assert(std::is_move_constructible<LineRef>::value, "LineRef must be move constructible.");
static_assert(std::is_move_assignable<LineRef>::value, "LineRef must be move assignable.");
static_assert(std::is_convertible<LineRef, qint64>::value, "Can not convert LineRef to qint64.");
static_assert(std::is_convertible<qint64, LineRef>::value, "Can not convert qint64 to LineRef.");

using LineType = LineRef::LineType;

//Break in an obvious way if way can't get LineCounts from Qt supplied ints without overflow issues.
static_assert(sizeof(LineType) >= sizeof(qint32)); //Generally assumed by KDiff3
#endif
