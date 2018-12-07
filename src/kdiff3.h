/***************************************************************************
 *   Copyright (C) 2003-2007 by Joachim Eibl <joachim.eibl at gmx.de>      *
 *   Copyright (C) 2018 Michael Reeves reeves.87@gmail.com                 *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#ifndef KDIFF3_H
#define KDIFF3_H

#include "diff.h"

// include files for Qt
#include <QAction>
#include <QApplication>
#include <QEventLoop>
#include <QPointer>
#include <QScrollBar>
#include <QSplitter>
#include <QUrl>

// include files for KDE
#include <KAboutData>
#include <KConfigGroup>
#include <KMainWindow>
#include <KParts/MainWindow>
#include <KSharedConfig>
#include <KToggleAction>
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
class QStatusBar;
class QMenu;

class KToggleAction;
class KToolBar;
class KActionCollection;

namespace KParts {
class MainWindow;
}

class KDiff3Part;
class DirectoryMergeWindow;
class DirectoryMergeInfo;

class ReversibleScrollBar : public QScrollBar
{
    Q_OBJECT
    bool* m_pbRightToLeftLanguage;
    int m_realVal;

  public:
    ReversibleScrollBar(Qt::Orientation o, bool* pbRightToLeftLanguage)
        : QScrollBar(o)
    {
        m_pbRightToLeftLanguage = pbRightToLeftLanguage;
        m_realVal = 0;
        connect(this, SIGNAL(valueChanged(int)), this, SLOT(slotValueChanged(int)));
    }
    void setAgain() { setValue(m_realVal); }

    void setValue(int i)
    {
        if(m_pbRightToLeftLanguage && *m_pbRightToLeftLanguage)
            QScrollBar::setValue(maximum() - (i - minimum()));
        else
            QScrollBar::setValue(i);
    }

    int value() const
    {
        return m_realVal;
    }
  public Q_SLOTS:
    void slotValueChanged(int i)
    {
        m_realVal = i;
        if(m_pbRightToLeftLanguage && *m_pbRightToLeftLanguage)
            m_realVal = maximum() - (i - minimum());
        emit valueChanged2(m_realVal);
    }

  Q_SIGNALS:
    void valueChanged2(int);
};

class KDiff3App : public QSplitter
{
    Q_OBJECT

  public:
    /** constructor of KDiff3App, calls all init functions to create the application.
     */
    KDiff3App(QWidget* parent, const QString name, KDiff3Part* pKDiff3Part);
    ~KDiff3App() override;

    bool isPart();

    /** initializes the KActions of the application */
    void initActions(KActionCollection*);

    /** save general Options like all bar positions and status as well as the geometry
        and the recent file list to the configuration file */
    void saveOptions(KSharedConfigPtr);

    /** read general Options again and initialize all variables like the recent file list */
    void readOptions(KSharedConfigPtr);

    // Finish initialisation (virtual, so that it can be called from the shell too.)
    virtual void completeInit(const QString& fn1 = "", const QString& fn2 = "", const QString& fn3 = "");

    /** queryClose is called by KMainWindow on each closeEvent of a window. Against the
     * default implementation (only returns true), this calles saveModified() on the document object to ask if the document shall
     * be saved if Modified; on cancel the closeEvent is rejected.
     * @see KMainWindow#queryClose
     * @see KMainWindow#closeEvent
     */
    virtual bool queryClose();
    virtual bool isFileSaved();
    virtual bool isDirComparison();

  Q_SIGNALS:
    void createNewInstance(const QString& fn1, const QString& fn2, const QString& fn3);

  protected:
    void setLockPainting(bool bLock);
    void createCaption();
    void initDirectoryMergeActions();
    /** sets up the statusbar for the main window by initialzing a statuslabel. */
    void initStatusBar();

    /** creates the centerwidget of the KMainWindow instance and sets it as the view */
    void initView();

  public Q_SLOTS:

    /** open a file and load it into the document*/
    void slotFileOpen();
    void slotFileOpen2(QString fn1, QString fn2, QString fn3, QString ofn,
                       QString an1, QString an2, QString an3, const QSharedPointer<TotalDiffStatus> &pTotalDiffStatus);

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
    void slotStatusMsg(const QString& text);

  private:
    /** the configuration object of the application */
    //KConfig *config;

    // QAction pointers to enable/disable actions
    QAction* fileOpen;
    QAction* fileSave;
    QAction* fileSaveAs;
    QAction* filePrint;
    QAction* fileQuit;
    QAction* fileReload;
    QAction* editCut;
    QAction* editCopy;
    QAction* editPaste;
    QAction* editSelectAll;
    KToggleAction* viewToolBar;
    KToggleAction* viewStatusBar;

    ////////////////////////////////////////////////////////////////////////
    // Special KDiff3 specific stuff starts here
    QAction* editFind;
    QAction* editFindNext;

    QAction* goCurrent;
    QAction* goTop;
    QAction* goBottom;
    QAction* goPrevUnsolvedConflict;
    QAction* goNextUnsolvedConflict;
    QAction* goPrevConflict;
    QAction* goNextConflict;
    QAction* goPrevDelta;
    QAction* goNextDelta;
    KToggleAction* chooseA;
    KToggleAction* chooseB;
    KToggleAction* chooseC;
    KToggleAction* autoAdvance;
    KToggleAction* wordWrap;
    QAction* splitDiff;
    QAction* joinDiffs;
    QAction* addManualDiffHelp;
    QAction* clearManualDiffHelpList;
    KToggleAction* showWhiteSpaceCharacters;
    KToggleAction* showWhiteSpace;
    KToggleAction* showLineNumbers;
    QAction* chooseAEverywhere;
    QAction* chooseBEverywhere;
    QAction* chooseCEverywhere;
    QAction* chooseAForUnsolvedConflicts;
    QAction* chooseBForUnsolvedConflicts;
    QAction* chooseCForUnsolvedConflicts;
    QAction* chooseAForUnsolvedWhiteSpaceConflicts;
    QAction* chooseBForUnsolvedWhiteSpaceConflicts;
    QAction* chooseCForUnsolvedWhiteSpaceConflicts;
    QAction* autoSolve;
    QAction* unsolve;
    QAction* mergeHistory;
    QAction* mergeRegExp;
    KToggleAction* showWindowA;
    KToggleAction* showWindowB;
    KToggleAction* showWindowC;
    QAction* winFocusNext;
    QAction* winFocusPrev;
    QAction* winToggleSplitOrientation;
    KToggleAction* dirShowBoth;
    QAction* dirViewToggle;
    KToggleAction* overviewModeNormal;
    KToggleAction* overviewModeAB;
    KToggleAction* overviewModeAC;
    KToggleAction* overviewModeBC;

    QMenu* m_pMergeEditorPopupMenu;

    QSplitter* m_pMainSplitter;
    QWidget* m_pMainWidget;
    QWidget* m_pMergeWindowFrame;
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

    QSharedPointer<TotalDiffStatus> m_totalDiffStatus;

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
    bool m_bTimerBlock; // Synchronization

    OptionDialog* m_pOptionDialog;
    Options* m_pOptions;
    FindDialog* m_pFindDialog;

    void mainInit(QSharedPointer<TotalDiffStatus> pTotalDiffStatus=nullptr, bool bLoadFiles = true, bool bUseCurrentEncoding = false);
    bool m_bFinishMainInit;
    bool m_bLoadFiles;

    bool eventFilter(QObject* o, QEvent* e) override;
    void resizeEvent(QResizeEvent*) override;
    
    bool improveFilenames(bool bCreateNewInstance);

    bool canContinue();

    void choose(int choice);

    KActionCollection* actionCollection();
    QStatusBar* statusBar();
    KToolBar* toolBar(QLatin1String);
    KDiff3Part* m_pKDiff3Part;
    KParts::MainWindow* m_pKDiff3Shell;
    bool m_bAutoFlag;
    bool m_bAutoMode;
    void recalcWordWrap(int nofVisibleColumns = -1);
    bool m_bRecalcWordWrapPosted;
    void setHScrollBarRange();

    int m_iCumulativeWheelDelta;

    int m_visibleTextWidthForPrinting; // only needed during recalcWordWrap
    int m_firstD3LIdx;                 // only needed during recalcWordWrap
    QPointer<QEventLoop> m_pEventLoopForPrinting;

  public Q_SLOTS:
    void resizeDiffTextWindowHeight(int newHeight);
    void resizeMergeResultWindow();
    void slotRecalcWordWrap();
    void postRecalcWordWrap();
    void slotFinishRecalcWordWrap();

    void showPopupMenu(const QPoint& point);

    void scrollDiffTextWindow(int deltaX, int deltaY);
    void scrollMergeResultWindow(int deltaX, int deltaY);
    void setDiff3Line(int line);
    void sourceMask(int srcMask, int enabledMask);

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
    void slotFinishMainInit();
    void slotMergeCurrentFile();
    void slotReload();
    void slotCheckIfCanContinue(bool* pbContinue);
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
