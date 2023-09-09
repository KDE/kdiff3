// clang-format off
/*
 *  This file is part of KDiff3.
 *
 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
*/
// clang-format on

#include <QTextCodec>

// UTF8-Codec that saves a BOM
class UTF8BOMCodec: public QTextCodec
{
    QTextCodec* m_pUtf8Codec;
    class PublicTextCodec: public QTextCodec
    {
      public:
        QString publicConvertToUnicode(const char* p, qint32 len, ConverterState* pState) const
        {
            return convertToUnicode(p, len, pState);
        }
        QByteArray publicConvertFromUnicode(const QChar* input, qint32 number, ConverterState* pState) const
        {
            return convertFromUnicode(input, number, pState);
        }
    };

  public:
    UTF8BOMCodec()
    {
        m_pUtf8Codec = QTextCodec::codecForName("UTF-8");
    }
    QByteArray name() const override { return "UTF-8-BOM"; }
    qint32 mibEnum() const override { return 2123; }

    QByteArray convertFromUnicode(const QChar* input, qint32 number, ConverterState* pState) const override
    {
        QByteArray r;
        if(pState && pState->state_data[2] == 0) // state_data[2] not used by QUtf8::convertFromUnicode (see qutfcodec.cpp)
        {
            r += "\xEF\xBB\xBF";
            pState->state_data[2] = 1;
            pState->flags |= QTextCodec::IgnoreHeader;
        }

        r += ((PublicTextCodec*)m_pUtf8Codec)->publicConvertFromUnicode(input, number, pState);
        return r;
    }

    QString convertToUnicode(const char* p, qint32 len, ConverterState* pState) const override
    {
        return ((PublicTextCodec*)m_pUtf8Codec)->publicConvertToUnicode(p, len, pState);
    }
};
