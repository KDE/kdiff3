/***************************************************************************
 *   Copyright (C) 2003-2011 by Joachim Eibl                               *
 *   joachim.eibl at gmx.de                                                *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/
#include "stable.h"
#include "fileaccess.h"
#include "progress.h"
#include "common.h"

#include <QDir>
#include <QRegExp>
#include <QTextStream>
#include <QProcess>

#include <vector>
#include <cstdlib>

#include <klocale.h>
#include <ktemporaryfile.h>
#include <kio/global.h>
#include <kio/job.h>
#include <kio/jobclasses.h>
#include <kmessagebox.h>
#include <kio/jobuidelegate.h>
#include <kio/copyjob.h>

#include <sys/types.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <sys/utime.h>
#include <io.h>
#include <windows.h>
#include <process.h>
#else
#include <unistd.h>          // Needed for creating symbolic links via symlink().
#include <utime.h>
#endif


class FileAccess::Data
{
public:
   Data()
   {
      reset();
   }
   void reset()
   {
      m_url = KUrl();
      m_bValidData = false;
      m_name = QString();
      //m_creationTime = QDateTime();
      //m_accessTime = QDateTime();
      m_bReadable = false;
      m_bExecutable = false;
      m_linkTarget = "";
      //m_fileType = -1;
      m_bLocal = true;
      m_pParent = 0;
   }
   
   KUrl m_url;
   bool m_bLocal;
   bool m_bValidData;

   //QDateTime m_accessTime;
   //QDateTime m_creationTime;
   bool m_bReadable;
   bool m_bExecutable;
   //long m_fileType; // for testing only
   FileAccess* m_pParent;

   QString m_linkTarget;
   //QString m_user;
   //QString m_group;
   QString m_name;
   QString m_localCopy;
   QString m_statusText;  // Might contain an error string, when the last operation didn't succeed.
};


FileAccess::FileAccess( const QString& name, bool bWantToWrite )
{
   m_pData = 0;
   m_bUseData = false;
   setFile( name, bWantToWrite );
}

FileAccess::FileAccess()
{
   m_bUseData = false;
   m_bExists = false;
   m_bFile = false;
   m_bDir  = false;
   m_bSymLink = false;
   m_bWritable = false;
   m_bHidden = false;
   m_pParent = 0;
   m_size = 0;   
}

FileAccess::FileAccess(const FileAccess& other)
{
   m_pData = 0;
   m_bUseData = false;
   *this = other;
}

void FileAccess::createData()
{
   if ( d() == 0 )
   {
      FileAccess* pParent = m_pParent; // backup because in union with m_pData
      m_pData = new Data();
      m_bUseData = true;
      m_pData->m_pParent = pParent;
   }
}

const FileAccess& FileAccess::operator=(const FileAccess& other)
{
   m_size = other.m_size;
   m_filePath = other.m_filePath;
   m_modificationTime = other.m_modificationTime;
   m_bSymLink = other.m_bSymLink;
   m_bFile = other.m_bFile;
   m_bDir = other.m_bDir;
   m_bExists = other.m_bExists;
   m_bWritable = other.m_bWritable;
   m_bHidden = other.m_bHidden;

   if ( other.m_bUseData )
   {
      createData();
      *m_pData = *other.m_pData;
   }
   else
   {
      if ( m_bUseData )
      {
         delete m_pData;
      }
      m_bUseData = false;
      m_pParent = other.parent(); // should be 0 anyway
   }
   return *this;
}

FileAccess::~FileAccess()
{
   if ( m_bUseData )
   {
      if( ! d()->m_localCopy.isEmpty() )
      {
         removeTempFile( d()->m_localCopy );
      }
      delete m_pData;
   }
}

static QString nicePath( const QFileInfo& fi )
{
   QString fp = fi.filePath();
   if ( fp.length()>2 && fp[0] == '.' && fp[1] == '/' )
   {
      return fp.mid(2);
   }
   return fp;
}

// Two kinds of optimization are applied here:
// 1. Speed: don't ask for data as long as it is not needed or cheap to get.
//    When opening a file it is early enough to ask for details.
// 2. Memory usage: Don't store data that is not needed, and avoid redundancy.
//    For recursive directory trees don't store the full path if a parent is available.
//    Store urls only if files are not local.

void FileAccess::setFile( const QFileInfo& fi, FileAccess* pParent )
{
   m_filePath   = nicePath( fi.filePath() ); // remove "./" at start   

   m_bSymLink   = fi.isSymLink();
   if ( m_bSymLink || (!m_bExists  && m_filePath.contains("@@") ) )
   {
      createData();
   }

   if ( m_bUseData )
      d()->m_pParent = pParent;
   else
      m_pParent = pParent;
   
   if ( parent() || d() ) // if a parent is specified then we arrive here because of listing a directory
   {
      m_bFile      = fi.isFile();
      m_bDir       = fi.isDir();
      m_bExists    = fi.exists();
      m_size       = fi.size();
      m_modificationTime = fi.lastModified();
      m_bHidden    = fi.isHidden();

#if defined(Q_WS_WIN)
      m_bWritable    = pParent == 0 || fi.isWritable(); // in certain situations this might become a problem though
#else
      m_bWritable    = fi.isWritable();
#endif
   }
   
   if ( d() != 0 )
   {
#if defined(Q_WS_WIN)
      // On some windows machines in a network this takes very long.
      // and it's not so important anyway.
      d()->m_bReadable    = true;
      d()->m_bExecutable  = false;
#else
      d()->m_bReadable    = fi.isReadable();
      d()->m_bExecutable  = fi.isExecutable();
#endif

      //d()->m_creationTime = fi.created();
      //d()->m_modificationTime = fi.lastModified();
      //d()->m_accessTime = fi.lastRead();
      d()->m_name       = fi.fileName();
      if ( m_bSymLink ) 
         d()->m_linkTarget = fi.readLink();
      d()->m_bLocal = true;
      d()->m_bValidData = true;
      d()->m_url = KUrl( fi.filePath() );
      if ( ! d()->m_url.isValid() )
      {
         d()->m_url.setPath( absoluteFilePath() );
      }

      if ( !m_bExists  && absoluteFilePath().contains("@@") )
      {
         // Try reading a clearcase file
         d()->m_localCopy = FileAccess::tempFileName();
         QString cmd = "cleartool get -to \"" + d()->m_localCopy + "\"  \"" + absoluteFilePath() + "\"";
         QProcess process;
         process.start( cmd );
         process.waitForFinished(-1);
         //::system( cmd.local8Bit() );
         QFile::setPermissions( d()->m_localCopy, QFile::ReadUser | QFile::WriteUser ); // Clearcase creates a write protected file, allow delete.

         QFileInfo fi( d()->m_localCopy );
#if defined(Q_WS_WIN)
         d()->m_bReadable    = true;//fi.isReadable();
         m_bWritable    = true;//fi.isWritable();
         d()->m_bExecutable  = false;//fi.isExecutable();
#else
         d()->m_bReadable    = fi.isReadable();
         d()->m_bExecutable  = fi.isExecutable();
#endif
         //d()->m_creationTime = fi.created();
         //d()->m_accessTime = fi.lastRead();
         m_bExists = fi.exists();
         m_size = fi.size();
      }
   }
}

void FileAccess::setFile( const QString& name, bool bWantToWrite )
{
   m_bExists = false;
   m_bFile = false;
   m_bDir  = false;
   m_bSymLink = false;
   m_size = 0;
   m_modificationTime = QDateTime();

   if ( d()!=0 )
   {  
      d()->reset();
      d()->m_pParent = 0;
   }
   else
      m_pParent = 0;

   // Note: Checking if the filename-string is empty is necessary for Win95/98/ME.
   //       The isFile() / isDir() queries would cause the program to crash.
   //       (This is a Win95-bug which has been corrected only in WinNT/2000/XP.)
   if ( !name.isEmpty() )
   {
      KUrl url( name );
      
      // FileAccess tries to detect if the given name is an URL or a local file.
      // This is a problem if the filename looks like an URL (i.e. contains a colon ':').
      // e.g. "file:f.txt" is a valid filename.
      // Most of the time it is sufficient to check if the file exists locally.
      // 2 Problems remain:
      //   1. When the local file exists and the remote location is wanted nevertheless. (unlikely)
      //   2. When the local file doesn't exist and should be written to.

      bool bExistsLocal = QDir().exists(name);
      if ( url.isLocalFile() || url.isRelative() || !url.isValid() || bExistsLocal ) // assuming that invalid means relative
      {
         QString localName = name;

#if defined(Q_WS_WIN)
         if ( localName.startsWith("/tmp/") )
         {
            // git on Cygwin will put files in /tmp
            // A workaround for the a native kdiff3 binary to find them...
         
            QString cygwinBin = getenv("CYGWIN_BIN");
            if ( !cygwinBin.isEmpty() )
            {
               localName = QString("%1\\..%2").arg(cygwinBin).arg(name);
            }
         }
#endif

         if ( !bExistsLocal && url.isLocalFile() && name.left(5).toLower()=="file:" )
         {
            localName = url.path(); // I want the path without preceding "file:"
         }

         QFileInfo fi( localName );
         setFile( fi, 0 );
      }
      else
      {
         createData();
         d()->m_url = url;
         d()->m_name   = d()->m_url.fileName();
         d()->m_bLocal = false;

         FileAccessJobHandler jh( this ); // A friend, which writes to the parameters of this class!
         jh.stat(2/*all details*/, bWantToWrite); // returns bSuccess, ignored

         m_filePath = name;
         d()->m_bValidData = true; // After running stat() the variables are initialised
                                 // and valid even if the file doesn't exist and the stat
                                 // query failed.
      }
   }
}

