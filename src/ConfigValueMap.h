// clang-format off
/*
 *  This file is part of KDiff3.
 *
 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
*/
// clang-format on

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
    void writeEntry(const QString& s, qint32 v) override
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
    qint32 readNumEntry(const QString& s, qint32 defaultVal) override
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
