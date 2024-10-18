// clang-format off
/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
// clang-format on

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
#include "compat.h"
#include "diff.h"
#include "EncodedDataStream.h"
#include "LineRef.h"
#include "Logging.h"
#include "Utils.h"

#include <algorithm>         // for min
#include <memory>
#include <optional>
#include <vector>            // for vector

#include <QtGlobal>

#include <QByteArray>
#include <QProcess>
#include <QString>
#include <QTemporaryFile>

extern std::unique_ptr<Options> gOptions;

void SourceData::reset()
{
    mFromClipBoard = false;
    mEncoding = QByteArray();
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
    if(filename.isEmpty())
    {
        reset();
    }
    else
    {
        setFileAccess(FileAccess(filename));
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
    mFromClipBoard = false;

    m_fileAccess = fileAccess;
    m_aliasName = QString();
    if(!m_tempInputFileName.isEmpty())
    {
        m_tempFile.remove();
        m_tempInputFileName = "";
    }

    mErrors.clear();
}

void SourceData::setEncoding(const QByteArray& encoding)
{
    mEncoding = encoding;
}

void SourceData::setData(const QString& data)
{
    mErrors.clear();
    // Create a temp file for preprocessing:
    if(m_tempInputFileName.isEmpty())
    {
        FileAccess::createTempFile(m_tempFile);
        m_tempInputFileName = m_tempFile.fileName();
    }
    m_fileAccess = FileAccess(m_tempInputFileName);
    auto fromString = QStringEncoder(QStringEncoder::Utf8);
    QByteArray ba = fromString(data);
    bool bSuccess = m_fileAccess.writeFile(ba.constData(), ba.length());
    if(!bSuccess)
    {
        mErrors.append(i18n("Writing clipboard data to temp file failed."));
        return;
    }
    else
    {
        m_aliasName = i18n("From Clipboard");
        mFromClipBoard = true;
    }
}

const std::shared_ptr<LineDataVector>& SourceData::getLineDataForDiff() const
{
    if(m_lmppData.m_pBuf == nullptr)
    {
        return m_normalData.m_v;
    }
    else
    {
        return m_lmppData.m_v;
    }
}

const std::shared_ptr<LineDataVector>& SourceData::getLineDataForDisplay() const
{
    return m_normalData.m_v;
}

LineType SourceData::getSizeLines() const
{
    return SafeInt<LineType>(m_normalData.lineCount());
}

qint64 SourceData::getSizeBytes() const
{
    return m_normalData.byteCount();
}

const char* SourceData::getBuf() const
{
    return m_normalData.m_pBuf.get();
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
    m_pBuf.reset();
    m_v->clear();
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

    if(!file.isNormal())
        return true;

    mDataSize = file.sizeForReading();
    /*
        If the extra bytes are removed an unknown heap currption issue is triggered in the
        diff code. I don't have time to track this down to its true root cause.
    */
    m_pBuf = std::make_unique<char[]>(mDataSize + 100); // Alloc 100 byte extra: Safety hack, not nice but does no harm.
                                                        // Some extra bytes at the end of the buffer are needed by
                                                        // the diff algorithm. See also GnuDiff::diff_2_files().
    bool bSuccess = file.readFile(m_pBuf.get(), mDataSize);
    if(!bSuccess)
    {
        m_pBuf = nullptr;
        mDataSize = 0;
    }
    else
    {
        //null terminate buffer
        m_pBuf[mDataSize + 1] = 0;
        m_pBuf[mDataSize + 2] = 0;
        m_pBuf[mDataSize + 3] = 0;
        m_pBuf[mDataSize + 4] = 0;
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
    return readFile(fa);
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
    bool bSuccess = fa.writeFile(m_pBuf.get(), mDataSize);
    return bSuccess;
}

//Deprecated
void SourceData::FileData::copyBufFrom(const FileData& src) //TODO: Remove me.
{
    reset();
    mDataSize = src.mDataSize;
    m_pBuf = std::make_unique<char[]>(mDataSize + 100);
    assert(src.m_pBuf != nullptr);
    memcpy(m_pBuf.get(), src.m_pBuf.get(), mDataSize);
}

std::optional<const QByteArray> SourceData::detectEncoding(const QString& fileName)
{
    QFile f(fileName);
    if(f.open(QIODevice::ReadOnly))
    {
        char buf[400];

        qint64 size = f.read(buf, sizeof(buf));
        return detectEncoding(buf, size);
    }
    return {};
}

void SourceData::readAndPreprocess(const QByteArray& encoding, bool bAutoDetect)
{
    QTemporaryFile fileIn1, fileOut1;
    QString fileNameIn1;
    QString fileNameOut1;
    QString fileNameIn2;
    QString fileNameOut2;

    // Detect the input for the preprocessing operations
    if(!mFromClipBoard)
    {
        //Routine result of directory compare finding a file that isn't in all locations.
        if(!m_fileAccess.isValid()) return;

        mEncoding = encoding;
        if(mEncoding.isEmpty())
            mEncoding = u8"UTF-8";

        assert(!m_fileAccess.exists() || !m_fileAccess.isDir());
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
    }
    else // The input was set via setData(), probably from clipboard.
    {
        /*
            Used to happen during early startup this is now a bug.
        */
        assert(!m_tempInputFileName.isEmpty());

        fileNameIn1 = m_tempInputFileName;
        mEncoding = "UTF-8";
    }

    if(bAutoDetect)
    {
        mEncoding = detectEncoding(fileNameIn1).value_or(encoding);
    }

    QByteArray pEncoding1 = getEncoding();
    QByteArray pEncoding2 = getEncoding();
    const QString overSizedFile = i18nc("Error message. %1 = filepath", "File %1 too large to process. Skipping.", fileNameIn1);

    m_normalData.reset();
    m_lmppData.reset();

    FileAccess faIn(fileNameIn1);
    qint64 fileInSize = faIn.size();

    if(faIn.exists() && !faIn.isBrokenLink())
    {
        try
        {
            // Run the first preprocessor
            if(gOptions->m_PreProcessorCmd.isEmpty())
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
                unsigned char b;
                //Don't fail the preprocessor command if the file can't be read.
                if(!faIn.readFile(&b, 1))
                {
                    mErrors.append(faIn.getStatusText());
                    mErrors.append(i18n("    Temp file is: %1", fileNameIn1));
                    return;
                }

                QTemporaryFile tmpInPPFile;
                QString fileNameInPP = fileNameIn1;

                if(pEncoding1 != gOptions->mEncodingPP)
                {
                    // Before running the preprocessor convert to the format that the preprocessor expects.
                    FileAccess::createTempFile(tmpInPPFile);
                    fileNameInPP = tmpInPPFile.fileName();
                    pEncoding1 = gOptions->mEncodingPP;
                    convertFileEncoding(fileNameIn1, encoding, fileNameInPP, pEncoding1);
                }

                QString ppCmd = gOptions->m_PreProcessorCmd;
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
                    mErrors.append(
                        i18n("Preprocessing possibly failed. Check this command:\n\n  %1"
                             "\n\nThe preprocessing command will be disabled now.",
                             ppCmd) +
                        errorReason);
                    gOptions->m_PreProcessorCmd = "";

                    pEncoding1 = getEncoding();
                }
            }
        }
        catch(const std::bad_alloc&)
        {
            m_normalData.reset();
            mErrors.append(overSizedFile);
            return;
        }

        if(!m_normalData.preprocess(pEncoding1, false))
        {
            mErrors.append(overSizedFile);
            return;
        }
        //exit early for non text data further processing assumes a text file as input
        if(!m_normalData.isText())
            return;

        // LineMatching Preprocessor
        if(!gOptions->m_LineMatchingPreProcessorCmd.isEmpty())
        {
            QTemporaryFile tempOut2, fileInPP;
            fileNameIn2 = fileNameOut1.isEmpty() ? fileNameIn1 : fileNameOut1;
            QString fileNameInPP = fileNameIn2;
            pEncoding2 = pEncoding1;
            if(pEncoding2 != gOptions->mEncodingPP)
            {
                // Before running the preprocessor convert to the format that the preprocessor expects.
                FileAccess::createTempFile(fileInPP);
                fileNameInPP = fileInPP.fileName();
                pEncoding2 = gOptions->mEncodingPP;
                convertFileEncoding(fileNameIn2, pEncoding1, fileNameInPP, pEncoding2);
            }

            QString ppCmd = gOptions->m_LineMatchingPreProcessorCmd;
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
                gOptions->m_LineMatchingPreProcessorCmd = "";
                if(!m_lmppData.readFile(fileNameIn2))
                {
                    mErrors.append(i18nc("Read error message. %1 = filepath", "Failed to read file: %1", fileNameIn2));
                    return;
                }
            }
        }
        else if(gOptions->ignoreComments() || gOptions->m_bIgnoreCase)
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
        mErrors.append(overSizedFile);
        return;
    }

    assert(m_lmppData.isText());
    //TODO: Needed?
    if(m_lmppData.lineCount() < m_normalData.lineCount())
    {
        // Preprocessing command may result in smaller data buffer so adjust size
        for(qint64 i = m_lmppData.lineCount(); i < m_normalData.lineCount(); ++i)
        { // Set all empty lines to point to the end of the buffer.
            m_lmppData.m_v->push_back(LineData(m_lmppData.m_unicodeBuf, m_lmppData.m_unicodeBuf->length()));
        }

        m_lmppData.mLineCount = m_normalData.lineCount();
    }

    // Ignore comments
    if(gOptions->ignoreComments() && hasData())
    {
        qint64 vSize = std::min(m_normalData.lineCount(), m_lmppData.lineCount());

        for(qint64 i = 0; i < vSize; ++i)
        {
            //TODO: Phase this out. We should not be messing with these flags outside the parser.
            (*m_normalData.m_v)[i].setPureComment((*m_lmppData.m_v)[i].isPureComment());
            (*m_normalData.m_v)[i].setSkipable((*m_lmppData.m_v)[i].isSkipable());
        }
    }
}