void FileAccess::addPath( const QString& txt )
{
   if ( d()!=0 && d()->m_url.isValid() )
   {
      d()->m_url.addPath( txt );
      setFile( d()->m_url.url() );  // reinitialise
   }
   else
   {
      QString slash = (txt.isEmpty() || txt[0]=='/') ? "" : "/";
      setFile( absoluteFilePath() + slash + txt );
   }
}

/*     Filetype:
       S_IFMT     0170000   bitmask for the file type bitfields
       S_IFSOCK   0140000   socket
       S_IFLNK    0120000   symbolic link
       S_IFREG    0100000   regular file
       S_IFBLK    0060000   block device
       S_IFDIR    0040000   directory
       S_IFCHR    0020000   character device
       S_IFIFO    0010000   fifo
       S_ISUID    0004000   set UID bit
       S_ISGID    0002000   set GID bit (see below)
       S_ISVTX    0001000   sticky bit (see below)

       Access:
       S_IRWXU    00700     mask for file owner permissions
       S_IRUSR    00400     owner has read permission
       S_IWUSR    00200     owner has write permission
       S_IXUSR    00100     owner has execute permission
       S_IRWXG    00070     mask for group permissions
       S_IRGRP    00040     group has read permission
       S_IWGRP    00020     group has write permission
       S_IXGRP    00010     group has execute permission
       S_IRWXO    00007     mask for permissions for others (not in group)
       S_IROTH    00004     others have read permission
       S_IWOTH    00002     others have write permisson
       S_IXOTH    00001     others have execute permission
*/

#ifdef KREPLACEMENTS_H
void FileAccess::setUdsEntry( const KIO::UDSEntry& ){}  // not needed if KDE is not available
#else
void FileAccess::setUdsEntry( const KIO::UDSEntry& e )
{
   long acc = 0;
   long fileType = 0;
   QList< uint > fields = e.listFields();
   for( QList< uint >::ConstIterator ei=fields.constBegin(); ei!=fields.constEnd(); ++ei )
   {
      uint f = *ei;
      switch( f )
      {
         case KIO::UDSEntry::UDS_SIZE :              m_size   = e.numberValue(f);   break;
         //case KIO::UDSEntry::UDS_USER :               d()->m_user   = e.stringValue(f);    break;
         //case KIO::UDSEntry::UDS_GROUP :              d()->m_group  = e.stringValue(f);    break;
         case KIO::UDSEntry::UDS_NAME :              m_filePath  = e.stringValue(f);    break;  // During listDir the relative path is given here.
         case KIO::UDSEntry::UDS_MODIFICATION_TIME : m_modificationTime.setTime_t( e.numberValue(f) ); break;
         //case KIO::UDSEntry::UDS_ACCESS_TIME :       d()->m_accessTime.setTime_t( e.numberValue(f) ); break;
         //case KIO::UDSEntry::UDS_CREATION_TIME :     d()->m_creationTime.setTime_t( e.numberValue(f) ); break;
         case KIO::UDSEntry::UDS_LINK_DEST :         d()->m_linkTarget       = e.stringValue(f); break;
         case KIO::UDSEntry::UDS_ACCESS :
         {
            acc = e.numberValue(f);
            d()->m_bReadable   = (acc & S_IRUSR)!=0;
            m_bWritable      = (acc & S_IWUSR)!=0;
            d()->m_bExecutable = (acc & S_IXUSR)!=0;
            break;
         }
         case KIO::UDSEntry::UDS_FILE_TYPE :
         {
            fileType = e.numberValue(f);
            m_bDir     = ( fileType & S_IFMT ) == S_IFDIR;
            m_bFile    = ( fileType & S_IFMT ) == S_IFREG;
            m_bSymLink = ( fileType & S_IFMT ) == S_IFLNK;
            m_bExists  = fileType != 0;
            //d()->m_fileType = fileType;
            break;
         }

         case KIO::UDSEntry::UDS_URL :               // m_url = KUrl( e.stringValue(f) );
                                           break;
         case KIO::UDSEntry::UDS_MIME_TYPE :         break;
         case KIO::UDSEntry::UDS_GUESSED_MIME_TYPE : break;
         case KIO::UDSEntry::UDS_XML_PROPERTIES :    break;
         default: break;
      }
   }

   m_bExists = acc!=0 || fileType!=0;

   d()->m_bLocal = false;
   d()->m_bValidData = true;
   m_bSymLink = !d()->m_linkTarget.isEmpty();
   if ( d()->m_name.isEmpty() )
   {
      int pos = m_filePath.lastIndexOf('/') + 1;
      d()->m_name = m_filePath.mid( pos );
   }
   m_bHidden = d()->m_name[0]=='.';
}
#endif


bool FileAccess::isValid() const       
{   
   return d()==0 ? !m_filePath.isEmpty() : d()->m_bValidData;  
}

