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
#include <QFile>
#include <QIODevice>
#include <QPointer>
#include <QString>
#include <QStringDecoder>

/*
    EncodedDataStream needs to be lean and fast readChar maybe called thounds or even millions of times
    depending on the size of data.

    Using QIODevice/QBuffer for in memmory reads comes with speed penality I don't fully understand.
*/
class EncodedDataStream
{
  private:
    Q_DISABLE_COPY(EncodedDataStream);

    QStringDecoder mDecoder = QStringDecoder("UTF-8", QStringConverter::Flag::ConvertInitialBom);
    QStringEncoder mEncoder = QStringEncoder("UTF-8", QStringEncoder::Flag::ConvertInitialBom);
    QByteArray mEncoding = "UTF-8";
    bool mGenerateBOM = false;
    bool mError = false;

    char curData = u8'\0';
    QByteArray mBuf = QByteArray::fromRawData(&curData, sizeof(curData));
    QByteArray &mFileData;
    QByteArray::iterator it;

  public:
    EncodedDataStream(QByteArray &a):
        mFileData(a),
        it(mFileData.begin())
    {
    }

    void reset() { it = mFileData.begin(); }

    void setGenerateByteOrderMark(bool generate) { mGenerateBOM = generate; }

    bool hasBOM() const { return mGenerateBOM; }

    void setEncoding(const QByteArray &inEncoding)
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

    qint64 readChar(QChar &c)
    {
        qint64 len = 0;
        QString s;

        if(!mDecoder.isValid()) return 0;

        /*
            As long as the encoding is incomplete QStringDecoder returns an empty string and waits for more data.
            This loop proceeds one byte at a time until we hit the end of input, get an encoded char and encounter an error.
        */
        do
        {
            if(it == mFileData.end())
                break;

            len++;
            curData = *it++;
            s = mDecoder(mBuf);
        } while(!mDecoder.hasError() && s.isEmpty() && !atEnd());

        mError = mDecoder.hasError() || (s.isEmpty() && atEnd());
        if(!mError)
            c = s[0];
        else
            c = QChar::SpecialCharacter::ReplacementCharacter;

        return len;
    }

    quint64 peekChar(QChar &c)
    {
        QStringDecoder decoder = QStringDecoder(mEncoding);
        qsizetype len = std::max<qsizetype>(4, mFileData.size());
        QByteArray buf = QByteArray::fromRawData(it, len);

        if(len > 0)
        {
            QString s = decoder(buf);
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
        if(!mError)
            mFileData.append(data);

        return *this;
    };

    [[nodiscard]] bool hasError() const { return mError; }
    [[nodiscard]] bool atEnd() const { return it == mFileData.end(); }

    EncodedDataStream &operator<<(const QByteArray &bytes)
    {
        mFileData.append(bytes);

        return *this;
    };
};

/*
    Extend QFile with encoding support. Warning QDeviceIO and therefor QFile can cause slow down on large files
*/
class EncodedFile: public QFile
{
  private:
    QStringDecoder mDecoder = QStringDecoder("UTF-8", QStringConverter::Flag::ConvertInitialBom);
    QStringEncoder mEncoder = QStringEncoder("UTF-8", QStringEncoder::Flag::ConvertInitialBom);
    QByteArray mEncoding = "UTF-8";
    bool mGenerateBOM = false;
    bool mError = false;
    char curData = u8'\0';
    QByteArray mBuf = QByteArray::fromRawData(&curData, sizeof(curData));

  public:
    using QFile::QFile;

    void setGenerateByteOrderMark(bool generate) { mGenerateBOM = generate; }

    bool hasBOM() const { return mGenerateBOM; }

    void setEncoding(const QByteArray &inEncoding)
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

    qint64 readChar(QChar &c)
    {
        //char curData = u8'\0';
        qint64 len = 0;
        QString s;

        if(!mDecoder.isValid()) return 0;

        /*
            As long as the encoding is incomplete QStringDecoder returns an empty string and waits for more data.
            This loop proceeds one byte at a time until we hit the end of input, get an encoded char and encounter an error.
        */
        do
        {
            len += read(&curData, 1);
            s = mDecoder(mBuf);
        } while(!mDecoder.hasError() && s.isEmpty() && !atEnd());

        mError = mDecoder.hasError() || (s.isEmpty() && atEnd());
        if(!mError)
            c = s[0];
        else
            c = QChar::SpecialCharacter::ReplacementCharacter;

        return len;
    }

    quint64 peekChar(QChar &c)
    {
        QStringDecoder decoder = QStringDecoder(mEncoding);
        char buf[4];
        qint64 len = peek(buf, sizeof(buf));

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

    EncodedFile &operator<<(const QString &s)
    {
        QByteArray data = mEncoder(s);

        mError = mEncoder.hasError();
        qint64 len = write(data.constData(), data.length());
        if(len != data.length())
            mError = true;

        return *this;
    };

    [[nodiscard]] bool hasError() const { return mError; }

    EncodedFile &operator<<(const QByteArray &bytes)
    {
        qint64 len = write(bytes.constData(), bytes.length());

        if(len != bytes.length())
            mError = true;

        return *this;
    };
};

#endif /* ENCODEDDATASTREAM_H */
