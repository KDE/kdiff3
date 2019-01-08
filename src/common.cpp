/***************************************************************************
 *   Copyright (C) 2004-2007 by Joachim Eibl                               *
 *   joachim.eibl at gmx.de                                                   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.          *
 ***************************************************************************/

#include "common.h"
#include <QColor>
#include <QSize>
#include <QStringList>
#include <QTextStream>
#include <map>
#include <qfont.h>
#include <qpoint.h>

ValueMap::ValueMap()
{
}

ValueMap::~ValueMap()
{
}

void ValueMap::save(QTextStream& ts)
{
    std::map<QString, QString>::iterator i;
    for(i = m_map.begin(); i != m_map.end(); ++i)
    {
        QString key = i->first;
        QString val = i->second;
        ts << key << "=" << val << "\n";
    }
}

QString ValueMap::getAsString()
{
    QString result;
    std::map<QString, QString>::iterator i;
    for(i = m_map.begin(); i != m_map.end(); ++i)
    {
        QString key = i->first;
        QString val = i->second;
        result += key + '=' + val + '\n';
    }
    return result;
}

void ValueMap::load(QTextStream& ts)
{
    while(!ts.atEnd())
    {                              // until end of file...
        QString s = ts.readLine(); // line of text excluding '\n'
        int pos = s.indexOf('=');
        if(pos > 0) // seems not to have a tag
        {
            QString key = s.left(pos);
            QString val = s.mid(pos + 1);
            m_map[key] = val;
        }
    }
}
/*
void ValueMap::load( const QString& s )
{
   int pos=0;
   while ( pos<(int)s.length() )
   {                                 // until end of file...
      int pos2 = s.find('=', pos);
      int pos3 = s.find('\n', pos2 );
      if (pos3<0)
         pos3=s.length();
      if( pos2 > 0 )                     // seems not to have a tag
      {
         QString key = s.mid(pos, pos2-pos);
         QString val = s.mid(pos2+1, pos3-pos2-1);
         m_map[key] = val;
      }
      pos = pos3;
   }
}
*/

// safeStringJoin and safeStringSplit allow to convert a stringlist into a string and back
// safely, even if the individual strings in the list contain the separator character.
QString safeStringJoin(const QStringList& sl, char sepChar, char metaChar)
{
    // Join the strings in the list, using the separator ','
    // If a string contains the separator character, it will be replaced with "\,".
    // Any occurrences of "\" (one backslash) will be replaced with "\\" (2 backslashes)

    Q_ASSERT(sepChar != metaChar);

    QString sep;
    sep += sepChar;
    QString meta;
    meta += metaChar;

    QString safeString;

    QStringList::const_iterator i;
    for(i = sl.begin(); i != sl.end(); ++i)
    {
        QString s = *i;
        s.replace(meta, meta + meta); //  "\" -> "\\"
        s.replace(sep, meta + sep);   //  "," -> "\,"
        if(i == sl.begin())
            safeString = s;
        else
            safeString += sep + s;
    }
    return safeString;
}

// Split a string that was joined with safeStringJoin
QStringList safeStringSplit(const QString& s, char sepChar, char metaChar)
{
    Q_ASSERT(sepChar != metaChar);
    QStringList sl;
    // Miniparser
    int i = 0;
    int len = s.length();
    QString b;
    for(i = 0; i < len; ++i)
    {
        if(i + 1 < len && s[i] == metaChar && s[i + 1] == metaChar) {
            b += metaChar;
            ++i;
        }
        else if(i + 1 < len && s[i] == metaChar && s[i + 1] == sepChar)
        {
            b += sepChar;
            ++i;
        }
        else if(s[i] == sepChar) // real separator
        {
            sl.push_back(b);
            b = "";
        }
        else
        {
            b += s[i];
        }
    }
    if(!b.isEmpty())
        sl.push_back(b);

    return sl;
}

static QString numStr(int n)
{
    QString s;
    s.setNum(n);
    return s;
}
static int num(QString& s, int idx)
{
    return s.split(',')[idx].toInt();
}

void ValueMap::writeEntry(const QString& k, const QFont& v)
{
    m_map[k] = v.family() + QLatin1String(",") + QString::number(v.pointSize()) + QLatin1String(",") + (v.bold() ? QLatin1String("bold") : QLatin1String("normal"));
}

void ValueMap::writeEntry(const QString& k, const QColor& v)
{
    m_map[k] = numStr(v.red()) + QLatin1String(",") + numStr(v.green()) + QLatin1String(",") + numStr(v.blue());
}

void ValueMap::writeEntry(const QString& k, const QSize& v)
{
    m_map[k] = numStr(v.width()) + QLatin1String(",") + numStr(v.height());
}