bool FileAccess::isFile() const        
{
   if ( parent() || d() )
      return m_bFile;
   else
      return QFileInfo( absoluteFilePath() ).isFile();
}

bool FileAccess::isDir() const         
{   
   if ( parent() || d() )
      return m_bDir;        
   else
      return QFileInfo( absoluteFilePath() ).isDir();
}

bool FileAccess::isSymLink() const     
{   
   return m_bSymLink;    
}

bool FileAccess::exists() const        
{   
   if ( parent() || d())
      return m_bExists;     
   else
      return QFileInfo( absoluteFilePath() ).exists();
}

qint64 FileAccess::size() const        
{
   if ( parent() || d())
      return m_size;
   else
      return QFileInfo( absoluteFilePath() ).size();
}

KUrl FileAccess::url() const           
{  
   if ( d()!=0 )
      return d()->m_url;
   else
   {
      KUrl url( m_filePath );
      if ( ! url.isValid() )
      {
         url.setPath( absoluteFilePath() );
      }
      return url;
   }   
}

bool FileAccess::isLocal() const       
{   
   return d()==0 || d()->m_bLocal;   
}

bool FileAccess::isReadable() const    
{
#if defined(Q_WS_WIN)
   // On some windows machines in a network this takes very long to find out and it's not so important anyway.
   return true;
#else
   if ( d()!=0 )      
      return d()->m_bReadable; 
   else
      return QFileInfo( absoluteFilePath() ).isReadable();
#endif
}

bool FileAccess::isWritable() const    
{
   if ( parent() || d())
      return m_bWritable;
   else
      return QFileInfo( absoluteFilePath() ).isWritable();
}

bool FileAccess::isExecutable() const    
{
#if defined(Q_WS_WIN)
   // On some windows machines in a network this takes very long to find out and it's not so important anyway.
   return true;
#else
   if ( d()!=0 )
      return d()->m_bExecutable;
   else
      return QFileInfo( absoluteFilePath() ).isExecutable();
#endif
}

bool FileAccess::isHidden() const      
{  
   if ( parent() || d() )
      return m_bHidden;   
   else
      return QFileInfo( absoluteFilePath() ).isHidden();
}

QString FileAccess::readLink() const   
{  
   if ( d()!=0 )
      return d()->m_linkTarget;
   else
      return QString();
}

QString FileAccess::absoluteFilePath() const
{  
   if ( parent() != 0 )
      return parent()->absoluteFilePath() + "/" + m_filePath;
   else
      return m_filePath;
}  // Full abs path

// Just the name-part of the path, without parent directories
QString FileAccess::fileName() const   
{   
   if ( d()!=0 )
      return d()->m_name;          
   else if ( parent() )
      return m_filePath;
   else
      return QFileInfo( m_filePath ).fileName();
}

void FileAccess::setSharedName(const QString& name)
{
   if ( name == m_filePath )
      m_filePath = name; // reduce memory because string is only used once.
}

QString FileAccess::filePath() const   
{  
   if ( parent() && parent()->parent() )
      return parent()->filePath() + "/" + m_filePath;
   else
      return m_filePath;   // The path-string that was used during construction
}

FileAccess* FileAccess::parent() const
{
   if ( m_bUseData )
      return d()->m_pParent;
   else
      return m_pParent;
}

FileAccess::Data* FileAccess::d()
{
   if ( m_bUseData )
      return m_pData;
   else
      return 0;
}

const FileAccess::Data* FileAccess::d() const
{
   if ( m_bUseData )
      return m_pData;
   else
      return 0;
}

QString FileAccess::prettyAbsPath() const 
{ 
   return isLocal() ? absoluteFilePath() : d()->m_url.prettyUrl();    
}

/*
QDateTime FileAccess::created() const
{
   if ( d()!=0 )
   {
      if ( isLocal() && d()->m_creationTime.isNull() )
         const_cast<FileAccess*>(this)->d()->m_creationTime = QFileInfo( absoluteFilePath() ).created();
      return ( d()->m_creationTime.isValid() ?  d()->m_creationTime : lastModified() );
   }
   else
   {
      QDateTime created = QFileInfo( absoluteFilePath() ).created();
      return created.isValid() ? created : lastModified();
   }
}
*/

QDateTime FileAccess::lastModified() const
{
   if ( isLocal() && m_modificationTime.isNull() )
      const_cast<FileAccess*>(this)->m_modificationTime = QFileInfo( absoluteFilePath() ).lastModified();
   return m_modificationTime;
}

/*
QDateTime FileAccess::lastRead() const
{
   QDateTime accessTime = d()!=0 ? d()->m_accessTime : QFileInfo( absoluteFilePath() ).lastRead();
   return ( accessTime.isValid() ?  accessTime : lastModified() );
}
*/

static bool interruptableReadFile( QFile& f, void* pDestBuffer, unsigned long maxLength )
{
   ProgressProxy pp;
   const unsigned long maxChunkSize = 100000;
   unsigned long i=0;
   while( i<maxLength )
   {
      unsigned long nextLength = min2( maxLength-i, maxChunkSize );
      unsigned long reallyRead = f.read( (char*)pDestBuffer+i, nextLength );
      if ( reallyRead != nextLength )
      {
         return false;
      }
      i+=reallyRead;

      pp.setCurrent( double(i)/maxLength );
      if ( pp.wasCancelled() ) return false;
   }
   return true;
}

bool FileAccess::readFile( void* pDestBuffer, unsigned long maxLength )
{
   if ( d()!=0 && !d()->m_localCopy.isEmpty() )
   {
      QFile f( d()->m_localCopy );
      if ( f.open( QIODevice::ReadOnly ) )
         return interruptableReadFile(f, pDestBuffer, maxLength);// maxLength == f.read( (char*)pDestBuffer, maxLength );
   }
   else if (isLocal())
   {
      QFile f( absoluteFilePath() );

      if ( f.open( QIODevice::ReadOnly ) )
         return interruptableReadFile(f, pDestBuffer, maxLength); //maxLength == f.read( (char*)pDestBuffer, maxLength );
   }
   else
   {
      FileAccessJobHandler jh( this );
      return jh.get( pDestBuffer, maxLength );
   }
   return false;
}

bool FileAccess::writeFile( const void* pSrcBuffer, unsigned long length )
{
   ProgressProxy pp;
   if ( isLocal() )
   {
      QFile f( absoluteFilePath() );
      if ( f.open( QIODevice::WriteOnly ) )
      {
         const unsigned long maxChunkSize = 100000;
         unsigned long i=0;
         while( i<length )
         {
            unsigned long nextLength = min2( length-i, maxChunkSize );
            unsigned long reallyWritten = f.write( (char*)pSrcBuffer+i, nextLength );
            if ( reallyWritten != nextLength )
            {
               return false;
            }
            i+=reallyWritten;

            pp.setCurrent( double(i)/length );
            if ( pp.wasCancelled() ) return false;
         }
         f.close();
#ifndef _WIN32
         if ( isExecutable() )  // value is true if the old file was executable
         {
            // Preserve attributes
            f.setPermissions(f.permissions() | QFile::ExeUser);
            //struct stat srcFileStatus;
            //int statResult = ::stat( filePath().toLocal8Bit().constData(), &srcFileStatus );
            //if (statResult==0)
            //{
            //   ::chmod ( filePath().toLocal8Bit().constData(), srcFileStatus.st_mode | S_IXUSR );
            //}
         }
#endif

         return true;
      }
   }
   else
   {
      FileAccessJobHandler jh( this );
      return jh.put( pSrcBuffer, length, true /*overwrite*/ );
   }
   return false;
}

