/**
 * Copyright (C) 2019 Michael Reeves <reeves.87@gmail.com>
 *
 * This file is part of KDiff3.
 *
 * KDiff3 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * KDiff3 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with KDiff3.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef COMMENTPARSER_H
#define COMMENTPARSER_H

#include <QChar>
#include <QString>

class CommentParser
{
  public:
    virtual void processChar(const QString &line, const QChar &inChar) = 0;
    virtual void processLine(const QString &line) = 0;
    virtual bool inComment() const = 0;
    virtual bool isPureComment() const = 0;
    virtual ~CommentParser(){};
};

class DefaultCommentParser : public CommentParser
{
  private:
    typedef enum {none, singleLine, multiLine}CommentType;
    typedef enum {no, yes, unknown}TriState;

  public:
    void processLine(const QString &line) override;

    inline bool inComment() const override { return mCommentType != none; };
    inline bool isPureComment() const override { return mIsPureComment == yes; };
  protected:
    friend class CommentParserTest;

    void processChar(const QString &line, const QChar &inChar) override;
    //For tests only.
    inline bool isEscaped(){ return bIsEscaped; }
    inline bool inString(){ return bInString; }
  private:
    QChar mLastChar, mStartChar;

    bool isFirstLine = false;
    TriState mIsPureComment = unknown;
    bool bInString = false;
    bool bIsEscaped = false;

    CommentType mCommentType = none;
};

#endif // !COMMENTPASER_H
