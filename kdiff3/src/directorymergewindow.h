/***************************************************************************
                          directorymergewindow.h
                             -------------------
    begin                : Sat Oct 19 2002
    copyright            : (C) 2002 by Joachim Eibl
    email                : joachim.eibl@gmx.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

/***************************************************************************
 * $Log$
 * Revision 1.2  2003/10/14 20:51:45  joachim99
 * Bugfix for Syncmode.
 *
 * Revision 1.1  2003/10/06 18:38:48  joachim99
 * KDiff3 version 0.9.70
 *                                                                   *
 ***************************************************************************/

#ifndef DIRECTORY_MERGE_WINDOW_H
#define DIRECTORY_MERGE_WINDOW_H

#include <qfileinfo.h>
#include <qlistview.h>
#include <qtimer.h>
#include <qdir.h>
#include <list>
#include <map>
#include "common.h"
#include "fileaccess.h"

class OptionDialog;
class KIconLoader;
class StatusMessageBox;
class StatusInfo;
class DirectoryMergeInfo;
class OneDirectoryInfo;
class QLabel;

enum e_MergeOperation
{
   eTitleId,
   eNoOperation,
   // Operations in sync mode (with only two directories):
   eCopyAToB, eCopyBToA, eDeleteA, eDeleteB, eDeleteAB, eMergeToA, eMergeToB, eMergeToAB,

   // Operations in merge mode (with two or three directories)
   eCopyAToDest, eCopyBToDest, eCopyCToDest, eDeleteFromDest, eMergeABCToDest,
   eMergeABToDest,
   eConflictingFileTypes, // Error
   eConflictingAges       // Equal age but files are not!
};

class DirMergeItem;

enum e_Age { eNew, eMiddle, eOld, eNotThere, eAgeEnd };

class MergeFileInfos
{
public:
   MergeFileInfos(){ m_bEqualAB=false; m_bEqualAC=false; m_bEqualBC=false;
                     m_pDMI=0; m_pParent=0;
                     m_bExistsInA=false;m_bExistsInB=false;m_bExistsInC=false;
                     m_bDirA=false;  m_bDirB=false;  m_bDirC=false;
                     m_bLinkA=false; m_bLinkB=false; m_bLinkC=false;
                     m_bOperationComplete=false; m_bSimOpComplete = false;
                     m_eMergeOperation=eNoOperation;
                     m_ageA = eNotThere; m_ageB=eNotThere; m_ageC=eNotThere;
                     m_bConflictingAges=false; }
   bool operator>( const MergeFileInfos& );
   QString m_subPath;

   bool m_bExistsInA;
   bool m_bExistsInB;
   bool m_bExistsInC;
   bool m_bEqualAB;
   bool m_bEqualAC;
   bool m_bEqualBC;
   DirMergeItem* m_pDMI;
   MergeFileInfos* m_pParent;
   e_MergeOperation m_eMergeOperation;
   void setMergeOperation( e_MergeOperation eMOp );
   bool m_bDirA;
   bool m_bDirB;
   bool m_bDirC;
   bool m_bLinkA;
   bool m_bLinkB;
   bool m_bLinkC;
   bool m_bOperationComplete;
   bool m_bSimOpComplete;
   e_Age m_ageA;
   e_Age m_ageB;
   e_Age m_ageC;
   bool m_bConflictingAges;       // Equal age but files are not!

   FileAccess m_fileInfoA;
   FileAccess m_fileInfoB;
   FileAccess m_fileInfoC;
};

class DirMergeItem : public QListViewItem
{
public:
   DirMergeItem( QListView* pParent, const QString&, MergeFileInfos*);
   DirMergeItem( DirMergeItem* pParent, const QString&, MergeFileInfos*);
   ~DirMergeItem();
   MergeFileInfos* m_pMFI;
#if QT_VERSION!=230
   virtual int compare(QListViewItem *i, int col, bool ascending) const
   {
      DirMergeItem* pDMI = static_cast<DirMergeItem*>(i);
      bool bDir1 =  m_pMFI->m_bDirA || m_pMFI->m_bDirB || m_pMFI->m_bDirC;
      bool bDir2 =  pDMI->m_pMFI->m_bDirA || pDMI->m_pMFI->m_bDirB || pDMI->m_pMFI->m_bDirC;
      if ( m_pMFI==0 || pDMI->m_pMFI==0 || bDir1 == bDir2 )
         return QListViewItem::compare( i, col, ascending );
      else
         return bDir1 ? -1 : 1;
   }
#endif
};