bool FileAccess::copyFile( const QString& dest )
{
   FileAccessJobHandler jh( this );
   return jh.copyFile( dest );   // Handles local and remote copying.
}

bool FileAccess::rename( const QString& dest )
{
   FileAccessJobHandler jh( this );
   return jh.rename( dest );
}

bool FileAccess::removeFile()
{
   if ( isLocal() )
   {
      return QDir().remove( absoluteFilePath() );
   }
   else
   {
      FileAccessJobHandler jh( this );
      return jh.removeFile( absoluteFilePath() );
   }
}

bool FileAccess::removeFile( const QString& name ) // static
{
   return FileAccess(name).removeFile();
}

bool FileAccess::listDir( t_DirectoryList* pDirList, bool bRecursive, bool bFindHidden,
   const QString& filePattern, const QString& fileAntiPattern, const QString& dirAntiPattern,
   bool bFollowDirLinks, bool bUseCvsIgnore )
{
   FileAccessJobHandler jh( this );
   return jh.listDir( pDirList, bRecursive, bFindHidden, filePattern, fileAntiPattern,
                      dirAntiPattern, bFollowDirLinks, bUseCvsIgnore );
}

QString FileAccess::tempFileName()
{
   #ifdef KREPLACEMENTS_H

      QString fileName;
      #if defined(_WIN32) || defined(Q_OS_OS2)
         QString tmpDir = getenv("TEMP");
      #else
         QString tmpDir = "/tmp";
      #endif
      for(int i=0; ;++i)
      {
         // short filenames for WIN98 because for system() the command must not exceed 120 characters.
         #ifdef _WIN32
         if ( QSysInfo::WindowsVersion & QSysInfo::WV_DOS_based ) // Win95, 98, ME
            fileName = tmpDir + "\\" + QString::number(i);
         else
            fileName = tmpDir + "/kdiff3_" + QString::number(_getpid()) + "_" + QString::number(i) +".tmp";
         #else
            fileName = tmpDir + "/kdiff3_" + QString::number(getpid()) + "_" + QString::number(i) +".tmp";
         #endif
         if ( ! FileAccess::exists(fileName) && 
              QFile(fileName).open(QIODevice::WriteOnly) ) // open, truncate and close the file, true if successful
         {
            break;
         }
      }
      return QDir::convertSeparators(fileName+".2");

   #else  // using KDE

      KTemporaryFile tmpFile;
      tmpFile.open();
      //tmpFile.setAutoDelete( true );  // We only want the name. Delete the precreated file immediately.
      QString name = tmpFile.fileName()+".2";
      tmpFile.close();
      return name;

   #endif
}

bool FileAccess::removeTempFile( const QString& name ) // static
{
   if (name.endsWith(".2"))
      FileAccess(name.left(name.length()-2)).removeFile();
   return FileAccess(name).removeFile();
}


bool FileAccess::makeDir( const QString& dirName )
{
   FileAccessJobHandler fh(0);
   return fh.mkDir( dirName );
}

bool FileAccess::removeDir( const QString& dirName )
{
   FileAccessJobHandler fh(0);
   return fh.rmDir( dirName );
}

#if defined(_WIN32) || defined(Q_OS_OS2)
bool FileAccess::symLink( const QString& /*linkTarget*/, const QString& /*linkLocation*/ )
{
   return false;
}
#else
bool FileAccess::symLink( const QString& linkTarget, const QString& linkLocation )
{
   return 0==::symlink( linkTarget.toLocal8Bit().constData(), linkLocation.toLocal8Bit().constData() );
   //FileAccessJobHandler fh(0);
   //return fh.symLink( linkTarget, linkLocation );
}
#endif

bool FileAccess::exists( const QString& name )
{
   FileAccess fa( name );
   return fa.exists();
}

// If the size couldn't be determined by stat() then the file is copied to a local temp file.
qint64 FileAccess::sizeForReading()
{
   if ( !isLocal() && m_size == 0 )
   {
      // Size couldn't be determined. Copy the file to a local temp place.
      QString localCopy = tempFileName();
      bool bSuccess = copyFile( localCopy );
      if ( bSuccess )
      {
         QFileInfo fi( localCopy );
         m_size = fi.size();
         d()->m_localCopy = localCopy;
         return m_size;
      }
      else
      {
         return 0;
      }
   }
   else
      return size();
}

QString FileAccess::getStatusText()
{
   return d()==0 ? QString() : d()->m_statusText;
}

void FileAccess::setStatusText( const QString& s )
{
   if (  ! s.isEmpty() || d() != 0 )
   {
      createData();
      d()->m_statusText = s;
   }
}

QString FileAccess::cleanPath( const QString& path ) // static
{
   KUrl url(path);
   if ( url.isLocalFile() || ! url.isValid() )
   {
      return QDir().cleanPath( path );
   }
   else
   {
      return path;
   }
}

bool FileAccess::createBackup( const QString& bakExtension )
{
   if ( exists() )
   {
      createData();
      setFile( absoluteFilePath() ); // make sure Data is initialized
      // First rename the existing file to the bak-file. If a bak-file file exists, delete that.
      QString bakName = absoluteFilePath() + bakExtension;
      FileAccess bakFile( bakName, true /*bWantToWrite*/ );
      if ( bakFile.exists() )
      {
         bool bSuccess = bakFile.removeFile();
         if ( !bSuccess )
         {
            setStatusText( i18n("While trying to make a backup, deleting an older backup failed. \nFilename: ") + bakName );
            return false;
         }
      }
      bool bSuccess = rename( bakName );
      if (!bSuccess)
      {
         setStatusText( i18n("While trying to make a backup, renaming failed. \nFilenames: ") +
               absoluteFilePath() + " -> " + bakName );
         return false;
      }
   }
   return true;
}

FileAccessJobHandler::FileAccessJobHandler( FileAccess* pFileAccess )
{
   m_pFileAccess = pFileAccess;
   m_bSuccess = false;
}

bool FileAccessJobHandler::stat( int detail, bool bWantToWrite )
{
   m_bSuccess = false;
   m_pFileAccess->setStatusText( QString() );
   KIO::StatJob* pStatJob = KIO::stat( m_pFileAccess->url(), 
         bWantToWrite ? KIO::StatJob::DestinationSide : KIO::StatJob::SourceSide, 
         detail, KIO::HideProgressInfo );

   connect( pStatJob, SIGNAL(result(KJob*)), this, SLOT(slotStatResult(KJob*)));

   ProgressProxy::enterEventLoop( pStatJob, i18n("Getting file status: %1",m_pFileAccess->prettyAbsPath()) );

   return m_bSuccess;
}

void FileAccessJobHandler::slotStatResult(KJob* pJob)
{
   if ( pJob->error() )
   {
      //pJob->uiDelegate()->showErrorMessage();
      m_pFileAccess->m_bExists = false;
      m_bSuccess = true;
   }
   else
   {
      m_bSuccess = true;

      m_pFileAccess->d()->m_bValidData = true;
      const KIO::UDSEntry e = static_cast<KIO::StatJob*>(pJob)->statResult();

      m_pFileAccess->setUdsEntry( e );
   }

   ProgressProxy::exitEventLoop();
}


