/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
*/

/* Features of class SourceData:
- Read a file (from the given URL) or accept data via a string.
- Allocate and free buffers as necessary.
- Run a preprocessor, when specified.
- Run the line-matching preprocessor, when specified.
- Run other preprocessing steps: Uppercase, ignore comments,
                                 remove carriage return, ignore numbers.

Order of operation:
 1. If data was given via a string then save it to a temp file. (see setData())
 2. If the specified file is nonlocal (URL) copy it to a temp file. (TODO revisit this)
 3. If a preprocessor was specified, run the input file through it.
 4. Read the output of the preprocessor.
 5. If Uppercase was specified: Turn the read data to uppercase.
 6. Write the result to a temp file.
 7. If a line-matching preprocessor was specified, run the temp file through it.
 8. Read the output of the line-matching preprocessor.
 9. If ignore numbers was specified, strip the LMPP-output of all numbers.
10. If ignore comments was specified, strip the LMPP-output of comments.

Optimizations: Skip unneeded steps.
*/
#include "SourceData.h"

#include "CommentParser.h"
#include "diff.h"
#include "kdiff3.h"
#include "Logging.h"
#include "Utils.h"

#include <QProcess>
#include <QScopedPointer>
#include <QString>
#include <QTemporaryFile>
#include <QTextCodec>
#include <QTextStream>
#include <QVector>

#include <KLocalizedString>

void SourceData::setupConnections()
{
    connections.push_back(KDiff3App::encodingChanged.connect(boost::bind(&SourceData::setEncoding, this, boost::placeholders::_1)));
}

void SourceData::reset()
{
    mFromClipBoard = false;
    m_pEncoding = nullptr;
    m_fileAccess = FileAccess();
    m_normalData.reset();
    m_lmppData.reset();
    if(!m_tempInputFileName.isEmpty())
    {
        m_tempFile.remove();
        m_tempInputFileName = "";
    }

    mErrors.clear();
}

void SourceData::setFilename(const QString& filename)
{
    mFromClipBoard = false;

    if(filename.isEmpty())
    {
        reset();
    }
    else
    {
        FileAccess fa(filename);
        setFileAccess(fa);
    }
}

bool SourceData::isEmpty() const
{
    return getFilename().isEmpty();
}

bool SourceData::hasData() const
{
    return m_normalData.m_pBuf != nullptr;
}

bool SourceData::isValid() const
{
    return isEmpty() || hasData();
}

void SourceData::setOptions(const QSharedPointer<Options> &pOptions)
{
    m_pOptions = pOptions;
}

QString SourceData::getFilename() const
{
    return m_fileAccess.absoluteFilePath();
}

QString SourceData::getAliasName() const
{
    return m_aliasName.isEmpty() ? m_fileAccess.prettyAbsPath() : m_aliasName;
}

void SourceData::setAliasName(const QString& name)
{
    m_aliasName = name;
}

void SourceData::setFileAccess(const FileAccess& fileAccess)
{
    m_fileAccess = fileAccess;
    m_aliasName = QString();
    if(!m_tempInputFileName.isEmpty())
    {
        m_tempFile.remove();
        m_tempInputFileName = "";
    }

    mErrors.clear();
}
void SourceData::setEncoding(QTextCodec* pEncoding)
{
    m_pEncoding = pEncoding;
}

const QString SourceData::setData(const QString& data)
{
    mErrors.clear();
    // Create a temp file for preprocessing:
    if(m_tempInputFileName.isEmpty())
    {
        FileAccess::createTempFile(m_tempFile);
        m_tempInputFileName = m_tempFile.fileName();
    }
    m_fileAccess = FileAccess(m_tempInputFileName);
    QByteArray ba = QTextCodec::codecForName("UTF-8")->fromUnicode(data);
    bool bSuccess = m_fileAccess.writeFile(ba.constData(), ba.length());
    if(!bSuccess)
    {
        mErrors.append(i18n("Writing clipboard data to temp file failed."));
        return mErrors[0];
    }
    else
    {
        m_aliasName = i18n("From Clipboard");
        mFromClipBoard = true;
        //m_fileAccess = FileAccess(); // Insure m_fileAccess is not valid
    }

    return u8"";
}

