/*
  SPDX-FileCopyrightText: 2002-2007 Joachim Eibl, joachim.eibl at gmx.de
  SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
  SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "fileaccess.h"

#include <QString>

FileAccess::FileAccess(const QString& name, bool bWantToWrite)
{
  assert(!bWantToWrite);

  m_name = name;
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
    return true;
}
qint64 FileAccess::size() const
{
    return 64;
}

qint64 FileAccess::sizeForReading()
{
    return 64;
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
    return QString("");
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

bool FileAccess::readFile(void* pDestBuffer, qint64 maxLength )
{
    Q_UNUSED(pDestBuffer)
    Q_UNUSED(maxLength);
    return true;
}
bool FileAccess::writeFile(const void* pSrcBuffer, qint64 length )
{
    Q_UNUSED(pSrcBuffer);
    Q_UNUSED(length);
    return true;
}

//   bool listDir( t_DirectoryList* pDirList, bool bRecursive, bool bFindHidden,
//                 const QString& filePattern, const QString& fileAntiPattern,
//                 const QString& dirAntiPattern, bool bFollowDirLinks, bool bUseCvsIgnore );
bool FileAccess::copyFile( const QString& destUrl )
{
    Q_UNUSED(destUrl);
    return true;
}
//   bool createBackup( const QString& bakExtension );
//
QString FileAccess::getTempName() const
{
    return QString("");
}

bool FileAccess::removeFile()
{
    return true;
}