bool FileAccessJobHandler::get(void* pDestBuffer, long maxLength )
{
   ProgressProxyExtender pp; // Implicitly used in slotPercent()
   if ( maxLength>0 && !pp.wasCancelled() )
   {
      KIO::TransferJob* pJob = KIO::get( m_pFileAccess->url(), KIO::NoReload );
      m_transferredBytes = 0;
      m_pTransferBuffer = (char*)pDestBuffer;
      m_maxLength = maxLength;
      m_bSuccess = false;
      m_pFileAccess->setStatusText( QString() );

      connect( pJob, SIGNAL(result(KJob*)), this, SLOT(slotSimpleJobResult(KJob*)));
      connect( pJob, SIGNAL(data(KJob*,const QByteArray &)), this, SLOT(slotGetData(KJob*, const QByteArray&)));
      connect( pJob, SIGNAL(percent(KJob*,unsigned long)), &pp, SLOT(slotPercent(KJob*, unsigned long)));

      ProgressProxy::enterEventLoop( pJob, i18n("Reading file: %1",m_pFileAccess->prettyAbsPath()) );
      return m_bSuccess;
   }
   else
      return true;
}

void FileAccessJobHandler::slotGetData( KJob* pJob, const QByteArray& newData )
{
   if ( pJob->error() )
   {
      pJob->uiDelegate()->showErrorMessage();
   }
   else
   {
      qint64 length = min2( qint64(newData.size()), m_maxLength - m_transferredBytes );
      ::memcpy( m_pTransferBuffer + m_transferredBytes, newData.data(), newData.size() );
      m_transferredBytes += length;
   }
}

bool FileAccessJobHandler::put(const void* pSrcBuffer, long maxLength, bool bOverwrite, bool bResume, int permissions )
{
   ProgressProxyExtender pp; // Implicitly used in slotPercent()
   if ( maxLength>0 )
   {
      KIO::TransferJob* pJob = KIO::put( m_pFileAccess->url(), permissions, 
         KIO::HideProgressInfo | (bOverwrite ? KIO::Overwrite : KIO::DefaultFlags) | (bResume ? KIO::Resume : KIO::DefaultFlags) );
      m_transferredBytes = 0;
      m_pTransferBuffer = (char*)pSrcBuffer;
      m_maxLength = maxLength;
      m_bSuccess = false;
      m_pFileAccess->setStatusText( QString() );

      connect( pJob, SIGNAL(result(KJob*)), this, SLOT(slotPutJobResult(KJob*)));
      connect( pJob, SIGNAL(dataReq(KIO::Job*, QByteArray&)), this, SLOT(slotPutData(KIO::Job*, QByteArray&)));
      connect( pJob, SIGNAL(percent(KJob*,unsigned long)), &pp, SLOT(slotPercent(KJob*, unsigned long)));

      ProgressProxy::enterEventLoop( pJob, i18n("Writing file: %1",m_pFileAccess->prettyAbsPath()) );
      return m_bSuccess;
   }
   else
      return true;
}

void FileAccessJobHandler::slotPutData( KIO::Job* pJob, QByteArray& data )
{
   if ( pJob->error() )
   {
      pJob->uiDelegate()->showErrorMessage();
   }
   else
   {
      qint64 maxChunkSize = 100000;
      qint64 length = min2( maxChunkSize, m_maxLength - m_transferredBytes );
      data.resize( length );
      if ( data.size()==length )
      {
         if ( length>0 )
         {
            ::memcpy( data.data(), m_pTransferBuffer + m_transferredBytes, data.size() );
            m_transferredBytes += length;
         }
      }
      else
      {
         KMessageBox::error( ProgressProxy::getDialog(), i18n("Out of memory") );
         data.resize(0);
         m_bSuccess = false;
      }
   }
}

void FileAccessJobHandler::slotPutJobResult(KJob* pJob)
{
   if ( pJob->error() )
   {
      pJob->uiDelegate()->showErrorMessage();
   }
   else
   {
      m_bSuccess = (m_transferredBytes == m_maxLength); // Special success condition
   }
   ProgressProxy::exitEventLoop();  // Close the dialog, return from exec()
}

bool FileAccessJobHandler::mkDir( const QString& dirName )
{
   KUrl dirURL = KUrl( dirName );
   if ( dirName.isEmpty() )
      return false;
   else if ( dirURL.isLocalFile() || dirURL.isRelative() )
   {
      return QDir().mkdir( dirURL.path() );
   }
   else
   {
      m_bSuccess = false;
      KIO::SimpleJob* pJob = KIO::mkdir( dirURL );
      connect( pJob, SIGNAL(result(KJob*)), this, SLOT(slotSimpleJobResult(KJob*)));

      ProgressProxy::enterEventLoop( pJob, i18n("Making directory: %1", dirName) );
      return m_bSuccess;
   }
}

bool FileAccessJobHandler::rmDir( const QString& dirName )
{
   KUrl dirURL = KUrl( dirName );
   if ( dirName.isEmpty() )
      return false;
   else if ( dirURL.isLocalFile() )
   {
      return QDir().rmdir( dirURL.path() );
   }
   else
   {
      m_bSuccess = false;
      KIO::SimpleJob* pJob = KIO::rmdir( dirURL );
      connect( pJob, SIGNAL(result(KJob*)), this, SLOT(slotSimpleJobResult(KJob*)));

      ProgressProxy::enterEventLoop(pJob, i18n("Removing directory: %1",dirName));
      return m_bSuccess;
   }
}

bool FileAccessJobHandler::removeFile( const QString& fileName )
{
   if ( fileName.isEmpty() )
      return false;
   else
   {
      m_bSuccess = false;
      KIO::SimpleJob* pJob = KIO::file_delete( fileName, KIO::HideProgressInfo );
      connect( pJob, SIGNAL(result(KJob*)), this, SLOT(slotSimpleJobResult(KJob*)));

      ProgressProxy::enterEventLoop( pJob, i18n("Removing file: %1",fileName) );
      return m_bSuccess;
   }
}

bool FileAccessJobHandler::symLink( const QString& linkTarget, const QString& linkLocation )
{
   if ( linkTarget.isEmpty() || linkLocation.isEmpty() )
      return false;
   else
   {
      m_bSuccess = false;
      KIO::CopyJob* pJob = KIO::link( linkTarget, linkLocation, KIO::HideProgressInfo );
      connect( pJob, SIGNAL(result(KJob*)), this, SLOT(slotSimpleJobResult(KJob*)));

      ProgressProxy::enterEventLoop( pJob,
         i18n("Creating symbolic link: %1 -> %2",linkLocation,linkTarget) );
      return m_bSuccess;
   }
}