/** Prepare the linedata vector for every input line.*/
bool SourceData::FileData::preprocess(const QByteArray& encoding, bool removeComments)
{
    if(m_pBuf == nullptr)
        return true;

    QString line;
    QChar curChar, prevChar = '\0';
    LineType lines = 0;
    qsizetype lastOffset = 0;
    std::unique_ptr<CommentParser> parser(new DefaultCommentParser());

    // detect line end style
    std::vector<e_LineEndStyle> vOrigDataLineEndStyle;
    m_eLineEndStyle = eLineEndStyleUndefined;

    QByteArray pCodec = detectEncoding(m_pBuf.get(), mDataSize).value_or(encoding);

    if(mDataSize > limits<qint32>::max())
    {
        reset();
        return false;
    }

    try
    {
        const QByteArray ba = QByteArray::fromRawData(m_pBuf.get(), (qsizetype)(mDataSize));
        EncodedDataStream ds(ba);

        ds.setEncoding(encoding);
        mHasBOM = ds.hasBOM();
        m_bIncompleteConversion = false;
        m_unicodeBuf->clear();

        assert(m_unicodeBuf->length() == 0);

        mHasEOLTermination = false;
        while(!ds.atEnd())
        {
            line.clear();
            if(lines >= limits<LineType>::max() - 5)
            {
                reset();
                return false;
            }

            prevChar = curChar;
            ds.readChar(curChar);

            qsizetype firstNonwhite = 0;
            bool foundNonWhite = false;

            while(curChar != '\n' && curChar != '\r')
            {
                if(curChar.isNull() || curChar.isNonCharacter())
                {
                    m_v->clear();
                    return true;
                }

                if(curChar == QChar::ReplacementCharacter)
                    m_bIncompleteConversion = true;

                line.append(curChar);
                if(!curChar.isSpace() && !foundNonWhite)
                {
                    firstNonwhite = line.length();
                    foundNonWhite = true;
                }

                if(ds.atEnd())
                    break;

                prevChar = curChar;
                ds.readChar(curChar);
            }

            switch(curChar.unicode())
            {
                case '\n':
                    vOrigDataLineEndStyle.push_back(eLineEndStyleUnix);
                    break;
                case '\r':
                    if((FileOffset)lastOffset < mDataSize)
                    {
                        QChar nextChar;
                        quint64 i = ds.peekChar(nextChar);
                        if(i == 0)
                            break;

                        if(nextChar == '\n')
                        {
                            prevChar = curChar;
                            ds.readChar(curChar);
                            vOrigDataLineEndStyle.push_back(eLineEndStyleDos);
                            break;
                        }
                    }

                    //old mac style ending.
                    vOrigDataLineEndStyle.push_back(eLineEndStyleOldMac);
                    break;
            }
            parser->processLine(line);
            if(removeComments)
                parser->removeComment(line);
            //Qt6 intrudes 64bit sizes
            if(line.size() >= limits<LineType>::max())
            {
                reset();
                return false;
            }

            ++lines;
            m_v->push_back(LineData(m_unicodeBuf, lastOffset, line.length(), firstNonwhite, parser->isSkipable(), parser->isPureComment()));
            //The last line may not have an EOL mark. In that case don't add one to our buffer.
            m_unicodeBuf->append(line);
            if(curChar == '\n' || curChar == '\r' || prevChar == '\r')
            {
                //kdiff3 internally uses only unix style endings for simplicity.
                m_unicodeBuf->append('\n');
            }

            assert(m_unicodeBuf->length() != lastOffset);
            lastOffset = m_unicodeBuf->length();
        }

        /*
            Process trailing new line as if there were a blank non-terminated line after it.
            But do nothing to the data buffer since this is a phantom line needed for internal purposes.
        */
        if(curChar == '\n' || curChar == '\r')
        {
            mHasEOLTermination = true;
            ++lines;

            parser->processLine("");
            m_v->push_back(LineData(m_unicodeBuf, lastOffset, 0, 0, parser->isSkipable(), parser->isPureComment()));
        }

        m_v->push_back(LineData(m_unicodeBuf, lastOffset));

        m_bIsText = true;

        if(!vOrigDataLineEndStyle.empty())
            m_eLineEndStyle = vOrigDataLineEndStyle[0];

        mLineCount = lines;
        return true;
    }
    catch(const std::bad_alloc&)
    {
        reset();
        return false;
    }
}

