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

#include <QBuffer>
#include <QByteArray>
#include <QIODevice>
#include <QPointer>
#include <QString>
#include <QStringDecoder>

class EncodedDataStream: public QIODeviceBase
{
  private:
    Q_DISABLE_COPY(EncodedDataStream);
    QPointer<QIODevice> dev;

    QStringDecoder mDecoder = QStringDecoder("UTF-8", QStringConverter::Flag::ConvertInitialBom);
    QStringEncoder mEncoder = QStringEncoder("UTF-8", QStringEncoder::Flag::ConvertInitialBom);
    QByteArray mEncoding = "UTF-8";
    bool mGenerateBOM = false;
    bool mError = false;

  public:
    EncodedDataStream(QIODevice *d)
    {
        //For simplicty we assume ownership of the device as does QTextStream
        dev = d;
    }

    EncodedDataStream(const QByteArray &a, OpenMode flags = QIODevice::ReadOnly)
    {
        QBuffer *buf = new QBuffer();
        buf->setData(a);
        buf->open(flags);
        dev = buf;
    }

    EncodedDataStream(QByteArray *ba, OpenMode flags = QIODevice::ReadOnly)
    {
        assert(ba != nullptr && !dev.isNull());
        dev = new QBuffer(ba);
        dev->open(flags);
    }

    void setGenerateByteOrderMark(bool generate) { mGenerateBOM = generate; }

    inline bool hasBOM() const noexcept { return mGenerateBOM; }

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
            mGenerateBOM = inEncoding.startsWith("UTF-16") || inEncoding.startsWith("UTF-32");
            mEncoding = inEncoding;
        }

        mDecoder = QStringDecoder(mEncoding, mGenerateBOM ? QStringConverter::Flag::WriteBom : QStringConverter::Flag::ConvertInitialBom);
        mEncoder = QStringEncoder(mEncoding, mGenerateBOM ? QStringConverter::Flag::WriteBom : QStringConverter::Flag::ConvertInitialBom);

        assert(mDecoder.isValid() && mEncoder.isValid());
        assert(!mGenerateBOM || ((inEncoding.startsWith("UTF-16") || inEncoding.startsWith("UTF-32")) || inEncoding == "UTF-8-BOM"));
    };

    inline qint64 readChar(QChar& c)
    {
        char curData = QChar::Null;
        qint64 len = 0;
        QString s;

        if(!mDecoder.isValid()) return 0;

        do
        {
            len += dev->read(&curData, 1);
            s = mDecoder(QByteArray::fromRawData(&curData, sizeof(curData)));
        } while(!mDecoder.hasError() && s.isEmpty() && !atEnd());

        mError = mDecoder.hasError() || (s.isEmpty() && atEnd());
        if(!mError)
            c = s[0];
        else
            c = QChar::ReplacementCharacter;

        return len;
    }

    quint64 peekChar(QChar &c)
    {
        QStringDecoder decoder = QStringDecoder(mEncoding);
        char buf[4];
        qint64 len = dev->peek(buf, sizeof(buf));

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

        mError = mEncoder.hasError();
        qint64 len = dev->write(data.constData(), data.length());
        if(len != data.length())
            mError = true;

        return *this;
    };

    inline QString errorString() { return dev->errorString(); }
    inline bool hasError() const noexcept { return mError; }
    inline bool atEnd() const noexcept { return dev->atEnd(); }

    EncodedDataStream &operator<<(const QByteArray &bytes)
    {
        qint64 len = dev->write(bytes.constData(), bytes.length());

        if(len != bytes.length())
            mError = true;

        return *this;
    };
};

#endif /* ENCODEDDATASTREAM_H */