void ValueMap::writeEntry(const QString& k, const QPoint& v)
{
    m_map[k] = numStr(v.x()) + QLatin1String(",") + numStr(v.y());
}

void ValueMap::writeEntry(const QString& k, int v)
{
    m_map[k] = numStr(v);
}

void ValueMap::writeEntry(const QString& k, bool v)
{
    m_map[k] = numStr(v);
}

void ValueMap::writeEntry(const QString& k, const QString& v)
{
    m_map[k] = v;
}

void ValueMap::writeEntry(const QString& k, const char* v)
{
    m_map[k] = QLatin1String(v);
}

void ValueMap::writeEntry(const QString& k, const QStringList& v)
{
    m_map[k] = safeStringJoin(v);
}

QFont ValueMap::readFontEntry(const QString& k, const QFont* defaultVal)
{
    QFont f = *defaultVal;
    std::map<QString, QString>::iterator i = m_map.find(k);
    if(i != m_map.end())
    {
        f.setFamily(i->second.split(',')[0]);
        f.setPointSize(i->second.split(',')[1].toInt());
        f.setBold(i->second.split(',')[2] == "bold");
    }
    return f;
}

QColor ValueMap::readColorEntry(const QString& k, const QColor* defaultVal)
{
    QColor c = *defaultVal;
    std::map<QString, QString>::iterator i = m_map.find(k);
    if(i != m_map.end())
    {
        QString s = i->second;
        c = QColor(num(s, 0), num(s, 1), num(s, 2));
    }

    return c;
}

QSize ValueMap::readSizeEntry(const QString& k, const QSize* defaultVal)
{
    QSize size = defaultVal ? *defaultVal : QSize(600, 400);
    std::map<QString, QString>::iterator i = m_map.find(k);
    if(i != m_map.end())
    {

        QString s = i->second;
        size = QSize(num(s, 0), num(s, 1));
    }

    return size;
}

QPoint ValueMap::readPointEntry(const QString& k, const QPoint* defaultVal)
{
    QPoint point = defaultVal ? *defaultVal : QPoint(0, 0);
    std::map<QString, QString>::iterator i = m_map.find(k);
    if(i != m_map.end())
    {
        QString s = i->second;
        point = QPoint(num(s, 0), num(s, 1));
    }

    return point;
}

bool ValueMap::readBoolEntry(const QString& k, bool bDefault)
{
    bool b = bDefault;
    std::map<QString, QString>::iterator i = m_map.find(k);
    if(i != m_map.end())
    {
        QString s = i->second;
        b = (bool)num(s, 0);
    }

    return b;
}

int ValueMap::readNumEntry(const QString& k, int iDefault)
{
    int ival = iDefault;
    std::map<QString, QString>::iterator i = m_map.find(k);
    if(i != m_map.end())
    {
        QString s = i->second;
        ival = num(s, 0);
    }

    return ival;
}

QString ValueMap::readStringEntry(const QString& k, const QString& sDefault)
{
    QString sval = sDefault;
    std::map<QString, QString>::iterator i = m_map.find(k);
    if(i != m_map.end())
    {
        sval = i->second;
    }

    return sval;
}

QStringList ValueMap::readListEntry(const QString& k, const QStringList& defaultVal)
{
    QStringList strList;

    std::map<QString, QString>::iterator i = m_map.find(k);
    if(i != m_map.end()) {
        strList = safeStringSplit(i->second);
        return strList;
    }
    else
        return defaultVal;
}

QString ValueMap::readEntry(const QString& s, const QString& defaultVal)
{
    return readStringEntry(s, defaultVal);
}
QString ValueMap::readEntry(const QString& s, const char* defaultVal)
{
    return readStringEntry(s, QString::fromLatin1(defaultVal));
}
QFont ValueMap::readEntry(const QString& s, const QFont& defaultVal)
{
    return readFontEntry(s, &defaultVal);
}
QColor ValueMap::readEntry(const QString& s, const QColor defaultVal)
{
    return readColorEntry(s, &defaultVal);
}
QSize ValueMap::readEntry(const QString& s, const QSize defaultVal)
{
    return readSizeEntry(s, &defaultVal);
}
QPoint ValueMap::readEntry(const QString& s, const QPoint defaultVal)
{
    return readPointEntry(s, &defaultVal);
}
bool ValueMap::readEntry(const QString& s, bool bDefault)
{
    return readBoolEntry(s, bDefault);
}
int ValueMap::readEntry(const QString& s, int iDefault)
{
    return readNumEntry(s, iDefault);
}
QStringList ValueMap::readEntry(const QString& s, const QStringList& defaultVal)
{
    return readListEntry(s, defaultVal);
}
