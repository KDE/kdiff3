// clang-format off
/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
// clang-format on

#ifndef COMMON_H
#define COMMON_H

#include <algorithm>
#include <map>

#include <QAtomicInt>
#include <QString>
#include <QStringList>

class QFont;
class QColor;
class QSize;
class QPoint;
class QTextStream;

class ValueMap
{
  private:
    std::map<QString, QString> m_map;

  public:
    ValueMap();
    virtual ~ValueMap();

    void save(QTextStream& ts);
    void load(QTextStream& ts);
    QString getAsString();

    virtual void writeEntry(const QString&, const QFont&);
    virtual void writeEntry(const QString&, const QColor&);
    virtual void writeEntry(const QString&, const QSize&);
    virtual void writeEntry(const QString&, const QPoint&);
    virtual void writeEntry(const QString&, qint32);
    virtual void writeEntry(const QString&, bool);
    virtual void writeEntry(const QString&, const QStringList&);
    virtual void writeEntry(const QString&, const QString&);
    virtual void writeEntry(const QString&, const char*);

    QString     readEntry(const QString& s, const QString& defaultVal);
    QString     readEntry(const QString& s, const char* defaultVal);
    QFont       readEntry(const QString& s, const QFont& defaultVal);
    QColor      readEntry(const QString& s, const QColor defaultVal);
    QSize       readEntry(const QString& s, const QSize defaultVal);
    QPoint      readEntry(const QString& s, const QPoint defaultVal);
    bool        readEntry(const QString& s, bool bDefault);
    qint32         readEntry(const QString& s, qint32 iDefault);
    QStringList readEntry(const QString& s, const QStringList& defaultVal);

  private:
    virtual QFont       readFontEntry(const QString&, const QFont* defaultVal);
    virtual QColor      readColorEntry(const QString&, const QColor* defaultVal);
    virtual QSize       readSizeEntry(const QString&, const QSize* defaultVal);
    virtual QPoint      readPointEntry(const QString&, const QPoint* defaultVal);
    virtual bool        readBoolEntry(const QString&, bool bDefault);
    virtual qint32         readNumEntry(const QString&, qint32 iDefault);
    virtual QStringList readListEntry(const QString&, const QStringList& defaultVal);
    virtual QString     readStringEntry(const QString&, const QString&);
};

QStringList safeStringSplit(const QString& s, char sepChar = ';', char metaChar = '\\');
QString safeStringJoin(const QStringList& sl, char sepChar = ';', char metaChar = '\\');

#endif
