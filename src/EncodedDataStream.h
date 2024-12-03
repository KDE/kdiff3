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
#include <QIODevice>
#include <QString>
#include <QStringDecoder>

class EncodedDataStream: public QDataStream
{
  private:
    QStringEncoder mEncoder = QStringEncoder("UTF-8", QStringEncoder::Flag::ConvertInitialBom);
    QByteArray mEncoding = "UTF-8";
    bool mGenerateBOM = false;
    bool mError = false;

  public:
    using QDataStream::QDataStream;

    void setGenerateByteOrderMark(bool generate) { mGenerateBOM = generate; }

    inline bool hasBOM() const { return mGenerateBOM; }

    inline void setEncoding(const QByteArray &inEncoding) 
    {
        assert(!inEncoding.isEmpty());

        if(inEncoding == "UTF-8-BOM")
        {
            mGenerateBOM = true;
            mEncoding = "UTF-8";
        }
        else
        {
            mGenerateBOM = inEncoding.startsWith("UTF-16") || inEncoding.startsWith("UTF-32");
            mEncoding = inEncoding;
        }

        mEncoder = QStringEncoder(mEncoding, mGenerateBOM ? QStringConverter::Flag::WriteBom : QStringConverter::Flag::ConvertInitialBom);

        assert(!mGenerateBOM || ((inEncoding.startsWith("UTF-16") || inEncoding.startsWith("UTF-32")) || inEncoding == "UTF-8-BOM"));
    };

    inline qint32 readChar(QChar& c)
    {
        char curData = QChar::Null;
        qint32 len = 0;
        QString s;
        QStringDecoder decoder = QStringDecoder(mEncoding, mGenerateBOM ? QStringConverter::Flag::WriteBom : QStringConverter::Flag::ConvertInitialBom);
        assert(decoder.isValid());
        if(!decoder.isValid()) return 0;

        do
        {
            len += readRawData(&curData, 1);

            s = decoder(QByteArray::fromRawData(&curData, sizeof(curData)));
        } while(!decoder.hasError() && s.isEmpty() && !atEnd());

        mError = decoder.hasError() || (s.isEmpty() && atEnd());
        if(!mError)
            c = s[0];
        else
            c = QChar::ReplacementCharacter;

        return len;
    }

    quint64 peekChar(QChar &c)
    {
        QStringDecoder decoder = QStringDecoder(mEncoding);
        QIODevice *d = device();
        char buf[4];
        qint64 len = d->peek(buf, sizeof(buf));

        if(len > 0)
        {
            QString s = decoder(QByteArray::fromRawData(buf, sizeof(buf)));
            if(s.isEmpty()) return 0;

            c = s[0];
        }
        else
            c = QChar::Null;
        return len;
    }

    EncodedDataStream &operator<<(const QString &s)
    {
        QByteArray data = mEncoder(s);

        assert(mEncoder.isValid());
        mError = mEncoder.hasError();
        writeRawData(data.constData(), data.length());
        return *this;
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
