/***************************************************************************
                          diff.cpp  -  description
                             -------------------
    begin                : Mon Mar 18 2002
    copyright            : (C) 2002-2007 by Joachim Eibl
    email                : joachim.eibl at gmx.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#include "stable.h"

#include <cstdlib>

#include "diff.h"
#include "fileaccess.h"
#include "gnudiff_diff.h"
#include "options.h"
#include "progress.h"

#include <kmessagebox.h>
#include <klocale.h>

#include <QFileInfo>
#include <QDir>
#include <QTextCodec>
#include <QTextStream>
#include <QProcess>

#include <map>
#include <assert.h>
#include <ctype.h>
//using namespace std;


int LineData::width(int tabSize) const
{
   int w=0;
   int j=0;
   for( int i=0; i<size; ++i )
   {
      if ( pLine[i]=='\t' )
      {
         for(j %= tabSize; j<tabSize; ++j)
            ++w;
         j=0;
      }
      else
      {
         ++w;
         ++j;
      }
   }
   return w;
}


// The bStrict flag is true during the test where a nonmatching area ends.
// Then the equal()-function requires that the match has more than 2 nonwhite characters.
// This is to avoid matches on trivial lines (e.g. with white space only).
// This choice is good for C/C++.
bool equal( const LineData& l1, const LineData& l2, bool bStrict )
{
   if ( l1.pLine==0 || l2.pLine==0) return false;

   if ( bStrict && g_bIgnoreTrivialMatches )//&& (l1.occurances>=5 || l2.occurances>=5) )
      return false;

   // Ignore white space diff
   const QChar* p1 = l1.pLine;
   const QChar* p1End = p1 + l1.size;

   const QChar* p2 = l2.pLine;
   const QChar* p2End = p2 + l2.size;

   if ( g_bIgnoreWhiteSpace )
   {
      int nonWhite = 0;
      for(;;)
      {
         while( isWhite( *p1 ) && p1!=p1End ) ++p1;
         while( isWhite( *p2 ) && p2!=p2End ) ++p2;

         if ( p1 == p1End  &&  p2 == p2End )
         {
            if ( bStrict && g_bIgnoreTrivialMatches )
            {  // Then equality is not enough
               return nonWhite>2;
            }
            else  // equality is enough
               return true;
         }
         else if ( p1 == p1End || p2 == p2End )
            return false;

         if( *p1 != *p2 )
            return false;
         ++p1;
         ++p2;
         ++nonWhite;
      }
   }

   else
   {
      if ( l1.size==l2.size && memcmp(p1, p2, l1.size)==0)
         return true;
      else
         return false;
   }
}


static bool isLineOrBufEnd( const QChar* p, int i, int size )
{
   return 
      i>=size        // End of file
      || p[i]=='\n'  // Normal end of line

      // No support for Mac-end of line yet, because incompatible with GNU-diff-routines.      
      // || ( p[i]=='\r' && (i>=size-1 || p[i+1]!='\n') 
      //                 && (i==0        || p[i-1]!='\n') )  // Special case: '\r' without '\n'
      ;
}


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

SourceData::SourceData()
{
   m_pOptions = 0;
   reset();
}

SourceData::~SourceData()
{
   reset();
}

void SourceData::reset()
{
   m_pEncoding = 0;   
   m_fileAccess = FileAccess();
   m_normalData.reset();
   m_lmppData.reset();
   if ( !m_tempInputFileName.isEmpty() )
   {
      FileAccess::removeFile( m_tempInputFileName );
      m_tempInputFileName = "";
   }
}

void SourceData::setFilename( const QString& filename )
{
   if (filename.isEmpty())
   {
      reset();
   }
   else
   {
      FileAccess fa( filename );
      setFileAccess( fa );
   }
}

bool SourceData::isEmpty() 
{ 
   return getFilename().isEmpty(); 
}

bool SourceData::hasData() 
{ 
   return m_normalData.m_pBuf != 0;
}

bool SourceData::isValid()
{
   return isEmpty() || hasData();
}

void SourceData::setOptions( Options* pOptions )
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

void SourceData::setAliasName( const QString& name )
{
   m_aliasName = name;
}

void SourceData::setFileAccess( const FileAccess& fileAccess )
{
   m_fileAccess = fileAccess;
   m_aliasName = QString();
   if ( !m_tempInputFileName.isEmpty() )
   {
      FileAccess::removeFile( m_tempInputFileName );
      m_tempInputFileName = "";
   }
}
void SourceData::setEncoding(QTextCodec* pEncoding)
{
    m_pEncoding = pEncoding;
}

QStringList SourceData::setData( const QString& data )
{
   QStringList errors;

   // Create a temp file for preprocessing:
   if ( m_tempInputFileName.isEmpty() )
   {
      m_tempInputFileName = FileAccess::tempFileName();
   }

   FileAccess f( m_tempInputFileName );
   QByteArray ba = QTextCodec::codecForName("UTF-8")->fromUnicode(data);
   bool bSuccess = f.writeFile( ba.constData(), ba.length() );
   if ( !bSuccess )
   {
      errors.append( i18n("Writing clipboard data to temp file failed.") );
   }
   else
   {
      m_aliasName = i18n("From Clipboard");
      m_fileAccess = FileAccess("");  // Effect: m_fileAccess.isValid() is false
   }

   return errors;
}

const LineData* SourceData::getLineDataForDiff() const
{
   if ( m_lmppData.m_pBuf==0 )
      return m_normalData.m_v.size()>0 ? &m_normalData.m_v[0] : 0;
   else
      return m_lmppData.m_v.size()>0   ? &m_lmppData.m_v[0]   : 0;
}

const LineData* SourceData::getLineDataForDisplay() const
{
   return m_normalData.m_v.size()>0 ? &m_normalData.m_v[0] : 0;
}

int  SourceData::getSizeLines() const
{
   return m_normalData.m_vSize;
}

int SourceData::getSizeBytes() const
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
   return m_normalData.m_bIsText;
}

bool SourceData::isIncompleteConversion()
{
   return m_normalData.m_bIncompleteConversion;
}

bool SourceData::isFromBuffer()
{
   return !m_fileAccess.isValid();
}


bool SourceData::isBinaryEqualWith( const SourceData& other ) const
{
   return m_fileAccess.exists() && other.m_fileAccess.exists() &&
          getSizeBytes() == other.getSizeBytes() && 
          ( getSizeBytes()==0 || memcmp( getBuf(), other.getBuf(), getSizeBytes() )==0 );
}

void SourceData::FileData::reset()
{
   delete[] (char*)m_pBuf;
   m_pBuf = 0;
   m_v.clear();
   m_size = 0;
   m_vSize = 0;
   m_bIsText = true;
   m_bIncompleteConversion = false;
   m_eLineEndStyle = eLineEndStyleUndefined;
}

bool SourceData::FileData::readFile( const QString& filename )
{
   reset();
   if ( filename.isEmpty() )   { return true; }

   FileAccess fa( filename );
   m_size = fa.sizeForReading();
   char* pBuf;
   m_pBuf = pBuf = new char[m_size+100];  // Alloc 100 byte extra: Savety hack, not nice but does no harm.
                                // Some extra bytes at the end of the buffer are needed by
                                // the diff algorithm. See also GnuDiff::diff_2_files().
   bool bSuccess = fa.readFile( pBuf, m_size );
   if ( !bSuccess )
   {
      delete pBuf;
      m_pBuf = 0;
      m_size = 0;
   }
   return bSuccess;
}

bool SourceData::saveNormalDataAs( const QString& fileName )
{
   return m_normalData.writeFile( fileName );
}

bool SourceData::FileData::writeFile( const QString& filename )
{
   if ( filename.isEmpty() )   { return true; }

   FileAccess fa( filename );
   bool bSuccess = fa.writeFile(m_pBuf, m_size);
   return bSuccess;
}

void SourceData::FileData::copyBufFrom( const FileData& src )
{
   reset();
   char* pBuf;
   m_size = src.m_size;
   m_pBuf = pBuf = new char[m_size+100];
   memcpy( pBuf, src.m_pBuf, m_size );
}

// Convert the input file from input encoding to output encoding and write it to the output file.
static bool convertFileEncoding( const QString& fileNameIn, QTextCodec* pCodecIn,
                                 const QString& fileNameOut, QTextCodec* pCodecOut )
{
   QFile in( fileNameIn );
   if ( ! in.open(QIODevice::ReadOnly ) )
      return false;
   QTextStream inStream( &in );
   inStream.setCodec( pCodecIn );
   inStream.setAutoDetectUnicode( false );

   QFile out( fileNameOut );
   if ( ! out.open( QIODevice::WriteOnly ) )
      return false;
   QTextStream outStream( &out );
   outStream.setCodec( pCodecOut );

   QString data = inStream.readAll();
   outStream << data;

   return true;
}

static QTextCodec* getEncodingFromTag( const QByteArray& s, const QByteArray& encodingTag )
{   
   int encodingPos = s.indexOf( encodingTag );
   if ( encodingPos>=0 )
   {
      int apostrophPos = s.indexOf( '"', encodingPos + encodingTag.length() );
      int apostroph2Pos = s.indexOf( '\'', encodingPos + encodingTag.length() );
      char apostroph = '"';
      if ( apostroph2Pos>=0 && ( apostrophPos<0 || (apostrophPos>=0 && apostroph2Pos < apostrophPos) ) )
      {
         apostroph = '\'';
         apostrophPos = apostroph2Pos;
      }

      int encodingEnd = s.indexOf( apostroph, apostrophPos+1 );
      if ( encodingEnd>=0 ) // e.g.: <meta charset="utf-8"> or <?xml version="1.0" encoding="ISO-8859-1"?>
      {
         QByteArray encoding = s.mid( apostrophPos+1, encodingEnd - (apostrophPos + 1) );
         return QTextCodec::codecForName( encoding );
      }
      else // e.g.: <meta http-equiv="Content-Type" content="text/html; charset=utf-8">
      {
         QByteArray encoding = s.mid( encodingPos+encodingTag.length(), apostrophPos - ( encodingPos+encodingTag.length() ) );
         return QTextCodec::codecForName( encoding );
      }
   }
   return 0;
}

static QTextCodec* detectEncoding( const char* buf, qint64 size, qint64& skipBytes )
{
   if (size>=2)
   {
      if (buf[0]=='\xFF' && buf[1]=='\xFE' )
      {
         skipBytes = 2;
         return QTextCodec::codecForName( "UTF-16LE" );
      }

      if (buf[0]=='\xFE' && buf[1]=='\xFF' )
      {
         skipBytes = 2;
         return QTextCodec::codecForName( "UTF-16BE" );
      }
   }
   if (size>=3)
   {
      if (buf[0]=='\xEF' && buf[1]=='\xBB' && buf[2]=='\xBF' )
      {
         skipBytes = 3;
         return QTextCodec::codecForName( "UTF-8-BOM" );
      }
   }
   skipBytes = 0;
   QByteArray s( buf, size );
   int xmlHeaderPos = s.indexOf( "<?xml" );
   if ( xmlHeaderPos >= 0 )
   {
      int xmlHeaderEnd = s.indexOf( "?>", xmlHeaderPos );
      if ( xmlHeaderEnd>=0 )
      {
         QTextCodec* pCodec = getEncodingFromTag( s.mid( xmlHeaderPos, xmlHeaderEnd - xmlHeaderPos ), "encoding=" );
         if (pCodec)
            return pCodec;
      }
   }
   else // HTML
   {
      int metaHeaderPos = s.indexOf( "<meta" );
      while ( metaHeaderPos >= 0)
      {
         int metaHeaderEnd = s.indexOf( ">", metaHeaderPos );
         if (metaHeaderEnd>=0)
         {
            QTextCodec* pCodec = getEncodingFromTag( s.mid( metaHeaderPos, metaHeaderEnd - metaHeaderPos ), "charset=" );
            if (pCodec)
               return pCodec;

            metaHeaderPos = s.indexOf( "<meta", metaHeaderEnd );
         }
         else
            break;
      }
   }
   return 0;
}

QTextCodec* SourceData::detectEncoding( const QString& fileName, QTextCodec* pFallbackCodec )
{
   QFile f(fileName);
   if ( f.open(QIODevice::ReadOnly) )
   {
      char buf[200];
      qint64 size = f.read( buf, sizeof(buf) );
      qint64 skipBytes = 0;
      QTextCodec* pCodec = ::detectEncoding( buf, size, skipBytes );
      if (pCodec)
         return pCodec;
   }
   return pFallbackCodec;
}

/* Split the command line into arguments.
 * Normally split at white space separators except when quoting with " or '.
 * Backslash is treated as meta character within single quotes ' only.
 * Detect parsing errors like unclosed quotes.
 * The first item in the list will be the command itself.
 * Returns the error reasor as string or an empty string on success.
 * Eg. >"1" "2"<           => >1<, >2<
 * Eg. >'\'\\'<            => >'\<   backslash is a meta character between single quotes
 * Eg. > "\\" <            => >\\<   but not between double quotes
 * Eg. >"c:\sed" 's/a/\' /g'<  => >c:\sed<, >s/a/' /g<
 */
