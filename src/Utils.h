// clang-format off
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

class Utils
{
  public:
    // QStringConverter::availableCodecs() is too broad for our needs.
    static QStringList availableCodecs();

    static bool isLocalFile(const QUrl& url);
    /*
      QUrl::toLocalFile does some special handling for locally visable windows network drives.
      HOwever if QUrl::isLocal returns false we get an empty string back.
    */
    static QString urlToString(const QUrl& url);
    static bool wildcardMultiMatch(const QString& wildcard, const QString& testString, bool bCaseSensitive);
    static QString getArguments(QString cmd, QString& program, QStringList& args);
    inline static bool isEndOfLine(QChar c) { return c == '\n'; } // interally all line endings are converted to '\n'

    static void calcTokenPos(const QString& s, qint32 posOnScreen, qsizetype& pos1, qsizetype& pos2);
    static QString calcHistoryLead(const QString& s);

  private:
    static bool isCTokenChar(QChar c);
};

#endif
