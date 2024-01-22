/**
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2023 Michael Reeves <reeves.87@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 */

#ifndef ENCODEDDATASTREAM_H
#define ENCODEDDATASTREAM_H

#include "TypeUtils.h"

#include <QByteArray>
#include <QDataStream>
#include <QString>
#include <QTextCodec>
#include <QTextDecoder>

class EncodedDataStream: public QDataStream
{
  private:
    QByteArray mEncoding = "UTF-8";
    bool mGenerateBOM = false;
    bool mError = false;

  public:
    using QDataStream::QDataStream;

    void setGenerateByteOrderMark(bool generate) { mGenerateBOM = generate; }

    //Let the compiler choose the optimal solution based on c++ rules.
    inline void setEncoding(const QByteArray &inEncoding) noexcept
    {
        assert(!inEncoding.isEmpty());
        if(inEncoding == "UTF-8-BOM")
        {
            mGenerateBOM = true;
            mEncoding = "UTF-8";
        }
        else
            mEncoding = inEncoding;
    };

    inline void setEncoding(const QByteArray &&inEncoding) noexcept
    {
        assert(!inEncoding.isEmpty());
        if(inEncoding == "UTF-8-BOM")
        {
            mGenerateBOM = true;
            mEncoding = "UTF-8";
        }
        else
            mEncoding = inEncoding;
    };

    inline qint32 readChar(QChar& c)
    {
        char curByte;
        qint32 len = 0;
        QString s;

#if(QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
        QTextDecoder decoder = QTextDecoder(QTextCodec::codecForName(mEncoding), mGenerateBOM ? QTextCodec::ConversionFlag::DefaultConversion : QTextCodec::ConversionFlag::IgnoreHeader);
#else
        QTextDecoder decoder = QTextDecoder(QTextCodec::codecForName(mEncoding), mGenerateBOM ? QStringConverter::Flag::WriteBom : QStringConverter::Flag::ConvertInitialBom);
#endif

        do
        {
            len += readRawData(&curByte, 1);
            s = decoder.toUnicode(&curByte, 1);
        } while(!decoder.hasFailure() && decoder.needsMoreData() && !atEnd());

        mError = decoder.hasFailure();
        if(!mError)
            c = s[0];
        else
            c = QChar::ReplacementCharacter;

        return len;
    }

    EncodedDataStream &operator<<(const QString &s)
    {
        QTextEncoder encoder(QTextCodec::codecForName(mEncoding), mGenerateBOM ? QTextCodec::ConversionFlag::DefaultConversion : QTextCodec::ConversionFlag::IgnoreHeader);

        QByteArray data = encoder.fromUnicode(s);
        mError = encoder.hasFailure();

        return *this << data;
    };

    inline bool hasError() noexcept { return mError; }

    //Not implemented but may be inherieted from QDataStream
    EncodedDataStream &operator<<(const QChar &) = delete;
    EncodedDataStream &operator<<(const char *&) = delete;
    EncodedDataStream &operator<<(const char &) = delete;

    EncodedDataStream &operator<<(const QByteArray &bytes)
    {
        writeRawData(bytes.constData(), bytes.length());
        return *this;
    };

    EncodedDataStream &operator>>(QByteArray &) = delete;
    EncodedDataStream &operator>>(QString &) = delete;
    EncodedDataStream &operator>>(QChar &) = delete;
    EncodedDataStream &operator>>(char *&) = delete;
    EncodedDataStream &operator>>(char &) = delete;
};

#endif /* ENCODEDDATASTREAM_H */