const QVector<LineData>* SourceData::getLineDataForDiff() const
{
    if(m_lmppData.m_pBuf == nullptr)
        return m_normalData.m_v.size() > 0 ? &m_normalData.m_v : nullptr;
    else
        return m_lmppData.m_v.size() > 0 ? &m_lmppData.m_v : nullptr;
}

const QVector<LineData>* SourceData::getLineDataForDisplay() const
{
    return m_normalData.m_v.size() > 0 ? &m_normalData.m_v : nullptr;
}

LineRef SourceData::getSizeLines() const
{
    return m_normalData.lineCount();
}

qint64 SourceData::getSizeBytes() const
{
    return m_normalData.byteCount();
}

const char* SourceData::getBuf() const
{
    return m_normalData.m_pBuf;
}

const QString& SourceData::getText() const
{
    return *m_normalData.m_unicodeBuf;
}

bool SourceData::isText() const
{
    return m_normalData.isText() || m_normalData.isEmpty();
}

bool SourceData::isIncompleteConversion() const
{
    return m_normalData.m_bIncompleteConversion;
}

bool SourceData::isFromBuffer() const
{
    return mFromClipBoard;
}

bool SourceData::isBinaryEqualWith(const QSharedPointer<SourceData>& other) const
{
    return m_fileAccess.exists() && other->m_fileAccess.exists() &&
           getSizeBytes() == other->getSizeBytes() &&
           (getSizeBytes() == 0 || memcmp(getBuf(), other->getBuf(), getSizeBytes()) == 0);
}

/*
    Warning: Do not call this function without re-running the comparison or
    otherwise resetting the DiffTextWindows as these store a pointer to the file
    data stored here.
*/
void SourceData::FileData::reset()
{
    if(m_pBuf != nullptr)
    {
        delete[](char*) m_pBuf;
        m_pBuf = nullptr;
    }
    m_v.clear();
    mDataSize = 0;
    mLineCount = 0;
    m_bIsText = false;
    m_bIncompleteConversion = false;
    m_eLineEndStyle = eLineEndStyleUndefined;
}

bool SourceData::FileData::readFile(FileAccess& file)
{
    reset();
    if(file.fileName().isEmpty())
    {
        return true;
    }

    //FileAccess fa(filename);

    if(!file.isNormal())
        return true;

    mDataSize = file.sizeForReading();
    char* pBuf;
    m_pBuf = pBuf = new char[mDataSize + 100]; // Alloc 100 byte extra: Safety hack, not nice but does no harm.
                                               // Some extra bytes at the end of the buffer are needed by
                                               // the diff algorithm. See also GnuDiff::diff_2_files().
    bool bSuccess = file.readFile(pBuf, mDataSize);
    if(!bSuccess)
    {
        delete[] pBuf;
        m_pBuf = nullptr;
        mDataSize = 0;
    }
    else
    {
        //null terminate buffer
        pBuf[mDataSize + 1] = 0;
        pBuf[mDataSize + 2] = 0;
        pBuf[mDataSize + 3] = 0;
        pBuf[mDataSize + 4] = 0;
    }
    return bSuccess;
}

bool SourceData::FileData::readFile(const QString& filename)
{
    reset();
    if(filename.isEmpty())
    {
        return true;
    }

    FileAccess fa(filename);

    if(!fa.isNormal())
        return true;

    mDataSize = fa.sizeForReading();
    char* pBuf;
    m_pBuf = pBuf = new char[mDataSize + 100]; // Alloc 100 byte extra: Safety hack, not nice but does no harm.
                                               // Some extra bytes at the end of the buffer are needed by
                                               // the diff algorithm. See also GnuDiff::diff_2_files().
    bool bSuccess = fa.readFile(pBuf, mDataSize);
    if(!bSuccess)
    {
        delete[] pBuf;
        m_pBuf = nullptr;
        mDataSize = 0;
    }
    return bSuccess;
}

bool SourceData::saveNormalDataAs(const QString& fileName)
{
    return m_normalData.writeFile(fileName);
}

bool SourceData::FileData::writeFile(const QString& filename)
{
    if(filename.isEmpty())
    {
        return true;
    }

    FileAccess fa(filename);
    bool bSuccess = fa.writeFile(m_pBuf, mDataSize);
    return bSuccess;
}

