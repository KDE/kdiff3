/***************************************************************************
 *   Copyright (C) 2003-2007 by Joachim Eibl <joachim.eibl at gmx.de>      *
 *   Copyright (C) 2018 Michael Reeves reeves.87@gmail.com                 *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

/* Features of class SourceData:
- Read a file (from the given URL) or accept data via a string.
- Allocate and free buffers as necessary.
- Run a preprocessor, when specified.
- Run the line-matching preprocessor, when specified.
- Run other preprocessing steps: Uppercase, ignore comments,
                                 remove carriage return, ignore numbers.

Order of operation:
 1. If data was given via a string then save it to a temp file. (see setData())
 2. If the specified file is nonlocal (URL) copy it to a temp file.
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

#include "Utils.h"
#include "diff.h"
#include "Logging.h"

#include <QProcess>
#include <QString>
#include <QTemporaryFile>
#include <QTextCodec>
#include <QTextStream>
#include <QVector>

#include <KLocalizedString>

SourceData::SourceData()
{
    m_pOptions = nullptr;
    reset();
}

SourceData::~SourceData()
{
    reset();
}

void SourceData::reset()
{
    m_pEncoding = nullptr;
    m_fileAccess = FileAccess();
    m_normalData.reset();
    m_lmppData.reset();
    if(!m_tempInputFileName.isEmpty())
    {
        m_tempFile.remove();
        m_tempInputFileName = "";
    }
}

void SourceData::setFilename(const QString& filename)
{
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

bool SourceData::isEmpty()
{
    return getFilename().isEmpty();
}

bool SourceData::hasData()
{
    return m_normalData.m_pBuf != nullptr;
}

bool SourceData::isValid()
{
    return isEmpty() || hasData();
}

void SourceData::setOptions(Options* pOptions)
{
    m_pOptions = pOptions;
}

QString SourceData::getFilename()
{
    return m_fileAccess.absoluteFilePath();
}

QString SourceData::getAliasName()
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
}
void SourceData::setEncoding(QTextCodec* pEncoding)
{
    m_pEncoding = pEncoding;
}

QStringList SourceData::setData(const QString& data)
{
    QStringList errors;

    // Create a temp file for preprocessing:
    if(m_tempInputFileName.isEmpty())
    {
        FileAccess::createTempFile(m_tempFile);
        m_tempInputFileName = m_tempFile.fileName();
    }

    FileAccess f(m_tempInputFileName);
    QByteArray ba = QTextCodec::codecForName("UTF-8")->fromUnicode(data);
    bool bSuccess = f.writeFile(ba.constData(), ba.length());
    if(!bSuccess)
    {
        errors.append(i18n("Writing clipboard data to temp file failed."));
    }
    else
    {
        m_aliasName = i18n("From Clipboard");
        m_fileAccess = FileAccess(""); // Effect: m_fileAccess.isValid() is false
    }

    return errors;
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
    return (LineRef)(m_normalData.m_vSize);
}

qint64 SourceData::getSizeBytes() const
{
    return m_normalData.m_size;
}

const char* SourceData::getBuf() const
{
    return m_normalData.m_pBuf;
}

const QString& SourceData::getText() const
{
    return m_normalData.m_unicodeBuf;
}

bool SourceData::isText()
{
    return m_normalData.isText();
}

bool SourceData::isIncompleteConversion()
{
    return m_normalData.m_bIncompleteConversion;
}

bool SourceData::isFromBuffer()
{
    return !m_fileAccess.isValid();
}

bool SourceData::isBinaryEqualWith(const SourceData& other) const
{
    return m_fileAccess.exists() && other.m_fileAccess.exists() &&
           getSizeBytes() == other.getSizeBytes() &&
           (getSizeBytes() == 0 || memcmp(getBuf(), other.getBuf(), getSizeBytes()) == 0);
}

void SourceData::FileData::reset()
{
    delete[](char*) m_pBuf;
    m_pBuf = nullptr;
    m_v.clear();
    m_size = 0;
    m_vSize = 0;
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

    m_size = file.sizeForReading();
    char* pBuf;
    m_pBuf = pBuf = new char[m_size + 100]; // Alloc 100 byte extra: Safety hack, not nice but does no harm.
                                            // Some extra bytes at the end of the buffer are needed by
                                            // the diff algorithm. See also GnuDiff::diff_2_files().
    bool bSuccess = file.readFile(pBuf, m_size);
    if(!bSuccess)
    {
        delete[] pBuf;
        m_pBuf = nullptr;
        m_size = 0;
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

    m_size = fa.sizeForReading();
    char* pBuf;
    m_pBuf = pBuf = new char[m_size + 100]; // Alloc 100 byte extra: Safety hack, not nice but does no harm.
                                            // Some extra bytes at the end of the buffer are needed by
                                            // the diff algorithm. See also GnuDiff::diff_2_files().
    bool bSuccess = fa.readFile(pBuf, m_size);
    if(!bSuccess)
    {
        delete[] pBuf;
        m_pBuf = nullptr;
        m_size = 0;
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
    bool bSuccess = fa.writeFile(m_pBuf, m_size);
    return bSuccess;
}

void SourceData::FileData::copyBufFrom(const FileData& src)
{
    reset();
    char* pBuf;
    m_size = src.m_size;
    m_pBuf = pBuf = new char[m_size + 100];
    Q_ASSERT(src.m_pBuf != nullptr);
    memcpy(pBuf, src.m_pBuf, m_size);
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

QStringList SourceData::readAndPreprocess(QTextCodec* pEncoding, bool bAutoDetectUnicode)
{
    m_pEncoding = pEncoding;
    QTemporaryFile fileIn1, fileOut1;
    QString fileNameIn1;
    QString fileNameOut1;
    QString fileNameIn2;
    QString fileNameOut2;
    QStringList errors;

    if(m_fileAccess.isValid() && !m_fileAccess.isNormal())
    {
        errors.append(i18n("%1 is not a normal file.", m_fileAccess.prettyAbsPath()));
        return errors;
    }

    bool bTempFileFromClipboard = !m_fileAccess.isValid();

    // Detect the input for the preprocessing operations
    if(!bTempFileFromClipboard)
    {
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
                errors.append(faIn.getStatusText());
                return errors;
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
            if(fileInSize > 0 && (!bSuccess || m_normalData.m_size == 0))
            {
                if(!m_normalData.readFile(faIn))
                {
                    errors.append(faIn.getStatusText());
                    errors.append(i18n("    Temp file is: %1", fileNameIn1));
                    return errors;
                }
                //Don't fail the preprocessor command if the file cann't be read.
                errors.append(
                    i18n("Preprocessing possibly failed. Check this command:\n\n  %1"
                         "\n\nThe preprocessing command will be disabled now.", ppCmd) +
                    errorReason);
                m_pOptions->m_PreProcessorCmd = "";

                pEncoding1 = m_pEncoding;
            }
        }

        if(!m_normalData.preprocess(m_pOptions->m_bPreserveCarriageReturn, pEncoding1))
        {
            errors.append(i18n("File %1 too large to process. Skipping.", fileNameIn1));
            return errors;
        }
        //exit early for non text data further processing assumes a text file as input
        if(!m_normalData.isText())
            return errors;

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
            if(FileAccess(fileNameIn2).size() > 0 && (!bSuccess || m_lmppData.m_size == 0))
            {
                errors.append(
                    i18n("The line-matching-preprocessing possibly failed. Check this command:\n\n  %1"
                         "\n\nThe line-matching-preprocessing command will be disabled now.", ppCmd) +
                    errorReason);
                m_pOptions->m_LineMatchingPreProcessorCmd = "";
                if(!m_lmppData.readFile(fileNameIn2))
                {
                    errors.append(i18n("Failed to read file: %1", fileNameIn2));
                    return errors;
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
        //exit early for non-existant files
        return errors;
    }

    if(!m_lmppData.preprocess(false, pEncoding2))
    {
        errors.append(i18n("File %1 too large to process. Skipping.", fileNameIn1));
        return errors;
    }

    Q_ASSERT(m_lmppData.isText());
    //TODO: Needed?
    if(m_lmppData.m_vSize < m_normalData.m_vSize)
    {
        // Preprocessing command may result in smaller data buffer so adjust size
        m_lmppData.m_v.resize((int)m_normalData.m_vSize);
        for(qint64 i = m_lmppData.m_vSize; i < m_normalData.m_vSize; ++i)
        { // Set all empty lines to point to the end of the buffer.
            m_lmppData.m_v[(int)i].setLine(m_lmppData.m_unicodeBuf.unicode() + m_lmppData.m_unicodeBuf.length());
        }

        m_lmppData.m_vSize = m_normalData.m_vSize;
    }

    // Ignore comments
    if(m_pOptions->m_bIgnoreComments && hasData())
    {
        m_lmppData.removeComments();
        qint64 vSize = std::min(m_normalData.m_vSize, m_lmppData.m_vSize);
        Q_ASSERT(vSize < std::numeric_limits<int>::max());
        for(int i = 0; i < vSize; ++i)
        {
            m_normalData.m_v[i].setPureComment(m_lmppData.m_v[i].isPureComment());
            //Don't crash if vSize is too large.
            if(i == std::numeric_limits<int>::max())
                break;
        }
    }

    return errors;
}

/** Prepare the linedata vector for every input line.*/
bool SourceData::FileData::preprocess(bool bPreserveCR, QTextCodec* pEncoding)
{
    qint64 i;
    // detect line end style
    QVector<e_LineEndStyle> vOrigDataLineEndStyle;
    m_eLineEndStyle = eLineEndStyleUndefined;
    for(i = 0; i < m_size; ++i)
    {
        if(m_pBuf[i] == '\r')
        {
            if(i + 1 < m_size && m_pBuf[i + 1] == '\n') // not 16-bit unicode
            {
                vOrigDataLineEndStyle.push_back(eLineEndStyleDos);
                ++i;
            }
            else if(i > 0 && i + 2 < m_size && m_pBuf[i - 1] == '\0' && m_pBuf[i + 1] == '\0' && m_pBuf[i + 2] == '\n') // 16-bit unicode
            {
                vOrigDataLineEndStyle.push_back(eLineEndStyleDos);
                i += 2;
            }
            else // old mac line end style ?
            {
                vOrigDataLineEndStyle.push_back(eLineEndStyleUndefined);
                const_cast<char*>(m_pBuf)[i] = '\n'; // fix it in original data
            }
        }
        else if(m_pBuf[i] == '\n')
        {
            vOrigDataLineEndStyle.push_back(eLineEndStyleUnix);
        }
    }

    if(!vOrigDataLineEndStyle.isEmpty())
        m_eLineEndStyle = vOrigDataLineEndStyle[0];

    qint64 skipBytes = 0;
    QTextCodec* pCodec = detectEncoding(m_pBuf, m_size, skipBytes);
    if(pCodec != pEncoding)
        skipBytes = 0;

    if(m_size - skipBytes > TYPE_MAX(QtNumberType))
        return false;

    QByteArray ba = QByteArray::fromRawData(m_pBuf + skipBytes, (int)(m_size - skipBytes));
    QTextStream ts(ba, QIODevice::ReadOnly | QIODevice::Text);
    ts.setCodec(pEncoding);
    ts.setAutoDetectUnicode(false);
    m_unicodeBuf = ts.readAll();
    ba.clear();

    int ucSize = m_unicodeBuf.length();
    const QChar* p = m_unicodeBuf.unicode();

    m_bIsText = true;
    LineCount lines = 1;
    m_bIncompleteConversion = false;
    for(i = 0; i < ucSize; ++i)
    {
        if(i >= ucSize || p[i] == '\n')
        {
            if(lines >= TYPE_MAX(LineCount) - 5)
                return false;

            ++lines;
        }
        if(p[i].isNull())
        {
            m_bIsText = false;
        }
        if(p[i] == QChar::ReplacementCharacter)
        {
            m_bIncompleteConversion = true;
        }
    }

    m_v.resize(lines + 5);
    int lineIdx = 0;
    int lineLength = 0;
    bool bNonWhiteFound = false;
    int whiteLength = 0;
    for(i = 0; i <= ucSize; ++i)
    {
        if(i >= ucSize || p[i] == '\n')
        {
            const QChar* pLine = &p[i - lineLength];
            m_v[lineIdx].setLine(&p[i - lineLength]);
            while(/*!bPreserveCR  &&*/ lineLength > 0 && m_v[lineIdx].getLine()[lineLength - 1] == '\r')
            {
                --lineLength;
            }
            m_v[lineIdx].setFirstNonWhiteChar(m_v[lineIdx].getLine() + std::min(whiteLength, lineLength));
            if(lineIdx < vOrigDataLineEndStyle.count() && bPreserveCR && i < ucSize)
            {
                ++lineLength;
                const_cast<QChar*>(pLine)[lineLength] = '\r';
                //switch ( vOrigDataLineEndStyle[lineIdx] )
                //{
                //case eLineEndStyleUnix: const_cast<QChar*>(pLine)[lineLength] = '\n'; break;
                //case eLineEndStyleDos:  const_cast<QChar*>(pLine)[lineLength] = '\r'; break;
                //case eLineEndStyleUndefined: const_cast<QChar*>(pLine)[lineLength] = '\x0b'; break;
                //}
            }
            m_v[lineIdx].setSize(lineLength);

            lineLength = 0;
            bNonWhiteFound = false;
            whiteLength = 0;
            ++lineIdx;
        }
        else
        {
            ++lineLength;

            if(!bNonWhiteFound && isWhite(p[i]))
                ++whiteLength;
            else
                bNonWhiteFound = true;
        }
    }
    Q_ASSERT(lineIdx == lines);

    m_vSize = lines;
    return true;
}