static QString getArguments( QString cmd, QString& program, QStringList& args )
{
   program = QString();
   args.clear();
   for ( int i=0; i<cmd.length(); ++i )
   {
      while ( i<cmd.length() && cmd[i].isSpace() )
      {
         ++i;
      }
      if ( cmd[i]=='"' || cmd[i]=='\'' ) // argument beginning with a quote
      {
         QChar quoteChar = cmd[i];
         ++i;
         int argStart = i;
         bool bSkip = false;
         while ( i<cmd.length() && ( cmd[i]!=quoteChar || bSkip ) )
         {
            if ( bSkip )
            {
               bSkip = false;
               if ( cmd[i]=='\\' || cmd[i]==quoteChar )
               {
                  cmd.remove( i-1, 1 );  // remove the backslash '\'
                  continue;
               }
            }
            else if ( cmd[i]=='\\' && quoteChar=='\'')
               bSkip = true;
            ++i;
         }
         if ( i<cmd.length() )
         {
            args << cmd.mid( argStart, i-argStart );
            if ( i+1<cmd.length() && !cmd[i+1].isSpace() )
               return i18n("Expecting space after closing apostroph.");
         }
         else
            return i18n("Not matching apostrophs.");
         continue;
      }
      else
      {            
         int argStart = i;
         //bool bSkip = false;
         while ( i<cmd.length() && ( !cmd[i].isSpace() /*|| bSkip*/ ) )
         {
            /*if ( bSkip )
            {
               bSkip = false;
               if ( cmd[i]=='\\' || cmd[i]=='"' || cmd[i]=='\'' || cmd[i].isSpace() )
               {
                  cmd.remove( i-1, 1 );  // remove the backslash '\'
                  continue;
               }
            }
            else if ( cmd[i]=='\\' )
               bSkip = true;
            else */
            if ( cmd[i]=='"' || cmd[i]=='\'' )
               return i18n("Unexpected apostroph within argument.");
            ++i;
         }
         args << cmd.mid( argStart, i-argStart );
      }      
   }
   if ( args.isEmpty() )
      return i18n("No program specified.");
   else
   {
      program = args[0];
      args.pop_front();
      #ifdef WIN32
      if ( program=="sed" )
      {
         QString prg = QCoreApplication::applicationDirPath() + "/bin/sed.exe"; // in subdir bin
         if ( QFile::exists( prg ) )
         {
            program = prg;
         }
         else
         {
            prg = QCoreApplication::applicationDirPath() + "/sed.exe"; // in same dir
            if ( QFile::exists( prg ) )
            {
               program = prg;
            }
         }
      }
      #endif 
   }
   return QString();
}

