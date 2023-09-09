/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
// clang-format on
#include "Utils.h"

#include "fileaccess.h"
#include "TypeUtils.h"

#include <KLocalizedString>

#include <QString>
#include <QStringList>
#include <QHash>
#include <QRegularExpression>

/* Split the command line into arguments.
 * Normally split at white space separators except when quoting with " or '.
 * Backslash is treated as meta.
 * Detect parsing errors like unclosed quotes.
 * The first item in the list will be the command itself.
 * Returns the error reason as string or an empty string on success.
 * Eg. >"1" "2"<           => >1<, >2<
 * Eg. >'\'\\'<            => >'\<   backslash is a meta character
 * Eg. > "\\" <            => >\<
 * Eg. >"c:\sed" 's/a/\' /g'<  => >c:\sed<, >s/a/' /g<
 */
QString Utils::getArguments(QString cmd, QString& program, QStringList& args)
{
    program = QString();
    args.clear();
    for(qint32 i = 0; i < cmd.length(); ++i)
    {
        while(i < cmd.length() && cmd[i].isSpace())
        {
            ++i;
        }
        if(cmd[i] == '"' || cmd[i] == '\'') // argument beginning with a quote
        {
            QChar quoteChar = cmd[i];
            ++i;
            qint32 argStart = i;
            bool bSkip = false;
            while(i < cmd.length() && (cmd[i] != quoteChar || bSkip))
            {
                if(bSkip)
                {
                    bSkip = false;
                    //Don't emulate bash here we are not talking to it.
                    //For us all quotes are the same.
                    if(cmd[i] == '\\' || cmd[i] == '\'' || cmd[i] == '"')
                    {
                        cmd.remove(i - 1, 1); // remove the backslash '\'
                        continue;
                    }
                }
                else if(cmd[i] == '\\')
                    bSkip = true;
                ++i;
            }
            if(i < cmd.length())
            {
                args << cmd.mid(argStart, i - argStart);
                if(i + 1 < cmd.length() && !cmd[i + 1].isSpace())
                    return i18n("Expecting space after closing quote.");
            }
            else
                return i18n("Unmatched quote.");
            continue;
        }
        else
        {
            qint32 argStart = i;
            while(i < cmd.length() && (!cmd[i].isSpace() /*|| bSkip*/))
            {
                if(cmd[i] == '"' || cmd[i] == '\'')
                    return i18n("Unexpected quote character within argument.");
                ++i;
            }
            args << cmd.mid(argStart, i - argStart);
        }
    }
    if(args.isEmpty())
        return i18n("No program specified.");
    else
    {
        program = args[0];
        args.pop_front();
    }
    return QString();
}

bool Utils::wildcardMultiMatch(const QString& wildcard, const QString& testString, bool bCaseSensitive)
{
    static QHash<QString, QRegularExpression> s_patternMap;

    const QStringList regExpList = wildcard.split(QChar(';'));

    for(const QString& regExp : regExpList)
    {
        QHash<QString, QRegularExpression>::iterator patIt = s_patternMap.find(regExp);
        if(patIt == s_patternMap.end())
        {
            QRegularExpression pattern(QRegularExpression::wildcardToRegularExpression(regExp), bCaseSensitive ? QRegularExpression::NoPatternOption : QRegularExpression::CaseInsensitiveOption);
            patIt = s_patternMap.insert(regExp, pattern);
        }

        if(patIt.value().match(testString).hasMatch())
            return true;
    }

    return false;
}

//TODO: Only used by calcTokenPos.
bool Utils::isCTokenChar(QChar c)
{
    return (c == '_') ||
           (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9');
}


//TODO: Needed? Only user of isCTokenChar.
/// Calculate where a token starts and ends, given the x-position on screen.
void Utils::calcTokenPos(const QString& s, qint32 posOnScreen, QtSizeType& pos1, QtSizeType& pos2)
{
    QtSizeType pos = std::max(0, posOnScreen);
    if(pos >= s.length())
    {
        pos1 = s.length();
        pos2 = s.length();
        return;
    }

    pos1 = pos;
    pos2 = pos + 1;

    if(isCTokenChar(s[pos1]))
    {
        while(pos1 >= 0 && isCTokenChar(s[pos1]))
            --pos1;
        ++pos1;

        while(pos2 < s.length() && isCTokenChar(s[pos2]))
            ++pos2;
    }
}

QString Utils::calcHistoryLead(const QString& s)
{
    static const QRegularExpression nonWhitespace("\\S"), whitespace("\\s");

    // Return the start of the line until the first white char after the first non white char.
    qint32 i = s.indexOf(nonWhitespace);
    if(i == -1)
        return QString("");

    i = s.indexOf(whitespace, i);
    if(Q_UNLIKELY(i == -1))
        return s;// Very unlikely

    return s.left(i);
}

/*
    QUrl::toLocalFile does some special handling for locally visable windows network drives.
    If QUrl::isLocal however it returns false and we get an empty string back.
*/
QString Utils::urlToString(const QUrl &url)
{
    if(!FileAccess::isLocal(url))
        return url.toString();

    QString result = url.toLocalFile();
    if(result.isEmpty())
        return url.path();

    return result;
}

