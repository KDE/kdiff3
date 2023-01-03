// clang-format off
/*
 *  This file is part of KDiff3.
 *
 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
*/
// clang-format on

#ifndef SOURCEDATA_H
#define SOURCEDATA_H

#include "diff.h"
#include "fileaccess.h"
#include "LineRef.h"
#include "options.h"

#include <memory>

#include <QTextCodec>
#include <QTemporaryFile>
#include <QSharedPointer>
#include <QString>
#include <QStringList>

class SourceData
{
  public:
    void setOptions(const QSharedPointer<Options> &pOptions);

    Q_REQUIRED_RESULT LineCount getSizeLines() const;
    Q_REQUIRED_RESULT qint64 getSizeBytes() const;
    Q_REQUIRED_RESULT const char* getBuf() const;
    Q_REQUIRED_RESULT const QString& getText() const;
    Q_REQUIRED_RESULT const std::shared_ptr<LineDataVector>& getLineDataForDisplay() const;
    Q_REQUIRED_RESULT const std::shared_ptr<LineDataVector>& getLineDataForDiff() const;

    void setFilename(const QString& filename);
    void setFileAccess(const FileAccess& fileAccess);
    Q_REQUIRED_RESULT QString getFilename() const;
    void setAliasName(const QString& name);
    Q_REQUIRED_RESULT QString getAliasName() const;
    Q_REQUIRED_RESULT bool isEmpty() const;                // File was set
    Q_REQUIRED_RESULT bool hasData() const;                // Data was readable
    Q_REQUIRED_RESULT bool isText() const;                 // is it pure text (vs. binary data)
    Q_REQUIRED_RESULT bool isIncompleteConversion() const; // true if some replacement characters were found
    Q_REQUIRED_RESULT bool isFromBuffer() const;           // was it set via setData() (vs. setFileAccess() or setFilename())
    Q_REQUIRED_RESULT const QString setData(const QString& data);
    Q_REQUIRED_RESULT bool isValid() const; // Either no file is specified or reading was successful

    // Returns a list of error messages if anything went wrong
    void readAndPreprocess(QTextCodec* pEncoding, bool bAutoDetectUnicode);
    bool saveNormalDataAs(const QString& fileName);

    Q_REQUIRED_RESULT bool isBinaryEqualWith(const QSharedPointer<SourceData>& other) const;

    void reset();

    Q_REQUIRED_RESULT bool isDir() const { return m_fileAccess.isDir(); }

    Q_REQUIRED_RESULT QTextCodec* getEncoding() const { return m_pEncoding; }
    Q_REQUIRED_RESULT e_LineEndStyle getLineEndStyle() const { return m_normalData.m_eLineEndStyle; }
    [[nodiscard]] inline bool hasEOLTermiantion() { return m_normalData.hasEOLTermiantion(); }


    Q_REQUIRED_RESULT const QStringList& getErrors() const { return mErrors; }

    void setEncoding(QTextCodec* pEncoding);

  protected:
    bool convertFileEncoding(const QString& fileNameIn, QTextCodec* pCodecIn,
                                const QString& fileNameOut, QTextCodec* pCodecOut);

    static QTextCodec* dectectUTF8(const QByteArray& data);
    static QTextCodec* detectEncoding(const char* buf, qint64 size, FileOffset& skipBytes);
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
        std::unique_ptr<char[]> m_pBuf; //TODO: Phase out needlessly wastes memory and time by keeping second copy of file data.
        quint64 mDataSize = 0;
        qint64 mLineCount = 0; // Number of lines in m_pBuf1 and size of m_v1, m_dv12 and m_dv13
        QSharedPointer<QString> m_unicodeBuf=QSharedPointer<QString>::create();
        std::shared_ptr<LineDataVector> m_v=std::make_shared<LineDataVector>();
        bool m_bIsText = false;
        bool m_bIncompleteConversion = false;
        e_LineEndStyle m_eLineEndStyle = eLineEndStyleUndefined;
        bool mHasEOLTermination = false;

      public:
        bool readFile(FileAccess& file);
        bool readFile(const QString& filename);
        bool writeFile(const QString& filename);

        bool preprocess(QTextCodec* pEncoding, bool removeComments);
        void reset();
        void copyBufFrom(const FileData& src);

        [[nodiscard]] bool isEmpty() const { return mDataSize == 0; }

        [[nodiscard]] bool isText() const { return m_bIsText || isEmpty(); }
        [[nodiscard]] inline bool hasEOLTermiantion() { return m_bIsText && mHasEOLTermination; }

        [[nodiscard]] inline qint64 lineCount() const { return mLineCount; }
        [[nodiscard]] inline qint64 byteCount() const { return mDataSize; }
    };
    FileData m_normalData;
    FileData m_lmppData;
    QTextCodec* m_pEncoding = nullptr;
};

#endif // !SOURCEDATA_H