QStringList SourceData::readAndPreprocess( QTextCodec* pEncoding, bool bAutoDetectUnicode )
{
   m_pEncoding = pEncoding;
   QString fileNameIn1;
   QString fileNameOut1;
   QString fileNameIn2;
   QString fileNameOut2;
   QStringList errors;

   bool bTempFileFromClipboard = !m_fileAccess.isValid();

   // Detect the input for the preprocessing operations
   if ( !bTempFileFromClipboard )
   {
      if ( m_fileAccess.isLocal() )
      {
         fileNameIn1 = m_fileAccess.absoluteFilePath();
      }
      else    // File is not local: create a temporary local copy:
      {
         if ( m_tempInputFileName.isEmpty() )  { m_tempInputFileName = FileAccess::tempFileName(); }

         m_fileAccess.copyFile(m_tempInputFileName);
         fileNameIn1 = m_tempInputFileName;
      }
      if ( bAutoDetectUnicode )
      {
         m_pEncoding = detectEncoding( fileNameIn1, pEncoding );
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
   int fileInSize = faIn.size();

   if ( faIn.exists() ) // fileInSize > 0 )
   {

#if defined(_WIN32) || defined(Q_OS_OS2)
      QString catCmd = "type";
      fileNameIn1.replace( '/', "\\" );
#else
      QString catCmd = "cat";
#endif

      // Run the first preprocessor
      if ( m_pOptions->m_PreProcessorCmd.isEmpty() )
      {
         // No preprocessing: Read the file directly:
         m_normalData.readFile( fileNameIn1 );
      }
      else
      {
         QString fileNameInPP = fileNameIn1;

         if ( pEncoding1 != m_pOptions->m_pEncodingPP )
         {
            // Before running the preprocessor convert to the format that the preprocessor expects.
            fileNameInPP = FileAccess::tempFileName();
            pEncoding1 = m_pOptions->m_pEncodingPP;
            convertFileEncoding( fileNameIn1, pEncoding, fileNameInPP, pEncoding1 );
         }

         QString ppCmd = m_pOptions->m_PreProcessorCmd;
         fileNameOut1 = FileAccess::tempFileName();

         QProcess ppProcess;
         ppProcess.setStandardInputFile( fileNameInPP );
         ppProcess.setStandardOutputFile( fileNameOut1 );
         QString program;
         QStringList args;
         QString errorReason = getArguments(ppCmd, program, args);
         if ( errorReason.isEmpty() )
         {
            ppProcess.start( program, args );
            ppProcess.waitForFinished(-1);
         }
         else
            errorReason = "\n("+errorReason+")";
         //QString cmd = catCmd + " \"" + fileNameInPP + "\" | " + ppCmd  + " >\"" + fileNameOut1+"\"";
         //::system( encodeString(cmd) );
         bool bSuccess = errorReason.isEmpty() && m_normalData.readFile( fileNameOut1 );
         if ( fileInSize >0 && ( !bSuccess || m_normalData.m_size==0 ) )
         {

            errors.append(
               i18n("Preprocessing possibly failed. Check this command:\n\n  %1"
                  "\n\nThe preprocessing command will be disabled now."
               ).arg(ppCmd) + errorReason );
            m_pOptions->m_PreProcessorCmd = "";
            m_normalData.readFile( fileNameIn1 );
            pEncoding1 = m_pEncoding;
         }
         if (fileNameInPP != fileNameIn1)
         {
            FileAccess::removeTempFile( fileNameInPP );
         }
      }

      // LineMatching Preprocessor
      if ( ! m_pOptions->m_LineMatchingPreProcessorCmd.isEmpty() )
      {
         fileNameIn2 = fileNameOut1.isEmpty() ? fileNameIn1 : fileNameOut1;
         QString fileNameInPP = fileNameIn2;
         pEncoding2 = pEncoding1;
         if ( pEncoding2 != m_pOptions->m_pEncodingPP )
         {
            // Before running the preprocessor convert to the format that the preprocessor expects.
            fileNameInPP = FileAccess::tempFileName();
            pEncoding2 = m_pOptions->m_pEncodingPP;
            convertFileEncoding( fileNameIn2, pEncoding1, fileNameInPP, pEncoding2 );
         }

         QString ppCmd = m_pOptions->m_LineMatchingPreProcessorCmd;
         fileNameOut2 = FileAccess::tempFileName();
         QProcess ppProcess;
         ppProcess.setStandardInputFile( fileNameInPP );
         ppProcess.setStandardOutputFile( fileNameOut2 );
         QString program;
         QStringList args;
         QString errorReason = getArguments(ppCmd, program, args);
         if ( errorReason.isEmpty() )
         {
            ppProcess.start( program, args );
            ppProcess.waitForFinished(-1);
         }
         else
            errorReason = "\n("+errorReason+")";
         //QString cmd = catCmd + " \"" + fileNameInPP + "\" | " + ppCmd  + " >\"" + fileNameOut2 + "\"";
         //::system( encodeString(cmd) );
         bool bSuccess = errorReason.isEmpty() && m_lmppData.readFile( fileNameOut2 );
         if ( FileAccess(fileNameIn2).size()>0 && ( !bSuccess || m_lmppData.m_size==0 ) )
         {
            errors.append(
               i18n("The line-matching-preprocessing possibly failed. Check this command:\n\n  %1"
                    "\n\nThe line-matching-preprocessing command will be disabled now."
                   ).arg(ppCmd) + errorReason );
            m_pOptions->m_LineMatchingPreProcessorCmd = "";
            m_lmppData.readFile( fileNameIn2 );
         }
         FileAccess::removeTempFile( fileNameOut2 );
         if (fileNameInPP != fileNameIn2)
         {
            FileAccess::removeTempFile( fileNameInPP );
         }
      }
      else if ( m_pOptions->m_bIgnoreComments || m_pOptions->m_bIgnoreCase )
      {
         // We need a copy of the normal data.
         m_lmppData.copyBufFrom( m_normalData );
      }
      else
      {  // We don't need any lmpp data at all.
         m_lmppData.reset();
      }
   }

   m_normalData.preprocess( m_pOptions->m_bPreserveCarriageReturn, pEncoding1 );
   m_lmppData.preprocess( false, pEncoding2 );

   if ( m_lmppData.m_vSize < m_normalData.m_vSize )
   {
      // This probably is the fault of the LMPP-Command, but not worth reporting.
      m_lmppData.m_v.resize( m_normalData.m_vSize );
      for(int i=m_lmppData.m_vSize; i<m_normalData.m_vSize; ++i )
      {  // Set all empty lines to point to the end of the buffer.
         m_lmppData.m_v[i].pLine = m_lmppData.m_unicodeBuf.unicode()+m_lmppData.m_unicodeBuf.length();
      }

      m_lmppData.m_vSize = m_normalData.m_vSize;
   }

   // Internal Preprocessing: Uppercase-conversion   
   if ( m_pOptions->m_bIgnoreCase )
   {
      int i;
      QChar* pBuf = const_cast<QChar*>(m_lmppData.m_unicodeBuf.unicode());
      int ucSize = m_lmppData.m_unicodeBuf.length();
      for(i=0; i<ucSize; ++i)
      {
         pBuf[i] = pBuf[i].toUpper();
      }
   }

   // Ignore comments
   if ( m_pOptions->m_bIgnoreComments )
   {
      m_lmppData.removeComments();
      int vSize = min2(m_normalData.m_vSize, m_lmppData.m_vSize);
      for(int i=0; i<vSize; ++i )
      {
         m_normalData.m_v[i].bContainsPureComment = m_lmppData.m_v[i].bContainsPureComment;
      }
   }

   // Remove unneeded temporary files. (A temp file from clipboard must not be deleted.)
   if ( !bTempFileFromClipboard && !m_tempInputFileName.isEmpty() )
   {
      FileAccess::removeTempFile( m_tempInputFileName );
      m_tempInputFileName = "";
   }

   if ( !fileNameOut1.isEmpty() )
   {
      FileAccess::removeTempFile( fileNameOut1 );
      fileNameOut1="";
   }

   return errors;
}


/** Prepare the linedata vector for every input line.*/
void SourceData::FileData::preprocess( bool bPreserveCR, QTextCodec* pEncoding )
{
   //m_unicodeBuf = decodeString( m_pBuf, m_size, eEncoding );

   qint64 i;
   // detect line end style
   m_eLineEndStyle = eLineEndStyleUndefined;
   for( i=0; i<m_size; ++i )
   {
      if ( m_pBuf[i]=='\n' )
      {
         if ( (i>0 && m_pBuf[i-1]=='\r') ||  // normal file
              (i>3 && m_pBuf[i-1]=='\0' && m_pBuf[i-2]=='\r' && m_pBuf[i-3]=='\0')) // 16-bit unicode: TODO only little endian covered here
            m_eLineEndStyle = eLineEndStyleDos;
         else
            m_eLineEndStyle = eLineEndStyleUnix;
         break;   // Only analyze first line
      }
   }
   qint64 skipBytes = 0;
   QTextCodec* pCodec = ::detectEncoding( m_pBuf, m_size, skipBytes );
   if ( pCodec != pEncoding )
      skipBytes=0;

   QByteArray ba = QByteArray::fromRawData( m_pBuf+skipBytes, m_size-skipBytes );
   if ( m_eLineEndStyle == eLineEndStyleUndefined ) // normally only for one liners except when old mac line end style is used
   {
      for( int j=0; j<ba.size(); ++j ) // int because QByteArray does not support operator[](qint64)
      {
         if ( ba[j]=='\r' )
            ba[j]='\n'; // We only fix the old mac line end style, but leave it as "undefined"
      }
   }
   QTextStream ts( ba, QIODevice::ReadOnly );
   ts.setCodec( pEncoding);
   ts.setAutoDetectUnicode( false );
   m_unicodeBuf = ts.readAll();
   ba.clear();

   int ucSize = m_unicodeBuf.length();
   const QChar* p = m_unicodeBuf.unicode();

   m_bIsText = true;
   int lines = 1;
   m_bIncompleteConversion = false;
   for( i=0; i<ucSize; ++i )
   {
      if ( isLineOrBufEnd(p,i,ucSize) )
      {
         ++lines;
      }
      if ( p[i]=='\0' )
      {
         m_bIsText = false;
      }
      if ( p[i]==QChar::ReplacementCharacter )
      {
         m_bIncompleteConversion = true;
      }
   }

   m_v.resize( lines+5 );
   int lineIdx=0;
   int lineLength=0;
   bool bNonWhiteFound = false;
   int whiteLength = 0;
   for( i=0; i<=ucSize; ++i )
   {
      if ( isLineOrBufEnd( p, i, ucSize ) )
      {
         m_v[lineIdx].pLine = &p[ i-lineLength ];
         while ( !bPreserveCR  &&  lineLength>0  &&  m_v[lineIdx].pLine[lineLength-1]=='\r'  )
         {
            --lineLength;
         }
         m_v[lineIdx].pFirstNonWhiteChar = m_v[lineIdx].pLine + min2(whiteLength,lineLength);
         m_v[lineIdx].size = lineLength;
         lineLength = 0;
         bNonWhiteFound = false;
         whiteLength = 0;
         ++lineIdx;
      }
      else
      {
         ++lineLength;

         if ( ! bNonWhiteFound && isWhite( p[i] ) )
            ++whiteLength;
         else
            bNonWhiteFound = true;
      }
   }
   assert( lineIdx == lines );

   m_vSize = lines;
}


// Must not be entered, when within a comment.
// Returns either at a newline-character p[i]=='\n' or when i==size.
// A line that contains only comments is still "white".
// Comments in white lines must remain, while comments in
// non-white lines are overwritten with spaces.
static void checkLineForComments(
   QChar* p,   // pointer to start of buffer
   int& i,    // index of current position (in, out)
   int size,  // size of buffer
   bool& bWhite,          // false if this line contains nonwhite characters (in, out)
   bool& bCommentInLine,  // true if any comment is within this line (in, out)
   bool& bStartsOpenComment  // true if the line ends within an comment (out)
   )
{
   bStartsOpenComment = false;
   for(; i<size; ++i )
   {
      // A single apostroph ' has prio over a double apostroph " (e.g. '"')
      // (if not in a string)
      if ( p[i]=='\'' )
      {
         bWhite = false;
         ++i;
         for( ; !isLineOrBufEnd(p,i,size) && p[i]!='\''; ++i)
            ;
         if (p[i]=='\'') ++i;
      }

      // Strings have priority over comments: e.g. "/* Not a comment, but a string. */"
      else if ( p[i]=='"' )
      {
         bWhite = false;
         ++i;
         for( ; !isLineOrBufEnd(p,i,size) && !(p[i]=='"' && p[i-1]!='\\'); ++i)
            ;
         if (p[i]=='"') ++i;
      }

      // C++-comment
      else if ( p[i]=='/' && i+1<size && p[i+1] =='/' )
      {
         int commentStart = i;
         bCommentInLine = true;
         i+=2;
         for( ; !isLineOrBufEnd(p,i,size); ++i)
            ;
         if ( !bWhite )
         {
            memset( &p[commentStart], ' ', i-commentStart );
         }
         return;
      }

      // C-comment
      else if ( p[i]=='/' && i+1<size && p[i+1] =='*' )
      {
         int commentStart = i;
         bCommentInLine = true;
         i+=2;
         for( ; !isLineOrBufEnd(p,i,size); ++i)
         {
            if ( i+1<size && p[i]=='*' && p[i+1]=='/')  // end of the comment
            {
               i+=2;

               // More comments in the line?
               checkLineForComments( p, i, size, bWhite, bCommentInLine, bStartsOpenComment );
               if ( !bWhite )
               {
                  memset( &p[commentStart], ' ', i-commentStart );
               }
               return;
            }
         }
         bStartsOpenComment = true;
         return;
      }


      if ( isLineOrBufEnd(p,i,size) )
      {
         return;
      }
      else if ( !p[i].isSpace() )
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
   int line=0;
   QChar* p = const_cast<QChar*>(m_unicodeBuf.unicode());
   bool bWithinComment=false;
   int size = m_unicodeBuf.length();
   for(int i=0; i<size; ++i )
   {
//      std::cout << "2        " << std::string(&p[i], m_v[line].size) << std::endl;
      bool bWhite = true;
      bool bCommentInLine = false;

      if ( bWithinComment )
      {
         int commentStart = i;
         bCommentInLine = true;

         for( ; !isLineOrBufEnd(p,i,size); ++i)
         {
            if ( i+1<size && p[i]=='*' && p[i+1]=='/')  // end of the comment
            {
               i+=2;

               // More comments in the line?
               checkLineForComments( p, i, size, bWhite, bCommentInLine, bWithinComment );
               if ( !bWhite )
               {
                  memset( &p[commentStart], ' ', i-commentStart );
               }
               break;
            }
         }
      }
      else
      {
         checkLineForComments( p, i, size, bWhite, bCommentInLine, bWithinComment );
      }

      // end of line
      assert( isLineOrBufEnd(p,i,size));
      m_v[line].bContainsPureComment = bCommentInLine && bWhite;
/*      std::cout << line << " : " <<
       ( bCommentInLine ?  "c" : " " ) <<
       ( bWhite ? "w " : "  ") <<
       std::string(pLD[line].pLine, pLD[line].size) << std::endl;*/

      ++line;
   }
}



// First step
void calcDiff3LineListUsingAB(
   const DiffList* pDiffListAB,
   Diff3LineList& d3ll
   )
{
   // First make d3ll for AB (from pDiffListAB)

   DiffList::const_iterator i=pDiffListAB->begin();
   int lineA=0;
   int lineB=0;
   Diff d(0,0,0);

   for(;;)
   {
      if ( d.nofEquals==0 && d.diff1==0 && d.diff2==0 )
      {
         if ( i!=pDiffListAB->end() )
         {
            d=*i;
            ++i;
         }
         else
            break;
      }

      Diff3Line d3l;
      if( d.nofEquals>0 )
      {
         d3l.bAEqB = true;
         d3l.lineA = lineA;
         d3l.lineB = lineB;
         --d.nofEquals;
         ++lineA;
         ++lineB;
      }
      else if ( d.diff1>0 && d.diff2>0 )
      {
         d3l.lineA = lineA;
         d3l.lineB = lineB;
         --d.diff1;
         --d.diff2;
         ++lineA;
         ++lineB;
      }
      else if ( d.diff1>0 )
      {
         d3l.lineA = lineA;
         --d.diff1;
         ++lineA;
      }
      else if ( d.diff2>0 )
      {
         d3l.lineB = lineB;
         --d.diff2;
         ++lineB;
      }

      d3ll.push_back( d3l );
   }
}


// Second step
void calcDiff3LineListUsingAC(
   const DiffList* pDiffListAC,
   Diff3LineList& d3ll
   )
{
   ////////////////
   // Now insert data from C using pDiffListAC

   DiffList::const_iterator i=pDiffListAC->begin();
   Diff3LineList::iterator i3 = d3ll.begin();
   int lineA=0;
   int lineC=0;
   Diff d(0,0,0);

   for(;;)
   {
      if ( d.nofEquals==0 && d.diff1==0 && d.diff2==0 )
      {
         if ( i!=pDiffListAC->end() )
         {
            d=*i;
            ++i;
         }
         else
            break;
      }

      Diff3Line d3l;
      if( d.nofEquals>0 )
      {
         // Find the corresponding lineA
         while( (*i3).lineA!=lineA )
            ++i3;

         (*i3).lineC = lineC;
         (*i3).bAEqC = true;
         (*i3).bBEqC = (*i3).bAEqB;

         --d.nofEquals;
         ++lineA;
         ++lineC;
         ++i3;
      }
      else if ( d.diff1>0 && d.diff2>0 )
      {
         d3l.lineC = lineC;
         d3ll.insert( i3, d3l );
         --d.diff1;
         --d.diff2;
         ++lineA;
         ++lineC;
      }
      else if ( d.diff1>0 )
      {
         --d.diff1;
         ++lineA;
      }
      else if ( d.diff2>0 )
      {
         d3l.lineC = lineC;
         d3ll.insert( i3, d3l );
         --d.diff2;
         ++lineC;
      }
   }
}

// Third step
void calcDiff3LineListUsingBC(
   const DiffList* pDiffListBC,
   Diff3LineList& d3ll
   )
{
   ////////////////
   // Now improve the position of data from C using pDiffListBC
   // If a line from C equals a line from A then it is in the
   // same Diff3Line already.
   // If a line from C equals a line from B but not A, this
   // information will be used here.

   DiffList::const_iterator i=pDiffListBC->begin();
   Diff3LineList::iterator i3b = d3ll.begin();
   Diff3LineList::iterator i3c = d3ll.begin();
   int lineB=0;
   int lineC=0;
   Diff d(0,0,0);

   for(;;)
   {
      if ( d.nofEquals==0 && d.diff1==0 && d.diff2==0 )
      {
         if ( i!=pDiffListBC->end() )
         {
            d=*i;
            ++i;
         }
         else
            break;
      }

      Diff3Line d3l;
      if( d.nofEquals>0 )
      {
         // Find the corresponding lineB and lineC
         while( i3b!=d3ll.end() && (*i3b).lineB!=lineB )
            ++i3b;

         while( i3c!=d3ll.end() && (*i3c).lineC!=lineC )
            ++i3c;

         assert(i3b!=d3ll.end());
         assert(i3c!=d3ll.end());

         if ( i3b==i3c )
         {
            assert( (*i3b).lineC == lineC );
            (*i3b).bBEqC = true;
         }
         else
         {
            // Is it possible to move this line up?
            // Test if no other B's are used between i3c and i3b

            // First test which is before: i3c or i3b ?
            Diff3LineList::iterator i3c1 = i3c;
            Diff3LineList::iterator i3b1 = i3b;
            while( i3c1!=i3b  &&  i3b1!=i3c )
            {
               assert(i3b1!=d3ll.end() || i3c1!=d3ll.end());
               if( i3c1!=d3ll.end() ) ++i3c1;
               if( i3b1!=d3ll.end() ) ++i3b1;
            }

            if( i3c1==i3b  &&  !(*i3b).bAEqB ) // i3c before i3b
            {
               Diff3LineList::iterator i3 = i3c;
               int nofDisturbingLines = 0;
               while( i3 != i3b && i3!=d3ll.end() )
               {
                  if ( (*i3).lineB != -1 )
                     ++nofDisturbingLines;
                  ++i3;
               }

               if ( nofDisturbingLines>0 )//&& nofDisturbingLines < d.nofEquals*d.nofEquals+4 )
               {
                  // Move the disturbing lines up, out of sight.
                  i3 = i3c;
                  while( i3 != i3b )
                  {
                     if ( (*i3).lineB != -1 )
                     {
                        Diff3Line d3l;
                        d3l.lineB = (*i3).lineB;
                        (*i3).lineB = -1;
                        (*i3).bAEqB = false;
                        (*i3).bBEqC = false;
                        d3ll.insert( i3c, d3l );
                     }
                     ++i3;
                  }
                  nofDisturbingLines=0;
               }

               if ( nofDisturbingLines == 0 )
               {
                  // Yes, the line from B can be moved.
                  (*i3b).lineB = -1;   // This might leave an empty line: removed later.
                  (*i3b).bAEqB = false;
                  (*i3b).bAEqC = false;
                  (*i3b).bBEqC = false;
                  (*i3c).lineB = lineB;
                  (*i3c).bBEqC = true;
               }
            }
            else if( i3b1==i3c  &&  !(*i3c).bAEqC)
            {
               Diff3LineList::iterator i3 = i3b;
               int nofDisturbingLines = 0;
               while( i3 != i3c && i3!=d3ll.end() )
               {
                  if ( (*i3).lineC != -1 )
                     ++nofDisturbingLines;
                  ++i3;
               }

               if ( nofDisturbingLines>0 )//&& nofDisturbingLines < d.nofEquals*d.nofEquals+4 )
               {
                  // Move the disturbing lines up.
                  i3 = i3b;
                  while( i3 != i3c )
                  {
                     if ( (*i3).lineC != -1 )
                     {
                        Diff3Line d3l;
                        d3l.lineC = (*i3).lineC;
                        (*i3).lineC = -1;
                        (*i3).bAEqC = false;
                        (*i3).bBEqC = false;
                        d3ll.insert( i3b, d3l );
                     }
                     ++i3;
                  }
                  nofDisturbingLines=0;
               }

               if ( nofDisturbingLines == 0 )
               {
                  // Yes, the line from C can be moved.
                  (*i3c).lineC = -1;   // This might leave an empty line: removed later.
                  (*i3c).bAEqC = false;
                  (*i3c).bBEqC = false;
                  (*i3b).lineC = lineC;
                  (*i3b).bBEqC = true;
               }
            }
         }

         --d.nofEquals;
         ++lineB;
         ++lineC;
         ++i3b;
         ++i3c;
      }
      else if ( d.diff1>0 )
      {
         Diff3LineList::iterator i3 = i3b;
         while( (*i3).lineB!=lineB )
            ++i3;
         if( i3 != i3b  &&  (*i3).bAEqB==false )
         {
            // Take B from this line and move it up as far as possible
            d3l.lineB = lineB;
            d3ll.insert( i3b, d3l );
            (*i3).lineB = -1;
         }
         else
         {
            i3b=i3;
         }
         --d.diff1;
         ++lineB;
         ++i3b;

         if( d.diff2>0 )
         {
            --d.diff2;
            ++lineC;
         }
      }
      else if ( d.diff2>0 )
      {
         --d.diff2;
         ++lineC;
      }
   }
/*
   Diff3LineList::iterator it = d3ll.begin();
   int li=0;
   for( ; it!=d3ll.end(); ++it, ++li )
   {
      printf( "%4d %4d %4d %4d  A%c=B A%c=C B%c=C\n",
         li, (*it).lineA, (*it).lineB, (*it).lineC,
         (*it).bAEqB ? '=' : '!', (*it).bAEqC ? '=' : '!', (*it).bBEqC ? '=' : '!' );
   }
   printf("\n");*/
}

#ifdef _WIN32
using ::equal;
#endif

// Test if the move would pass a barrier. Return true if not.
static bool isValidMove( ManualDiffHelpList* pManualDiffHelpList, int line1, int line2, int winIdx1, int winIdx2 )
{
   if (line1>=0 && line2>=0)
   {
      ManualDiffHelpList::const_iterator i;
      for( i = pManualDiffHelpList->begin(); i!=pManualDiffHelpList->end(); ++i )
      {
         const ManualDiffHelpEntry& mdhe = *i;

         // Barrier
         int l1 = winIdx1 == 1 ? mdhe.lineA1 : winIdx1==2 ? mdhe.lineB1 : mdhe.lineC1 ;
         int l2 = winIdx2 == 1 ? mdhe.lineA1 : winIdx2==2 ? mdhe.lineB1 : mdhe.lineC1 ;

         if ( l1>=0 && l2>=0 )
         {
            if ( (line1>=l1 && line2<l2) || (line1<l1 && line2>=l2) )
               return false;
            l1 = winIdx1 == 1 ? mdhe.lineA2 : winIdx1==2 ? mdhe.lineB2 : mdhe.lineC2 ;
            l2 = winIdx2 == 1 ? mdhe.lineA2 : winIdx2==2 ? mdhe.lineB2 : mdhe.lineC2 ;
            ++l1;
            ++l2;
            if ( (line1>=l1 && line2<l2) || (line1<l1 && line2>=l2) )
               return false;
         }
      }
   }
   return true; // no barrier passed.
}

static bool runDiff( const LineData* p1, int size1, const LineData* p2, int size2, DiffList& diffList,
                     Options *pOptions)
{
   ProgressProxy pp;
   static GnuDiff gnuDiff;  // All values are initialized with zeros.

   pp.setCurrent(0);

   diffList.clear();
   if ( p1[0].pLine==0 || p2[0].pLine==0 || size1==0 || size2==0 )
   {
      Diff d( 0,0,0);
      if ( p1[0].pLine==0 && p2[0].pLine==0 && size1 == size2 )
         d.nofEquals = size1;
      else
      {
         d.diff1=size1;
         d.diff2=size2;
      }

      diffList.push_back(d);
   }
   else
   {
      GnuDiff::comparison comparisonInput;
      memset( &comparisonInput, 0, sizeof(comparisonInput) );
      comparisonInput.parent = 0;
      comparisonInput.file[0].buffer = p1[0].pLine;//ptr to buffer
      comparisonInput.file[0].buffered = (p1[size1-1].pLine-p1[0].pLine+p1[size1-1].size); // size of buffer
      comparisonInput.file[1].buffer = p2[0].pLine;//ptr to buffer
      comparisonInput.file[1].buffered = (p2[size2-1].pLine-p2[0].pLine+p2[size2-1].size); // size of buffer

      gnuDiff.ignore_white_space = GnuDiff::IGNORE_ALL_SPACE;  // I think nobody needs anything else ...
      gnuDiff.bIgnoreWhiteSpace = true;
      gnuDiff.bIgnoreNumbers    = pOptions->m_bIgnoreNumbers;
      gnuDiff.minimal = pOptions->m_bTryHard;
      gnuDiff.ignore_case = false;
      GnuDiff::change* script = gnuDiff.diff_2_files( &comparisonInput );

      int equalLinesAtStart =  comparisonInput.file[0].prefix_lines;
      int currentLine1 = 0;
      int currentLine2 = 0;
      GnuDiff::change* p=0;
      for (GnuDiff::change* e = script; e; e = p)
      {
         Diff d(0,0,0);
         d.nofEquals = e->line0 - currentLine1;
         assert( d.nofEquals == e->line1 - currentLine2 );
         d.diff1 = e->deleted;
         d.diff2 = e->inserted;
         currentLine1 += d.nofEquals + d.diff1;
         currentLine2 += d.nofEquals + d.diff2;
         diffList.push_back(d);

         p = e->link;
         free (e);
      }

      if ( diffList.empty() )
      {
         Diff d(0,0,0);
         d.nofEquals = min2(size1,size2);
         d.diff1 = size1 - d.nofEquals;
         d.diff2 = size2 - d.nofEquals;
         diffList.push_back(d);
/*         Diff d(0,0,0);
         d.nofEquals = equalLinesAtStart;
         if ( gnuDiff.files[0].missing_newline != gnuDiff.files[1].missing_newline )
         {
            d.diff1 = gnuDiff.files[0].missing_newline ? 0 : 1;
            d.diff2 = gnuDiff.files[1].missing_newline ? 0 : 1;
            ++d.nofEquals;
         }
         else if ( !gnuDiff.files[0].missing_newline )
         {
            ++d.nofEquals;
         }
         diffList.push_back(d);
*/
      }
      else
      {
         diffList.front().nofEquals += equalLinesAtStart;   
         currentLine1 += equalLinesAtStart;
         currentLine2 += equalLinesAtStart;

         int nofEquals = min2(size1-currentLine1,size2-currentLine2);
         if ( nofEquals==0 )
         {
            diffList.back().diff1 += size1-currentLine1;
            diffList.back().diff2 += size2-currentLine2;
         }
         else
         {
            Diff d( nofEquals,size1-currentLine1-nofEquals,size2-currentLine2-nofEquals);
            diffList.push_back(d);
         }

         /*
         if ( gnuDiff.files[0].missing_newline != gnuDiff.files[1].missing_newline )
         {
            diffList.back().diff1 += gnuDiff.files[0].missing_newline ? 0 : 1;
            diffList.back().diff2 += gnuDiff.files[1].missing_newline ? 0 : 1;
         }
         else if ( !gnuDiff.files[0].missing_newline )
         {
            ++ diffList.back().nofEquals;
         }
         */
      }
   }

#ifndef NDEBUG
   // Verify difflist
   {
      int l1=0;
      int l2=0;
      DiffList::iterator i;
      for( i = diffList.begin(); i!=diffList.end(); ++i )
      {
         l1+= i->nofEquals + i->diff1;
         l2+= i->nofEquals + i->diff2;
      }

      //if( l1!=p1-p1start || l2!=p2-p2start )
      if( l1!=size1 || l2!=size2 )
         assert( false );
   }
#endif

   pp.setCurrent(1.0);

   return true;
}

bool runDiff( const LineData* p1, int size1, const LineData* p2, int size2, DiffList& diffList,
              int winIdx1, int winIdx2,
              ManualDiffHelpList *pManualDiffHelpList,
              Options *pOptions)
{
   diffList.clear();
   DiffList diffList2;

   int l1begin = 0;
   int l2begin = 0;
   ManualDiffHelpList::const_iterator i;
   for( i = pManualDiffHelpList->begin(); i!=pManualDiffHelpList->end(); ++i )
   {
      const ManualDiffHelpEntry& mdhe = *i;

      int l1end = winIdx1 == 1 ? mdhe.lineA1 : winIdx1==2 ? mdhe.lineB1 : mdhe.lineC1 ;
      int l2end = winIdx2 == 1 ? mdhe.lineA1 : winIdx2==2 ? mdhe.lineB1 : mdhe.lineC1 ;

      if ( l1end>=0 && l2end>=0 )
      {
         runDiff( p1+l1begin, l1end-l1begin, p2+l2begin, l2end-l2begin, diffList2, pOptions);
         diffList.splice( diffList.end(), diffList2 );
         l1begin = l1end;
         l2begin = l2end;

         l1end = winIdx1 == 1 ? mdhe.lineA2 : winIdx1==2 ? mdhe.lineB2 : mdhe.lineC2 ;
         l2end = winIdx2 == 1 ? mdhe.lineA2 : winIdx2==2 ? mdhe.lineB2 : mdhe.lineC2 ;

         if ( l1end>=0 && l2end>=0 )
         {
            ++l1end; // point to line after last selected line
            ++l2end;
            runDiff( p1+l1begin, l1end-l1begin, p2+l2begin, l2end-l2begin, diffList2, pOptions);
            diffList.splice( diffList.end(), diffList2 );
            l1begin = l1end;
            l2begin = l2end;
         }
      }
   }
   runDiff( p1+l1begin, size1-l1begin, p2+l2begin, size2-l2begin, diffList2, pOptions);
   diffList.splice( diffList.end(), diffList2 );
   return true;
}

void correctManualDiffAlignment( Diff3LineList& d3ll, ManualDiffHelpList* pManualDiffHelpList )
{
   if ( pManualDiffHelpList->empty() )
      return;

   // If a line appears unaligned in comparison to the manual alignment, correct this.

   ManualDiffHelpList::iterator iMDHL;
   for( iMDHL =  pManualDiffHelpList->begin(); iMDHL != pManualDiffHelpList->end(); ++iMDHL )
   {
      Diff3LineList::iterator i3 = d3ll.begin();
      int winIdxPreferred = 0;
      int missingWinIdx = 0;
      int alignedSum = (iMDHL->lineA1<0?0:1) + (iMDHL->lineB1<0?0:1) + (iMDHL->lineC1<0?0:1);
      if (alignedSum==2)
      {
         // If only A & B are aligned then let C rather be aligned with A
         // If only A & C are aligned then let B rather be aligned with A
         // If only B & C are aligned then let A rather be aligned with B
         missingWinIdx = iMDHL->lineA1<0 ? 1 : (iMDHL->lineB1<0 ? 2 : 3 );
         winIdxPreferred = missingWinIdx == 1 ? 2 : 1; 
      }
      else if (alignedSum<=1)
      {
         return;
      }

      // At the first aligned line, move up the two other lines into new d3ls until the second input is aligned
      // Then move up the third input until all three lines are aligned.
      int wi=0;
      for( ; i3!=d3ll.end(); ++i3 )
      {
         for ( wi=1; wi<=3; ++wi )
         {
            if ( i3->getLineInFile(wi) >= 0 && iMDHL->firstLine(wi) == i3->getLineInFile(wi) )
               break;
         }
         if ( wi<=3 )
            break;
      }

      if (wi>=1 && wi <= 3)
      {
         // Found manual alignment for one source
         Diff3LineList::iterator iDest = i3;

         // Move lines up until the next firstLine is found. Omit wi from move and search.
         int wi2=0;
         for( ; i3!=d3ll.end(); ++i3 )
         {
            for ( wi2=1; wi2<=3; ++wi2 )
            {
               if ( wi!=wi2 && i3->getLineInFile(wi2) >= 0 && iMDHL->firstLine(wi2) == i3->getLineInFile(wi2) )
                  break;
            }
            if (wi2>3)
            {  // Not yet found
               // Move both others up
               Diff3Line d3l;
               // Move both up
               if (wi==1) // Move B and C up
               {
                  d3l.bBEqC = i3->bBEqC;
                  d3l.lineB = i3->lineB;
                  d3l.lineC = i3->lineC;
                  i3->lineB = -1;
                  i3->lineC = -1;
               }
               if (wi==2) // Move A and C up
               {
                  d3l.bAEqC = i3->bAEqC;
                  d3l.lineA = i3->lineA;
                  d3l.lineC = i3->lineC;
                  i3->lineA = -1;
                  i3->lineC = -1;
               }
               if (wi==3) // Move A and B up
               {
                  d3l.bAEqB = i3->bAEqB;
                  d3l.lineA = i3->lineA;
                  d3l.lineB = i3->lineB;
                  i3->lineA = -1;
                  i3->lineB = -1;
               }
               i3->bAEqB = false;
               i3->bAEqC = false;
               i3->bBEqC = false;
               d3ll.insert( iDest, d3l );
            }
            else 
            {
               // align the found line with the line we already have here
               if ( i3 != iDest )
               {
                  if (wi2==1)
                  {
                     iDest->lineA = i3->lineA;
                     i3->lineA = -1;
                     i3->bAEqB = false;
                     i3->bAEqC = false;                   
                  }
                  else if (wi2==2)
                  {
                     iDest->lineB = i3->lineB;
                     i3->lineB = -1;
                     i3->bAEqB = false;
                     i3->bBEqC = false;                   
                  }
                  else if (wi2==3)
                  {
                     iDest->lineC = i3->lineC;
                     i3->lineC = -1;
                     i3->bBEqC = false;
                     i3->bAEqC = false;                   
                  }
               }

               if ( missingWinIdx!=0 )
               {
                  for( ; i3!=d3ll.end(); ++i3 )
                  {
                     int wi3 = missingWinIdx;
                     if ( i3->getLineInFile(wi3) >= 0 )
                     {
                        // not found, move the line before iDest
                        Diff3Line d3l;
                        if ( wi3==1 )
                        {
                           if (i3->bAEqB)  // Stop moving lines up if one equal is found.
                              break;
                           d3l.lineA = i3->lineA;
                           i3->lineA = -1;
                           i3->bAEqB = false;
                           i3->bAEqC = false;
                        }
                        if ( wi3==2 )
                        {
                           if (i3->bAEqB)
                              break;
                           d3l.lineB = i3->lineB;
                           i3->lineB = -1;
                           i3->bAEqB = false;
                           i3->bBEqC = false;
                        }
                        if ( wi3==3 )
                        {
                           if (i3->bAEqC)
                              break;
                           d3l.lineC = i3->lineC;
                           i3->lineC = -1;
                           i3->bAEqC = false;
                           i3->bBEqC = false;
                        }
                        d3ll.insert( iDest, d3l );
                     }
                  } // for(), searching for wi3
               }
               break;
            }
         } // for(), searching for wi2
      } // if, wi found
   } // for (iMDHL)
}

// Fourth step
void calcDiff3LineListTrim(
   Diff3LineList& d3ll, const LineData* pldA, const LineData* pldB, const LineData* pldC, ManualDiffHelpList* pManualDiffHelpList
   )
{
   const Diff3Line d3l_empty;
   d3ll.remove( d3l_empty );

   Diff3LineList::iterator i3 = d3ll.begin();
   Diff3LineList::iterator i3A = d3ll.begin();
   Diff3LineList::iterator i3B = d3ll.begin();
   Diff3LineList::iterator i3C = d3ll.begin();

   int line=0;  // diff3line counters
   int lineA=0; // 
   int lineB=0;
   int lineC=0;

   ManualDiffHelpList::iterator iMDHL = pManualDiffHelpList->begin();
   // The iterator i3 and the variable line look ahead.
   // The iterators i3A, i3B, i3C and corresponding lineA, lineB and lineC stop at empty lines, if found.
   // If possible, then the texts from the look ahead will be moved back to the empty places.

   for( ; i3!=d3ll.end(); ++i3, ++line )
   {
      if ( iMDHL!=pManualDiffHelpList->end() )
      {
         if ( (i3->lineA >= 0 && i3->lineA==iMDHL->lineA1) || 
              (i3->lineB >= 0 && i3->lineB==iMDHL->lineB1) || 
              (i3->lineC >= 0 && i3->lineC==iMDHL->lineC1) )
         {
            i3A = i3;
            i3B = i3;
            i3C = i3;
            lineA = line;
            lineB = line;
            lineC = line;
            ++iMDHL;
         }
      }

      if( line>lineA && (*i3).lineA != -1 && (*i3A).lineB!=-1 && (*i3A).bBEqC  &&
          ::equal( pldA[(*i3).lineA], pldB[(*i3A).lineB], false ) &&
          isValidMove( pManualDiffHelpList, (*i3).lineA, (*i3A).lineB, 1, 2 ) &&
          isValidMove( pManualDiffHelpList, (*i3).lineA, (*i3A).lineC, 1, 3 ) )
      {
         // Empty space for A. A matches B and C in the empty line. Move it up.
         (*i3A).lineA = (*i3).lineA;
         (*i3A).bAEqB = true;
         (*i3A).bAEqC = true;
         (*i3).lineA = -1;
         (*i3).bAEqB = false;
         (*i3).bAEqC = false;
         ++i3A;
         ++lineA;
      }

      if( line>lineB && (*i3).lineB != -1 && (*i3B).lineA!=-1 && (*i3B).bAEqC  &&
          ::equal( pldB[(*i3).lineB], pldA[(*i3B).lineA], false ) &&
          isValidMove( pManualDiffHelpList, (*i3).lineB, (*i3B).lineA, 2, 1 ) &&
          isValidMove( pManualDiffHelpList, (*i3).lineB, (*i3B).lineC, 2, 3 ) )
      {
         // Empty space for B. B matches A and C in the empty line. Move it up.
         (*i3B).lineB = (*i3).lineB;
         (*i3B).bAEqB = true;
         (*i3B).bBEqC = true;
         (*i3).lineB = -1;
         (*i3).bAEqB = false;
         (*i3).bBEqC = false;
         ++i3B;
         ++lineB;
      }

      if( line>lineC && (*i3).lineC != -1 && (*i3C).lineA!=-1 && (*i3C).bAEqB  &&
          ::equal( pldC[(*i3).lineC], pldA[(*i3C).lineA], false )&&
          isValidMove( pManualDiffHelpList, (*i3).lineC, (*i3C).lineA, 3, 1 ) &&
          isValidMove( pManualDiffHelpList, (*i3).lineC, (*i3C).lineB, 3, 2 ) )
      {
         // Empty space for C. C matches A and B in the empty line. Move it up.
         (*i3C).lineC = (*i3).lineC;
         (*i3C).bAEqC = true;
         (*i3C).bBEqC = true;
         (*i3).lineC = -1;
         (*i3).bAEqC = false;
         (*i3).bBEqC = false;
         ++i3C;
         ++lineC;
      }

      if( line>lineA && (*i3).lineA != -1 && !(*i3).bAEqB && !(*i3).bAEqC && 
          isValidMove( pManualDiffHelpList, (*i3).lineA, (*i3A).lineB, 1, 2 ) &&
          isValidMove( pManualDiffHelpList, (*i3).lineA, (*i3A).lineC, 1, 3 ) )      {
         // Empty space for A. A doesn't match B or C. Move it up.
         (*i3A).lineA = (*i3).lineA;
         (*i3).lineA = -1;
         ++i3A;
         ++lineA;
      }

      if( line>lineB && (*i3).lineB != -1 && !(*i3).bAEqB && !(*i3).bBEqC  &&
          isValidMove( pManualDiffHelpList, (*i3).lineB, (*i3B).lineA, 2, 1 ) &&
          isValidMove( pManualDiffHelpList, (*i3).lineB, (*i3B).lineC, 2, 3 ) )
      {
         // Empty space for B. B matches neither A nor C. Move B up.
         (*i3B).lineB = (*i3).lineB;
         (*i3).lineB = -1;
         ++i3B;
         ++lineB;
      }

      if( line>lineC && (*i3).lineC != -1 && !(*i3).bAEqC && !(*i3).bBEqC &&
          isValidMove( pManualDiffHelpList, (*i3).lineC, (*i3C).lineA, 3, 1 ) &&
          isValidMove( pManualDiffHelpList, (*i3).lineC, (*i3C).lineB, 3, 2 ) )
      {
         // Empty space for C. C matches neither A nor B. Move C up.
         (*i3C).lineC = (*i3).lineC;
         (*i3).lineC = -1;
         ++i3C;
         ++lineC;
      }

      if( line>lineA && line>lineB && (*i3).lineA != -1 && (*i3).bAEqB && !(*i3).bAEqC )
      {
         // Empty space for A and B. A matches B, but not C. Move A & B up.
         Diff3LineList::iterator i = lineA > lineB ? i3A   : i3B;
         int                     l = lineA > lineB ? lineA : lineB;

         if ( isValidMove( pManualDiffHelpList, i->lineC, (*i3).lineA, 3, 1 ) &&
              isValidMove( pManualDiffHelpList, i->lineC, (*i3).lineB, 3, 2 ) )
         {
            (*i).lineA = (*i3).lineA;
            (*i).lineB = (*i3).lineB;
            (*i).bAEqB = true;

            (*i3).lineA = -1;
            (*i3).lineB = -1;
            (*i3).bAEqB = false;
            i3A = i;
            i3B = i;
            ++i3A;
            ++i3B;
            lineA=l+1;
            lineB=l+1;
         }
      }
      else if( line>lineA && line>lineC && (*i3).lineA != -1 && (*i3).bAEqC && !(*i3).bAEqB )
      {
         // Empty space for A and C. A matches C, but not B. Move A & C up.
         Diff3LineList::iterator i = lineA > lineC ? i3A   : i3C;
         int                     l = lineA > lineC ? lineA : lineC;

         if ( isValidMove( pManualDiffHelpList, i->lineB, (*i3).lineA, 2, 1 ) &&
              isValidMove( pManualDiffHelpList, i->lineB, (*i3).lineC, 2, 3 ) )
         {
            (*i).lineA = (*i3).lineA;
            (*i).lineC = (*i3).lineC;
            (*i).bAEqC = true;

            (*i3).lineA = -1;
            (*i3).lineC = -1;
            (*i3).bAEqC = false;
            i3A = i;
            i3C = i;
            ++i3A;
            ++i3C;
            lineA=l+1;
            lineC=l+1;
         }
      }
      else if( line>lineB && line>lineC && (*i3).lineB != -1 && (*i3).bBEqC && !(*i3).bAEqC )
      {
         // Empty space for B and C. B matches C, but not A. Move B & C up.
         Diff3LineList::iterator i = lineB > lineC ? i3B   : i3C;
         int                     l = lineB > lineC ? lineB : lineC;

         if ( isValidMove( pManualDiffHelpList, i->lineA, (*i3).lineB, 1, 2 ) &&
              isValidMove( pManualDiffHelpList, i->lineA, (*i3).lineC, 1, 3 ) )
         {
            (*i).lineB = (*i3).lineB;
            (*i).lineC = (*i3).lineC;
            (*i).bBEqC = true;

            (*i3).lineB = -1;
            (*i3).lineC = -1;
            (*i3).bBEqC = false;
            i3B = i;
            i3C = i;
            ++i3B;
            ++i3C;
            lineB=l+1;
            lineC=l+1;
         }
      }

      if ( (*i3).lineA != -1 )
      {
         lineA = line+1;
         i3A = i3;
         ++i3A;
      }
      if ( (*i3).lineB != -1 )
      {
         lineB = line+1;
         i3B = i3;
         ++i3B;
      }
      if ( (*i3).lineC != -1 )
      {
         lineC = line+1;
         i3C = i3;
         ++i3C;
      }
   }

   d3ll.remove( d3l_empty );

/*

   Diff3LineList::iterator it = d3ll.begin();
   int li=0;
   for( ; it!=d3ll.end(); ++it, ++li )
   {
      printf( "%4d %4d %4d %4d  A%c=B A%c=C B%c=C\n",
         li, (*it).lineA, (*it).lineB, (*it).lineC,
         (*it).bAEqB ? '=' : '!', (*it).bAEqC ? '=' : '!', (*it).bBEqC ? '=' : '!' );

   }
*/
}

void DiffBufferInfo::init( Diff3LineList* pD3ll, const Diff3LineVector* pD3lv,
   const LineData* pldA, int sizeA, const LineData* pldB, int sizeB, const LineData* pldC, int sizeC )
{
   m_pDiff3LineList = pD3ll;
   m_pDiff3LineVector = pD3lv;
   m_pLineDataA = pldA;
   m_pLineDataB = pldB;
   m_pLineDataC = pldC;
   m_sizeA = sizeA;
   m_sizeB = sizeB;
   m_sizeC = sizeC;
   Diff3LineList::iterator i3 = pD3ll->begin();
   for( ; i3!=pD3ll->end(); ++i3 )
   {
      i3->m_pDiffBufferInfo = this;
   }
}

void calcWhiteDiff3Lines(
   Diff3LineList& d3ll, const LineData* pldA, const LineData* pldB, const LineData* pldC
   )
{
   Diff3LineList::iterator i3 = d3ll.begin();

   for( ; i3!=d3ll.end(); ++i3 )
   {
      i3->bWhiteLineA = ( (*i3).lineA == -1  ||  pldA==0 ||  pldA[(*i3).lineA].whiteLine() || pldA[(*i3).lineA].bContainsPureComment );
      i3->bWhiteLineB = ( (*i3).lineB == -1  ||  pldB==0 ||  pldB[(*i3).lineB].whiteLine() || pldB[(*i3).lineB].bContainsPureComment );
      i3->bWhiteLineC = ( (*i3).lineC == -1  ||  pldC==0 ||  pldC[(*i3).lineC].whiteLine() || pldC[(*i3).lineC].bContainsPureComment );
   }
}

inline bool equal( QChar c1, QChar c2, bool /*bStrict*/ )
{
   // If bStrict then white space doesn't match

   //if ( bStrict &&  ( c1==' ' || c1=='\t' ) )
   //   return false;

   return c1==c2;
}


// My own diff-invention:
template <class T>
void calcDiff( const T* p1, int size1, const T* p2, int size2, DiffList& diffList, int match, int maxSearchRange )
{
   diffList.clear();

   const T* p1start = p1;
   const T* p2start = p2;
   const T* p1end=p1+size1;
   const T* p2end=p2+size2;
   for(;;)
   {
      int nofEquals = 0;
      while( p1!=p1end &&  p2!=p2end && equal(*p1, *p2, false) )
      {
         ++p1;
         ++p2;
         ++nofEquals;
      }

      bool bBestValid=false;
      int bestI1=0;
      int bestI2=0;
      int i1=0;
      int i2=0;
      for( i1=0; ; ++i1 )
      {
         if ( &p1[i1]==p1end || ( bBestValid && i1>= bestI1+bestI2))
         {
            break;
         }
         for(i2=0;i2<maxSearchRange;++i2)
         {
            if( &p2[i2]==p2end ||  ( bBestValid && i1+i2>=bestI1+bestI2) )
            {
               break;
            }
            else if(  equal( p2[i2], p1[i1], true ) &&
                      ( match==1 ||  abs(i1-i2)<3  || ( &p2[i2+1]==p2end  &&  &p1[i1+1]==p1end ) ||
                         ( &p2[i2+1]!=p2end  &&  &p1[i1+1]!=p1end  && equal( p2[i2+1], p1[i1+1], false ))
                      )
                   )
            {
               if ( i1+i2 < bestI1+bestI2 || bBestValid==false )
               {
                  bestI1 = i1;
                  bestI2 = i2;
                  bBestValid = true;
                  break;
               }
            }
         }
      }

      // The match was found using the strict search. Go back if there are non-strict
      // matches.
      while( bestI1>=1 && bestI2>=1 && equal( p1[bestI1-1], p2[bestI2-1], false ) )
      {
         --bestI1;
         --bestI2;
      }


      bool bEndReached = false;
      if (bBestValid)
      {
         // continue somehow
         Diff d(nofEquals, bestI1, bestI2);
         diffList.push_back( d );

         p1 += bestI1;
         p2 += bestI2;
      }
      else
      {
         // Nothing else to match.
         Diff d(nofEquals, p1end-p1, p2end-p2);
         diffList.push_back( d );

         bEndReached = true; //break;
      }

      // Sometimes the algorithm that chooses the first match unfortunately chooses
      // a match where later actually equal parts don't match anymore.
      // A different match could be achieved, if we start at the end.
      // Do it, if it would be a better match.
      int nofUnmatched = 0;
      const T* pu1 = p1-1;
      const T* pu2 = p2-1;
      while ( pu1>=p1start && pu2>=p2start && equal( *pu1, *pu2, false ) )
      {
         ++nofUnmatched;
         --pu1;
         --pu2;
      }

      Diff d = diffList.back();
      if ( nofUnmatched > 0 )
      {
         // We want to go backwards the nofUnmatched elements and redo
         // the matching
         d = diffList.back();
         Diff origBack = d;
         diffList.pop_back();

         while (  nofUnmatched > 0 )
         {
            if ( d.diff1 > 0  &&  d.diff2 > 0 )
            {
               --d.diff1;
               --d.diff2;
               --nofUnmatched;
            }
            else if ( d.nofEquals > 0 )
            {
               --d.nofEquals;
               --nofUnmatched;
            }

            if ( d.nofEquals==0 && (d.diff1==0 || d.diff2==0) &&  nofUnmatched>0 )
            {
               if ( diffList.empty() )
                  break;
               d.nofEquals += diffList.back().nofEquals;
               d.diff1 += diffList.back().diff1;
               d.diff2 += diffList.back().diff2;
               diffList.pop_back();
               bEndReached = false;
            }
         }

         if ( bEndReached )
            diffList.push_back( origBack );
         else
         {

            p1 = pu1 + 1 + nofUnmatched;
            p2 = pu2 + 1 + nofUnmatched;
            diffList.push_back( d );
         }
      }
      if ( bEndReached )
         break;
   }

#ifndef NDEBUG
   // Verify difflist
   {
      int l1=0;
      int l2=0;
      DiffList::iterator i;
      for( i = diffList.begin(); i!=diffList.end(); ++i )
      {
         l1+= i->nofEquals + i->diff1;
         l2+= i->nofEquals + i->diff2;
      }

      //if( l1!=p1-p1start || l2!=p2-p2start )
      if( l1!=size1 || l2!=size2 )
         assert( false );
   }
#endif
}

bool fineDiff(
   Diff3LineList& diff3LineList,
   int selector,
   const LineData* v1,
   const LineData* v2
   )
{
   // Finetuning: Diff each line with deltas
   ProgressProxy pp;
   int maxSearchLength=500;
   Diff3LineList::iterator i;
   int k1=0;
   int k2=0;
   bool bTextsTotalEqual = true;
   int listSize = diff3LineList.size();
   int listIdx = 0;
   for( i= diff3LineList.begin(); i!= diff3LineList.end(); ++i)
   {
      if      (selector==1){ k1=i->lineA; k2=i->lineB; }
      else if (selector==2){ k1=i->lineB; k2=i->lineC; }
      else if (selector==3){ k1=i->lineC; k2=i->lineA; }
      else assert(false);
      if( (k1==-1 && k2!=-1)  ||  (k1!=-1 && k2==-1) ) bTextsTotalEqual=false;
      if( k1!=-1 && k2!=-1 )
      {
         if ( v1[k1].size != v2[k2].size || memcmp( v1[k1].pLine, v2[k2].pLine, v1[k1].size<<1)!=0 )
         {
            bTextsTotalEqual = false;
            DiffList* pDiffList = new DiffList;
            calcDiff( v1[k1].pLine, v1[k1].size, v2[k2].pLine, v2[k2].size, *pDiffList, 2, maxSearchLength );

            // Optimize the diff list.
            DiffList::iterator dli;
            bool bUsefulFineDiff = false;
            for( dli = pDiffList->begin(); dli!=pDiffList->end(); ++dli)
            {
               if( dli->nofEquals >= 4 )
               {
                  bUsefulFineDiff = true;
                  break;
               }
            }

            for( dli = pDiffList->begin(); dli!=pDiffList->end(); ++dli)
            {
               if( dli->nofEquals < 4  &&  (dli->diff1>0 || dli->diff2>0) 
                  && !( bUsefulFineDiff && dli==pDiffList->begin() )
               )
               {
                  dli->diff1 += dli->nofEquals;
                  dli->diff2 += dli->nofEquals;
                  dli->nofEquals = 0;
               }
            }

            if      (selector==1){ delete (*i).pFineAB; (*i).pFineAB = pDiffList; }
            else if (selector==2){ delete (*i).pFineBC; (*i).pFineBC = pDiffList; }
            else if (selector==3){ delete (*i).pFineCA; (*i).pFineCA = pDiffList; }
            else assert(false);
         }

         if ( (v1[k1].bContainsPureComment || v1[k1].whiteLine()) && (v2[k2].bContainsPureComment || v2[k2].whiteLine()))
         {
            if      (selector==1){ i->bAEqB = true; }
            else if (selector==2){ i->bBEqC = true; }
            else if (selector==3){ i->bAEqC = true; }
            else assert(false);
         }
      }
      ++listIdx;
      pp.setCurrent(double(listIdx)/listSize);
   }
   return bTextsTotalEqual;
}


// Convert the list to a vector of pointers
void calcDiff3LineVector( Diff3LineList& d3ll, Diff3LineVector& d3lv )
{
   d3lv.resize( d3ll.size() );
   Diff3LineList::iterator i;
   int j=0;
   for( i= d3ll.begin(); i!= d3ll.end(); ++i, ++j)
   {
      d3lv[j] = &(*i);
   }
   assert( j==(int)d3lv.size() );
}


