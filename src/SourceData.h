/***************************************************************************
 *   Copyright (C) 2003-2007 by Joachim Eibl <joachim.eibl at gmx.de>      *
 *   Copyright (C) 2018 Michael Reeves reeves.87@gmail.com                 *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef SOURCEDATA_H
#define SOURCEDATA_H
#include <QTextCodec>
#include <QString>

#include "options.h"
#include "fileaccess.h"
#include "gnudiff_diff.h"

class LineData;
class SourceData
{
  public:
    SourceData();
    ~SourceData();

    void setOptions(Options* pOptions);

    LineRef getSizeLines() const;
    qint64 getSizeBytes() const;
    const char* getBuf() const;
    const QString& getText() const;
    const LineData* getLineDataForDisplay() const;
    const LineData* getLineDataForDiff() const;

    void setFilename(const QString& filename);
    void setFileAccess(const FileAccess& fileAccess);
    void setEncoding(QTextCodec* pEncoding);
    //FileAccess& getFileAccess();
    QString getFilename();
    void setAliasName(const QString& name);
    QString getAliasName();
    bool isEmpty();                // File was set
    bool hasData();                // Data was readable
    bool isText();                 // is it pure text (vs. binary data)
    bool isIncompleteConversion(); // true if some replacement characters were found
    bool isFromBuffer();           // was it set via setData() (vs. setFileAccess() or setFilename())
    QStringList setData(const QString& data);
    bool isValid(); // Either no file is specified or reading was successful

    // Returns a list of error messages if anything went wrong
    QStringList readAndPreprocess(QTextCodec* pEncoding, bool bAutoDetectUnicode);
    bool saveNormalDataAs(const QString& fileName);

    bool isBinaryEqualWith(const SourceData& other) const;

    void reset();

    QTextCodec* getEncoding() const { return m_pEncoding; }
    e_LineEndStyle getLineEndStyle() const { return m_normalData.m_eLineEndStyle; }

  private:
    QTextCodec* detectEncoding(const QString& fileName, QTextCodec* pFallbackCodec);
    QString m_aliasName;
    FileAccess m_fileAccess;
    Options* m_pOptions;
    QString m_tempInputFileName;
    QTemporaryFile m_tempFile; //Created from clipboard content.

    class FileData
    {
      private:
        friend SourceData;
        const char* m_pBuf = nullptr;
        qint64 m_size = 0;
        qint64 m_vSize = 0; // Nr of lines in m_pBuf1 and size of m_v1, m_dv12 and m_dv13
        QString m_unicodeBuf;
        QVector<LineData> m_v;
        bool m_bIsText = false;
        bool m_bIncompleteConversion = false;
        e_LineEndStyle m_eLineEndStyle = eLineEndStyleUndefined;

      public:
        ~FileData() { reset(); }
        bool readFile(const QString& filename);
        bool writeFile(const QString& filename);
        bool preprocess(bool bPreserveCR, QTextCodec* pEncoding);
        void reset();
        void removeComments();
        void copyBufFrom(const FileData& src);

        void checkLineForComments(
            const QChar* p,          // pointer to start of buffer
            int& i,                  // index of current position (in, out)
            int size,                // size of buffer
            bool& bWhite,            // false if this line contains nonwhite characters (in, out)
            bool& bCommentInLine,    // true if any comment is within this line (in, out)
            bool& bStartsOpenComment // true if the line ends within an comment (out)
        );

        bool isEmpty() { return m_size == 0; }

        bool isText() { return m_bIsText; }
    };
    FileData m_normalData;
    FileData m_lmppData;
    QTextCodec* m_pEncoding;
};

#endif // !SOURCEDATA_H
