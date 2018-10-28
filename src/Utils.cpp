/**
 * Copyright (C) 2003-2007 by Joachim Eibl <joachim.eibl at gmx.de>
 * Copyright (C) 2018 Michael Reeves
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
#ifdef Q_OS_WIN
#include <qt_windows.h>

#include <QCoreApplication>
#endif

#include <QString>
#include <QStringList>

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
                    if(cmd[i] == '\\' || cmd[i] == quoteChar)
                    {
                        cmd.remove(i - 1, 1); // remove the backslash '\'
                        continue;
                    }
                }
                else if(cmd[i] == '\\' && quoteChar == '\'')
                    bSkip = true;
                ++i;
            }
            if(i < cmd.length())
            {
                args << cmd.mid(argStart, i - argStart);
                if(i + 1 < cmd.length() && !cmd[i + 1].isSpace())
                    return i18n("Expecting space after closing apostroph.");
            }
            else
                return i18n("Not matching apostrophs.");
            continue;
        }
        else
        {
            int argStart = i;
            //bool bSkip = false;
            while(i < cmd.length() && (!cmd[i].isSpace() /*|| bSkip*/))
            {
                /*if ( bSkip )
            {
               bSkip = false;
               if ( cmd[i]=='\\' || cmd[i]=='"' || cmd[i]=='\'' || cmd[i].isSpace() )
               {
                  cmd.remove( i-1, 1 );  // remove the backslash '\'
                  continue;
               }
            }
            else if ( cmd[i]=='\\' )
               bSkip = true;
            else */
                if(cmd[i] == '"' || cmd[i] == '\'')
                    return i18n("Unexpected apostroph within argument.");
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
#ifdef Q_OS_WIN
        if(program == QLatin1String("sed"))
        {
            QString prg = QCoreApplication::applicationDirPath() + QLatin1String("/bin/sed.exe"); // in subdir bin
            if(QFile::exists(prg))
            {
                program = prg;
            }
            else
            {
                prg = QCoreApplication::applicationDirPath() + QLatin1String("/sed.exe"); // in same dir
                if(QFile::exists(prg))
                {
                    program = prg;
                }
            }
        }
#endif
    }
    return QString();
}