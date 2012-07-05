/***************************************************************************
                          kdiff3.h  -  description
                             -------------------
    begin                : Don Jul 11 12:31:29 CEST 2002
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

#ifndef KDIFF3_H
#define KDIFF3_H

#include "diff.h"

#include <config-kdiff3.h>

// include files for Qt
#include <QSplitter>
#include <QScrollBar>

// include files for KDE
#include <kapplication.h>
#include <kmainwindow.h>
#include <kaction.h>
#include <kurl.h>
#include <kparts/mainwindow.h>
#include <ktoggleaction.h>


// forward declaration of the KDiff3 classes
class OptionDialog;
class FindDialog;
class ManualDiffHelpDialog;
class DiffTextWindow;
class DiffTextWindowFrame;
class MergeResultWindow;
class WindowTitleWidget;
class Overview;

class QScrollBar;
class QSplitter;


class KDiff3Part;
class DirectoryMergeWindow;
class DirectoryMergeInfo;


class ReversibleScrollBar : public QScrollBar
{
   Q_OBJECT
   bool* m_pbRightToLeftLanguage;
   int m_realVal;
public:
   ReversibleScrollBar( Qt::Orientation o, bool* pbRightToLeftLanguage )
      : QScrollBar( o )
   {
      m_pbRightToLeftLanguage=pbRightToLeftLanguage;
      m_realVal=0;
      connect( this, SIGNAL(valueChanged(int)), this, SLOT(slotValueChanged(int)));
   }
   void setAgain(){ setValue(m_realVal); }
public slots:
   void slotValueChanged(int i)
   {
      m_realVal = i;
      if(m_pbRightToLeftLanguage && *m_pbRightToLeftLanguage)
         m_realVal = maximum()-(i-minimum());
      emit valueChanged2(m_realVal);
   }
   void setValue(int i)
   {
      if(m_pbRightToLeftLanguage && *m_pbRightToLeftLanguage)
         QScrollBar::setValue( maximum()-(i-minimum())  );
      else
         QScrollBar::setValue( i );
   }
signals:
   void valueChanged2(int);
};

class KDiff3App : public QSplitter
{
  Q_OBJECT

  public:
    /** constructor of KDiff3App, calls all init functions to create the application.
     */
    KDiff3App( QWidget* parent, const char* name, KDiff3Part* pKDiff3Part );
    ~KDiff3App();

    bool isPart();

    /** initializes the KActions of the application */
    void initActions( KActionCollection* );

    /** save general Options like all bar positions and status as well as the geometry
        and the recent file list to the configuration file */
    void saveOptions( KSharedConfigPtr );

    /** read general Options again and initialize all variables like the recent file list */
    void readOptions( KSharedConfigPtr );

    // Finish initialisation (virtual, so that it can be called from the shell too.)
    virtual void completeInit(const QString& fn1="", const QString& fn2="", const QString& fn3="");

    /** queryClose is called by KMainWindow on each closeEvent of a window. Against the
     * default implementation (only returns true), this calles saveModified() on the document object to ask if the document shall
     * be saved if Modified; on cancel the closeEvent is rejected.
     * @see KMainWindow#queryClose
     * @see KMainWindow#closeEvent
     */
    virtual bool queryClose();
    virtual bool isFileSaved();
    virtual bool isDirComparison();

  signals:
     void createNewInstance( const QString& fn1, const QString& fn2, const QString& fn3 );
  protected:
    void initDirectoryMergeActions();
    /** sets up the statusbar for the main window by initialzing a statuslabel. */
    void initStatusBar();

    /** creates the centerwidget of the KMainWindow instance and sets it as the view */
    void initView();

  public slots:

    /** open a file and load it into the document*/
    void slotFileOpen();
    void slotFileOpen2( QString fn1, QString fn2, QString fn3, QString ofn,
                        QString an1, QString an2, QString an3, TotalDiffStatus* pTotalDiffStatus );

    void slotFileNameChanged(const QString& fileName, int winIdx);

    /** save a document */
    void slotFileSave();
    /** save a document by a new filename*/
    void slotFileSaveAs();

    void slotFilePrint();

    /** closes all open windows by calling close() on each memberList item until the list is empty, then quits the application.
     * If queryClose() returns false because the user canceled the saveModified() dialog, the closing breaks.
     */
    void slotFileQuit();
    /** put the marked text/object into the clipboard and remove
     *  it from the document
     */
    void slotEditCut();
    /** put the marked text/object into the clipboard
     */
    void slotEditCopy();
    /** paste the clipboard into the document
     */
    void slotEditPaste();
    /** toggles the toolbar
     */
    void slotViewToolBar();
    /** toggles the statusbar
     */
    void slotViewStatusBar();
    /** changes the statusbar contents for the standard label permanently, used to indicate current actions.
     * @param text the text that is displayed in the statusbar
     */
    void slotStatusMsg(const QString &text);

  private:
    /** the configuration object of the application */
    //KConfig *config;

    // KAction pointers to enable/disable actions
    KAction* fileOpen;
    KAction* fileSave;
    KAction* fileSaveAs;
    KAction* filePrint;
    KAction* fileQuit;
    KAction* fileReload;
    KAction* editCut;
    KAction* editCopy;
    KAction* editPaste;
    KAction* editSelectAll;
    KToggleAction* viewToolBar;
    KToggleAction* viewStatusBar;