//Depriated
void SourceData::FileData::copyBufFrom(const FileData& src) //TODO: Remove me.
{
    reset();
    char* pBuf;
    mDataSize = src.mDataSize;
    m_pBuf = pBuf = new char[mDataSize + 100];
    Q_ASSERT(src.m_pBuf != nullptr);
    memcpy(pBuf, src.m_pBuf, mDataSize);
}

QTextCodec* SourceData::detectEncoding(const QString& fileName, QTextCodec* pFallbackCodec)
{
    QFile f(fileName);
    if(f.open(QIODevice::ReadOnly))
    {
        char buf[200];
        qint64 size = f.read(buf, sizeof(buf));
        qint64 skipBytes = 0;
        QTextCodec* pCodec = detectEncoding(buf, size, skipBytes);
        if(pCodec)
            return pCodec;
    }
    return pFallbackCodec;
}

void SourceData::readAndPreprocess(QTextCodec* pEncoding, bool bAutoDetectUnicode)
{
    m_pEncoding = pEncoding;
    QTemporaryFile fileIn1, fileOut1;
    QString fileNameIn1;
    QString fileNameOut1;
    QString fileNameIn2;
    QString fileNameOut2;
    //bool bTempFileFromClipboard = !m_fileAccess.isValid();

    // Detect the input for the preprocessing operations
    if(!mFromClipBoard)
    {
        Q_ASSERT(!m_fileAccess.isDir());
        if(!m_fileAccess.isNormal())
        {
            mErrors.append(i18n("%1 is not a normal file.", m_fileAccess.prettyAbsPath()));
            return;
        }

        if(m_fileAccess.isLocal())
        {
            fileNameIn1 = m_fileAccess.absoluteFilePath();
        }
        else // File is not local: create a temporary local copy:
        {
            if(m_tempInputFileName.isEmpty())
            {
                m_fileAccess.createLocalCopy();
                m_tempInputFileName = m_fileAccess.getTempName();
            }

            fileNameIn1 = m_tempInputFileName;
        }
        if(bAutoDetectUnicode)
        {
            m_pEncoding = detectEncoding(fileNameIn1, pEncoding);
        }
    }
    else // The input was set via setData(), probably from clipboard.
    {
        /*
            Happens during initial startup. This means we have nothing to compare
        */
        if(m_tempInputFileName.isEmpty())
            return;

        fileNameIn1 = m_tempInputFileName;
        m_pEncoding = QTextCodec::codecForName("UTF-8");
    }
    QTextCodec* pEncoding1 = m_pEncoding;
    QTextCodec* pEncoding2 = m_pEncoding;

    m_normalData.reset();
    m_lmppData.reset();

    FileAccess faIn(fileNameIn1);
    qint64 fileInSize = faIn.size();

    if(faIn.exists())
    {
        // Run the first preprocessor
        if(m_pOptions->m_PreProcessorCmd.isEmpty())
        {
            // No preprocessing: Read the file directly:
            if(!m_normalData.readFile(faIn))
            {
                mErrors.append(faIn.getStatusText());
                return;
            }
        }
        else
        {
            QTemporaryFile tmpInPPFile;
            QString fileNameInPP = fileNameIn1;

            if(pEncoding1 != m_pOptions->m_pEncodingPP)
            {
                // Before running the preprocessor convert to the format that the preprocessor expects.
                FileAccess::createTempFile(tmpInPPFile);
                fileNameInPP = tmpInPPFile.fileName();
                pEncoding1 = m_pOptions->m_pEncodingPP;
                convertFileEncoding(fileNameIn1, pEncoding, fileNameInPP, pEncoding1);
            }

            QString ppCmd = m_pOptions->m_PreProcessorCmd;
            FileAccess::createTempFile(fileOut1);
            fileNameOut1 = fileOut1.fileName();

            QProcess ppProcess;
            ppProcess.setStandardInputFile(fileNameInPP);
            ppProcess.setStandardOutputFile(fileNameOut1);
            QString program;
            QStringList args;
            QString errorReason = Utils::getArguments(ppCmd, program, args);
            if(errorReason.isEmpty())
            {
                ppProcess.start(program, args);
                ppProcess.waitForFinished(-1);
            }
            else
                errorReason = "\n(" + errorReason + ')';

            bool bSuccess = errorReason.isEmpty() && m_normalData.readFile(fileNameOut1);
            if(fileInSize > 0 && (!bSuccess || m_normalData.byteCount() == 0))
            {
                //Don't fail the preprocessor command if the file can't be read.
                if(!m_normalData.readFile(faIn))
                {
                    mErrors.append(faIn.getStatusText());
                    mErrors.append(i18n("    Temp file is: %1", fileNameIn1));
                    return;
                }
                mErrors.append(
                    i18n("Preprocessing possibly failed. Check this command:\n\n  %1"
                         "\n\nThe preprocessing command will be disabled now.", ppCmd) +
                    errorReason);
                m_pOptions->m_PreProcessorCmd = "";

                pEncoding1 = m_pEncoding;
            }
        }

        if(!m_normalData.preprocess(pEncoding1, false))
        {
            mErrors.append(i18n("File %1 too large to process. Skipping.", fileNameIn1));
            return;
        }
        //exit early for non text data further processing assumes a text file as input
        if(!m_normalData.isText())
            return;

        // LineMatching Preprocessor
        if(!m_pOptions->m_LineMatchingPreProcessorCmd.isEmpty())
        {
            QTemporaryFile tempOut2, fileInPP;
            fileNameIn2 = fileNameOut1.isEmpty() ? fileNameIn1 : fileNameOut1;
            QString fileNameInPP = fileNameIn2;
            pEncoding2 = pEncoding1;
            if(pEncoding2 != m_pOptions->m_pEncodingPP)
            {
                // Before running the preprocessor convert to the format that the preprocessor expects.
                FileAccess::createTempFile(fileInPP);
                fileNameInPP = fileInPP.fileName();
                pEncoding2 = m_pOptions->m_pEncodingPP;
                convertFileEncoding(fileNameIn2, pEncoding1, fileNameInPP, pEncoding2);
            }

            QString ppCmd = m_pOptions->m_LineMatchingPreProcessorCmd;
            FileAccess::createTempFile(tempOut2);
            fileNameOut2 = tempOut2.fileName();
            QProcess ppProcess;
            ppProcess.setStandardInputFile(fileNameInPP);
            ppProcess.setStandardOutputFile(fileNameOut2);
            QString program;
            QStringList args;
            QString errorReason = Utils::getArguments(ppCmd, program, args);
            if(errorReason.isEmpty())
            {
                ppProcess.start(program, args);
                ppProcess.waitForFinished(-1);
            }
            else
                errorReason = "\n(" + errorReason + ')';

            bool bSuccess = errorReason.isEmpty() && m_lmppData.readFile(fileNameOut2);
            if(FileAccess(fileNameIn2).size() > 0 && (!bSuccess || m_lmppData.byteCount() == 0))
            {
                mErrors.append(
                    i18n("The line-matching-preprocessing possibly failed. Check this command:\n\n  %1"
                         "\n\nThe line-matching-preprocessing command will be disabled now.", ppCmd) +
                    errorReason);
                m_pOptions->m_LineMatchingPreProcessorCmd = "";
                if(!m_lmppData.readFile(fileNameIn2))
                {
                    mErrors.append(i18n("Failed to read file: %1", fileNameIn2));
                    return;
                }
            }
        }
        else if(m_pOptions->m_bIgnoreComments || m_pOptions->m_bIgnoreCase)
        {
            // We need a copy of the normal data.
            m_lmppData.copyBufFrom(m_normalData);
        }
    }
    else
    {
        //exit early for nonexistent files
        return;
    }

    if(!m_lmppData.preprocess(pEncoding2, true))
    {
        mErrors.append(i18n("File %1 too large to process. Skipping.", fileNameIn1));
        return;
    }

    Q_ASSERT(m_lmppData.isText());
    //TODO: Needed?
    if(m_lmppData.lineCount() < m_normalData.lineCount())
    {
        // Preprocessing command may result in smaller data buffer so adjust size
        for(qint64 i = m_lmppData.lineCount(); i < m_normalData.lineCount(); ++i)
        { // Set all empty lines to point to the end of the buffer.
            m_lmppData.m_v.push_back(LineData(m_lmppData.m_unicodeBuf, m_lmppData.m_unicodeBuf->length()));
        }

        m_lmppData.mLineCount = m_normalData.lineCount();
    }

    // Ignore comments
    if(m_pOptions->m_bIgnoreComments && hasData())
    {
        qint64 vSize = std::min(m_normalData.lineCount(), m_lmppData.lineCount());
        Q_ASSERT(vSize < TYPE_MAX(qint32));
        //Perform explcit cast to insure well defined behavior comparing 32-bit to a 64-bit value
        for(qint32 i = 0; (qint64)i < vSize; ++i)
        {
            m_normalData.m_v[i].setPureComment(m_lmppData.m_v[i].isPureComment());
        }
    }
}

