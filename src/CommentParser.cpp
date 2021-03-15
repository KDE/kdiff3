/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2019-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
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
                    mIsPureComment = mIsCommentOrWhite = line.startsWith(QLatin1String("//"));
                    lastComment.startOffset = offset - 1;
                    if(lastComment.startOffset != 0) //whitespace at begining
                    {
                        mIsPureComment = false;
                    }
                }
                else if(mLastChar == '*' && mCommentType == multiLine)
                {
                    //ending multi-line comment
                    mCommentType = none;
                    lastComment.endOffset = offset + 1; //include last char in offset
                    comments.push_back(lastComment);
                    if(!isFirstLine)
                        mIsPureComment = mIsCommentOrWhite = line.endsWith(QLatin1String("*/")) ? true : mIsCommentOrWhite;
                }
                break;
            case '*':
                if(bInString)
                    break;

                if(mLastChar == '/' && !inComment())
                {
                    mCommentType = multiLine;
                    mIsPureComment = mIsCommentOrWhite = line.startsWith(QLatin1String("/*")) ? true : mIsCommentOrWhite;
                    isFirstLine = true;
                    lastComment.startOffset = offset - 1;
                    if(lastComment.startOffset != 0) //whitespace at begining
                    {
                        mIsPureComment = false;
                    }
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
                    mIsPureComment = mIsCommentOrWhite = true;
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

                mIsPureComment = mIsCommentOrWhite = false;
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
    Find comments if any and set its pure comment flag if it has nothing but whitespace and comments.
*/
void DefaultCommentParser::processLine(const QString &line)
{
    offset = line.indexOf(QRegularExpression("[\\S]", QRegularExpression::UseUnicodePropertiesOption));
    const QtNumberType  trailIndex = line.lastIndexOf(QRegularExpression("\\s+$", QRegularExpression::UseUnicodePropertiesOption));
    lastComment.startOffset = lastComment.endOffset = 0; //reset these for each line
    comments.clear();

    //remove trailing and ending spaces.
    const QString trimmedLine = line.trimmed();

    for(const QChar &c : trimmedLine)
    {
        processChar(trimmedLine, c);
    }
    
    /*
        Line has trailing space after multi-line comment ended.
    */
    if(trailIndex != -1 && !inComment())
    {
        mIsPureComment = false;
    }

    processChar(trimmedLine, '\n');
}

/*
 Modifies the input data, and replaces comments with whitespace when the line contains other data too. 
*/
void DefaultCommentParser::removeComment(QString &line)
{
    if(isSkipable() || lastComment.startOffset == lastComment.endOffset) return;

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