////////////////////////////////////////////////////////////////////////
// Special KDiff3 specific stuff starts here
    KAction *editFind;
    KAction *editFindNext;

    KAction *goCurrent;
    KAction *goTop;
    KAction *goBottom;
    KAction *goPrevUnsolvedConflict;
    KAction *goNextUnsolvedConflict;
    KAction *goPrevConflict;
    KAction *goNextConflict;
    KAction *goPrevDelta;
    KAction *goNextDelta;
    KToggleAction *chooseA;
    KToggleAction *chooseB;
    KToggleAction *chooseC;
    KToggleAction *autoAdvance;
    KToggleAction *wordWrap;
    KAction* splitDiff;
    KAction* joinDiffs;
    KAction* addManualDiffHelp;
    KAction* clearManualDiffHelpList;
    KToggleAction *showWhiteSpaceCharacters;
    KToggleAction *showWhiteSpace;
    KToggleAction *showLineNumbers;
    KAction* chooseAEverywhere;
    KAction* chooseBEverywhere;
    KAction* chooseCEverywhere;
    KAction* chooseAForUnsolvedConflicts;
    KAction* chooseBForUnsolvedConflicts;
    KAction* chooseCForUnsolvedConflicts;
    KAction* chooseAForUnsolvedWhiteSpaceConflicts;
    KAction* chooseBForUnsolvedWhiteSpaceConflicts;
    KAction* chooseCForUnsolvedWhiteSpaceConflicts;
    KAction* autoSolve;
    KAction* unsolve;
    KAction* mergeHistory;
    KAction* mergeRegExp;
    KToggleAction *showWindowA;
    KToggleAction *showWindowB;
    KToggleAction *showWindowC;
    KAction *winFocusNext;
    KAction *winFocusPrev;
    KAction* winToggleSplitOrientation;
    KToggleAction *dirShowBoth;
    KAction *dirViewToggle;
    KToggleAction *overviewModeNormal;
    KToggleAction *overviewModeAB;
    KToggleAction *overviewModeAC;
    KToggleAction *overviewModeBC;


    QMenu* m_pMergeEditorPopupMenu;

    QSplitter*  m_pMainSplitter;
    QWidget*    m_pMainWidget;
    QWidget*    m_pMergeWindowFrame;
    ReversibleScrollBar* m_pHScrollBar;
    QScrollBar* m_pDiffVScrollBar;
    QScrollBar* m_pMergeVScrollBar;

    DiffTextWindow* m_pDiffTextWindow1;
    DiffTextWindow* m_pDiffTextWindow2;
    DiffTextWindow* m_pDiffTextWindow3;
    DiffTextWindowFrame* m_pDiffTextWindowFrame1;
    DiffTextWindowFrame* m_pDiffTextWindowFrame2;
    DiffTextWindowFrame* m_pDiffTextWindowFrame3;
    QSplitter* m_pDiffWindowSplitter;

    MergeResultWindow* m_pMergeResultWindow;
    WindowTitleWidget* m_pMergeResultWindowTitle;
    bool m_bTripleDiff;

    QSplitter* m_pDirectoryMergeSplitter;
    DirectoryMergeWindow* m_pDirectoryMergeWindow;
    DirectoryMergeInfo* m_pDirectoryMergeInfo;
    bool m_bDirCompare;

    Overview* m_pOverview;

    QWidget* m_pCornerWidget;

    TotalDiffStatus m_totalDiffStatus;

    SourceData m_sd1;
    SourceData m_sd2;
    SourceData m_sd3;

   QString m_outputFilename;
   bool m_bDefaultFilename;

   DiffList m_diffList12;
   DiffList m_diffList23;
   DiffList m_diffList13;

   DiffBufferInfo m_diffBufferInfo;
   Diff3LineList m_diff3LineList;
   Diff3LineVector m_diff3LineVector;
   //ManualDiffHelpDialog* m_pManualDiffHelpDialog;
   ManualDiffHelpList m_manualDiffHelpList;

   int m_neededLines;
   int m_DTWHeight;
   bool m_bOutputModified;
   bool m_bFileSaved;
   bool m_bTimerBlock;      // Synchronisation

   OptionDialog* m_pOptionDialog;
   Options*      m_pOptions;
   FindDialog*   m_pFindDialog;

   void init( bool bAuto=false, TotalDiffStatus* pTotalDiffStatus=0, bool bLoadFiles=true, bool bUseCurrentEncoding = false);

   virtual bool eventFilter( QObject* o, QEvent* e );
   virtual void resizeEvent(QResizeEvent*);

   bool improveFilenames(bool bCreateNewInstance);

   bool canContinue();

   void choose(int choice);

   KActionCollection* actionCollection();
   KStatusBar*        statusBar();
   KToolBar*          toolBar(const char*);
   KDiff3Part*        m_pKDiff3Part;
   KParts::MainWindow*       m_pKDiff3Shell;
   bool m_bAutoFlag;
   bool m_bAutoMode;
   void recalcWordWrap(int nofVisibleColumns=-1);
   void setHScrollBarRange();

   int m_iCumulativeWheelDelta;

