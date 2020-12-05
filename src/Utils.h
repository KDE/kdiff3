/*
 * KDiff3 - Text Diff And Merge Tool
 * 
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef UTILS_H
#define UTILS_H

#include <QChar>
#include <QFontMetrics>
#include <QString>
#include <QStringList>

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

      static void calcTokenPos(const QString& s, int posOnScreen, int& pos1, int& pos2);
      static QString calcHistoryLead(const QString& s);

    private:
      static bool isCTokenChar(QChar c);
};

#endif
