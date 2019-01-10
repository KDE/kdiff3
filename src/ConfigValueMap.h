/**
 * Copyright (C) 2019 Michael Reeves <reeves.87@gmail.com>
 * Copyright (C) 2002-2009  Joachim Eibl, joachim.eibl at gmx.de
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
 */
#ifndef CONFIGVALUEMAP_H
#define CONFIGVALUEMAP_H

#include "common.h"

#include <KConfigGroup>
#include <QColor>
#include <QFont>
#include <QString>
#include <QSize>
#include <QPoint>
class ConfigValueMap : public ValueMap
{
  private:
    KConfigGroup m_config;

  public:
    explicit ConfigValueMap(const KConfigGroup& config) : m_config(config) {}

    void writeEntry(const QString& s, const QFont& v) override
    {
        m_config.writeEntry(s, v);
    }
    void writeEntry(const QString& s, const QColor& v) override
    {
        m_config.writeEntry(s, v);
    }
    void writeEntry(const QString& s, const QSize& v) override
    {
        m_config.writeEntry(s, v);
    }
    void writeEntry(const QString& s, const QPoint& v) override
    {
        m_config.writeEntry(s, v);
    }
    void writeEntry(const QString& s, int v) override
    {
        m_config.writeEntry(s, v);
    }
    void writeEntry(const QString& s, bool v) override
    {
        m_config.writeEntry(s, v);
    }
    void writeEntry(const QString& s, const QString& v) override
    {
        m_config.writeEntry(s, v);
    }
    void writeEntry(const QString& s, const char* v) override
    {
        m_config.writeEntry(s, v);
    }
private:
    QFont readFontEntry(const QString& s, const QFont* defaultVal) override
    {
        return m_config.readEntry(s, *defaultVal);
    }
    QColor readColorEntry(const QString& s, const QColor* defaultVal) override
    {
        return m_config.readEntry(s, *defaultVal);
    }
    QSize readSizeEntry(const QString& s, const QSize* defaultVal) override
    {
        return m_config.readEntry(s, *defaultVal);
    }
    QPoint readPointEntry(const QString& s, const QPoint* defaultVal) override
    {
        return m_config.readEntry(s, *defaultVal);
    }
    bool readBoolEntry(const QString& s, bool defaultVal) override
    {
        return m_config.readEntry(s, defaultVal);
    }
    int readNumEntry(const QString& s, int defaultVal) override
    {
        return m_config.readEntry(s, defaultVal);
    }
    QString readStringEntry(const QString& s, const QString& defaultVal) override
    {
        return m_config.readEntry(s, defaultVal);
    }

    void writeEntry(const QString& s, const QStringList& v) override
    {
        m_config.writeEntry(s, v);
    }
    QStringList readListEntry(const QString& s, const QStringList& def) override
    {
        return m_config.readEntry(s, def);
    }
};

#endif // !CONFIGVALUEMAP_H
