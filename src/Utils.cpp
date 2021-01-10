/**
 * Copyright (C) 2003-2007 by Joachim Eibl <joachim.eibl at gmx.de>
 * Copyright (C) 2018 Michael Reeves reeves.87@gmail.com
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
 * 
 */
#include "Utils.h"

#include "fileaccess.h"

#include <QString>
#include <QStringList>
#include <QHash>
#include <QRegExp>
#include <KLocalizedString>

/* Split the command line into arguments.
 * Normally split at white space separators except when quoting with " or '.
 * Backslash is treated as meta character within single quotes ' only.
 * Detect parsing errors like unclosed quotes.
 * The first item in the list will be the command itself.
 * Returns the error reasor as string or an empty string on success.
 * Eg. >"1" "2"<           => >1<, >2<
 * Eg. >'\'\\'<            => >'\<   backslash is a meta character between single quotes
 * Eg. > "\\" <            => >\\<   but not between double quotes
 * Eg. >"c:\sed" 's/a/\' /g'<  => >c:\sed<, >s/a/' /g<
 */
QString Utils::getArguments(QString cmd, QString& program, QStringList& args)
{
    program = QString();
    args.clear();
    for(int i = 0; i < cmd.length(); ++i)
    {
        while(i < cmd.length() && cmd[i].isSpace())
        {
            ++i;
        }
        if(cmd[i] == '"' || cmd[i] == '\'') // argument beginning with a quote
        {
            QChar quoteChar = cmd[i];
            ++i;
            int argStart = i;
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
            int argStart = i;
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
    static QHash<QString, QRegExp> s_patternMap;

    QStringList sl = wildcard.split(QChar(';'));

    for(QStringList::Iterator it = sl.begin(); it != sl.end(); ++it)
    {
        QHash<QString, QRegExp>::iterator patIt = s_patternMap.find(*it);
        if(patIt == s_patternMap.end())
        {
            QRegExp pattern(*it, bCaseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive, QRegExp::Wildcard);
            patIt = s_patternMap.insert(*it, pattern);
        }

        if(patIt.value().exactMatch(testString))
            return true;
    }

    return false;
}

/*
    QUrl::toLocalFile does some special handling for locally visable windows network drives.
    If QUrl::isLocal however it returns false we get an empty string back.
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
