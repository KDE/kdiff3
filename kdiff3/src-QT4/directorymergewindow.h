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

#include <QTreeWidget>
#include <QEvent>
#include <list>
#include <map>
#include "common.h"
#include "fileaccess.h"
#include "diff.h" //TotalDiffStatus

class Options;
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
   eChangedAndDeleted,    // Error
   eConflictingAges       // Equal age but files are not!
};

enum e_Age { eNew, eMiddle, eOld, eNotThere, eAgeEnd };

class MergeFileInfos;

class DirectoryMergeWindow : public QTreeView
{
   Q_OBJECT
public:
   DirectoryMergeWindow( QWidget* pParent, Options* pOptions, KIconLoader* pIconLoader );
   ~DirectoryMergeWindow();
   void setDirectoryMergeInfo(DirectoryMergeInfo* p);
   bool init(
      FileAccess& dirA,
      FileAccess& dirB,
      FileAccess& dirC,
      FileAccess& dirDest,
      bool bDirectoryMerge,
      bool bReload = false
   );
   bool isFileSelected();
   bool isDirectoryMergeInProgress();
   int totalColumnWidth();
   bool isSyncMode();
   bool isScanning();
   void initDirectoryMergeActions( QObject* pKDiff3App, KActionCollection* ac );
   void updateAvailabilities( bool bDirCompare, bool bDiffWindowVisible,
      KToggleAction* chooseA, KToggleAction* chooseB, KToggleAction* chooseC );
   void updateFileVisibilities();

   virtual void mousePressEvent( QMouseEvent* e );
   virtual void keyPressEvent( QKeyEvent* e );
   virtual void focusInEvent( QFocusEvent* e );
   virtual void focusOutEvent( QFocusEvent* e );
   virtual void contextMenuEvent( QContextMenuEvent* e );
   QString getDirNameA();
   QString getDirNameB();
   QString getDirNameC();
   QString getDirNameDest();

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

signals:
   void startDiffMerge(QString fn1,QString fn2, QString fn3, QString ofn, QString,QString,QString,TotalDiffStatus*);
   void checkIfCanContinue( bool* pbContinue );
   void updateAvailabilities();
   void statusBarMessage( const QString& msg );
protected slots:
   void onDoubleClick( const QModelIndex& );
   void onExpanded();
   void	currentChanged( const QModelIndex & current, const QModelIndex & previous ); // override
private:
   class Data;
   friend class Data;
   Data* d;
   class DirMergeItemDelegate;
   friend class DirMergeItemDelegate;
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
   QTreeWidget* getInfoList() {return m_pInfoList;}
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

   QTreeWidget* m_pInfoList;
};


#endif
