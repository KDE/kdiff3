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
                    mIsPureComment = line.startsWith(QLatin1String("//"));
                    lastComment.startOffset = offset - 1;
                }
                else if(mLastChar == '*' && mCommentType == multiLine)
                {
                    //ending multi-line comment
                    mCommentType = none;
                    lastComment.endOffset = offset + 1; //include last char in offset
                    comments.push_back(lastComment);
                    if(!isFirstLine)
                        mIsPureComment = line.endsWith(QLatin1String("*/")) ? true : mIsPureComment;
                }
                break;
            case '*':
                if(bInString)
                    break;

                if(mLastChar == '/' && !inComment())
                {
                    mCommentType = multiLine;
                    mIsPureComment = line.startsWith(QLatin1String("/*")) ? true : mIsPureComment;
                    isFirstLine = true;
                    lastComment.startOffset = offset - 1;
                }
                break;
            case '\n':
                if(mCommentType == singleLine)
                {
                    mCommentType = none;
                    lastComment.endOffset = offset;
                    comments.push_back(lastComment);
                }

                if(mCommentType == multiLine && !isFirstLine)
                {
                    mIsPureComment = true;
                }

                if(lastComment.startOffset > 0 && lastComment.endOffset == 0)
                {
                    lastComment.endOffset = offset;
                    comments.push_back(lastComment);
                }

                isFirstLine = false;

                break;
            default:
                if(inComment())
                {
                    break;
                }

                mIsPureComment = false;
                break;
        }

        mLastChar = inChar;
    }
    else
    {
        bIsEscaped = false;
        mLastChar = QChar();
    }

    ++offset;
};

/*
    Find comments if any and set is pure comment flag it it has nothing but whitespace and comments.
*/
void DefaultCommentParser::processLine(const QString &line)
{
    offset = line.indexOf(QRegularExpression("[\\S]", QRegularExpression::UseUnicodePropertiesOption));
    lastComment.startOffset = lastComment.endOffset = 0; //reset these for each line
    comments.clear();

    //remove trailing and ending spaces.
    const QString trimmedLine = line.trimmed();

    for(const QChar &c : trimmedLine)
    {
        processChar(trimmedLine, c);
    }

    processChar(trimmedLine, '\n');
}

/*
 Modifies the input data, and replaces comments with whitespace when the line contains other data too. 
*/
void DefaultCommentParser::removeComment(QString &line)
{
    if(isPureComment() || lastComment.startOffset == lastComment.endOffset) return;

    for(const CommentRange &range : comments)
    {
        /*
            Q_ASSERT isn't useful during auto testing as it causes the QTest not to show the actual line that
            the test failed on.
        */
#ifndef AUTOTEST
        Q_ASSERT(range.endOffset <= line.length() && range.startOffset <= line.length());
        Q_ASSERT(range.endOffset >= range.startOffset);
#endif
        qint32 size = range.endOffset - range.startOffset;
        line.replace(range.startOffset, size, QString(" ").repeated(size));
    }
}
