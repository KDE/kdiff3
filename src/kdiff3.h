/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KDIFF3_H
#define KDIFF3_H

#include "diff.h"
#include "defmac.h"
#include "combiners.h"

#include <boost/signals2.hpp>

// include files for Qt
#include <QAction>
#include <QApplication>
#include <QEventLoop>
#include <QPointer>
#include <QScrollBar>
#include <QSplitter>

// include files for KDE
#include <KConfigGroup>
#include <KMainWindow>
#include <KParts/MainWindow>
#include <KSharedConfig>
#include <KToggleAction>
// forward declaration of the KDiff3 classes
class OptionDialog;

class Overview;
enum class e_OverviewMode;
class FindDialog;
//class ManualDiffHelpDialog;
class DiffTextWindow;
class DiffTextWindowFrame;
class MergeResultWindow;
class WindowTitleWidget;

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
        chk_connect_a(this, &ReversibleScrollBar::valueChanged, this, &ReversibleScrollBar::slotValueChanged);
    }
    void setAgain() { setValue(m_realVal); }

    void setValue(int i)
    {
        if(m_pbRightToLeftLanguage != nullptr && *m_pbRightToLeftLanguage)
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
        if(m_pbRightToLeftLanguage != nullptr && *m_pbRightToLeftLanguage)
            m_realVal = maximum() - (i - minimum());
        Q_EMIT valueChanged2(m_realVal);
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
    KDiff3App(QWidget* parent, const QString& name, KDiff3Part* pKDiff3Part);
    ~KDiff3App() override;

    bool isPart() const;

    /** initializes the KActions of the application */
    void initActions(KActionCollection*);

    /** save general Options like all bar positions and status as well as the geometry
        and the recent file list to the configuration file */
    void saveOptions(KSharedConfigPtr);

    /** read general Options again and initialize all variables like the recent file list */
    void readOptions(KSharedConfigPtr);

    // Finish initialisation (virtual, so that it can be called from the shell too.)
    virtual void completeInit(const QString& fn1 = QString(), const QString& fn2 = QString(), const QString& fn3 = QString());

    /** queryClose is called by KMainWindow on each closeEvent of a window. Against the
     * default implementation (only returns true), this calles saveModified() on the document object to ask if the document shall
     * be saved if Modified; on cancel the closeEvent is rejected.
     * @see KMainWindow#queryClose
     * @see KMainWindow#closeEvent
     */
    virtual bool queryClose();
    virtual bool isFileSaved() const;
    virtual bool isDirComparison() const;

    static bool isTripleDiff() { return m_bTripleDiff; }

    KActionCollection* actionCollection() const;

    static boost::signals2::signal<bool (), or> allowCut;
    static boost::signals2::signal<bool (), and> shouldContinue;
  Q_SIGNALS:
    void createNewInstance(const QString& fn1, const QString& fn2, const QString& fn3);

    void sigRecalcWordWrap();

    void finishDrop();

    void showWhiteSpaceToggled();
    void showLineNumbersToggled();
    void doRefresh();

    void autoSolve();
    void unsolve();
    void mergeHistory();
    void regExpAutoMerge();

    void goCurrent();
    void goTop();
    void goBottom();
    void goPrevUnsolvedConflict();

    void goNextUnsolvedConflict();
    void goPrevConflict();

    void goNextConflict();
    void goPrevDelta();
    void goNextDelta();

    void cut();

    void selectAll();

    void changeOverViewMode(e_OverviewMode);
public Q_SLOTS:

    /** open a file and load it into the document*/
    void slotFileOpen();
    void slotFileOpen2(const QString& fn1, const QString& fn2, const QString& fn3, const QString& ofn,
                       const QString& an1, const QString& an2, const QString& an3, TotalDiffStatus* pTotalDiffStatus);

    void slotFileNameChanged(const QString& fileName, e_SrcSelector winIdx);

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

    void resizeDiffTextWindowHeight(int newHeight);
    void slotRecalcWordWrap();
    void postRecalcWordWrap();
    void slotFinishRecalcWordWrap(int visibleTextWidth);

    void showPopupMenu(const QPoint& point);

    void scrollDiffTextWindow(int deltaX, int deltaY);
    void scrollMergeResultWindow(int deltaX, int deltaY);
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
    void slotEncodingChanged(QTextCodec*);

    void slotFinishDrop();

    void setHScrollBarRange();

    void slotFocusChanged(QWidget *old, QWidget *now);

  protected:
    void setLockPainting(bool bLock);
    void createCaption();
    void initDirectoryMergeActions();
    /** sets up the statusbar for the main window by initialzing a statuslabel. */
    void initStatusBar();

    /** creates the centerwidget of the KMainWindow instance and sets it as the view */
    void initView();

  private:
    void mainInit(TotalDiffStatus* pTotalDiffStatus = nullptr, bool bLoadFiles = true, bool bUseCurrentEncoding = false);

    void mainWindowEnable(bool bEnable);
    virtual void wheelEvent(QWheelEvent* pWheelEvent) override;
    virtual void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent*) override;

    bool improveFilenames(bool bCreateNewInstance);

    bool canContinue();

    void choose(e_SrcSelector choice);

    QStatusBar* statusBar() const;
    KToolBar* toolBar(const QLatin1String &toolBarId) const;
    void recalcWordWrap(int visibleTextWidthForPrinting = -1);

    bool canCut();

    /** the configuration object of the application */
    //KConfig *config;

    // QAction pointers to enable/disable actions
    QPointer<QAction> fileOpen;
    QPointer<QAction> fileSave;
    QPointer<QAction> fileSaveAs;
    QPointer<QAction> filePrint;
    QPointer<QAction> fileQuit;
    QPointer<QAction> fileReload;
    QPointer<QAction> editCut;
    QPointer<QAction> editCopy;
    QPointer<QAction> editPaste;
    QPointer<QAction> editSelectAll;
    KToggleAction* viewToolBar = nullptr;
    KToggleAction* viewStatusBar;

    ////////////////////////////////////////////////////////////////////////
    // Special KDiff3 specific stuff starts here
    QPointer<QAction> editFind;
    QPointer<QAction> editFindNext;

    QPointer<QAction> mGoCurrent;
    QPointer<QAction> mGoTop;
    QPointer<QAction> mGoBottom;
    QPointer<QAction> mGoPrevUnsolvedConflict;
    QPointer<QAction> mGoNextUnsolvedConflict;
    QPointer<QAction> mGoPrevConflict;
    QPointer<QAction> mGoNextConflict;
    QPointer<QAction> mGoPrevDelta;
    QPointer<QAction> mGoNextDelta;
    KToggleAction* chooseA;
    KToggleAction* chooseB;
    KToggleAction* chooseC;
    KToggleAction* autoAdvance;
    KToggleAction* wordWrap;
    QPointer<QAction> splitDiff;
    QPointer<QAction> joinDiffs;
    QPointer<QAction> addManualDiffHelp;
    QPointer<QAction> clearManualDiffHelpList;
    KToggleAction* showWhiteSpaceCharacters;
    KToggleAction* showWhiteSpace;
    KToggleAction* showLineNumbers;
    QPointer<QAction> mAutoSolve;
    QPointer<QAction> mUnsolve;
    QPointer<QAction> mMergeHistory;
    QPointer<QAction> mergeRegExp;
    KToggleAction* showWindowA;
    KToggleAction* showWindowB;
    KToggleAction* showWindowC;
    QPointer<QAction> winFocusNext;
    QPointer<QAction> winFocusPrev;
    QPointer<QAction> winToggleSplitOrientation;
    KToggleAction* dirShowBoth;
    QPointer<QAction> dirViewToggle;
    KToggleAction* overviewModeNormal;
    KToggleAction* overviewModeAB;
    KToggleAction* overviewModeAC;
    KToggleAction* overviewModeBC;

    QMenu* m_pMergeEditorPopupMenu;

    QSplitter* m_pMainSplitter = nullptr;
    QWidget* m_pMainWidget = nullptr;
    QWidget* m_pMergeWindowFrame = nullptr;
    ReversibleScrollBar* m_pHScrollBar = nullptr;

    DiffTextWindow* m_pDiffTextWindow1 = nullptr;
    DiffTextWindow* m_pDiffTextWindow2 = nullptr;
    DiffTextWindow* m_pDiffTextWindow3 = nullptr;
    DiffTextWindowFrame* m_pDiffTextWindowFrame1 = nullptr;
    DiffTextWindowFrame* m_pDiffTextWindowFrame2 = nullptr;
    DiffTextWindowFrame* m_pDiffTextWindowFrame3 = nullptr;
    QSplitter* m_pDiffWindowSplitter = nullptr;

    MergeResultWindow* m_pMergeResultWindow = nullptr;
    WindowTitleWidget* m_pMergeResultWindowTitle;
    static bool m_bTripleDiff;

    QSplitter* m_pDirectoryMergeSplitter = nullptr;
    DirectoryMergeWindow* m_pDirectoryMergeWindow = nullptr;
    DirectoryMergeInfo* m_pDirectoryMergeInfo;
    bool m_bDirCompare = false;

    Overview* m_pOverview = nullptr;

    QWidget* m_pCornerWidget = nullptr;

    TotalDiffStatus m_totalDiffStatus;

    QSharedPointer<SourceData> m_sd1 = QSharedPointer<SourceData>::create();
    QSharedPointer<SourceData> m_sd2 = QSharedPointer<SourceData>::create();
    QSharedPointer<SourceData> m_sd3 = QSharedPointer<SourceData>::create();

    QSharedPointer<class DirectoryInfo> m_dirinfo;

    QString m_outputFilename;
    bool m_bDefaultFilename;

    DiffList m_diffList12;
    DiffList m_diffList23;
    DiffList m_diffList13;

    QSharedPointer<DiffBufferInfo> m_diffBufferInfo = QSharedPointer<DiffBufferInfo>::create();
    Diff3LineList m_diff3LineList;
    Diff3LineVector m_diff3LineVector;
    //ManualDiffHelpDialog* m_pManualDiffHelpDialog;
    ManualDiffHelpList m_manualDiffHelpList;

    int m_neededLines;
    int m_DTWHeight;
    bool m_bOutputModified = false;
    bool m_bFileSaved = false;
    bool m_bTimerBlock = false; // Synchronization

    OptionDialog* m_pOptionDialog = nullptr;
    QSharedPointer<Options> m_pOptions = nullptr;
    FindDialog* m_pFindDialog = nullptr;

    bool m_bFinishMainInit = false;
    bool m_bLoadFiles = false;

    KDiff3Part* m_pKDiff3Part = nullptr;
    KParts::MainWindow* m_pKDiff3Shell = nullptr;
    bool m_bAutoFlag = false;
    bool m_bAutoMode = false;
    bool m_bRecalcWordWrapPosted = false;

    int m_firstD3LIdx;                 // only needed during recalcWordWrap
    QPointer<QEventLoop> m_pEventLoopForPrinting;

    /*
      This list exists solely to auto disconnect boost signals.
    */
    std::list<boost::signals2::scoped_connection> connections;
};

#endif // KDIFF3_H