bool FileAccessJobHandler::rename( const QString& dest )
{
   if ( dest.isEmpty() )
      return false;

   KUrl kurl( dest );
   if ( !kurl.isValid() )
      kurl = KUrl( QDir().absoluteFilePath(dest) ); // assuming that invalid means relative

   if ( m_pFileAccess->isLocal() && kurl.isLocalFile() )
   {
      return QDir().rename( m_pFileAccess->absoluteFilePath(), kurl.path() );
   }
   else
   {
      ProgressProxyExtender pp;
      int permissions=-1;
      m_bSuccess = false;
      KIO::FileCopyJob* pJob = KIO::file_move( m_pFileAccess->url(), kurl, permissions, KIO::HideProgressInfo );
      connect( pJob, SIGNAL(result(KJob*)), this, SLOT(slotSimpleJobResult(KJob*)));
      connect( pJob, SIGNAL(percent(KJob*,unsigned long)), this, SLOT(slotPercent(KJob*, unsigned long)));

      ProgressProxy::enterEventLoop( pJob,
         i18n("Renaming file: %1 -> %2",m_pFileAccess->prettyAbsPath(),dest) );
      return m_bSuccess;
   }
}

void FileAccessJobHandler::slotSimpleJobResult(KJob* pJob)
{
   if ( pJob->error() )
   {
      pJob->uiDelegate()->showErrorMessage();
   }
   else
   {
      m_bSuccess = true;
   }
   ProgressProxy::exitEventLoop();  // Close the dialog, return from exec()
}


// Copy local or remote files.
bool FileAccessJobHandler::copyFile( const QString& dest )
{
   ProgressProxyExtender pp;
   KUrl destUrl( dest );
   m_pFileAccess->setStatusText( QString() );
   if ( ! m_pFileAccess->isLocal() || ! destUrl.isLocalFile() ) // if either url is nonlocal
   {
      int permissions = (m_pFileAccess->isExecutable()?0111:0)+(m_pFileAccess->isWritable()?0222:0)+(m_pFileAccess->isReadable()?0444:0);
      m_bSuccess = false;
      KIO::FileCopyJob* pJob = KIO::file_copy ( m_pFileAccess->url(), destUrl, permissions, KIO::HideProgressInfo );
      connect( pJob, SIGNAL(result(KJob*)), this, SLOT(slotSimpleJobResult(KJob*)));
      connect( pJob, SIGNAL(percent(KJob*,unsigned long)), &pp, SLOT(slotPercent(KJob*, unsigned long)));
      ProgressProxy::enterEventLoop( pJob,
         i18n("Copying file: %1 -> %2",m_pFileAccess->prettyAbsPath(),dest) );

      return m_bSuccess;
      // Note that the KIO-slave preserves the original date, if this is supported.
   }

   // Both files are local:
   QString srcName = m_pFileAccess->absoluteFilePath();
   QString destName = dest;
   QFile srcFile( srcName );
   QFile destFile( destName );
   bool bReadSuccess = srcFile.open( QIODevice::ReadOnly );
   if ( bReadSuccess == false )
   {
      m_pFileAccess->setStatusText( i18n("Error during file copy operation: Opening file for reading failed. Filename: %1",srcName) );
      return false;
   }
   bool bWriteSuccess = destFile.open( QIODevice::WriteOnly );
   if ( bWriteSuccess == false )
   {
      m_pFileAccess->setStatusText( i18n("Error during file copy operation: Opening file for writing failed. Filename: %1",destName) );
      return false;
   }

#if QT_VERSION==230
   typedef long Q_LONG;
#endif
   std::vector<char> buffer(100000);
   qint64 bufSize = buffer.size();
   qint64 srcSize = srcFile.size();
   while ( srcSize > 0 && !pp.wasCancelled() )
   {
      qint64 readSize = srcFile.read( &buffer[0], min2( srcSize, bufSize ) );
      if ( readSize==-1 || readSize==0 )
      {
         m_pFileAccess->setStatusText( i18n("Error during file copy operation: Reading failed. Filename: %1",srcName) );
         return false;
      }
      srcSize -= readSize;
      while ( readSize > 0 )
      {
         qint64 writeSize = destFile.write( &buffer[0], readSize );
         if ( writeSize==-1 || writeSize==0 )
         {
            m_pFileAccess->setStatusText( i18n("Error during file copy operation: Writing failed. Filename: %1",destName) );
            return false;
         }
         readSize -= writeSize;
      }
      destFile.flush();
      pp.setCurrent( (double)(srcFile.size()-srcSize)/srcFile.size(), false );
   }
   srcFile.close();
   destFile.close();

   // Update the times of the destFile
#ifdef _WIN32
   struct _stat srcFileStatus;
   int statResult = ::_stat( srcName.toLocal8Bit().constData(), &srcFileStatus );
   if (statResult==0)
   {
      _utimbuf destTimes;
      destTimes.actime = srcFileStatus.st_atime;/* time of last access */
      destTimes.modtime = srcFileStatus.st_mtime;/* time of last modification */

      _utime ( destName.toLocal8Bit().constData(), &destTimes );
      _chmod ( destName.toLocal8Bit().constData(), srcFileStatus.st_mode );
   }
#else
   struct stat srcFileStatus;
   int statResult = ::stat( srcName.toLocal8Bit().constData(), &srcFileStatus );
   if (statResult==0)
   {
      utimbuf destTimes;
      destTimes.actime = srcFileStatus.st_atime;/* time of last access */
      destTimes.modtime = srcFileStatus.st_mtime;/* time of last modification */

      utime ( destName.toLocal8Bit().constData(), &destTimes );
      chmod ( destName.toLocal8Bit().constData(), srcFileStatus.st_mode );
   }
#endif
   return true;
}

bool wildcardMultiMatch( const QString& wildcard, const QString& testString, bool bCaseSensitive )
{
   static QHash<QString,QRegExp> s_patternMap;

   QStringList sl = wildcard.split( ";" );

   for ( QStringList::Iterator it = sl.begin(); it != sl.end(); ++it )
   {
      QHash<QString,QRegExp>::iterator patIt = s_patternMap.find( *it );
      if ( patIt == s_patternMap.end() )
      {
         QRegExp pattern( *it, bCaseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive, QRegExp::Wildcard );
         patIt = s_patternMap.insert( *it, pattern );
      }
      
      if ( patIt.value().exactMatch( testString ) )
         return true;
   }

   return false;
}


// class CvsIgnoreList from Cervisia cvsdir.cpp
//    Copyright (C) 1999-2002 Bernd Gehrmann <bernd at mail.berlios.de>
// with elements from class StringMatcher
//    Copyright (c) 2003 Andre Woebbeking <Woebbeking at web.de>
// Modifications for KDiff3 by Joachim Eibl
class CvsIgnoreList
{
public:
    CvsIgnoreList(){}
    void init(FileAccess& dir, bool bUseLocalCvsIgnore );
    bool matches(const QString& fileName, bool bCaseSensitive ) const;

private:
    void addEntriesFromString(const QString& str);
    void addEntriesFromFile(const QString& name);
    void addEntry(const QString& entry);

    QStringList m_exactPatterns;
    QStringList m_startPatterns;
    QStringList m_endPatterns;
    QStringList m_generalPatterns;
};