public slots:
   void resizeDiffTextWindow(int newWidth, int newHeight);
   void resizeMergeResultWindow();
   void slotRecalcWordWrap();

   void showPopupMenu( const QPoint& point );

   void scrollDiffTextWindow( int deltaX, int deltaY );
   void scrollMergeResultWindow( int deltaX, int deltaY );
   void setDiff3Line( int line );
   void sourceMask( int srcMask, int enabledMask );

   void slotDirShowBoth();
   void slotDirViewToggle();

   void slotUpdateAvailabilities();
   void slotEditSelectAll();
   void slotEditFind();
   void slotEditFindNext();
   void slotGoCurrent();
   void slotGoTop();
   void slotGoBottom();
   void slotGoPrevUnsolvedConflict();
   void slotGoNextUnsolvedConflict();
   void slotGoPrevConflict();
   void slotGoNextConflict();
   void slotGoPrevDelta();
   void slotGoNextDelta();
   void slotChooseA();
   void slotChooseB();
   void slotChooseC();
   void slotAutoSolve();
   void slotUnsolve();
   void slotMergeHistory();
   void slotRegExpAutoMerge();
   void slotChooseAEverywhere();
   void slotChooseBEverywhere();
   void slotChooseCEverywhere();
   void slotChooseAForUnsolvedConflicts();
   void slotChooseBForUnsolvedConflicts();
   void slotChooseCForUnsolvedConflicts();
   void slotChooseAForUnsolvedWhiteSpaceConflicts();
   void slotChooseBForUnsolvedWhiteSpaceConflicts();
   void slotChooseCForUnsolvedWhiteSpaceConflicts();
   void slotConfigure();
   void slotConfigureKeys();
   void slotRefresh();
   void slotSelectionEnd();
   void slotSelectionStart();
   void slotClipboardChanged();
   void slotOutputModified(bool);
   void slotAfterFirstPaint();
   void slotMergeCurrentFile();
   void slotReload();
   void slotCheckIfCanContinue( bool* pbContinue );
   void slotShowWhiteSpaceToggled();
   void slotShowLineNumbersToggled();
   void slotAutoAdvanceToggled();
   void slotWordWrapToggled();
   void slotShowWindowAToggled();
   void slotShowWindowBToggled();
   void slotShowWindowCToggled();
   void slotWinFocusNext();
   void slotWinFocusPrev();
   void slotWinToggleSplitterOrientation();
   void slotOverviewNormal();
   void slotOverviewAB();
   void slotOverviewAC();
   void slotOverviewBC();
   void slotSplitDiff();
   void slotJoinDiffs();
   void slotAddManualDiffHelp();
   void slotClearManualDiffHelpList();
   void slotNoRelevantChangesDetected();
   void slotEncodingChangedA(QTextCodec*);
   void slotEncodingChangedB(QTextCodec*);
   void slotEncodingChangedC(QTextCodec*);
};

#endif // KDIFF3_H