// Must not be entered, when within a comment.
// Returns either at a newline-character p[i]=='\n' or when i==size.
// A line that contains only comments is still "white".
// Comments in white lines must remain, while comments in
// non-white lines are overwritten with spaces.
void SourceData::FileData::checkLineForComments(
    const QChar* p,          // pointer to start of buffer
    int& i,                  // index of current position (in, out)
    int size,                // size of buffer
    bool& bWhite,            // false if this line contains nonwhite characters (in, out)
    bool& bCommentInLine,    // true if any comment is within this line (in, out)
    bool& bStartsOpenComment // true if the line ends within an comment (out)
)
{
    bStartsOpenComment = false;
    for(; i < size; ++i)
    {
        // A single apostroph ' has prio over a double apostroph " (e.g. '"')
        // (if not in a string)
        if(p[i] == '\'')
        {
            bWhite = false;
            ++i;
            for(; !isLineOrBufEnd(p, i, size) && p[i] != '\''; ++i)
                ;
            if(p[i] == '\'') ++i;
        }

        // Strings have priority over comments: e.g. "/* Not a comment, but a string. */"
        else if(p[i] == '"')
        {
            bWhite = false;
            ++i;
            for(; !isLineOrBufEnd(p, i, size) && !(p[i] == '"' && p[i - 1] != '\\'); ++i)
                ;
            if(p[i] == '"') ++i;
        }

        // C++-comment
        else if(p[i] == '/' && i + 1 < size && p[i + 1] == '/')
        {
            int commentStart = i;
            bCommentInLine = true;
            i += 2;
            for(; !isLineOrBufEnd(p, i, size); ++i)
                ;
            if(!bWhite)
            {
                size = i - commentStart;
                m_unicodeBuf.replace(commentStart, size, QString(" ").repeated(size));
            }
            return;
        }

        // C-comment
        else if(p[i] == '/' && i + 1 < size && p[i + 1] == '*')
        {
            int commentStart = i;
            bCommentInLine = true;
            i += 2;
            for(; !isLineOrBufEnd(p, i, size); ++i)
            {
                if(i + 1 < size && p[i] == '*' && p[i + 1] == '/') // end of the comment
                {
                    i += 2;

                    // More comments in the line?
                    checkLineForComments(p, i, size, bWhite, bCommentInLine, bStartsOpenComment);
                    if(!bWhite)
                    {
                        size = i - commentStart;
                        m_unicodeBuf.replace(commentStart, size, QString(" ").repeated(size));
                    }
                    return;
                }
            }
            bStartsOpenComment = true;
            return;
        }

        if(isLineOrBufEnd(p, i, size))
        {
            return;
        }
        else if(!p[i].isSpace())
        {
            bWhite = false;
        }
    }
}

