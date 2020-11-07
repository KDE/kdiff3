/*
 *  This file is part of KDiff3.
 *
 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef SOURCEDATA_H
#define SOURCEDATA_H

#include "options.h"
#include "fileaccess.h"
#include "LineRef.h"

#include <boost/signals2.hpp>

#include <QTextCodec>
#include <QTemporaryFile>
#include <QString>
#include <QVector>

class LineData;
class SourceData
{
  public:
    void setupConnections();
    void setOptions(const QSharedPointer<Options> &pOptions);

    LineRef getSizeLines() const;
    qint64 getSizeBytes() const;
    const char* getBuf() const;
    const QString& getText() const;
    const QVector<LineData>* getLineDataForDisplay() const;
    const QVector<LineData>* getLineDataForDiff() const;

    void setFilename(const QString& filename);
    void setFileAccess(const FileAccess& fileAccess);
    QString getFilename() const;
    void setAliasName(const QString& name);
    QString getAliasName() const;
    bool isEmpty() const;                // File was set
    bool hasData() const;                // Data was readable
    bool isText() const;                 // is it pure text (vs. binary data)
    bool isIncompleteConversion() const; // true if some replacement characters were found
    bool isFromBuffer() const;           // was it set via setData() (vs. setFileAccess() or setFilename())
    const QString setData(const QString& data);
    bool isValid() const; // Either no file is specified or reading was successful

    // Returns a list of error messages if anything went wrong
    const QStringList& readAndPreprocess(QTextCodec* pEncoding, bool bAutoDetectUnicode);
    bool saveNormalDataAs(const QString& fileName);

    bool isBinaryEqualWith(const QSharedPointer<SourceData>& other) const;

    void reset();

    bool isDir() { return m_fileAccess.isDir(); }

    QTextCodec* getEncoding() const { return m_pEncoding; }
    e_LineEndStyle getLineEndStyle() const { return m_normalData.m_eLineEndStyle; }

    const QStringList& getErrors() { return mErrors; }

    void setEncoding(QTextCodec* pEncoding);

  private:
    bool convertFileEncoding(const QString& fileNameIn, QTextCodec* pCodecIn,
                                const QString& fileNameOut, QTextCodec* pCodecOut);

    static QTextCodec* detectEncoding(const char* buf, qint64 size, qint64& skipBytes);
    static QTextCodec* getEncodingFromTag(const QByteArray& s, const QByteArray& encodingTag);

    QTextCodec* detectEncoding(const QString& fileName, QTextCodec* pFallbackCodec);
    QString m_aliasName;
    FileAccess m_fileAccess;
    QSharedPointer<Options> m_pOptions;
    QString m_tempInputFileName;
    QTemporaryFile m_tempFile; //Created from clipboard content.

    QStringList mErrors;

    bool mFromClipBoard = false;

    class FileData
    {
      private:
        friend SourceData;
        const char* m_pBuf = nullptr; //TODO: Phase out needlessly wastes memmory and time by keeping second copy of file data.
        qint64 mDataSize = 0;
        qint64 mLineCount = 0; // Nr of lines in m_pBuf1 and size of m_v1, m_dv12 and m_dv13
        QSharedPointer<QString> m_unicodeBuf=QSharedPointer<QString>::create();
        QVector<LineData> m_v;
        bool m_bIsText = false;
        bool m_bIncompleteConversion = false;
        e_LineEndStyle m_eLineEndStyle = eLineEndStyleUndefined;

      public:
        ~FileData() { reset(); }

        bool readFile(FileAccess& file);
        bool readFile(const QString& filename);
        bool writeFile(const QString& filename);

        bool preprocess(QTextCodec* pEncoding, bool removeComments);
        void reset();
        void copyBufFrom(const FileData& src);

        bool isEmpty() const { return mDataSize == 0; }

        bool isText() const { return m_bIsText || isEmpty(); }

        inline qint64 lineCount() const { return mLineCount; }
        inline qint64 byteCount() const { return mDataSize; }
    };
    FileData m_normalData;
    FileData m_lmppData;
    QTextCodec* m_pEncoding = nullptr;

    /*
      This list exists solely to auto disconnect boost signals.
    */
    std::list<boost::signals2::scoped_connection> connections;
};

#endif // !SOURCEDATA_H