void CvsIgnoreList::init( FileAccess& dir, bool bUseLocalCvsIgnore )
{
   static const char *ignorestr = ". .. core RCSLOG tags TAGS RCS SCCS .make.state "
           ".nse_depinfo #* .#* cvslog.* ,* CVS CVS.adm .del-* *.a *.olb *.o *.obj "
           "*.so *.Z *~ *.old *.elc *.ln *.bak *.BAK *.orig *.rej *.exe _$* *$";

   addEntriesFromString(QString::fromLatin1(ignorestr));
   addEntriesFromFile(QDir::homePath() + "/.cvsignore");
   addEntriesFromString(QString::fromLocal8Bit(::getenv("CVSIGNORE")));

   if (bUseLocalCvsIgnore)
   {
      FileAccess file(dir);
      file.addPath( ".cvsignore" );
      int size = file.exists() ? file.sizeForReading() : 0;
      if ( size>0 )
      {
         char* buf=new char[size];
         if (buf!=0)
         {
            file.readFile( buf, size );
            int pos1 = 0;
            for ( int pos = 0; pos<=size; ++pos )
            {
               if( pos==size || buf[pos]==' ' || buf[pos]=='\t' || buf[pos]=='\n' || buf[pos]=='\r' )
               {
                  if (pos>pos1)
                  {
                     addEntry( QString::fromLatin1( &buf[pos1], pos-pos1 ) );
                  }
                  ++pos1;
               }
            }
            delete buf;
         }
      }
   }
}


void CvsIgnoreList::addEntriesFromString(const QString& str)
{
    int posLast(0);
    int pos;
    while ((pos = str.indexOf(' ', posLast)) >= 0)
    {
        if (pos > posLast)
            addEntry(str.mid(posLast, pos - posLast));
        posLast = pos + 1;
    }

    if (posLast < static_cast<int>(str.length()))
        addEntry(str.mid(posLast));
}


void CvsIgnoreList::addEntriesFromFile(const QString &name)
{
    QFile file(name);

    if( file.open(QIODevice::ReadOnly) )
    {
        QTextStream stream(&file);
        while( !stream.atEnd() )
        {
            addEntriesFromString(stream.readLine());
        }
    }
}

void CvsIgnoreList::addEntry(const QString& pattern)
{
   if (pattern != QString("!"))
   {
      if (pattern.isEmpty())    return;

      // The general match is general but slow.
      // Special tests for '*' and '?' at the beginning or end of a pattern
      // allow fast checks.

      // Count number of '*' and '?'
      unsigned int nofMetaCharacters = 0;

      const QChar* pos;
      pos = pattern.unicode();
      const QChar* posEnd;
      posEnd=pos + pattern.length();
      while (pos < posEnd)
      {
         if( *pos==QChar('*') || *pos==QChar('?') )  ++nofMetaCharacters;
         ++pos;
      }

      if ( nofMetaCharacters==0 )
      {
         m_exactPatterns.append(pattern);
      }
      else if ( nofMetaCharacters==1 )
      {
         if ( pattern.at(0) == QChar('*') )
         {
            m_endPatterns.append( pattern.right( pattern.length() - 1) );
         }
         else if (pattern.at(pattern.length() - 1) == QChar('*'))
         {
            m_startPatterns.append( pattern.left( pattern.length() - 1) );
         }
         else
         {
            m_generalPatterns.append(pattern.toLocal8Bit());
         }
      }
      else
      {
         m_generalPatterns.append(pattern.toLocal8Bit());
      }
   }
   else
   {
      m_exactPatterns.clear();
      m_startPatterns.clear();
      m_endPatterns.clear();
      m_generalPatterns.clear();
   }
}

bool CvsIgnoreList::matches(const QString& text, bool bCaseSensitive ) const
{
    if ( m_exactPatterns.indexOf(text) >=0 )
    {
        return true;
    }

    QStringList::ConstIterator it;
    QStringList::ConstIterator itEnd;
    for ( it=m_startPatterns.begin(), itEnd=m_startPatterns.end(); it != itEnd; ++it)
    {
        if (text.startsWith(*it))
        {
            return true;
        }
    }

    for ( it = m_endPatterns.begin(), itEnd=m_endPatterns.end(); it != itEnd; ++it)
    {
        if (text.mid( text.length() - (*it).length() )==*it)  //(text.endsWith(*it))
        {
            return true;
        }
    }

    /*
    for (QValueList<QCString>::const_iterator it(m_generalPatterns.begin()),
                                              itEnd(m_generalPatterns.end());
         it != itEnd; ++it)
    {
        if (::fnmatch(*it, text.local8Bit(), FNM_PATHNAME) == 0)
        {
            return true;
        }
    }
    */


   for ( it = m_generalPatterns.begin(); it != m_generalPatterns.end(); ++it )
   {
      QRegExp pattern( *it, bCaseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive, QRegExp::Wildcard );
      if ( pattern.exactMatch( text ) )
         return true;
   }

   return false;
}

static bool cvsIgnoreExists( t_DirectoryList* pDirList )
{
   t_DirectoryList::iterator i;
   for( i = pDirList->begin(); i!=pDirList->end(); ++i )
   {
      if ( i->fileName()==".cvsignore" )
         return true;
   }
   return false;
}