class DirectoryMergeWindow : public QListView
{
   Q_OBJECT
public:
   DirectoryMergeWindow( QWidget* pParent, OptionDialog* pOptions, KIconLoader* pIconLoader );
   ~DirectoryMergeWindow();
   void setDirectoryMergeInfo(DirectoryMergeInfo* p){ m_pDirectoryMergeInfo=p; }
   bool init(
      FileAccess& dirA,
      FileAccess& dirB,
      FileAccess& dirC,
      FileAccess& dirDest,
      bool bDirectoryMerge
   );
   bool isFileSelected();
   void allowResizeEvents(bool bAllowResizeEvents);
   bool isDirectoryMergeInProgress() { return m_bRealMergeStarted; }
   int totalColumnWidth();
   bool isSyncMode() { return m_bSyncMode; }

public slots:
   void mergeContinue();
   void reload();
   void mergeCurrentFile();
   void compareCurrentFile();
   void mergeResultSaved(const QString& fileName);
   void slotChooseAEverywhere();
   void slotChooseBEverywhere();
   void slotChooseCEverywhere();
   void slotAutoChooseEverywhere();
   void slotNoOpEverywhere();
   void slotFoldAllSubdirs();
   void slotUnfoldAllSubdirs();

protected:
   void resizeEvent(QResizeEvent* e);
   bool m_bAllowResizeEvents;

   void prepareListView();
   void calcSuggestedOperation( MergeFileInfos& mfi, e_MergeOperation eDefaultOperation );
   void setAllMergeOperations( e_MergeOperation eDefaultOperation );
   friend class MergeFileInfos;

   bool canContinue();

   void scanDirectory( const QString& dirName, t_DirectoryList& dirList );
   void scanLocalDirectory( const QString& dirName, t_DirectoryList& dirList );
   void fastFileComparison( FileAccess& fi1, FileAccess& fi2,
                            bool& bEqual, bool& bError, QString& status );
   void compareFilesAndCalcAges( MergeFileInfos& mfi );

   QString fullNameA( const MergeFileInfos& mfi )
   { return m_dirA.absFilePath() + "/" + mfi.m_subPath; }
   QString fullNameB( const MergeFileInfos& mfi )
   { return m_dirB.absFilePath() + "/" + mfi.m_subPath; }
   QString fullNameC( const MergeFileInfos& mfi )
   { return m_dirC.absFilePath() + "/" + mfi.m_subPath; }
   QString fullNameDest( const MergeFileInfos& mfi )
   { return m_dirDestInternal.absFilePath() + "/" + mfi.m_subPath; }

   bool copyFLD( const QString& srcName, const QString& destName );
   bool deleteFLD( const QString& name, bool bCreateBackup );
   bool makeDir( const QString& name, bool bQuiet=false );
   bool renameFLD( const QString& srcName, const QString& destName );
   bool mergeFLD( const QString& nameA,const QString& nameB,const QString& nameC,
                  const QString& nameDest, bool& bSingleFileMerge );

   FileAccess m_dirA;
   FileAccess m_dirB;
   FileAccess m_dirC;
   FileAccess m_dirDest;
   FileAccess m_dirDestInternal;

   std::map<QString, MergeFileInfos> m_fileMergeMap;

   bool m_bFollowDirLinks;
   bool m_bFollowFileLinks;
   bool m_bSimulatedMergeStarted;
   bool m_bRealMergeStarted;
   bool m_bSingleFileOperationStarted;
   bool m_bError;
   bool m_bSyncMode;
   bool m_bDirectoryMerge; // if true, then merge is the default operation, otherwise it's diff.

   OptionDialog* m_pOptions;
   KIconLoader* m_pIconLoader;
   DirMergeItem* m_pCurrentItemForOperation;
   StatusMessageBox* m_pStatusMessageBox;
   DirectoryMergeInfo* m_pDirectoryMergeInfo;
   StatusInfo* m_pStatusInfo;
signals:
   void startDiffMerge(QString fn1,QString fn2, QString fn3, QString ofn, QString,QString,QString);
   void checkIfCanContinue( bool* pbContinue );
   void updateAvailabilities();
protected slots:
   void onDoubleClick( QListViewItem* lvi );
   void onClick( QListViewItem* lvi, const QPoint&, int c );
   void onSelectionChanged(QListViewItem* lvi);
};

class DirectoryMergeInfo : public QFrame
{
public:
   DirectoryMergeInfo( QWidget* pParent );
   void setInfo(
      const FileAccess& APath,
      const FileAccess& BPath,
      const FileAccess& CPath,
      const FileAccess& DestPath,
      MergeFileInfos& mfi );
   QListView* getInfoList() {return m_pInfoList;}
private:
   QLabel* m_pInfoA;
   QLabel* m_pInfoB;
   QLabel* m_pInfoC;
   QLabel* m_pInfoDest;

   QLabel* m_pA;
   QLabel* m_pB;
   QLabel* m_pC;
   QLabel* m_pDest;

   QListView* m_pInfoList;
};


#endif
