/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
// clang-format on

#ifndef UTILS_H
#define UTILS_H

#include "TypeUtils.h"

#include <QChar>
#include <QFontMetrics>
#include <QString>
#include <QStringList>
#include <QUrl>

class Utils{
  public:
      /*
        QUrl::toLocalFile does some special handling for locally visable windows network drives.
        If QUrl::isLocal however it returns false we get an empty string back.
      */
      static QString urlToString(const QUrl &url);
      static bool wildcardMultiMatch(const QString& wildcard, const QString& testString, bool bCaseSensitive);
      static QString getArguments(QString cmd, QString& program, QStringList& args);
      inline static bool isEndOfLine(QChar c) { return c == '\n'; } //interally all line endings are converted to '\n'

      //Where possible use QTextLayout in place of these functions especially when dealing with non-latin scripts.
      inline static qint32 getHorizontalAdvance(const QFontMetrics &metrics, const QString& s, qint32 len = -1)
      {
        //Warning: The Qt API used here is not accurate for some non-latin characters.
        return metrics.horizontalAdvance(s, len);
      }

      inline static qint32 getHorizontalAdvance(const QFontMetrics &metrics, const QChar& c)
      {
        //Warning: The Qt API used here is not accurate for some non-latin characters.
        return metrics.horizontalAdvance(c);
      }

      static void calcTokenPos(const QString& s, qint32 posOnScreen, QtSizeType& pos1, QtSizeType& pos2);
      static QString calcHistoryLead(const QString& s);

    private:
      static bool isCTokenChar(QChar c);
};

#endif
