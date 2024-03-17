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
#include <optional>

#include <QTemporaryFile>
#include <QSharedPointer>
#include <QString>
#include <QStringList>

class SourceData
{
  public:
    [[nodiscard]] LineType getSizeLines() const;
    [[nodiscard]] qint64 getSizeBytes() const;
    [[nodiscard]] const char* getBuf() const;
    [[nodiscard]] const QString& getText() const;
    [[nodiscard]] const std::shared_ptr<LineDataVector>& getLineDataForDisplay() const;
    [[nodiscard]] const std::shared_ptr<LineDataVector>& getLineDataForDiff() const;

    void setFilename(const QString& filename);
    void setFileAccess(const FileAccess& fileAccess);
    [[nodiscard]] QString getFilename() const;
    void setAliasName(const QString& name);
    [[nodiscard]] QString getAliasName() const;
    [[nodiscard]] bool isEmpty() const;                // File was set
    [[nodiscard]] bool hasData() const;                // Data was readable
    [[nodiscard]] bool isText() const;                 // is it pure text (vs. binary data)
    [[nodiscard]] bool isIncompleteConversion() const; // true if some replacement characters were found
    [[nodiscard]] bool isFromBuffer() const;           // was it set via setData() (vs. setFileAccess() or setFilename())
    void setData(const QString& data);
    [[nodiscard]] bool isValid() const; // Either no file is specified or reading was successful

    // Returns a list of error messages if anything went wrong
    void readAndPreprocess(const char* encoding, bool bAutoDetectUnicode);
    bool saveNormalDataAs(const QString& fileName);

    [[nodiscard]] bool isBinaryEqualWith(const QSharedPointer<SourceData>& other) const;

    void reset();

    [[nodiscard]] bool isDir() const { return m_fileAccess.isDir(); }

    [[nodiscard]] const QByteArray getEncoding() const { return mEncoding; }
    [[nodiscard]] e_LineEndStyle getLineEndStyle() const { return m_normalData.m_eLineEndStyle; }
    [[nodiscard]] inline bool hasEOLTermiantion() { return m_normalData.hasEOLTermiantion(); }
    [[nodiscard]] inline bool hasBOM() const { return m_normalData.hasBOM(); }
    [[nodiscard]] const QStringList& getErrors() const { return mErrors; }

    void setEncoding(const QByteArray& encoding);

  protected:
    bool convertFileEncoding(const QString& fileNameIn, const QByteArray& pCodecIn,
                             const QString& fileNameOut, const QByteArray& pCodecOut);

    [[nodiscard]] static std::optional<const QByteArray> detectUTF8(const QByteArray& data);
    [[nodiscard]] static std::optional<const QByteArray> detectEncoding(const char* buf, qint64 size, FileOffset& skipBytes);
    [[nodiscard]] static std::optional<const QByteArray> getEncodingFromTag(const QByteArray& s, const QByteArray& encodingTag);

    std::optional<const QByteArray> detectEncoding(const QString& fileName);
    QString m_aliasName;
    FileAccess m_fileAccess;
    QString m_tempInputFileName;
    QTemporaryFile m_tempFile; //Created from clipboard content.

    QStringList mErrors;

    bool mFromClipBoard = false;

    class FileData
    {
      private:
        friend SourceData;
        bool mHasBOM = false;

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

        bool preprocess(const QByteArray& encoding, bool removeComments);
        void reset();
        void copyBufFrom(const FileData& src);

        [[nodiscard]] bool isEmpty() const { return mDataSize == 0; }

        [[nodiscard]] bool isText() const { return m_bIsText || isEmpty(); }
        [[nodiscard]] inline bool hasEOLTermiantion() const { return m_bIsText && mHasEOLTermination; }
        [[nodiscard]] inline bool hasBOM() const { return mHasBOM; }

        [[nodiscard]] inline qint64 lineCount() const { return mLineCount; }
        [[nodiscard]] inline qint64 byteCount() const { return mDataSize; }
    };
    FileData m_normalData;
    FileData m_lmppData;
    QByteArray mEncoding = u8"UTF-8";
};

#endif // !SOURCEDATA_H
