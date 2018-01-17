#include <assert.h>
#include "fileaccess.h"

FileAccess::FileAccess()
{
}

FileAccess::FileAccess(const QString& name, bool bWantToWrite)
{
  assert(!bWantToWrite);

  m_name = name;
}

FileAccess::~FileAccess()
{
}

//   FileAccess( const QString& name, bool bWantToWrite=false ); // name: local file or dirname or url (when supported)
//   void setFile( const QString& name, bool bWantToWrite=false );
//
bool FileAccess::isValid() const
{
  return m_name.length() != 0;
}

//   bool isFile() const;
//   bool isDir() const;
//   bool isSymLink() const;
bool FileAccess::exists() const
{
  assert(FALSE);
}
qint64 FileAccess::size() const
{
  assert(FALSE);
}

qint64 FileAccess::sizeForReading()
{
  assert(FALSE);
}

//   bool isReadable() const;
//   bool isWritable() const;
//   bool isExecutable() const;
//   bool isHidden() const;
//   QString readLink() const;
//
//   QDateTime   created()       const;
//   QDateTime   lastModified()  const;
//   QDateTime   lastRead()      const;
//
//   QString fileName() const; // Just the name-part of the path, without parent directories
//   QString filePath() const; // The path-string that was used during construction
QString FileAccess::prettyAbsPath() const
{
  assert(FALSE);
}
//   KUrl url() const;
QString FileAccess::absoluteFilePath() const
{
  return "";
}

bool FileAccess::isLocal() const
{
  return true;
}

bool FileAccess::readFile(void* pDestBuffer, unsigned long maxLength )
{
  assert(FALSE);
}
bool FileAccess::writeFile(const void* pSrcBuffer, unsigned long length )
{
  assert(FALSE);
}

//   bool listDir( t_DirectoryList* pDirList, bool bRecursive, bool bFindHidden,
//                 const QString& filePattern, const QString& fileAntiPattern,
//                 const QString& dirAntiPattern, bool bFollowDirLinks, bool bUseCvsIgnore );
bool FileAccess::copyFile( const QString& destUrl )
{
  assert(FALSE);
}
//   bool createBackup( const QString& bakExtension );
//
QString FileAccess::tempFileName()
{
  assert(FALSE);
}

bool FileAccess::removeTempFile( const QString& )
{
  assert(FALSE);
}

/*bool FileAccess::removeFile()
{
  assert(FALSE);
}*/
bool FileAccess::removeFile( const QString& )
{
  assert(FALSE);
}

