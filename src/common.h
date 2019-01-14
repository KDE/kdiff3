/***************************************************************************
                          common.h  -  Things that are needed often
                             -------------------
    begin                : Mon Mar 18 2002
    copyright            : (C) 2002-2007 by Joachim Eibl
    email                : joachim.eibl at gmx.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef COMMON_H
#define COMMON_H

#include <QAtomicInt>
#include <QString>
#include <algorithm>
#include <map>

template <class T>
T min3( T d1, T d2, T d3 )
{
   return std::min( std::min( d1, d2 ), d3 );
}

template <class T>
T max3( T d1, T d2, T d3 )
{
   return std::max( std::max( d1, d2 ), d3 );
}

inline int getAtomic(QAtomicInt& ai)
{
   return ai.load();
}

inline qint64 getAtomic(QAtomicInteger<qint64>& ai)
{
   return ai.load();
}

class QFont;
class QColor;
class QSize;
class QPoint;
class QStringList;
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
    // void load( const QString& s );

    virtual void writeEntry(const QString&, const QFont&);
    virtual void writeEntry(const QString&, const QColor&);
    virtual void writeEntry(const QString&, const QSize&);
    virtual void writeEntry(const QString&, const QPoint&);
    virtual void writeEntry(const QString&, int);
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
    int         readEntry(const QString& s, int iDefault);
    QStringList readEntry(const QString& s, const QStringList& defaultVal);

  private:
    virtual QFont       readFontEntry(const QString&, const QFont* defaultVal);
    virtual QColor      readColorEntry(const QString&, const QColor* defaultVal);
    virtual QSize       readSizeEntry(const QString&, const QSize* defaultVal);
    virtual QPoint      readPointEntry(const QString&, const QPoint* defaultVal);
    virtual bool        readBoolEntry(const QString&, bool bDefault);
    virtual int         readNumEntry(const QString&, int iDefault);
    virtual QStringList readListEntry(const QString&, const QStringList& defaultVal);
    virtual QString     readStringEntry(const QString&, const QString&);
};

QStringList safeStringSplit(const QString& s, char sepChar=';', char metaChar='\\' );
QString safeStringJoin(const QStringList& sl, char sepChar=';', char metaChar='\\' );

#endif