// Convert the input file from input encoding to output encoding and write it to the output file.
bool SourceData::convertFileEncoding(const QString& fileNameIn, const QByteArray& pCodecIn,
                                     const QString& fileNameOut, const QByteArray& pCodecOut)
{
    QFile in(fileNameIn);
    if(!in.open(QIODevice::ReadOnly))
        return false;
    EncodedDataStream inStream(&in);
    inStream.setEncoding(pCodecIn);

    QFile out(fileNameOut);
    if(!out.open(QIODevice::WriteOnly))
        return false;
    EncodedDataStream outStream(&out);
    outStream.setEncoding(pCodecOut);

    QString data;
    while(!inStream.atEnd())
    {
        QChar c;
        inStream.readChar(c);
        data += c;
    }
    outStream << data;

    return true;
}

std::optional<const QByteArray> SourceData::getEncodingFromTag(const QByteArray& s, const QByteArray& encodingTag)
{
    qsizetype encodingPos = s.indexOf(encodingTag);
    if(encodingPos >= 0)
    {
        qsizetype apostrophPos = s.indexOf('"', encodingPos + encodingTag.length());
        qsizetype apostroph2Pos = s.indexOf('\'', encodingPos + encodingTag.length());
        char apostroph = '"';
        if(apostroph2Pos >= 0 && (apostrophPos < 0 || apostroph2Pos < apostrophPos))
        {
            apostroph = '\'';
            apostrophPos = apostroph2Pos;
        }

        qsizetype encodingEnd = s.indexOf(apostroph, apostrophPos + 1);
        if(encodingEnd >= 0) // e.g.: <meta charset="utf-8"> or <?xml version="1.0" encoding="ISO-8859-1"?>
        {
            QByteArray encoding = s.mid(apostrophPos + 1, encodingEnd - (apostrophPos + 1));
            QStringDecoder decoder = QStringDecoder(encoding);
            if(decoder.isValid())
                return encoding.toUpper();
        }
        else // e.g.: <meta http-equiv="Content-Type" content="text/html; charset=utf-8">
        {
            QByteArray encoding = s.mid(encodingPos + encodingTag.length(), apostrophPos - (encodingPos + encodingTag.length()));
            QStringDecoder decoder = QStringDecoder(encoding);
            if(decoder.isValid())
                return encoding.toUpper();
        }
    }
    return {};
}

