/***************************************************************************
                          directorymergewindow.h
                             -------------------
    begin                : Sat Oct 19 2002
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

#ifndef DIRECTORY_MERGE_WINDOW_H
#define DIRECTORY_MERGE_WINDOW_H

#include <Qt3Support/q3listview.h>
#include <list>
#include <map>
#include "common.h"
#include "fileaccess.h"
#include "diff.h" //TotalDiffStatus

class OptionDialog;
class KIconLoader;
class StatusInfo;
class DirectoryMergeInfo;
class OneDirectoryInfo;
class QLabel;
class KAction;
class KToggleAction;
class KActionCollection;
class TotalDiffStatus;

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

   TotalDiffStatus m_totalDiffStatus;   
};

class DirMergeItem : public Q3ListViewItem
{
public:
   DirMergeItem( Q3ListView* pParent, const QString&, MergeFileInfos*);
   DirMergeItem( DirMergeItem* pParent, const QString&, MergeFileInfos*);
   ~DirMergeItem();
   MergeFileInfos* m_pMFI;
   virtual int compare(Q3ListViewItem *i, int col, bool ascending) const;
   virtual void paintCell(QPainter * p, const QColorGroup & cg, int column, int width, int align );
   void init(MergeFileInfos* pMFI);
};

class DirectoryMergeWindow : public Q3ListView
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
   bool isScanning() { return m_bScanning; }
   void initDirectoryMergeActions( QObject* pKDiff3App, KActionCollection* ac );
   void updateAvailabilities( bool bDirCompare, bool bDiffWindowVisible,
      KToggleAction* chooseA, KToggleAction* chooseB, KToggleAction* chooseC );
   void updateFileVisibilities();

   virtual void keyPressEvent( QKeyEvent* e );
   virtual void focusInEvent( QFocusEvent* e );
   virtual void focusOutEvent( QFocusEvent* e );

public slots:
   void reload();
   void mergeCurrentFile();
   void compareCurrentFile();
   void slotRunOperationForAllItems();
   void slotRunOperationForCurrentItem();
   void mergeResultSaved(const QString& fileName);
   void slotChooseAEverywhere();
   void slotChooseBEverywhere();
   void slotChooseCEverywhere();
   void slotAutoChooseEverywhere();
   void slotNoOpEverywhere();
   void slotFoldAllSubdirs();
   void slotUnfoldAllSubdirs();
   void slotShowIdenticalFiles();
   void slotShowDifferentFiles();
   void slotShowFilesOnlyInA();
   void slotShowFilesOnlyInB();
   void slotShowFilesOnlyInC();

   void slotSynchronizeDirectories();
   void slotChooseNewerFiles();

   void slotCompareExplicitlySelectedFiles();
   void slotMergeExplicitlySelectedFiles();

   // Merge current item (merge mode)
   void slotCurrentDoNothing();
   void slotCurrentChooseA();
   void slotCurrentChooseB();
   void slotCurrentChooseC();
   void slotCurrentMerge();
   void slotCurrentDelete();
   // Sync current item
   void slotCurrentCopyAToB();
   void slotCurrentCopyBToA();
   void slotCurrentDeleteA();
   void slotCurrentDeleteB();
   void slotCurrentDeleteAAndB();
   void slotCurrentMergeToA();
   void slotCurrentMergeToB();
   void slotCurrentMergeToAAndB();

   void slotSaveMergeState();
   void slotLoadMergeState();

protected:
   void mergeContinue( bool bStart, bool bVerbose );
   void resizeEvent(QResizeEvent* e);
   bool m_bAllowResizeEvents;

   void prepareListView(ProgressProxy& pp);
   void calcSuggestedOperation( MergeFileInfos& mfi, e_MergeOperation eDefaultOperation );
   void setAllMergeOperations( e_MergeOperation eDefaultOperation );
   friend class MergeFileInfos;

   bool canContinue();
   void prepareMergeStart( Q3ListViewItem* pBegin, Q3ListViewItem* pEnd, bool bVerbose );
   bool executeMergeOperation( MergeFileInfos& mfi, bool& bSingleFileMerge );

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

   QString m_dirMergeStateFilename;

   std::map<QString, MergeFileInfos> m_fileMergeMap;

   bool m_bFollowDirLinks;
   bool m_bFollowFileLinks;
   bool m_bSimulatedMergeStarted;
   bool m_bRealMergeStarted;
   bool m_bError;
   bool m_bSyncMode;
   bool m_bDirectoryMerge; // if true, then merge is the default operation, otherwise it's diff.
   bool m_bCaseSensitive;
   
   bool m_bScanning; // true while in init()

   OptionDialog* m_pOptions;
   KIconLoader* m_pIconLoader;
   DirectoryMergeInfo* m_pDirectoryMergeInfo;
   StatusInfo* m_pStatusInfo;

   typedef std::list<DirMergeItem*> MergeItemList;
   MergeItemList m_mergeItemList;
   MergeItemList::iterator m_currentItemForOperation;

   DirMergeItem* m_pSelection1Item;
   int m_selection1Column;
   DirMergeItem* m_pSelection2Item;
   int m_selection2Column;
   DirMergeItem* m_pSelection3Item;
   int m_selection3Column;
   void selectItemAndColumn(DirMergeItem* pDMI, int c, bool bContextMenu);
   friend class DirMergeItem;

   KAction* m_pDirStartOperation;
   KAction* m_pDirRunOperationForCurrentItem;
   KAction* m_pDirCompareCurrent;
   KAction* m_pDirMergeCurrent;
   KAction* m_pDirRescan;
   KAction* m_pDirChooseAEverywhere;
   KAction* m_pDirChooseBEverywhere;
   KAction* m_pDirChooseCEverywhere;
   KAction* m_pDirAutoChoiceEverywhere;
   KAction* m_pDirDoNothingEverywhere;
   KAction* m_pDirFoldAll;
   KAction* m_pDirUnfoldAll;

   KToggleAction* m_pDirShowIdenticalFiles;
   KToggleAction* m_pDirShowDifferentFiles;
   KToggleAction* m_pDirShowFilesOnlyInA;
   KToggleAction* m_pDirShowFilesOnlyInB;
   KToggleAction* m_pDirShowFilesOnlyInC;

   KToggleAction* m_pDirSynchronizeDirectories;
   KToggleAction* m_pDirChooseNewerFiles;

   KAction* m_pDirCompareExplicit;
   KAction* m_pDirMergeExplicit;

   KAction* m_pDirCurrentDoNothing;
   KAction* m_pDirCurrentChooseA;
   KAction* m_pDirCurrentChooseB;
   KAction* m_pDirCurrentChooseC;
   KAction* m_pDirCurrentMerge;
   KAction* m_pDirCurrentDelete;

   KAction* m_pDirCurrentSyncDoNothing;
   KAction* m_pDirCurrentSyncCopyAToB;
   KAction* m_pDirCurrentSyncCopyBToA;
   KAction* m_pDirCurrentSyncDeleteA;
   KAction* m_pDirCurrentSyncDeleteB;
   KAction* m_pDirCurrentSyncDeleteAAndB;
   KAction* m_pDirCurrentSyncMergeToA;
   KAction* m_pDirCurrentSyncMergeToB;
   KAction* m_pDirCurrentSyncMergeToAAndB;

   KAction* m_pDirSaveMergeState;
   KAction* m_pDirLoadMergeState;
signals:
   void startDiffMerge(QString fn1,QString fn2, QString fn3, QString ofn, QString,QString,QString,TotalDiffStatus*);
   void checkIfCanContinue( bool* pbContinue );
   void updateAvailabilities();
   void statusBarMessage( const QString& msg );
protected slots:
   void onDoubleClick( Q3ListViewItem* lvi );
   void onClick( int button, Q3ListViewItem* lvi, const QPoint&, int c );
   void slotShowContextMenu(Q3ListViewItem* lvi,const QPoint &,int c);
   void onSelectionChanged(Q3ListViewItem* lvi);
};

class DirectoryMergeInfo : public QFrame
{
   Q_OBJECT
public:
   DirectoryMergeInfo( QWidget* pParent );
   void setInfo(
      const FileAccess& APath,
      const FileAccess& BPath,
      const FileAccess& CPath,
      const FileAccess& DestPath,
      MergeFileInfos& mfi );
   Q3ListView* getInfoList() {return m_pInfoList;}
   virtual bool eventFilter( QObject* o, QEvent* e );
signals:
   void gotFocus();
private:
   QLabel* m_pInfoA;
   QLabel* m_pInfoB;
   QLabel* m_pInfoC;
   QLabel* m_pInfoDest;

   QLabel* m_pA;
   QLabel* m_pB;
   QLabel* m_pC;
   QLabel* m_pDest;

   Q3ListView* m_pInfoList;
};


#endif
