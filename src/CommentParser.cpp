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

#include "CommentParser.h"

#include <QChar>
#include <QRegularExpression>
#include <QSharedPointer>
#include <QString>

void DefaultCommentParser::processChar(const QString &line, const QChar &inChar)
{
    if(!bIsEscaped)
    {
        switch(inChar.unicode())
        {
            case '\\':
                if(bInString)
                    bIsEscaped = true;
                break;
            case '\'':
            case '"':
                if(!inComment())
                {
                    if(!bInString)
                    {
                        bInString = true;
                        mStartChar = inChar;
                    }
                    else if(mStartChar == inChar)
                    {
                        bInString = false;
                    }
                }
                break;
            case '/':
                if(bInString)
                    break;

                if(!inComment() && mLastChar == '/')
                {
                    mCommentType = singleLine;
                    mIsPureComment = line.startsWith("//") ? yes : no;
                }
                else if(mLastChar == '*' && mCommentType == multiLine)
                {
                    //ending multi-line comment
                    mCommentType = none;
                    if(!isFirstLine)
                        mIsPureComment = line.endsWith("*/") ? yes : mIsPureComment;
                }
                break;
            case '*':
                if(bInString)
                    break;

                if(mLastChar == '/' && !inComment())
                {
                    mCommentType = multiLine;
                    mIsPureComment = line.startsWith("/*") ? yes : mIsPureComment;
                    isFirstLine = true;
                }
                break;
            case '\n':
                if(mCommentType == singleLine)
                {
                    mCommentType = none;
                }

                if(mCommentType == multiLine && !isFirstLine)
                {
                    mIsPureComment = yes;
                }
                isFirstLine = false;

                break;
            default:
                if(inComment())
                {
                    break;
                }

                mIsPureComment = no;
                break;
        }

        mLastChar = inChar;
    }
    else
    {
        bIsEscaped = false;
        mLastChar = QChar();
    }
};

void DefaultCommentParser::processLine(const QString &line)
{
    for(const QChar &c : line)
    {
        processChar(line, c);
    }

    processChar(line, '\n');
}