/** Prepare the linedata vector for every input line.*/
bool SourceData::FileData::preprocess(QTextCodec* pEncoding, bool removeComments)
{
    if(m_pBuf == nullptr)
        return true;

    if(pEncoding == nullptr)
        return false;

    QString line;
    QChar curChar;
    LineCount lineCount = 0;
    qint64 lastOffset = 0;
    qint64 skipBytes = 0;
    QScopedPointer<CommentParser> parser(new DefaultCommentParser());

    // detect line end style
    QVector<e_LineEndStyle> vOrigDataLineEndStyle;
    m_eLineEndStyle = eLineEndStyleUndefined;

    QTextCodec* pCodec = detectEncoding(m_pBuf, mDataSize, skipBytes);
    if(pCodec != pEncoding)
        skipBytes = 0;

    if(mDataSize - skipBytes > TYPE_MAX(QtNumberType))
        return false;

    const QByteArray ba = QByteArray::fromRawData(m_pBuf + skipBytes, (int)(mDataSize - skipBytes));
    QTextStream ts(ba, QIODevice::ReadOnly); //Don't use text mode we need to see the actual line ending.
    ts.setCodec(pEncoding);
    ts.setAutoDetectUnicode(false);

    m_bIncompleteConversion = false;
    m_unicodeBuf->clear();
    Q_ASSERT(m_unicodeBuf->length() == 0);

    while(!ts.atEnd())
    {
        line.clear();
        if(lineCount >= TYPE_MAX(LineCount) - 5)
        {
            m_v.clear();
            return false;
        }

        ts >> curChar;

        quint32 firstNonwhite = 0;
        //QTextStream::readLine doesn't tell us about line endings.
        while(curChar != '\n' && curChar != '\r')
        {
            if(curChar.isNull() || curChar.isNonCharacter())
            {
                m_v.clear();
                return true;
            }

            if(curChar == QChar::ReplacementCharacter)
                m_bIncompleteConversion = true;

            if(!curChar.isSpace() && firstNonwhite == 0)
                firstNonwhite = line.length();

            line.append(curChar);

            if(ts.atEnd())
                break;

            ts >> curChar;
        }

        ++lineCount;

        switch(curChar.unicode())
        {
            case '\n':
                vOrigDataLineEndStyle.push_back(eLineEndStyleUnix);
                break;
            case '\r':
                if(lastOffset < mDataSize)
                {
                    //workaround for lack of peak API in QTextStream.
                    qint64 j;
                    for(j = 0; j < 4 && lastOffset + j < mDataSize; ++j)
                    {
                        if(m_pBuf[lastOffset + j] != '\0')
                            break;
                    }

                    if(m_pBuf[lastOffset + j] == '\n')
                    {
                        ts >> curChar;
                        vOrigDataLineEndStyle.push_back(eLineEndStyleDos);
                        lastOffset = ts.pos();
                        break;
                    }
                }

                //old mac style ending.
                vOrigDataLineEndStyle.push_back(eLineEndStyleUndefined);
                break;
        }
        parser->processLine(line);
        if(removeComments)
            parser->removeComment(line);

        //kdiff3 internally uses only unix style endings for simplicity.
        m_v.push_back(LineData(m_unicodeBuf, lastOffset, line.length(), firstNonwhite, parser->isPureComment()));
        m_unicodeBuf->append(line).append('\n');

        lastOffset = m_unicodeBuf->length();
    }

    m_v.push_back(LineData(m_unicodeBuf, lastOffset));
    Q_ASSERT(m_v.size() < 2 || m_v[m_v.size() - 1].getOffset() != m_v[m_v.size() - 2].getOffset());

    m_bIsText = true;

    if(!vOrigDataLineEndStyle.isEmpty())
        m_eLineEndStyle = vOrigDataLineEndStyle[0];

    mLineCount = lineCount;
    return true;
}

