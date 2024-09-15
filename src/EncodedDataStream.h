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
#include <QStringDecoder>

class EncodedDataStream: public QDataStream
{
  private:
    QByteArray mEncoding = "UTF-8";
    bool mGenerateBOM = false;
    bool mError = false;

  public:
    using QDataStream::QDataStream;

    void setGenerateByteOrderMark(bool generate) { mGenerateBOM = generate; }

    inline void setEncoding(const QByteArray &inEncoding) noexcept
    {
        assert(!inEncoding.isEmpty());
        if(inEncoding == "UTF-8-BOM")
        {
            mGenerateBOM = true;
            mEncoding = "UTF-8";
        }
        else
        {
            mGenerateBOM = inEncoding.startsWith("UTF") && !inEncoding.endsWith("-8");
            mEncoding = inEncoding;
        }
    };

    inline qint32 readChar(QChar& c)
    {
        char curByte;
        qint32 len = 0, nullCount = 0;
        QString s;
        QStringDecoder decoder = QStringDecoder(mEncoding, mGenerateBOM ? QStringConverter::Flag::WriteBom : QStringConverter::Flag::ConvertInitialBom);
        assert(decoder.isValid());
        if(!decoder.isValid()) return 0;

        do
        {
            len += readRawData(&curByte, 1);
            //Work around a bug in QStringDecoder when handling null bytes
            if(curByte == QChar::Null) ++nullCount;
            else nullCount = 0;

            s = decoder(QByteArray(sizeof(curByte), curByte));
        } while(!decoder.hasError() && s.isEmpty() && !atEnd() && nullCount < 4);

        mError = decoder.hasError();
        if(nullCount != 0)
            c = QChar::Null;

        if(!mError)
            c = s[0];
        else
            c = QChar::ReplacementCharacter;

        return len;
    }

    EncodedDataStream &operator<<(const QString &s)
    {
        QStringEncoder encoder(mEncoding, mGenerateBOM ? QStringConverter::Flag::WriteBom : QStringConverter::Flag::ConvertInitialBom);
        QByteArray data = encoder(s);
        assert(encoder.isValid());
        mError = encoder.hasError();

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
