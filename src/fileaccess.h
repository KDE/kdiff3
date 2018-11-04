/***************************************************************************
 *   Copyright (C) 2003-2007 by Joachim Eibl <joachim.eibl at gmx.de>      *
 *   Copyright (C) 2018 Michael Reeves reeves.87@gmail.com                 *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#ifndef FILEACCESS_H
#define FILEACCESS_H

#include "progress.h"
#include "ProgressProxyExtender.h"

#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QSharedPointer>
#include <QTemporaryFile>
#include <QUrl>

#include <KIO/UDSEntry>

namespace KIO {
    class Job;
}

class t_DirectoryList;

class FileAccess
{
public:
   FileAccess();
   ~FileAccess();
   FileAccess( const QString& name, bool bWantToWrite=false ); // name: local file or dirname or url (when supported)
   void setFile( const QString& name, bool bWantToWrite=false );
   void setFile( const QUrl& name, bool bWantToWrite = false);
   void setFile( FileAccess* pParent, QFileInfo fi );

   void loadData(void);

   bool isNormal() const;
   bool isValid() const;
   bool isFile() const;
   bool isDir() const;
   bool isSymLink() const;
   bool exists() const;
   qint64 size() const;            // Size as returned by stat().
   qint64 sizeForReading();  // If the size can't be determined by stat() then the file is copied to a local temp file.
   bool isReadable() const;
   bool isWritable() const;
   bool isExecutable() const;
   bool isHidden() const;
   QString readLink() const;

   QDateTime lastModified() const;

   QString fileName(bool needTmp = false) const; // Just the name-part of the path, without parent directories
   QString fileRelPath() const; // The path relitive to base comparison directory
   QString prettyAbsPath() const;
   QUrl url() const;
   void setUrl(const QUrl inUrl) { m_url = inUrl; }
   QString absoluteFilePath() const;

   bool isLocal() const;

   bool readFile(void* pDestBuffer, qint64 maxLength );
   bool writeFile(const void* pSrcBuffer, qint64 length );
   bool listDir( t_DirectoryList* pDirList, bool bRecursive, bool bFindHidden,
                 const QString& filePattern, const QString& fileAntiPattern,
                 const QString& dirAntiPattern, bool bFollowDirLinks, bool bUseCvsIgnore );
   bool copyFile( const QString& destUrl );
   bool createBackup( const QString& bakExtension );

   QString getTempName() const;
   bool createLocalCopy();
   static void createTempFile(QTemporaryFile& );
   static bool removeTempFile( const QString& );
   bool removeFile();
   static bool removeFile( const QString& );
   static bool makeDir( const QString& );
   static bool removeDir( const QString& );
   static bool exists( const QString& );
   static QString cleanPath( const QString& );

   //bool chmod( const QString& );
   bool rename( const QString& );
   static bool symLink( const QString& linkTarget, const QString& linkLocation );

   void addPath( const QString& txt );
   QString getStatusText();

   FileAccess* parent() const; // !=0 for listDir-results, but only valid if the parent was not yet destroyed.
   
   void doError(void);
   void filterList(t_DirectoryList* pDirList, const QString& filePattern,
                               const QString& fileAntiPattern, const QString& dirAntiPattern,
                               const bool bUseCvsIgnore);
 private:
   friend class FileAccessJobHandler;
   void setUdsEntry(const KIO::UDSEntry& e);
   void setStatusText( const QString& s );

   void reset();

   QUrl m_url;
   bool m_bValidData;
   
   //long m_fileType; // for testing only
   FileAccess* m_pParent;

   QDir m_baseDir;
   QFileInfo m_fileInfo;
   QString m_linkTarget;
   QString m_name;
   QString m_localCopy;
   QSharedPointer<QTemporaryFile> tmpFile;

   qint64 m_size;
   QDateTime m_modificationTime;
   bool m_bSymLink;
   bool m_bFile;
   bool m_bDir;
   bool m_bExists;
   bool m_bWritable;
   bool m_bReadable;
   bool m_bExecutable;
   bool m_bHidden;
   
   QString m_statusText; // Might contain an error string, when the last operation didn't succeed.
};

class t_DirectoryList : public std::list<FileAccess>
{};

class FileAccessJobHandler : public QObject
{
   Q_OBJECT
public:
   explicit FileAccessJobHandler( FileAccess* pFileAccess );

   bool get( void* pDestBuffer, long maxLength );
   bool put( const void* pSrcBuffer, long maxLength, bool bOverwrite, bool bResume=false, int permissions=-1 );
   bool stat(int detailLevel=2, bool bWantToWrite=false );
   bool copyFile( const QString& dest );
   bool rename( const QString& dest );
   bool listDir( t_DirectoryList* pDirList, bool bRecursive, bool bFindHidden,
                 const QString& filePattern, const QString& fileAntiPattern,
                 const QString& dirAntiPattern, bool bFollowDirLinks, bool bUseCvsIgnore );
   bool mkDir( const QString& dirName );
   bool rmDir( const QString& dirName );
   bool removeFile( const QUrl& dirName );
   bool symLink( const QUrl& linkTarget, const QUrl& linkLocation );

private:
   FileAccess* m_pFileAccess;
   bool m_bSuccess;

   // Data needed during Job
   qint64 m_transferredBytes;
   char* m_pTransferBuffer = nullptr;  // Needed during get or put
   qint64 m_maxLength;

   QString m_filePattern;
   QString m_fileAntiPattern;
   QString m_dirAntiPattern;
   t_DirectoryList* m_pDirList = nullptr;
   bool m_bFindHidden = false;
   bool m_bRecursive = false;
   bool m_bFollowDirLinks = false;

   bool scanLocalDirectory( const QString& dirName, t_DirectoryList* dirList );

private Q_SLOTS:
   void slotStatResult( KJob* );
   void slotSimpleJobResult( KJob* pJob );
   void slotPutJobResult( KJob* pJob );

   void slotGetData(KJob*,const QByteArray&);
   void slotPutData(KIO::Job*, QByteArray&);

   void slotListDirProcessNewEntries( KIO::Job *, const KIO::UDSEntryList& l );
};


#endif

