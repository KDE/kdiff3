/***************************************************************************
 *   Copyright (C) 2003 by Joachim Eibl                                    *
 *   joachim.eibl@gmx.de                                                   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#ifndef FILEACCESS_H
#define FILEACCESS_H

#include <qdialog.h>
#include <qfileinfo.h>
#include <kprogress.h>
#include <kio/job.h>
#include <kio/jobclasses.h>
#include <kurldrag.h>
#include <list>
#include <qstring.h>
#include <qdatetime.h>

class t_DirectoryList;

class FileAccess
{
public:
   FileAccess();
   ~FileAccess();
   FileAccess( const QString& name, bool bWantToWrite=false ); // name: local file or dirname or url (when supported)
   void setFile( const QString& name, bool bWantToWrite=false );

   bool isValid() const;
   bool isFile() const;
   bool isDir() const;
   bool isSymLink() const;
   bool exists() const;
   long size() const;            // Size as returned by stat().
   long sizeForReading();  // If the size can't be determined by stat() then the file is copied to a local temp file.
   bool isReadable() const;
   bool isWritable() const;
   bool isExecutable() const;
   bool isHidden() const;
   QString readLink() const;

   QDateTime   created()       const;
   QDateTime   lastModified()  const;
   QDateTime   lastRead()      const;

   QString fileName() const; // Just the name-part of the path, without parent directories
   QString filePath() const; // The path-string that was used during construction
   QString prettyAbsPath() const;
   KURL url() const;
   QString absFilePath() const;

   bool isLocal() const;

   bool readFile(void* pDestBuffer, unsigned long maxLength );
   bool writeFile(const void* pSrcBuffer, unsigned long length );
   bool listDir( t_DirectoryList* pDirList, bool bRecursive, bool bFindHidden,
                 const QString& filePattern, const QString& fileAntiPattern,
                 const QString& dirAntiPattern, bool bFollowDirLinks, bool bUseCvsIgnore );
   bool copyFile( const QString& destUrl );
   bool createBackup( const QString& bakExtension );

   static QString tempFileName();
   bool removeFile();
   static bool removeFile( const QString& );
   static bool makeDir( const QString& );
   static bool removeDir( const QString& );
   static bool exists( const QString& );
   static QString cleanDirPath( const QString& );

   //bool chmod( const QString& );
   bool rename( const QString& );
   static bool symLink( const QString& linkTarget, const QString& linkLocation );

   void addPath( const QString& txt );
   QString getStatusText();
private:
   void setUdsEntry( const KIO::UDSEntry& e );
   KURL m_url;
   bool m_bLocal;
   bool m_bValidData;

   unsigned long m_size;
   QDateTime m_modificationTime;
   QDateTime m_accessTime;
   QDateTime m_creationTime;
   bool m_bReadable;
   bool m_bWritable;
   bool m_bExecutable;
   bool m_bExists;
   bool m_bFile;
   bool m_bDir;
   bool m_bSymLink;
   bool m_bHidden;
   long m_fileType; // for testing only

   QString m_linkTarget;
   QString m_user;
   QString m_group;
   QString m_name;
   QString m_path;
   QString m_absFilePath;
   QString m_localCopy;
   QString m_statusText;  // Might contain an error string, when the last operation didn't succeed.

   friend class FileAccessJobHandler;
};

class t_DirectoryList : public std::list<FileAccess>
{};


class FileAccessJobHandler : public QObject
{
   Q_OBJECT
public:
   FileAccessJobHandler( FileAccess* pFileAccess );

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
   bool removeFile( const QString& dirName );
   bool symLink( const QString& linkTarget, const QString& linkLocation );

private:
   FileAccess* m_pFileAccess;
   bool m_bSuccess;

   // Data needed during Job
   long m_transferredBytes;
   char* m_pTransferBuffer;  // Needed during get or put
   long m_maxLength;

   QString m_filePattern;
   QString m_fileAntiPattern;
   QString m_dirAntiPattern;
   t_DirectoryList* m_pDirList;
   bool m_bFindHidden;
   bool m_bRecursive;
   bool m_bFollowDirLinks;

   bool scanLocalDirectory( const QString& dirName, t_DirectoryList* dirList );

private slots:
   void slotStatResult( KIO::Job* );
   void slotSimpleJobResult( KIO::Job* pJob );
   void slotPutJobResult( KIO::Job* pJob );

   void slotGetData(KIO::Job*,const QByteArray&);
   void slotPutData(KIO::Job*, QByteArray&);

   void slotListDirInfoMessage( KIO::Job*, const QString& msg );
   void slotListDirProcessNewEntries( KIO::Job *, const KIO::UDSEntryList& l );

   void slotPercent( KIO::Job* pJob, unsigned long percent );
};

class ProgressDialog : public QDialog
{
   Q_OBJECT
public:
   ProgressDialog( QWidget* pParent );

   void setInformation( const QString& info, bool bRedrawUpdate=true );
   void setInformation( const QString& info, double dCurrent, bool bRedrawUpdate=true );
   void step( bool bRedrawUpdate=true);
   void setMaximum( int maximum );

   void setSubInformation(const QString& info, double dSubCurrent, bool bRedrawUpdate=true );
   void setSubCurrent( double dSubCurrent, bool bRedrawUpdate=true );

   // The progressbar goes from 0 to 1 usually.
   // By supplying a subrange transformation the subCurrent-values
   // 0 to 1 will be transformed to dMin to dMax instead.
   // Requirement: 0 < dMin < dMax < 1
   void setSubRangeTransformation( double dMin, double dMax );

   void exitEventLoop();
   void enterEventLoop( KIO::Job* pJob, const QString& jobInfo );

   void start();

   bool wasCancelled();
   void show();
   void hide();

   virtual void timerEvent(QTimerEvent*);
private:
   KProgress* m_pProgressBar;
   KProgress* m_pSubProgressBar;
   QLabel* m_pInformation;
   QLabel* m_pSubInformation;
   QLabel* m_pSlowJobInfo;
   QPushButton* m_pAbortButton;
   int m_maximum;
   double m_dCurrent;
   double m_dSubCurrent;
   double m_dSubMin;
   double m_dSubMax;
   void recalc(bool bRedrawUpdate);
   QTime m_t1;
   QTime m_t2;
   bool m_bWasCancelled;
   KIO::Job* m_pJob;
   QString m_currentJobInfo;  // Needed if the job doesn't stop after a reasonable time.
protected:
   virtual void reject();
private slots:
   void delayedHide();
   void slotAbort();
};

extern ProgressDialog* g_pProgressDialog;



#endif