std::optional<const QByteArray> SourceData::detectEncoding(const char* buf, qint64 size)
{
    std::optional<QStringConverter::Encoding> qtEnum = QStringConverter::encodingForData(buf, size);
    if(qtEnum.has_value())
    {
        QByteArray encoding = QByteArray(QStringConverter::nameForEncoding(qtEnum.value())).toUpper();
        //Only unambigious encodings are returned by QStringConverter::encodingForData this means UTF-8 with BOM.
        if(encoding == "UTF-8")
            encoding = "UTF-8-BOM";

        return encoding;
    }

    QByteArray s;
    /*
        We don't need the whole file here just the header.
    */
    if(size <= 5000)
        s = QByteArray(buf, (qsizetype)size);
    else
        s = QByteArray(buf, 5000);

    qsizetype xmlHeaderPos = s.indexOf("<?xml");
    if(xmlHeaderPos >= 0)
    {
        qsizetype xmlHeaderEnd = s.indexOf("?>", xmlHeaderPos);
        if(xmlHeaderEnd >= 0)
        {
            std::optional<const char*> encoding = getEncodingFromTag(s.mid(xmlHeaderPos, xmlHeaderEnd - xmlHeaderPos), "encoding=");
            if(encoding.has_value())
                return encoding;
        }
    }
    else // HTML
    {
        qsizetype metaHeaderPos = s.indexOf("<meta");
        while(metaHeaderPos >= 0)
        {
            qsizetype metaHeaderEnd = s.indexOf(">", metaHeaderPos);
            if(metaHeaderEnd >= 0)
            {
                std::optional<const char*> encoding = getEncodingFromTag(s.mid(metaHeaderPos, metaHeaderEnd - metaHeaderPos), "charset=");
                if(encoding.has_value())
                    return encoding;

                metaHeaderPos = s.indexOf("<meta", metaHeaderEnd);
            }
            else
                break;
        }
    }
    //Attempt to detect non-bom UTF8. This is a very common encoding.
    return detectUTF8(s);
}

std::optional<const QByteArray> SourceData::detectUTF8(const QByteArray& data)
{
    QStringDecoder decoder = QStringDecoder(QStringDecoder::Utf8);
    QString s = decoder(data);

    if(!decoder.hasError())
        for (qint32 i = 0; i < data.size(); i++)
            if ((unsigned)data.at(i) > 127)
                return "UTF-8";

    return {};
}