// Convert the input file from input encoding to output encoding and write it to the output file.
bool SourceData::convertFileEncoding(const QString& fileNameIn, QTextCodec* pCodecIn,
                                     const QString& fileNameOut, QTextCodec* pCodecOut)
{
    QFile in(fileNameIn);
    if(!in.open(QIODevice::ReadOnly))
        return false;
    QTextStream inStream(&in);
    inStream.setCodec(pCodecIn);
    inStream.setAutoDetectUnicode(false);

    QFile out(fileNameOut);
    if(!out.open(QIODevice::WriteOnly))
        return false;
    QTextStream outStream(&out);
    outStream.setCodec(pCodecOut);

    QString data = inStream.readAll();
    outStream << data;

    return true;
}

QTextCodec* SourceData::getEncodingFromTag(const QByteArray& s, const QByteArray& encodingTag)
{
    int encodingPos = s.indexOf(encodingTag);
    if(encodingPos >= 0)
    {
        int apostrophPos = s.indexOf('"', encodingPos + encodingTag.length());
        int apostroph2Pos = s.indexOf('\'', encodingPos + encodingTag.length());
        char apostroph = '"';
        if(apostroph2Pos >= 0 && (apostrophPos < 0 || apostroph2Pos < apostrophPos))
        {
            apostroph = '\'';
            apostrophPos = apostroph2Pos;
        }

        int encodingEnd = s.indexOf(apostroph, apostrophPos + 1);
        if(encodingEnd >= 0) // e.g.: <meta charset="utf-8"> or <?xml version="1.0" encoding="ISO-8859-1"?>
        {
            QByteArray encoding = s.mid(apostrophPos + 1, encodingEnd - (apostrophPos + 1));
            return QTextCodec::codecForName(encoding);
        }
        else // e.g.: <meta http-equiv="Content-Type" content="text/html; charset=utf-8">
        {
            QByteArray encoding = s.mid(encodingPos + encodingTag.length(), apostrophPos - (encodingPos + encodingTag.length()));
            return QTextCodec::codecForName(encoding);
        }
    }
    return nullptr;
}

