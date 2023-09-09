/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2019-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
// clang-format on

#include "CommentParser.h"
#include "TypeUtils.h"

#include <algorithm>

#include <QChar>
#include <QRegularExpression>
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
                    mIsPureComment = mIsCommentOrWhite = line.startsWith(u8"//");
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
                        mIsPureComment = mIsCommentOrWhite = line.endsWith(u8"*/") ? true : mIsCommentOrWhite;
                }
                break;
            case '*':
                if(bInString)
                    break;

                if(mLastChar == '/' && !inComment())
                {
                    mCommentType = multiLine;
                    mIsPureComment = mIsCommentOrWhite = line.startsWith(u8"/*") ? true : mIsCommentOrWhite;
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
    static const QRegularExpression nonWhiteRegexp = QRegularExpression("[\\S]", QRegularExpression::UseUnicodePropertiesOption);
    static const QRegularExpression tailRegexp = QRegularExpression("\\s+$", QRegularExpression::UseUnicodePropertiesOption);
    offset = line.indexOf(nonWhiteRegexp);
    const qint32 trailIndex = line.lastIndexOf(tailRegexp);

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

    //mIsPureComment = mIsPureComment && offset == 0;
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
            assert isn't useful during auto testing as it causes the QTest not to show the actual line that
            the test failed on.
        */
#ifndef AUTOTEST
        assert(range.endOffset <= line.length() && range.startOffset <= line.length());
        assert(range.endOffset >= range.startOffset);
#else
        if(range.endOffset > line.length() && range.startOffset > line.length()) return;
        if(range.endOffset < range.startOffset) return;
#endif
        QtSizeType size = range.endOffset - range.startOffset;
        line.replace(range.startOffset, size, QString(" ").repeated(size));
    }
}
