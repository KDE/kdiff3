/*
 * KDiff3 - Text Diff And Merge Tool

 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
*/
// clang-format on

#include "common.h"
#include "TypeUtils.h"

#include <map>
#include <utility>            // for pair

#include <QColor>
#include <QFont>
#include <QPoint>
#include <QLatin1String>
#include <QSize>
#include <QStringList>
#include <QStringLiteral>
#include <QTextStream>

ValueMap::ValueMap() = default;

ValueMap::~ValueMap() = default;

void ValueMap::save(QTextStream& ts)
{
    for(const auto &entry: m_map)
    {
        const QString key = entry.first;
        const QString val = entry.second;
        ts << key << "=" << val << "\n";
    }
}

QString ValueMap::getAsString()
{
    QString result;

    for(const auto &entry: m_map)
    {
        const QString key = entry.first;
        const QString val = entry.second;
        result += key + '=' + val + '\n';
    }
    return result;
}

void ValueMap::load(QTextStream& ts)
{
    while(!ts.atEnd())
    {                              // until end of file...
        QString s = ts.readLine(); // line of text excluding '\n'
        QtSizeType pos = s.indexOf('=');
        if(pos > 0) // seems not to have a tag
        {
            QString key = s.left(pos);
            QString val = s.mid(pos + 1);
            m_map[key] = val;
        }
    }
}

// safeStringJoin and safeStringSplit allow to convert a stringlist into a string and back
// safely, even if the individual strings in the list contain the separator character.
QString safeStringJoin(const QStringList& sl, char sepChar, char metaChar)
{
    // Join the strings in the list, using the separator ','
    // If a string contains the separator character, it will be replaced with "\,".
    // Any occurrences of "\" (one backslash) will be replaced with "\\" (2 backslashes)

    assert(sepChar != metaChar);

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
    assert(sepChar != metaChar);
    QStringList sl;
    // Miniparser
    QtSizeType i = 0;
    QtSizeType len = s.length();
    QString b;
    for(i = 0; i < len; ++i)
    {
        if(i + 1 < len && s[i] == metaChar && s[i + 1] == metaChar)
        {
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

void ValueMap::writeEntry(const QString& k, const QFont& v)
{
    m_map[k] = v.family() + u8"," + QString::number(v.pointSize()) + u8"," + (v.bold() ? QStringLiteral("bold") : QStringLiteral("normal"));
}

void ValueMap::writeEntry(const QString& k, const QColor& v)
{
    m_map[k].setNum(v.red()) + u8"," + QString().setNum(v.green()) + u8"," + QString().setNum(v.blue());
}

void ValueMap::writeEntry(const QString& k, const QSize& v)
{
    m_map[k].setNum(v.width()) + u8"," + QString().setNum(v.height());
}

void ValueMap::writeEntry(const QString& k, const QPoint& v)
{
    m_map[k].setNum(v.x()) + u8"," + QString().setNum(v.y());
}

void ValueMap::writeEntry(const QString& k, qint32 v)
{
    m_map[k].setNum(v);
}

void ValueMap::writeEntry(const QString& k, bool v)
{
    m_map[k].setNum(v);
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
        c = QColor(s.split(',')[0].toInt(), s.split(',')[1].toInt(), s.split(',')[2].toInt());
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
        size = QSize(s.split(',')[0].toInt(), s.split(',')[1].toInt());
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
        point = QPoint(s.split(',')[0].toInt(), s.split(',')[1].toInt());
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
        b = (s.split(',')[0].toInt() == 1);
    }

    return b;
}

qint32 ValueMap::readNumEntry(const QString& k, qint32 iDefault)
{
    qint32 ival = iDefault;
    std::map<QString, QString>::iterator i = m_map.find(k);
    if(i != m_map.end())
    {
        QString s = i->second;
        ival = s.split(',')[0].toInt();
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
qint32 ValueMap::readEntry(const QString& s, qint32 iDefault)
{
    return readNumEntry(s, iDefault);
}
QStringList ValueMap::readEntry(const QString& s, const QStringList& defaultVal)
{
    return readListEntry(s, defaultVal);
}
