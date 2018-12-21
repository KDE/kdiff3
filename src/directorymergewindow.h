/***************************************************************************
 *   Copyright (C) 2003-2007 by Joachim Eibl <joachim.eibl at gmx.de>      *
 *   Copyright (C) 2018 Michael Reeves reeves.87@gmail.com                 *
 *                                                                         *                                                                         *
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
class QAction;
class KToggleAction;
class KActionCollection;
class TotalDiffStatus;
class DirectoryInfo;

class MergeFileInfos;

class DirectoryMergeWindow : public QTreeView
{
   Q_OBJECT
public:
   DirectoryMergeWindow( QWidget* pParent, Options* pOptions );
   ~DirectoryMergeWindow() override;
   void setDirectoryMergeInfo(DirectoryMergeInfo* p);
   bool init(
      QSharedPointer<DirectoryInfo> dirInfo,
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

   void mousePressEvent( QMouseEvent* e ) override;
   void keyPressEvent( QKeyEvent* e ) override;
   void focusInEvent( QFocusEvent* e ) override;
   void focusOutEvent( QFocusEvent* e ) override;
   void contextMenuEvent( QContextMenuEvent* e ) override;

   QString getDirNameA() const;
   QString getDirNameB() const;
   QString getDirNameC() const;
   QString getDirNameDest() const;

 public Q_SLOTS:
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

Q_SIGNALS:
   void startDiffMerge(QString fn1,QString fn2, QString fn3, QString ofn, QString,QString,QString,const QSharedPointer<TotalDiffStatus>&);
   void checkIfCanContinue( bool* pbContinue );
   void updateAvailabilities();
   void statusBarMessage( const QString& msg );
protected Q_SLOTS:
   void onDoubleClick( const QModelIndex& );
   void onExpanded();
   void	currentChanged( const QModelIndex & current, const QModelIndex & previous ) override; // override
private:
  class DirectoryMergeWindowPrivate;
  friend class DirectoryMergeWindowPrivate;
  DirectoryMergeWindowPrivate* d;
  class DirMergeItemDelegate;
  friend class DirMergeItemDelegate;
};

class DirectoryMergeInfo : public QFrame
{
   Q_OBJECT
public:
   explicit DirectoryMergeInfo( QWidget* pParent );
   void setInfo(
      const FileAccess& APath,
      const FileAccess& BPath,
      const FileAccess& CPath,
      const FileAccess& DestPath,
      MergeFileInfos& mfi );
   QTreeWidget* getInfoList() {return m_pInfoList;}
   bool eventFilter( QObject* o, QEvent* e ) override;
Q_SIGNALS:
   void gotFocus();
private:
   void addListViewItem(const QString& dir, const QString& basePath, FileAccess* fi);

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