// Modifies the input data, and replaces C/C++ comments with whitespace
// when the line contains other data too. If the line contains only
// a comment or white data, remember this in the flag bContainsPureComment.
void SourceData::FileData::removeComments()
{
    int line = 0;
    const QChar* p = m_unicodeBuf.unicode();
    bool bWithinComment = false;
    int size = m_unicodeBuf.length();

    qCDebug(kdiffCore) << "m_v.size() = " << m_v.size() << ", size = " << size;
    Q_ASSERT(m_v.size() > 0);
    for(int i = 0; i < size; ++i)
    {
        qCDebug(kdiffCore) << "line = " << QString(&p[i], m_v[line].size());
        bool bWhite = true;
        bool bCommentInLine = false;

        if(bWithinComment)
        {
            int commentStart = i;
            bCommentInLine = true;

            for(; !isLineOrBufEnd(p, i, size); ++i)
            {
                if(i + 1 < size && p[i] == '*' && p[i + 1] == '/') // end of the comment
                {
                    i += 2;

                    // More comments in the line?
                    checkLineForComments(p, i, size, bWhite, bCommentInLine, bWithinComment);
                    if(!bWhite)
                    {
                        size = i - commentStart;
                        m_unicodeBuf.replace(commentStart, size, QString(" ").repeated(size));
                    }
                    break;
                }
            }
        }
        else
        {
            checkLineForComments(p, i, size, bWhite, bCommentInLine, bWithinComment);
        }

        // end of line
        Q_ASSERT(isLineOrBufEnd(p, i, size));
        m_v[line].setPureComment(bCommentInLine && bWhite);
        /*      std::cout << line << " : " <<
       ( bCommentInLine ?  "c" : " " ) <<
       ( bWhite ? "w " : "  ") <<
       std::string(pLD[line].pLine, pLD[line].size) << std::endl;*/

        ++line;
    }
}

bool SourceData::isLineOrBufEnd(const QChar* p, int i, int size)
{
    return i >= size                   // End of file
           || Utils::isEndOfLine(p[i]) // Normal end of line

        // No support for Mac-end of line yet, because incompatible with GNU-diff-routines.
        // || ( p[i]=='\r' && (i>=size-1 || p[i+1]!='\n')
        //                 && (i==0        || p[i-1]!='\n') )  // Special case: '\r' without '\n'
        ;
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
            return QTextCodec::codecForName("UTF-8-BOM");
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
