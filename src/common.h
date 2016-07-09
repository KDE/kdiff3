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

#ifndef _COMMON_H
#define _COMMON_H

#include <assert.h>


template< class T >
T min2( T x, T y )
{
   return x<y ? x : y;
}
template< class T >
T max2( T x, T y )
{
   return x>y ? x : y;
}

typedef unsigned char UINT8;
typedef unsigned short UINT16;
typedef unsigned int UINT32;


template <class T>
T min3( T d1, T d2, T d3 )
{
   if ( d1 < d2  &&  d1 < d3 ) return d1;
   if ( d2 < d3 ) return d2;
   return d3;
}

template <class T>
T max3( T d1, T d2, T d3 )
{

   if ( d1 > d2  &&  d1 > d3 ) return d1;


   if ( d2 > d3 ) return d2;
   return d3;

}

template <class T>
T minMaxLimiter( T d, T minimum, T maximum )
{
   assert(minimum<=maximum);
   if ( d < minimum ) return minimum;
   if ( d > maximum ) return maximum;
   return d;
}

#include <map>
#include <QString>
class QFont;
class QColor;
class QSize;
class QPoint;
class QStringList;
class QTextStream;

class ValueMap
{
private:
   std::map<QString,QString> m_map;
public:
   ValueMap();
   virtual ~ValueMap();

   void save( QTextStream& ts );
   void load( QTextStream& ts );
   QString getAsString();
   // void load( const QString& s );

   virtual void writeEntry(const QString&, const QFont& );
   virtual void writeEntry(const QString&, const QColor& );
   virtual void writeEntry(const QString&, const QSize& );
   virtual void writeEntry(const QString&, const QPoint& );
   virtual void writeEntry(const QString&, int );
   virtual void writeEntry(const QString&, bool );
   virtual void writeEntry(const QString&, const QStringList&, char separator='|' );
   virtual void writeEntry(const QString&, const QString& );
   virtual void writeEntry(const QString&, const char* );

   virtual QFont       readFontEntry (const QString&, const QFont* defaultVal );
   virtual QColor      readColorEntry(const QString&, const QColor* defaultVal );
   virtual QSize       readSizeEntry (const QString&, const QSize* defaultVal );
   virtual QPoint      readPointEntry(const QString&, const QPoint* defaultVal );
   virtual bool        readBoolEntry (const QString&, bool bDefault );
   virtual int         readNumEntry  (const QString&, int iDefault );
   virtual QStringList readListEntry (const QString&, const QStringList& defaultVal, char separator='|' );
   virtual QString     readStringEntry(const QString&, const QString& );

   QString     readEntry (const QString& s, const QString& defaultVal );
   QString     readEntry (const QString& s, const char* defaultVal );
   QFont       readEntry (const QString& s, const QFont& defaultVal );
   QColor      readEntry(const QString& s, const QColor defaultVal );
   QSize       readEntry (const QString& s, const QSize defaultVal );
   QPoint      readEntry(const QString& s, const QPoint defaultVal );
   bool        readEntry (const QString& s, bool bDefault );
   int         readEntry  (const QString& s, int iDefault );
   QStringList readEntry (const QString& s, const QStringList& defaultVal, char separator='|' );
};

QStringList safeStringSplit(const QString& s, char sepChar=',', char metaChar='\\' );
QString safeStringJoin(const QStringList& sl, char sepChar=',', char metaChar='\\' );

#endif
