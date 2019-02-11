/**
 * Copyright (C) 2002-2009  Joachim Eibl, joachim.eibl at gmx.de
 * Copyright (C) 2018 Michael Reeves <reeves.87@gmail.com>
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


#ifndef OPTIONITEMS_H
#define OPTIONITEMS_H

#include "common.h"

#include <QString>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QTextCodec>
#include <QLineEdit>

#include <KLocalizedString>

class OptionItemBase
{
  public:
    explicit OptionItemBase(const QString& saveName)
    {
        m_saveName = saveName;
        m_bPreserved = false;
    }
    virtual ~OptionItemBase(){};
    virtual void setToDefault() = 0;
    virtual void setToCurrent() = 0;

    virtual void apply() = 0;
    virtual void write(ValueMap*) = 0;
    virtual void read(ValueMap*) = 0;
    void doPreserve()
    {
        if(!m_bPreserved) {
            m_bPreserved = true;
            preserve();
        }
    }
    void doUnpreserve()
    {
        if(m_bPreserved) {
            unpreserve();
        }
    }
    QString getSaveName() { return m_saveName; }
  protected:
    virtual void preserve() = 0;
    virtual void unpreserve() = 0;
    bool m_bPreserved;
    QString m_saveName;

    Q_DISABLE_COPY(OptionItemBase)
};

template <class T>
class Option : public OptionItemBase
{
  public:
    explicit Option(const QString& saveName)
        : OptionItemBase(saveName)
    {
    }

    explicit Option(T* pVar, const QString& saveName):OptionItemBase(saveName)
    {
        m_pVar = pVar;
    }

    explicit Option(const T& defaultVal, const QString& saveName, T* pVar)
        : Option<T>(pVar, defaultVal, saveName)
    {}
    explicit Option(T* pVar, const T& defaultValue, const QString& saveName)
        : OptionItemBase(saveName)
    {
       m_pVar = pVar;
       m_defaultVal = defaultValue;
    }

    void setToDefault() override {};
    void setToCurrent() override {};
    const T& getDefault() const { return m_defaultVal; };
    const T getCurrent() const { return *m_pVar; };

    virtual void setCurrent(const T inValue) { *m_pVar= inValue; }

    void apply() override {};
    virtual void apply(const T& inValue) { *m_pVar = inValue; }

    void write(ValueMap* config) override { config->writeEntry(m_saveName, *m_pVar); }
    void read(ValueMap* config) override { *m_pVar = config->readEntry(m_saveName, m_defaultVal); }
  protected:
    void preserve() override { m_preservedVal = *m_pVar; }
    void unpreserve() override { *m_pVar = m_preservedVal; }
    T* m_pVar;
    T m_preservedVal;
    T m_defaultVal;
  private:
   Q_DISABLE_COPY(Option)
};


template <class T>
class OptionNum : public Option<T>
{
  public:
    using Option<T>::Option;
    explicit OptionNum(T* pVar, const QString& saveName)
        : Option<T>(pVar, saveName)
    {
        stringValue = QLocale().toString(*pVar);
    }

    explicit OptionNum(T* pVar, const T& defaultValue, const QString& saveName)
        : Option<T>(pVar, defaultValue, saveName)
    {
        stringValue = QLocale().toString(*pVar);
    }

    void setCurrent(const T inValue) override
    {
        //setNum does not use locale formatting instead it always use QLocale::C.
        stringValue = QLocale().toString(inValue);
        Option<T>::setCurrent(inValue);
    }

    static const QString toString(const T inValue)
    {
        //QString::setNum does not use locale formatting instead it always use QLocale::C.
        return QLocale().toString(inValue);
    }
    const QString& getString() const
    {
        return stringValue;
    }

  private:
    QString stringValue;

    Q_DISABLE_COPY(OptionNum)
};

typedef Option<bool> OptionToggleAction;
typedef OptionNum<int> OptionInt;
typedef Option<QPoint> OptionPoint;
typedef Option<QSize> OptionSize;
typedef Option<QStringList> OptionStringList;

typedef Option<bool> OptionBool;
typedef Option<QFont> OptionFont;
typedef Option<QColor> OptionColor;
typedef Option<QString> OptionString;

class OptionCodec : public OptionString
{
  public:
    using OptionString::Option;

    void setCurrent(const QString name) override { OptionString::setCurrent(name); };
    void setCurrent(const QByteArray& name) { OptionString::setCurrent(QString::fromLatin1(name)); }
    const QString& defaultName() const { return mDefaultName; }

    void saveDefaultIndex(const int i) { defaultIndex = i; };
    int getDefaultIndex() const { return defaultIndex; }

  private:
    const QString mDefaultName = QLatin1String(QTextCodec::codecForLocale()->name());
    int defaultIndex = 0;
    Q_DISABLE_COPY(OptionCodec)
};

#endif // !OPTIONITEMS_H
