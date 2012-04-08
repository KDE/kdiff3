/***************************************************************************
 *   Copyright (C) 2003-2007 by Joachim Eibl                               *
 *   joachim.eibl at gmx.de                                                   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#ifndef FILEACCESS_H
#define FILEACCESS_H

#include <QDialog>
#include <QDateTime>
#include <QEventLoop>
#include <QLabel>
#include <QProgressBar>

#include <kprogressdialog.h>
#include <kio/job.h>
#include <kio/jobclasses.h>

#include <list>

bool wildcardMultiMatch( const QString& wildcard, const QString& testString, bool bCaseSensitive );

class t_DirectoryList;
class QFileInfo;

class FileAccess
{
public:
   FileAccess();
   ~FileAccess();
   FileAccess( const QString& name, bool bWantToWrite=false ); // name: local file or dirname or url (when supported)
   FileAccess(const FileAccess& other);
   const FileAccess& operator=(const FileAccess& other);
   void setFile( const QString& name, bool bWantToWrite=false );

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

   //QDateTime   created()       const;
   QDateTime   lastModified()  const;
   //QDateTime   lastRead()      const;

   QString fileName() const; // Just the name-part of the path, without parent directories
   QString filePath() const; // The path-string that was used during construction
   QString prettyAbsPath() const;
   KUrl url() const;
   QString absoluteFilePath() const;

   bool isLocal() const;

   bool readFile(void* pDestBuffer, unsigned long maxLength );
   bool writeFile(const void* pSrcBuffer, unsigned long length );
   bool listDir( t_DirectoryList* pDirList, bool bRecursive, bool bFindHidden,
                 const QString& filePattern, const QString& fileAntiPattern,
                 const QString& dirAntiPattern, bool bFollowDirLinks, bool bUseCvsIgnore );
   bool copyFile( const QString& destUrl );
   bool createBackup( const QString& bakExtension );

   static QString tempFileName();
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
   void setSharedName( const QString& name ); // might reduce memory consumption
private:
   void setUdsEntry( const KIO::UDSEntry& e );
   void setFile( const QFileInfo& fi, FileAccess* pParent );
   void setStatusText( const QString& s );

   class Data;
   Data* d();
   const Data* d() const;
   void createData();

   union
   {
      Data* m_pData;
      FileAccess* m_pParent;
   };
   QString m_filePath; // might be absolute or relative if m_pParent!=0
   qint64 m_size;
   QDateTime m_modificationTime;
   bool m_bSymLink  : 1;
   bool m_bFile     : 1;
   bool m_bDir      : 1;
   bool m_bExists   : 1;
   bool m_bWritable : 1;
   bool m_bHidden   : 1;
   bool m_bUseData  : 1;

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
   qint64 m_transferredBytes;
   char* m_pTransferBuffer;  // Needed during get or put
   qint64 m_maxLength;

   QString m_filePattern;
   QString m_fileAntiPattern;
   QString m_dirAntiPattern;
   t_DirectoryList* m_pDirList;
   bool m_bFindHidden;
   bool m_bRecursive;
   bool m_bFollowDirLinks;

   bool scanLocalDirectory( const QString& dirName, t_DirectoryList* dirList );

private slots:
   void slotStatResult( KJob* );
   void slotSimpleJobResult( KJob* pJob );
   void slotPutJobResult( KJob* pJob );

   void slotGetData(KJob*,const QByteArray&);
   void slotPutData(KIO::Job*, QByteArray&);

   void slotListDirInfoMessage( KJob*, const QString& msg );
   void slotListDirProcessNewEntries( KIO::Job *, const KIO::UDSEntryList& l );

   void slotPercent( KJob* pJob, unsigned long percent );
};

class ProgressDialog : public QDialog
{
   Q_OBJECT
public:
   ProgressDialog( QWidget* pParent );

   void setStayHidden( bool bStayHidden );
   void setInformation( const QString& info, bool bRedrawUpdate=true );
   void setInformation( const QString& info, double dCurrent, bool bRedrawUpdate=true );
   void setCurrent( double dCurrent, bool bRedrawUpdate=true  );
   void step( bool bRedrawUpdate=true );
   void setMaxNofSteps( int dMaxNofSteps );
   void push();
   void pop(bool bRedrawUpdate=true);

   // The progressbar goes from 0 to 1 usually.
   // By supplying a subrange transformation the subCurrent-values
   // 0 to 1 will be transformed to dMin to dMax instead.
   // Requirement: 0 < dMin < dMax < 1
   void setRangeTransformation( double dMin, double dMax );
   void setSubRangeTransformation( double dMin, double dMax );

   void exitEventLoop();
   void enterEventLoop( KJob* pJob, const QString& jobInfo );

   bool wasCancelled();
   void show();
   void hide();
   
   virtual void timerEvent(QTimerEvent*);
private:

   struct ProgressLevelData
   {
      ProgressLevelData()
      {
         m_dCurrent=0; m_maxNofSteps=1; m_dRangeMin=0; m_dRangeMax=1; 
         m_dSubRangeMin = 0; m_dSubRangeMax = 1;
      }
      double m_dCurrent;
      int    m_maxNofSteps;     // when step() is used.
      double m_dRangeMax;
      double m_dRangeMin;
      double m_dSubRangeMax;
      double m_dSubRangeMin;
   };
   std::list<ProgressLevelData> m_progressStack;
   
   int m_progressDelayTimer;
   std::list<QEventLoop*> m_eventLoopStack;

   QProgressBar* m_pProgressBar;
   QProgressBar* m_pSubProgressBar;
   QLabel* m_pInformation;
   QLabel* m_pSubInformation;
   QLabel* m_pSlowJobInfo;
   QPushButton* m_pAbortButton;
   void recalc(bool bRedrawUpdate);
   QTime m_t1;
   QTime m_t2;
   bool m_bWasCancelled;
   KJob* m_pJob;
   QString m_currentJobInfo;  // Needed if the job doesn't stop after a reasonable time.
   bool m_bStayHidden;
protected:
   virtual void reject();
private slots:
   void delayedHide();
   void slotAbort();
};

// When using the ProgressProxy you need not take care of the push and pop, except when explicit.
class ProgressProxy
{
public:
   ProgressProxy();
   ~ProgressProxy();
   
   void setInformation( const QString& info, bool bRedrawUpdate=true );
   void setInformation( const QString& info, double dCurrent, bool bRedrawUpdate=true );
   void setCurrent( double dCurrent, bool bRedrawUpdate=true  );
   void step( bool bRedrawUpdate=true );
   void setMaxNofSteps( int dMaxNofSteps );
   bool wasCancelled();
   void setRangeTransformation( double dMin, double dMax );
   void setSubRangeTransformation( double dMin, double dMax );
private:
};

extern ProgressDialog* g_pProgressDialog;



#endif