bool FileAccessJobHandler::listDir( t_DirectoryList* pDirList, bool bRecursive, bool bFindHidden, const QString& filePattern,
   const QString& fileAntiPattern, const QString& dirAntiPattern, bool bFollowDirLinks, bool bUseCvsIgnore )
{
   ProgressProxyExtender pp;
   m_pDirList = pDirList;
   m_pDirList->clear();
   m_bFindHidden = bFindHidden;
   m_bRecursive = bRecursive;
   m_bFollowDirLinks = bFollowDirLinks;  // Only relevant if bRecursive==true.
   m_fileAntiPattern = fileAntiPattern;
   m_filePattern = filePattern;
   m_dirAntiPattern = dirAntiPattern;

   if ( pp.wasCancelled() )
      return true; // Cancelled is not an error.

   pp.setInformation( i18n("Reading directory: ") + m_pFileAccess->absoluteFilePath(), 0, false );

   if( m_pFileAccess->isLocal() )
   {
      QString currentPath = QDir::currentPath();
      m_bSuccess = QDir::setCurrent( m_pFileAccess->absoluteFilePath() );
      if ( m_bSuccess )
      {
#ifndef _WIN32
         m_bSuccess = true;
         QDir dir( "." );

         dir.setSorting( QDir::Name | QDir::DirsFirst );
         dir.setFilter( QDir::Files | QDir::Dirs | /* from KDE3 QDir::TypeMaskDirs | */ QDir::Hidden | QDir::System );

         QFileInfoList fiList = dir.entryInfoList();
         if ( fiList.isEmpty() )
         {
            // No Permission to read directory or other error.
            m_bSuccess = false;
         }
         else
         {
            foreach ( QFileInfo fi, fiList )       // for each file...
            {
               if ( fi.fileName() == "." ||  fi.fileName()==".." )
                  continue;

               FileAccess fa;
               fa.setFile( fi, m_pFileAccess );
               pDirList->push_back( fa );
            }
         }
#else
         QString pattern ="*.*";
         WIN32_FIND_DATA findData;

         Qt::HANDLE searchHandle = FindFirstFileW( (const wchar_t*)pattern.utf16(), &findData );

         if ( searchHandle != INVALID_HANDLE_VALUE )
         {
            QString absPath = m_pFileAccess->absoluteFilePath();
            QString relPath = m_pFileAccess->filePath();
            bool bFirst=true;
            while( ! pp.wasCancelled() )
            {
               if (!bFirst)
               {
                  if ( ! FindNextFileW(searchHandle,&findData) )
                     break;
               }
               bFirst = false;
               FileAccess fa;

               fa.m_filePath = QString::fromUtf16((const ushort*)findData.cFileName);
               if ( fa.m_filePath!="." && fa.m_filePath!=".." )
               {               
                  fa.m_size = ( qint64( findData.nFileSizeHigh ) << 32 ) + findData.nFileSizeLow;

                  FILETIME ft;
                  SYSTEMTIME t;
                  FileTimeToLocalFileTime( &findData.ftLastWriteTime, &ft ); FileTimeToSystemTime(&ft,&t);
                  fa.m_modificationTime = QDateTime( QDate(t.wYear, t.wMonth, t.wDay), QTime(t.wHour, t.wMinute, t.wSecond) );
                  //FileTimeToLocalFileTime( &findData.ftLastAccessTime, &ft ); FileTimeToSystemTime(&ft,&t);
                  //fa.m_accessTime       = QDateTime( QDate(t.wYear, t.wMonth, t.wDay), QTime(t.wHour, t.wMinute, t.wSecond) );
                  //FileTimeToLocalFileTime( &findData.ftCreationTime, &ft ); FileTimeToSystemTime(&ft,&t);
                  //fa.m_creationTime     = QDateTime( QDate(t.wYear, t.wMonth, t.wDay), QTime(t.wHour, t.wMinute, t.wSecond) );

                  int  a = findData.dwFileAttributes;
                  fa.m_bWritable   = ( a & FILE_ATTRIBUTE_READONLY) == 0;
                  fa.m_bDir        = ( a & FILE_ATTRIBUTE_DIRECTORY ) != 0;
                  fa.m_bFile       = !fa.m_bDir;
                  fa.m_bHidden     = ( a & FILE_ATTRIBUTE_HIDDEN) != 0;

                  //fa.m_bExecutable = false; // Useless on windows
                  fa.m_bExists     = true;
                  //fa.m_bReadable   = true;
                  //fa.m_bLocal      = true;
                  //fa.m_bValidData  = true;
                  fa.m_bSymLink    = false;
                  //fa.m_fileType    = 0;


                  //fa.m_filePath = fa.m_name;
                  //fa.m_absoluteFilePath = absPath + "/" + fa.m_name;
                  //fa.m_url.setPath( fa.m_absoluteFilePath );
                  if ( fa.d() )
                     fa.m_pData->m_pParent = m_pFileAccess;
                  else
                     fa.m_pParent = m_pFileAccess;
                  pDirList->push_back( fa );
               }
            }
            FindClose( searchHandle );
         }
         else
         {
            QDir::setCurrent( currentPath ); // restore current path
            return false;
         }
#endif
      }
      QDir::setCurrent( currentPath ); // restore current path
   }
   else
   {
      KIO::ListJob* pListJob=0;
      pListJob = KIO::listDir( m_pFileAccess->url(), KIO::HideProgressInfo, true /*bFindHidden*/ );

      m_bSuccess = false;
      if ( pListJob!=0 )
      {
         connect( pListJob, SIGNAL( entries( KIO::Job*, const KIO::UDSEntryList& ) ),
                  this,     SLOT( slotListDirProcessNewEntries( KIO::Job*, const KIO::UDSEntryList& )) );
         connect( pListJob, SIGNAL( result( KJob* )),
                  this,     SLOT( slotSimpleJobResult(KJob*) ) );

         connect( pListJob, SIGNAL( infoMessage(KJob*, const QString&)),
                  &pp,      SLOT( slotListDirInfoMessage(KJob*, const QString&) ));

         // This line makes the transfer via fish unreliable.:-(
         //connect( pListJob, SIGNAL(percent(KJob*,unsigned long)), this, SLOT(slotPercent(KJob*, unsigned long)));

         ProgressProxy::enterEventLoop( pListJob,
            i18n("Listing directory: %1",m_pFileAccess->prettyAbsPath()) );
      }
   }

   CvsIgnoreList cvsIgnoreList;
   if ( bUseCvsIgnore )
   {
      cvsIgnoreList.init( *m_pFileAccess, cvsIgnoreExists(pDirList) );
   }
#if defined(_WIN32) || defined(Q_OS_OS2)
   bool bCaseSensitive = false;
#else
   bool bCaseSensitive = true;
#endif

   // Now remove all entries that don't match:
   t_DirectoryList::iterator i;
   for( i = pDirList->begin(); i!=pDirList->end();  )
   {
      t_DirectoryList::iterator i2=i;
      ++i2;
      QString fn = i->fileName();
      if (  (!bFindHidden && i->isHidden() )
            ||
            (i->isFile() &&
               ( !wildcardMultiMatch( filePattern, fn, bCaseSensitive ) ||
                 wildcardMultiMatch( fileAntiPattern, fn, bCaseSensitive ) ) )
            ||
            (i->isDir() && wildcardMultiMatch( dirAntiPattern, fn, bCaseSensitive ) )
            ||
            cvsIgnoreList.matches( fn, bCaseSensitive )
         )
      {
         // Remove it
         pDirList->erase( i );
         i = i2;
      }
      else
      {
         ++i;
      }
   }

   if ( bRecursive )
   {
      t_DirectoryList subDirsList;

      t_DirectoryList::iterator i;
      for( i = m_pDirList->begin(); i!=m_pDirList->end(); ++i )
      {
         if  ( i->isDir() && (!i->isSymLink() || m_bFollowDirLinks))
         {
            t_DirectoryList dirList;
            i->listDir( &dirList, bRecursive, bFindHidden,
               filePattern, fileAntiPattern, dirAntiPattern, bFollowDirLinks, bUseCvsIgnore );

            t_DirectoryList::iterator j;
            for( j = dirList.begin(); j!=dirList.end(); ++j )
            {
               if ( j->parent()==0 )
                  j->m_filePath = i->fileName() + "/" + j->m_filePath;
            }

            // append data onto the main list
            subDirsList.splice( subDirsList.end(), dirList );
         }
      }

      m_pDirList->splice( m_pDirList->end(), subDirsList );
   }

   return m_bSuccess;
}


void FileAccessJobHandler::slotListDirProcessNewEntries( KIO::Job*, const KIO::UDSEntryList& l )
{
   KUrl parentUrl( m_pFileAccess->absoluteFilePath() );

   KIO::UDSEntryList::ConstIterator i;
   for ( i=l.begin(); i!=l.end(); ++i )
   {
      const KIO::UDSEntry& e = *i;
      FileAccess fa;
      fa.createData();
      fa.m_pData->m_pParent = m_pFileAccess;
      fa.setUdsEntry( e );

      if ( fa.fileName() != "." && fa.fileName() != ".." )
      {
         fa.d()->m_url = parentUrl;
         fa.d()->m_url.addPath( fa.fileName() );
         //fa.d()->m_absoluteFilePath = fa.url().url();
         m_pDirList->push_back( fa );
      }
   }
}

void ProgressProxyExtender::slotListDirInfoMessage( KJob*, const QString& msg )
{
   setInformation( msg, 0.0 );
}

void ProgressProxyExtender::slotPercent( KJob*, unsigned long percent )
{
   setCurrent( percent/100.0 );
}


//#include "fileaccess.moc"
