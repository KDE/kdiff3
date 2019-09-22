/**
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

#ifndef UTILS_H
#define UTILS_H

#include <QFontMetrics>
#include <QStringList>
#include <QString>

class Utils{
    public:
      static bool wildcardMultiMatch(const QString& wildcard, const QString& testString, bool bCaseSensitive);
      static QString getArguments(QString cmd, QString& program, QStringList& args);
      inline static bool isEndOfLine( QChar c ) { return c=='\n' || c=='\r' || c=='\x0b'; }

      //Where posiable use QTextLayout in place of these functions especially when dealing with non-latin scripts.
      inline static int getHorizontalAdvance(const QFontMetrics &metrics, const QString& s, int len = -1)
      {
        //Warning: The Qt API used here is not accurate for some non-latin characters.
        #if QT_VERSION < QT_VERSION_CHECK(5,12,0)
          return metrics.width(s, len);
        #else
          return metrics.horizontalAdvance(s, len);
        #endif
      }

      inline static int getHorizontalAdvance(const QFontMetrics &metrics, const QChar& c)
      {
        //Warning: The Qt API used here is not accurate for some non-latin characters.
        #if QT_VERSION < QT_VERSION_CHECK(5,12,0)
          return metrics.width(c);
        #else
          return metrics.horizontalAdvance(c);
        #endif
      }
};

#endif