QTextCodec* SourceData::detectEncoding(const char* buf, qint64 size, qint64& skipBytes)
{
    if(size >= 2)
    {
        if(buf[0] == '\xFF' && buf[1] == '\xFE')
        {
            skipBytes = 2;
            return QTextCodec::codecForName("UTF-16LE");
        }

        if(buf[0] == '\xFE' && buf[1] == '\xFF')
        {
            skipBytes = 2;
            return QTextCodec::codecForName("UTF-16BE");
        }
    }
    if(size >= 3)
    {
        if(buf[0] == '\xEF' && buf[1] == '\xBB' && buf[2] == '\xBF')
        {
            skipBytes = 3;
            return QTextCodec::codecForName("UTF-8");
        }
    }
    skipBytes = 0;
    QByteArray s;
    /*
        We don't need the whole file here just the header.
]    */
    if(size <= 5000)
        s = QByteArray(buf, (int)size);
    else
        s = QByteArray(buf, 5000);

    int xmlHeaderPos = s.indexOf("<?xml");
    if(xmlHeaderPos >= 0)
    {
        int xmlHeaderEnd = s.indexOf("?>", xmlHeaderPos);
        if(xmlHeaderEnd >= 0)
        {
            QTextCodec* pCodec = getEncodingFromTag(s.mid(xmlHeaderPos, xmlHeaderEnd - xmlHeaderPos), "encoding=");
            if(pCodec)
                return pCodec;
        }
    }
    else // HTML
    {
        int metaHeaderPos = s.indexOf("<meta");
        while(metaHeaderPos >= 0)
        {
            int metaHeaderEnd = s.indexOf(">", metaHeaderPos);
            if(metaHeaderEnd >= 0)
            {
                QTextCodec* pCodec = getEncodingFromTag(s.mid(metaHeaderPos, metaHeaderEnd - metaHeaderPos), "charset=");
                if(pCodec)
                    return pCodec;

                metaHeaderPos = s.indexOf("<meta", metaHeaderEnd);
            }
            else
                break;
        }
    }
    return nullptr;
}
