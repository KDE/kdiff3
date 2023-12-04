// clang-format off
/*
 *  This file is part of KDiff3.
 *
 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
*/
// clang-format on

// application specific includes
#include "kdiff3.h"

#include "defmac.h"
#include "directorymergewindow.h"
#include "fileaccess.h"
#include "guiutils.h"
#include "kdiff3_part.h"
#include "kdiff3_shell.h"
#include "optiondialog.h"
#include "progress.h"
#include "smalldialogs.h"
#include "difftextwindow.h"
#include "mergeresultwindow.h"
#include "RLPainter.h"
#include "Utils.h"
// Standard c/c++ includes
#include <memory>
#ifndef Q_OS_WIN
#include <unistd.h>
#endif
#include <utility>
// Anything else that isn't Qt/Frameworks
#include <boost/signals2.hpp>
// include files for QT
#include <QCheckBox>
#include <QClipboard>
#include <QCommandLineParser>
#include <QDesktopWidget>
#include <QDir>
#include <QDockWidget>
#include <QFileDialog>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QPaintDevice>
#include <QPainter>
#include <QPointer>
#include <QPrintDialog>
#include <QPrinter>
#include <QPushButton>
#include <QShortcut>
#include <QSplitter>
#include <QStatusBar>
#include <QTextEdit>
#include <QTextStream>
#include <QUrl>
// include files for KDE
#include <KCrash>
#include <KConfig>
#include <KLocalizedString>
#include <KMessageBox>
#include <KStandardAction>
#include <KActionCollection>
#include <KToggleAction>
#include <KToolBar>

bool KDiff3App::m_bTripleDiff = false;

boost::signals2::signal<QString (), FirstNonEmpty<QString>> KDiff3App::getSelection;
boost::signals2::signal<bool (), or> KDiff3App::allowCopy;
boost::signals2::signal<bool (), or> KDiff3App::allowCut;

/*
    To be a constexpr the QLatin1String constructor must be given the size of the string explicitly.
    Otherwise it calls strlen which is not a constexpr.
*/
constexpr QLatin1String MAIN_TOOLBAR_NAME = QLatin1String("mainToolBar", sizeof("mainToolBar") - 1);

KActionCollection* KDiff3App::actionCollection() const
{
    if(m_pKDiff3Shell == nullptr)
        return m_pKDiff3Part->actionCollection();

    return m_pKDiff3Shell->actionCollection();
}

QStatusBar* KDiff3App::statusBar() const
{
    if(m_pKDiff3Shell == nullptr)
        return nullptr;

    return m_pKDiff3Shell->statusBar();
}

KToolBar* KDiff3App::toolBar(const QLatin1String &toolBarId) const
{
    if(m_pKDiff3Shell == nullptr)
        return nullptr;

    return m_pKDiff3Shell->toolBar(toolBarId);
}

bool KDiff3App::isPart() const
{
    return m_pKDiff3Shell == nullptr;
}

bool KDiff3App::isFileSaved() const
{
    return m_bFileSaved;
}

bool KDiff3App::isDirComparison() const
{
    return m_bDirCompare;
}

/*
    Don't call completeInit from here it will be called in KDiff3Shell as needed.
*/
KDiff3App::KDiff3App(QWidget* pParent, const QString& name, KDiff3Part* pKDiff3Part):
    QMainWindow(pParent) //previously KMainWindow
{
    setWindowFlags(Qt::Widget);
    setObjectName(name);
    m_pKDiff3Part = pKDiff3Part;
    m_pKDiff3Shell = qobject_cast<KParts::MainWindow*>(pParent);

    m_pCentralWidget = new QWidget(this);
    QVBoxLayout* pCentralLayout = new QVBoxLayout(m_pCentralWidget);
    pCentralLayout->setContentsMargins(0, 0, 0, 0);
    pCentralLayout->setSpacing(0);
    setCentralWidget(m_pCentralWidget);

    m_pMainWidget = new QWidget(m_pCentralWidget);
    m_pMainWidget->setObjectName("MainWidget");
    pCentralLayout->addWidget(m_pMainWidget);
    m_pMainWidget->hide();

    setWindowTitle("KDiff3");
    setUpdatesEnabled(false);
    KCrash::initialize();

    // set Disabled to same color as enabled to prevent flicker in DirectoryMergeWindow
    QPalette pal;
    pal.setBrush(QPalette::Base, pal.brush(QPalette::Active, QPalette::Base));
    pal.setColor(QPalette::Text, pal.color(QPalette::Active, QPalette::Text));
    setPalette(pal);

    // Setup progress ProgressDialog. No progress can be shown before this point.
    // ProgressProxy will otherwise emit no-ops to disconnected boost signals.
    if(g_pProgressDialog == nullptr)
    {
        g_pProgressDialog = new ProgressDialog(this, statusBar());
        g_pProgressDialog->setStayHidden(true);
    }

    // All default values must be set before calling readOptions().
    m_pOptionDialog = new OptionDialog(m_pKDiff3Shell != nullptr, this);
    chk_connect(m_pOptionDialog, &OptionDialog::applyDone, this, &KDiff3App::slotRefresh);

    // This is just a convenience variable to make code that accesses options more readable
    m_pOptions = m_pOptionDialog->getOptions();

    m_pOptionDialog->readOptions(KSharedConfig::openConfig());

    // Option handling: Only when pParent==0 (no parent)
    qint32 argCount = KDiff3Shell::getParser()->optionNames().count() + KDiff3Shell::getParser()->positionalArguments().count();
    bool hasArgs = !isPart() && argCount > 0;
    if(hasArgs) {
        QString s;
        QString title;
        if(KDiff3Shell::getParser()->isSet("confighelp"))
        {
            s = m_pOptionDialog->calcOptionHelp();
            title = i18n("Current Configuration:");
        }
        else
        {
            s = m_pOptionDialog->parseOptions(KDiff3Shell::getParser()->values("cs"));
            title = i18n("Config Option Error:");
        }
        if(!s.isEmpty())
        {
#ifndef Q_OS_WIN
            if(isatty(fileno(stderr)) != 1)
#endif
            {
                QPointer<QDialog> pDialog = QPointer<QDialog>(new QDialog(this));
                pDialog->setAttribute(Qt::WA_DeleteOnClose);
                pDialog->setModal(true);
                pDialog->setWindowTitle(title);
                QPointer<QVBoxLayout> pVBoxLayout = new QVBoxLayout(pDialog);
                QPointer<QTextEdit> pTextEdit = QPointer<QTextEdit>(new QTextEdit(pDialog));
                pTextEdit->setText(s);
                pTextEdit->setReadOnly(true);
                pTextEdit->setWordWrapMode(QTextOption::NoWrap);
                pVBoxLayout->addWidget(pTextEdit);
                pDialog->resize(600, 400);
                pDialog->exec();
            }
#if !defined(Q_OS_WIN)
            else
            {
                // Launched from a console
                QTextStream outStream(stdout);
                outStream << title << "\n";
                outStream << s;//newline already appended by parseOptions
            }
#endif
            exit(1);
        }
    }

    m_sd1->setOptions(m_pOptions);
    m_sd2->setOptions(m_pOptions);
    m_sd3->setOptions(m_pOptions);

#ifdef ENABLE_AUTO
    m_bAutoFlag = hasArgs && KDiff3Shell::getParser()->isSet("auto") && !KDiff3Shell::getParser()->isSet("noauto");
#else
    m_bAutoFlag = false;
#endif

    m_bAutoMode = m_bAutoFlag || m_pOptions->m_bAutoSaveAndQuitOnMergeWithoutConflicts;
    if(hasArgs) {
        m_outputFilename = KDiff3Shell::getParser()->value("output");

        if(m_outputFilename.isEmpty())
            m_outputFilename = KDiff3Shell::getParser()->value("out");

        if(!m_outputFilename.isEmpty())
            m_outputFilename = FileAccess(m_outputFilename, true).absoluteFilePath();

        if(m_bAutoMode && m_outputFilename.isEmpty())
        {
            if(m_bAutoFlag)
            {
                QTextStream(stderr) << i18n("Option --auto used, but no output file specified.") << "\n";
            }
            m_bAutoMode = false;
        }

        if(m_outputFilename.isEmpty() && KDiff3Shell::getParser()->isSet("merge"))
        {
            m_outputFilename = "unnamed.txt";
            m_bDefaultFilename = true;
        }
        else
        {
            m_bDefaultFilename = false;
        }

        QStringList args = KDiff3Shell::getParser()->positionalArguments();

        m_sd1->setFilename(KDiff3Shell::getParser()->value("base"));
        if(m_sd1->isEmpty()) {
            if(args.count() > 0) m_sd1->setFilename(args[0]); // args->arg(0)
            if(args.count() > 1) m_sd2->setFilename(args[1]);
            if(args.count() > 2) m_sd3->setFilename(args[2]);
        }
        else
        {
            if(args.count() > 0) m_sd2->setFilename(args[0]);
            if(args.count() > 1) m_sd3->setFilename(args[1]);
        }
        //Set m_bDirCompare flag
        m_bDirCompare = m_sd1->isDir();

        QStringList aliasList = KDiff3Shell::getParser()->values( "fname" );
        QStringList::Iterator ali = aliasList.begin();

        QString an1 = KDiff3Shell::getParser()->value("L1");
        if(!an1.isEmpty()) {
            m_sd1->setAliasName(an1);
        }
        else if(ali != aliasList.end())
        {
            m_sd1->setAliasName(*ali);
            ++ali;
        }

        QString an2 = KDiff3Shell::getParser()->value("L2");
        if(!an2.isEmpty()) {
            m_sd2->setAliasName(an2);
        }
        else if(ali != aliasList.end())
        {
            m_sd2->setAliasName(*ali);
            ++ali;
        }

        QString an3 = KDiff3Shell::getParser()->value("L3");
        if(!an3.isEmpty()) {
            m_sd3->setAliasName(an3);
        }
        else if(ali != aliasList.end())
        {
            m_sd3->setAliasName(*ali);
            ++ali;
        }
    }
    else
    {
        m_bDefaultFilename = false;
    }
    g_pProgressDialog->setStayHidden(m_bAutoMode);

    ///////////////////////////////////////////////////////////////////
    // call inits to invoke all other construction parts
    initActions(actionCollection());
    //Warning: Call this before connecting KDiff3App::slotUpdateAvailabilities or calling KXMLGUIClient::setXMLFile
    MergeResultWindow::initActions(actionCollection());

    initStatusBar();

    m_pFindDialog = new FindDialog(this);
    chk_connect(m_pFindDialog, &FindDialog::findNext, this, &KDiff3App::slotEditFindNext);

    mEscapeAction->setEnabled(m_pOptions->m_bEscapeKeyQuits);
    autoAdvance->setChecked(m_pOptions->m_bAutoAdvance);
    showWhiteSpaceCharacters->setChecked(m_pOptions->m_bShowWhiteSpaceCharacters);
    showWhiteSpace->setChecked(m_pOptions->m_bShowWhiteSpace);
    showWhiteSpaceCharacters->setEnabled(m_pOptions->m_bShowWhiteSpace);
    showLineNumbers->setChecked(m_pOptions->m_bShowLineNumbers);
    wordWrap->setChecked(m_pOptions->wordWrapOn());
    if(!isPart())
    {
        /*
            No need to restore window size and position here that is done later.
                See KDiff3App::completeInit
        */
        viewStatusBar->setChecked(m_pOptions->isStatusBarVisable());
        slotViewStatusBar();

        KToolBar *mainToolBar = toolBar(MAIN_TOOLBAR_NAME);
        if(mainToolBar != nullptr){
            mainToolBar->mainWindow()->addToolBar(Qt::TopToolBarArea, mainToolBar);
        }

    }
    slotRefresh();

    m_pDirectoryMergeDock = new QDockWidget(i18n("Directory merge"), this);
    m_pDirectoryMergeWindow = new DirectoryMergeWindow(m_pDirectoryMergeDock, m_pOptions, *this);
    m_pDirectoryMergeDock->setObjectName("DirectoryMergeDock");
    m_pDirectoryMergeDock->setWidget(m_pDirectoryMergeWindow);
    m_pDirectoryMergeDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    m_pDirectoryMergeInfoDock = new QDockWidget(i18n("Merge info"), this);
    m_pDirectoryMergeInfoDock->setObjectName("MergeInfoDock");
    m_pDirectoryMergeInfo = new DirectoryMergeInfo(m_pDirectoryMergeInfoDock);
    m_pDirectoryMergeInfoDock->setWidget(m_pDirectoryMergeInfo);
    m_pDirectoryMergeInfoDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    m_pDirectoryMergeWindow->setDirectoryMergeInfo(m_pDirectoryMergeInfo);
    //Warning: Make sure DirectoryMergeWindow::initActions is called before this point or we can crash when selectionChanged is sent.
    m_pDirectoryMergeWindow->setupConnections(this);
    addDockWidget(Qt::LeftDockWidgetArea, m_pDirectoryMergeDock);
    splitDockWidget(m_pDirectoryMergeDock, m_pDirectoryMergeInfoDock, Qt::Vertical);

    chk_connect(QApplication::clipboard(), &QClipboard::dataChanged, this, &KDiff3App::slotClipboardChanged);
    chk_connect_q(this, &KDiff3App::sigRecalcWordWrap, this, &KDiff3App::slotRecalcWordWrap);
    chk_connect(this, &KDiff3App::finishDrop, this, &KDiff3App::slotFinishDrop);

    connections.push_back(allowCut.connect(boost::bind(&KDiff3App::canCut, this)));
    connections.push_back(allowCopy.connect(boost::bind(&KDiff3App::canCopy, this)));

    m_pDirectoryMergeWindow->initDirectoryMergeActions(this, actionCollection());

    if(qApp != nullptr)
        chk_connect(qApp, &QApplication::focusChanged, this, &KDiff3App::slotFocusChanged);

    chk_connect_a(this, &KDiff3App::updateAvailabilities, this, &KDiff3App::slotUpdateAvailabilities);
}

/*
    This function is only concerned with qt objects that don't support canCut.
    allowCut() or's the results from all canCut signals
*/
bool KDiff3App::canCut()
{
    QWidget* focus = focusWidget();

    return (qobject_cast<QLineEdit*>(focus) != nullptr || qobject_cast<QTextEdit*>(focus) != nullptr);
}

/*
    Please do not add logic for MerdgeResultWindow or DiffTextWindow here they have their own handlers.
    This function is only concerned with qt objects that don't support canCopy.
    allowCopy() or's the results from all canCopy signals sent via boost.

    returns true if a QLineEdit is in focus because Qt handles these internally.
*/
bool KDiff3App::canCopy()
{
    QWidget* focus = focusWidget();

    return (qobject_cast<QLineEdit*>(focus) != nullptr || qobject_cast<QTextEdit*>(focus) != nullptr);
}
/*
    Make sure Edit menu tracks focus correctly.
*/
void KDiff3App::slotFocusChanged(QWidget* old, QWidget* now)
{
    Q_UNUSED(now);
    Q_UNUSED(old);

    qCDebug(kdiffMain) << "[KDiff3App::slotFocusChanged] old = " << old << ", new =" << now;
    //This needs to be called after the focus change is complete
    QMetaObject::invokeMethod(this, &KDiff3App::updateAvailabilities, Qt::QueuedConnection);
}

// Do file comparision.
bool KDiff3App::doFileCompare()
{
    bool bSuccess = true;
    improveFilenames();
    m_pDirectoryMergeDock->hide();
    m_pDirectoryMergeInfoDock->hide();

    mainInit(m_totalDiffStatus);
    if(m_totalDiffStatus->getUnsolvedConflicts() != 0)
        bSuccess = false;

    if(m_bAutoMode && m_totalDiffStatus->getUnsolvedConflicts() == 0)
    {
        QSharedPointer<SourceData> pSD = nullptr;
        if(m_sd3->isEmpty())
        {
            if(m_totalDiffStatus->isBinaryEqualAB())
            {
                pSD = m_sd1;
            }
        }
        else
        {
            if(m_totalDiffStatus->isBinaryEqualBC() || m_totalDiffStatus->isBinaryEqualAB())
            {
                //if B==C (assume A is old), if A==B then C has changed
                pSD = m_sd3;
            }
            else if(m_totalDiffStatus->isBinaryEqualAC())
            {
                pSD = m_sd2; // assuming B has changed
            }
        }

        if(pSD != nullptr)
        {
            // Save this file directly, not via the merge result window.
            FileAccess fa(m_outputFilename);
            if(m_pOptions->m_bDmCreateBakFiles && fa.exists())
            {
                fa.createBackup(".orig");
            }

            bSuccess = pSD->saveNormalDataAs(m_outputFilename);
            if(!bSuccess)
                KMessageBox::error(this, i18n("Saving failed."));
        }
        else if(m_pMergeResultWindow->getNumberOfUnsolvedConflicts() == 0)
        {
            bSuccess = m_pMergeResultWindow->saveDocument(m_pMergeResultWindowTitle->getFileName(), m_pMergeResultWindowTitle->getEncoding(), m_pMergeResultWindowTitle->getLineEndStyle());
        }
        if(bSuccess)
        {
            QMetaObject::invokeMethod(qApp, &QApplication::quit, Qt::QueuedConnection);
        }
    }
    return bSuccess;
}

// Restore and show mainWindow.
void KDiff3App::showMainWindow()
{
    if(!m_pKDiff3Shell->isVisible() && !restoreWindow(KSharedConfig::openConfig()))
    {
        /*
            Set default state/geometry from config file.
            This will no longer be updated but serves as a fallback.
            Qt's restoreState/saveState can handle multiple screens this won't.
        */
        if(m_pOptions->isFullScreen())
            m_pKDiff3Shell->showFullScreen();
        else if(m_pOptions->isMaximised())
            m_pKDiff3Shell->showMaximized();

        QSize size = m_pOptions->getGeometry();
        QPoint pos = m_pOptions->getPosition();

        if(!size.isEmpty())
        {
            m_pKDiff3Shell->resize(size);

            QRect visibleRect = QRect(pos, size) & QApplication::desktop()->rect();
            if(visibleRect.width() > 100 && visibleRect.height() > 100)
                m_pKDiff3Shell->move(pos);
        }
    }

    m_pKDiff3Shell->show();
}

void KDiff3App::completeInit(const QString& fn1, const QString& fn2, const QString& fn3)
{
    if(!fn1.isEmpty()) {
        m_sd1->setFilename(fn1);
        m_bDirCompare = m_sd1->isDir();
    }
    if(!fn2.isEmpty()) {
        m_sd2->setFilename(fn2);
    }
    if(!fn3.isEmpty()) {
        m_sd3->setFilename(fn3);
    }

    //Should not fail ever.
    assert(m_bDirCompare == m_sd1->isDir());

    if(m_bAutoFlag && m_bAutoMode && m_bDirCompare)
    {
        QTextStream(stderr) << i18n("Option --auto ignored for folder comparison.") << "\n";
        m_bAutoMode = false;
    }

    if(!m_bAutoMode && !isPart())
        showMainWindow();

    g_pProgressDialog->setStayHidden(false);

    bool bSuccess = true;
    if(m_bDirCompare)
        bSuccess = doDirectoryCompare(false);
    else{
        bSuccess = doFileCompare();
    }

    if(bSuccess && m_bAutoMode) return;
    if(m_bAutoMode && !isPart())
        showMainWindow();

    m_bAutoMode = false;

    if(statusBar() != nullptr)
        statusBar()->setSizeGripEnabled(true);

    slotClipboardChanged(); // For initialisation.

    Q_EMIT updateAvailabilities();

    if(!m_bDirCompare && m_pKDiff3Shell != nullptr)
    {
        bool bFileOpenError = false;
        if((!m_sd1->getErrors().isEmpty()) ||
            (!m_sd2->getErrors().isEmpty()) ||
            (!m_sd3->getErrors().isEmpty()))
        {
            QString text(i18n("Opening of these files failed:"));
            text += "\n\n";
            if(!m_sd1->getErrors().isEmpty())
                text += " - " + m_sd1->getAliasName() + '\n' + m_sd1->getErrors().join('\n') + '\n';
            if(!m_sd2->getErrors().isEmpty())
                text += " - " + m_sd2->getAliasName() + '\n' + m_sd2->getErrors().join('\n') + '\n';
            if(!m_sd3->getErrors().isEmpty())
                text += " - " + m_sd3->getAliasName() + '\n' + m_sd3->getErrors().join('\n') + '\n';

            KMessageBox::error(this, text, i18n("File open error"));

            bFileOpenError = true;
        }

        if(m_sd1->isEmpty() || m_sd2->isEmpty() || bFileOpenError)
            slotFileOpen();
    }
    else if(!bSuccess) // Directory open failed
    {
        slotFileOpen();
    }
}

KDiff3App::~KDiff3App()
{
    delete m_totalDiffStatus;

    g_pProgressDialog->cancel(ProgressDialog::eExit);

    // Prevent spurious focus change signals from Qt from being picked up by KDiff3App during destruction.
    QObject::disconnect(qApp, &QApplication::focusChanged, this, &KDiff3App::slotFocusChanged);
};

/**
 * Helper function used to create actions into the ac collection
 */

void KDiff3App::initActions(KActionCollection* ac)
{
    if(ac == nullptr){
        KMessageBox::error(nullptr, "actionCollection==0");
        exit(-1);//we cannot recover from this.
    }
    fileOpen = KStandardAction::open(this, &KDiff3App::slotFileOpen, ac);
    fileOpen->setStatusTip(i18n("Opens documents for comparison..."));

    fileReload = GuiUtils::createAction<QAction>(i18n("Reload"), QKeySequence(QKeySequence::Refresh), this, &KDiff3App::slotReload, ac, u8"file_reload");

    fileSave = KStandardAction::save(this, &KDiff3App::slotFileSave, ac);
    fileSave->setStatusTip(i18n("Saves the merge result. All conflicts must be solved!"));
    fileSaveAs = KStandardAction::saveAs(this, &KDiff3App::slotFileSaveAs, ac);
    fileSaveAs->setStatusTip(i18n("Saves the current document as..."));
#ifndef QT_NO_PRINTER
    filePrint = KStandardAction::print(this, &KDiff3App::slotFilePrint, ac);
    filePrint->setStatusTip(i18n("Print the differences"));
#endif
    fileQuit = KStandardAction::quit(this, &KDiff3App::slotFileQuit, ac);
    fileQuit->setStatusTip(i18n("Quits the application"));

    editUndo = KStandardAction::undo(this, &KDiff3App::slotEditUndo, ac);
    editUndo->setShortcuts(QKeySequence::Undo);
    editUndo->setStatusTip(i18n("Undo last action."));

    editCut = KStandardAction::cut(this, &KDiff3App::slotEditCut, ac);
    editCut->setShortcuts(QKeySequence::Cut);
    editCut->setStatusTip(i18n("Cuts the selected section and puts it to the clipboard"));
    editCopy = KStandardAction::copy(this, &KDiff3App::slotEditCopy, ac);
    editCopy->setShortcut(QKeySequence::Copy);
    editCopy->setStatusTip(i18n("Copies the selected section to the clipboard"));
    editPaste = KStandardAction::paste(this, &KDiff3App::slotEditPaste, ac);
    editPaste->setStatusTip(i18n("Pastes the clipboard contents to current position"));
    editPaste->setShortcut(QKeySequence::Paste);
    editSelectAll = KStandardAction::selectAll(this, &KDiff3App::slotEditSelectAll, ac);
    editSelectAll->setStatusTip(i18n("Select everything in current window"));
    editFind = KStandardAction::find(this, &KDiff3App::slotEditFind, ac);
    editFind->setShortcut(QKeySequence::Find);
    editFind->setStatusTip(i18n("Search for a string"));
    editFindNext = KStandardAction::findNext(this, &KDiff3App::slotEditFindNext, ac);
    editFindNext->setStatusTip(i18n("Search again for the string"));

    viewStatusBar = KStandardAction::showStatusbar(this, &KDiff3App::slotViewStatusBar, ac);
    viewStatusBar->setStatusTip(i18n("Enables/disables the statusbar"));
    KStandardAction::keyBindings(this, &KDiff3App::slotConfigureKeys, ac);
    QAction* pAction = KStandardAction::preferences(this, &KDiff3App::slotConfigure, ac);
    if(isPart())
        pAction->setText(i18n("Configure KDiff3..."));

#include "xpm/autoadvance.xpm"
#include "xpm/currentpos.xpm"
#include "xpm/down1arrow.xpm"
#include "xpm/down2arrow.xpm"
#include "xpm/downend.xpm"
#include "xpm/gotoline.xpm"
#include "xpm/iconA.xpm"
#include "xpm/iconB.xpm"
#include "xpm/iconC.xpm"
#include "xpm/nextunsolved.xpm"
#include "xpm/prevunsolved.xpm"
#include "xpm/showlinenumbers.xpm"
#include "xpm/showwhitespace.xpm"
#include "xpm/showwhitespacechars.xpm"
#include "xpm/up1arrow.xpm"
#include "xpm/up2arrow.xpm"
#include "xpm/upend.xpm"

    mGoCurrent = GuiUtils::createAction<QAction>(i18n("Go to Current Delta"), QIcon(QPixmap(currentpos)), i18n("Current\nDelta"), QKeySequence(Qt::CTRL + Qt::Key_Space), this, &KDiff3App::slotGoCurrent, ac, "go_current");

    mGoTop = GuiUtils::createAction<QAction>(i18n("Go to First Delta"), QIcon(QPixmap(upend)), i18n("First\nDelta"), this, &KDiff3App::slotGoTop, ac, "go_top");

    mGoBottom = GuiUtils::createAction<QAction>(i18n("Go to Last Delta"), QIcon(QPixmap(downend)), i18n("Last\nDelta"), this, &KDiff3App::slotGoBottom, ac, "go_bottom");

    QString omitsWhitespace = ".\n" + i18n("(Skips white space differences when \"Show White Space\" is disabled.)");
    QString includeWhitespace = ".\n" + i18n("(Does not skip white space differences even when \"Show White Space\" is disabled.)");
    mGoPrevDelta = GuiUtils::createAction<QAction>(i18n("Go to Previous Delta"), QIcon(QPixmap(up1arrow)), i18n("Prev\nDelta"), QKeySequence(Qt::CTRL + Qt::Key_Up), this, &KDiff3App::slotGoPrevDelta, ac, "go_prev_delta");
    mGoPrevDelta->setToolTip(mGoPrevDelta->text() + omitsWhitespace);
    mGoNextDelta = GuiUtils::createAction<QAction>(i18n("Go to Next Delta"), QIcon(QPixmap(down1arrow)), i18n("Next\nDelta"), QKeySequence(Qt::CTRL + Qt::Key_Down), this, &KDiff3App::slotGoNextDelta, ac, "go_next_delta");
    mGoNextDelta->setToolTip(mGoNextDelta->text() + omitsWhitespace);
    mGoPrevConflict = GuiUtils::createAction<QAction>(i18n("Go to Previous Conflict"), QIcon(QPixmap(up2arrow)), i18n("Prev\nConflict"), QKeySequence(Qt::CTRL + Qt::Key_PageUp), this, &KDiff3App::slotGoPrevConflict, ac, "go_prev_conflict");
    mGoPrevConflict->setToolTip(mGoPrevConflict->text() + omitsWhitespace);
    mGoNextConflict = GuiUtils::createAction<QAction>(i18n("Go to Next Conflict"), QIcon(QPixmap(down2arrow)), i18n("Next\nConflict"), QKeySequence(Qt::CTRL + Qt::Key_PageDown), this, &KDiff3App::slotGoNextConflict, ac, "go_next_conflict");
    mGoNextConflict->setToolTip(mGoNextConflict->text() + omitsWhitespace);
    mGoPrevUnsolvedConflict = GuiUtils::createAction<QAction>(i18n("Go to Previous Unsolved Conflict"), QIcon(QPixmap(prevunsolved)), i18n("Prev\nUnsolved"), QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_PageUp), this, &KDiff3App::slotGoPrevUnsolvedConflict, ac, "go_prev_unsolved_conflict");
    mGoPrevUnsolvedConflict->setToolTip(mGoPrevUnsolvedConflict->text() + includeWhitespace);
    mGoNextUnsolvedConflict = GuiUtils::createAction<QAction>(i18n("Go to Next Unsolved Conflict"), QIcon(QPixmap(nextunsolved)), i18n("Next\nUnsolved"), QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_PageDown), this, &KDiff3App::slotGoNextUnsolvedConflict, ac, "go_next_unsolved_conflict");
    mGoNextUnsolvedConflict->setToolTip(mGoNextUnsolvedConflict->text() + includeWhitespace);
    mGotoLine = GuiUtils::createAction<QAction>(i18nc("Title for menu item", "Go to Line"), QIcon(QPixmap(gotoline)), i18nc("Text used for toolbar button.", "Go\nLine"), QKeySequence(Qt::CTRL + Qt::Key_G), this, &KDiff3App::slotGoToLine, ac, "go_to_line");
    mGotoLine->setToolTip(mGotoLine->text() + i18nc("Additional text intooltip", ".\n Goto specified line."));
    chooseA = GuiUtils::createAction<KToggleAction>(i18nc("Title for menu item", "Select Line(s) From A"), QIcon(QPixmap(iconA)), i18nc("Text used for select A toolbar button.", "Choose\nA"), QKeySequence(Qt::CTRL + Qt::Key_1), this, &KDiff3App::slotChooseA, ac, "merge_choose_a");
    chooseB = GuiUtils::createAction<KToggleAction>(i18nc("Title for menu item", "Select Line(s) From B"), QIcon(QPixmap(iconB)), i18nc("Text used for select B when toolbar button.", "Choose\nB"), QKeySequence(Qt::CTRL + Qt::Key_2), this, &KDiff3App::slotChooseB, ac, "merge_choose_b");
    chooseC = GuiUtils::createAction<KToggleAction>(i18nc("Title for menu item", "Select Line(s) From C"), QIcon(QPixmap(iconC)), i18nc("Text used for select C toolbar button.", "Choose\nC"), QKeySequence(Qt::CTRL + Qt::Key_3), this, &KDiff3App::slotChooseC, ac, "merge_choose_c");
    autoAdvance = GuiUtils::createAction<KToggleAction>(i18nc("Title for menu item", "Automatically Go to Next Unsolved Conflict After Source Selection"), QIcon(QPixmap(autoadvance)), i18nc("Auto goto next unsolced toolbar text.", "Auto\nNext"), this, &KDiff3App::slotAutoAdvanceToggled, ac, "merge_autoadvance");

    showWhiteSpaceCharacters = GuiUtils::createAction<KToggleAction>(i18n("Show Space && Tabulator Characters"), QIcon(QPixmap(showwhitespacechars)), i18nc("Show whitespace toolbar text.", "White\nCharacters"), this, &KDiff3App::slotShowWhiteSpaceToggled, ac, "diff_show_whitespace_characters");
    showWhiteSpace = GuiUtils::createAction<KToggleAction>(i18n("Show White Space"), QIcon(QPixmap(showwhitespace)), i18nc("Show whitespace changes toolbar text.", "White\nDeltas"), this, &KDiff3App::slotShowWhiteSpaceToggled, ac, "diff_show_whitespace");

    showLineNumbers = GuiUtils::createAction<KToggleAction>(i18n("Show Line Numbers"), QIcon(QPixmap(showlinenumbers)), i18nc("Show line numbers toolbar text", "Line\nNumbers"), this, &KDiff3App::slotShowLineNumbersToggled, ac, "diff_showlinenumbers");

    mAutoSolve = GuiUtils::createAction<QAction>(i18n("Automatically Solve Simple Conflicts"), this, &KDiff3App::slotAutoSolve, ac, "merge_autosolve");
    mUnsolve = GuiUtils::createAction<QAction>(i18n("Set Deltas to Conflicts"), this, &KDiff3App::slotUnsolve, ac, "merge_autounsolve");
    mergeRegExp = GuiUtils::createAction<QAction>(i18n("Run Regular Expression Auto Merge"), this, &KDiff3App::slotRegExpAutoMerge, ac, "merge_regexp_automerge");
    mMergeHistory = GuiUtils::createAction<QAction>(i18n("Automatically Solve History Conflicts"), this, &KDiff3App::slotMergeHistory, ac, "merge_versioncontrol_history");
    splitDiff = GuiUtils::createAction<QAction>(i18n("Split Diff At Selection"), this, &KDiff3App::slotSplitDiff, ac, "merge_splitdiff");
    joinDiffs = GuiUtils::createAction<QAction>(i18n("Join Selected Diffs"), this, &KDiff3App::slotJoinDiffs, ac, "merge_joindiffs");

    showWindowA = GuiUtils::createAction<KToggleAction>(i18n("Show Window A"), this, &KDiff3App::slotShowWindowAToggled, ac, "win_show_a");
    showWindowB = GuiUtils::createAction<KToggleAction>(i18n("Show Window B"), this, &KDiff3App::slotShowWindowBToggled, ac, "win_show_b");
    showWindowC = GuiUtils::createAction<KToggleAction>(i18n("Show Window C"), this, &KDiff3App::slotShowWindowCToggled, ac, "win_show_c");

    overviewModeNormal = GuiUtils::createAction<KToggleAction>(i18n("Normal Overview"), this, &KDiff3App::slotOverviewNormal, ac, "diff_overview_normal");
    overviewModeAB = GuiUtils::createAction<KToggleAction>(i18n("A vs. B Overview"), this, &KDiff3App::slotOverviewAB, ac, "diff_overview_ab");
    overviewModeAC = GuiUtils::createAction<KToggleAction>(i18n("A vs. C Overview"), this, &KDiff3App::slotOverviewAC, ac, "diff_overview_ac");
    overviewModeBC = GuiUtils::createAction<KToggleAction>(i18n("B vs. C Overview"), this, &KDiff3App::slotOverviewBC, ac, "diff_overview_bc");
    wordWrap = GuiUtils::createAction<KToggleAction>(i18n("Word Wrap Diff Windows"), this, &KDiff3App::slotWordWrapToggled, ac, "diff_wordwrap");
    addManualDiffHelp = GuiUtils::createAction<QAction>(i18n("Add Manual Diff Alignment"), QKeySequence(Qt::CTRL + Qt::Key_Y), this, &KDiff3App::slotAddManualDiffHelp, ac, "diff_add_manual_diff_help");
    clearManualDiffHelpList = GuiUtils::createAction<QAction>(i18n("Clear All Manual Diff Alignments"), QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_Y), this, &KDiff3App::slotClearManualDiffHelpList, ac, "diff_clear_manual_diff_help_list");

    winFocusNext = GuiUtils::createAction<QAction>(i18n("Focus Next Window"), QKeySequence(Qt::ALT + Qt::Key_Right), this, &KDiff3App::slotWinFocusNext, ac, "win_focus_next");
    winFocusPrev = GuiUtils::createAction<QAction>(i18n("Focus Prev Window"), QKeySequence(Qt::ALT + Qt::Key_Left), this, &KDiff3App::slotWinFocusPrev, ac, "win_focus_prev");
    winToggleSplitOrientation = GuiUtils::createAction<QAction>(i18n("Toggle Split Orientation"), this, &KDiff3App::slotWinToggleSplitterOrientation, ac, "win_toggle_split_orientation");

    dirShowBoth = GuiUtils::createAction<KToggleAction>(i18n("Folder && Text Split Screen View"), this, &KDiff3App::slotDirShowBoth, ac, "win_dir_show_both");
    dirShowBoth->setChecked(true);
    dirViewToggle = GuiUtils::createAction<QAction>(i18n("Toggle Between Folder && Text View"), this, &KDiff3App::slotDirViewToggle, ac, "win_dir_view_toggle");

    m_pMergeEditorPopupMenu = new QMenu(this);
    m_pMergeEditorPopupMenu->addAction(chooseA);
    m_pMergeEditorPopupMenu->addAction(chooseB);
    m_pMergeEditorPopupMenu->addAction(chooseC);

    mEscapeAction = new QShortcut(Qt::Key_Escape, this, this, &KDiff3App::slotFileQuit);
}

void KDiff3App::showPopupMenu(const QPoint& point)
{
    m_pMergeEditorPopupMenu->popup(point);
}

void KDiff3App::initStatusBar()
{
    ///////////////////////////////////////////////////////////////////
    // STATUSBAR
    if(statusBar() != nullptr)
        statusBar()->showMessage(i18n("Ready."));
}

void KDiff3App::saveWindow(const KSharedConfigPtr config)
{
    KConfigGroup group = config->group(KDIFF3_CONFIG_GROUP);
    group.writeEntry("mainWindow-geometry", saveGeometry());
    group.writeEntry("mainWindow-state", saveState(1));
    group.writeEntry("shell-geometry", m_pKDiff3Shell->saveGeometry());
    group.writeEntry("shell-state", m_pKDiff3Shell->saveState());
}

bool KDiff3App::restoreWindow(const KSharedConfigPtr config)
{
    KConfigGroup group = config->group(KDIFF3_CONFIG_GROUP);
    if(m_pKDiff3Shell->restoreState(group.readEntry("mainWindow-state", QVariant(QByteArray())).toByteArray()))
    {
        bool r = m_pKDiff3Shell->restoreGeometry(group.readEntry("mainWindow-geometry", QVariant(QByteArray())).toByteArray());
        group.deleteEntry("mainWindow-state");
        group.deleteEntry("mainWindow-geometry");
        saveWindow(config);
        return r;
    }

    return (restoreGeometry(group.readEntry("mainWindow-geometry", QVariant(QByteArray())).toByteArray()) &&
            restoreState(group.readEntry("mainWindow-state", QVariant(QByteArray())).toByteArray(), 1)) &&
           (m_pKDiff3Shell->restoreGeometry(group.readEntry("shell-geometry", QVariant(QByteArray())).toByteArray()) &&
            m_pKDiff3Shell->restoreState(group.readEntry("shell-state", QVariant(QByteArray())).toByteArray()));
}

void KDiff3App::saveOptions(KSharedConfigPtr config)
{
    if(!m_bAutoMode)
    {
        if(!isPart())
        {
            saveWindow(config);
        }

        m_pOptionDialog->saveOptions(std::move(config));
    }
}

bool KDiff3App::queryClose()
{
    saveOptions(KSharedConfig::openConfig());

    if(m_bOutputModified)
    {
        qint32 result = KMessageBox::warningYesNoCancel(this,
                                                     i18n("The merge result has not been saved."),
                                                     i18nc("Error dialog caption", "Warning"),
                                                     KGuiItem(i18n("Save && Quit")),
                                                     KGuiItem(i18n("Quit Without Saving")));
        if(result == KMessageBox::Cancel)
            return false;
        else if(result == KMessageBox::Yes)
        {
            slotFileSave();
            if(m_bOutputModified)
            {
                KMessageBox::error(this, i18n("Saving the merge result failed."), i18nc("Error dialog caption", "Warning"));
                return false;
            }
        }
    }

    m_bOutputModified = false;

    if(m_pDirectoryMergeWindow->isDirectoryMergeInProgress())
    {
        qint32 result = KMessageBox::warningYesNo(this,
                                               i18n("You are currently doing a folder merge. Are you sure, you want to abort?"),
                                               i18nc("Error dialog caption", "Warning"),
                                               KStandardGuiItem::quit(),
                                               KStandardGuiItem::cont() /* i18n("Continue Merging") */);
        if(result != KMessageBox::Yes)
            return false;
    }

    return true;
}

/////////////////////////////////////////////////////////////////////
// SLOT IMPLEMENTATION
/////////////////////////////////////////////////////////////////////

void KDiff3App::slotFileSave()
{
    if(m_bDefaultFilename)
    {
        slotFileSaveAs();
    }
    else
    {
        slotStatusMsg(i18n("Saving file..."));

        bool bSuccess = m_pMergeResultWindow->saveDocument(m_outputFilename, m_pMergeResultWindowTitle->getEncoding(), m_pMergeResultWindowTitle->getLineEndStyle());
        if(bSuccess)
        {
            m_bFileSaved = true;
            m_bOutputModified = false;
            if(m_bDirCompare)
                m_pDirectoryMergeWindow->mergeResultSaved(m_outputFilename);
        }

        slotStatusMsg(i18n("Ready."));
    }
}

void KDiff3App::slotFileSaveAs()
{
    slotStatusMsg(i18n("Saving file with a new filename..."));

    QString s = QFileDialog::getSaveFileUrl(this, i18n("Save As..."), QUrl::fromLocalFile(QDir::currentPath())).url(QUrl::PreferLocalFile);
    if(!s.isEmpty()) {
        m_outputFilename = s;
        m_pMergeResultWindowTitle->setFileName(m_outputFilename);
        bool bSuccess = m_pMergeResultWindow->saveDocument(m_outputFilename, m_pMergeResultWindowTitle->getEncoding(), m_pMergeResultWindowTitle->getLineEndStyle());
        if(bSuccess)
        {
            m_bOutputModified = false;
            if(m_bDirCompare)
                m_pDirectoryMergeWindow->mergeResultSaved(m_outputFilename);
        }
        //setWindowTitle(url.fileName(),doc->isModified());

        m_bDefaultFilename = false;
    }

    slotStatusMsg(i18n("Ready."));
}

void KDiff3App::slotFilePrint()
{
    if(m_pDiffTextWindow1 == nullptr || m_pDiffTextWindow2 == nullptr)
        return;
#ifdef QT_NO_PRINTER
    slotStatusMsg(i18n("Printing not implemented."));
#else
    QPrinter printer;
    QPointer<QPrintDialog> printDialog=QPointer<QPrintDialog>(new QPrintDialog(&printer, this));

    LineRef firstSelectionD3LIdx;
    LineRef lastSelectionD3LIdx;

    m_pDiffTextWindow1->getSelectionRange(&firstSelectionD3LIdx, &lastSelectionD3LIdx, eD3LLineCoords);

    if(!firstSelectionD3LIdx.isValid()) {
        m_pDiffTextWindow2->getSelectionRange(&firstSelectionD3LIdx, &lastSelectionD3LIdx, eD3LLineCoords);
    }
    if(!firstSelectionD3LIdx.isValid() && m_pDiffTextWindow3 != nullptr) {
        m_pDiffTextWindow3->getSelectionRange(&firstSelectionD3LIdx, &lastSelectionD3LIdx, eD3LLineCoords);
    }

    printDialog->setOption(QPrintDialog::PrintCurrentPage);

    if(firstSelectionD3LIdx.isValid()) {
        printDialog->setOption(QPrintDialog::PrintSelection);
        printDialog->setPrintRange(QAbstractPrintDialog::Selection);
    }

    if(!firstSelectionD3LIdx.isValid())
        printDialog->setPrintRange(QAbstractPrintDialog::AllPages);
    //printDialog.setMinMax(0,0);
    printDialog->setFromTo(0, 0);

    qint32 currentFirstLine = m_pDiffTextWindow1->getFirstLine();
    qint32 currentFirstD3LIdx = m_pDiffTextWindow1->convertLineToDiff3LineIdx(currentFirstLine);

    // do some printer initialization
    printer.setFullPage(false);

    // initialize the printer using the print dialog
    if(printDialog->exec() == QDialog::Accepted)
    {
        slotStatusMsg(i18n("Printing..."));
        // create a painter to paint on the printer object
        RLPainter painter(&printer, m_pOptions->m_bRightToLeftLanguage, width(), Utils::getHorizontalAdvance(fontMetrics(),'W'));

        QPaintDevice* pPaintDevice = painter.device();
        qint32 dpiy = pPaintDevice->logicalDpiY();
        qint32 columnDistance = qRound((0.5 / 2.54) * dpiy); // 0.5 cm between the columns

        qint32 columns = m_bTripleDiff ? 3 : 2;
        qint32 columnWidth = (pPaintDevice->width() - (columns - 1) * columnDistance) / columns;

        QFont f = m_pOptions->defaultFont();
        f.setPointSizeF(f.pointSizeF() - 1); // Print with slightly smaller font.
        painter.setFont(f);
        QFontMetrics fm = painter.fontMetrics();

        QString topLineText = i18n("Top line");

        //qint32 headerWidth = fm.width( m_sd1->getAliasName() + ", "+topLineText+": 01234567" );
        qint32 headerLines = Utils::getHorizontalAdvance(fm, m_sd1->getAliasName() + ", " + topLineText + ": 01234567") / columnWidth + 1;

        qint32 headerMargin = headerLines * fm.height() + 3; // Text + one horizontal line
        qint32 footerMargin = fm.height() + 3;

        QRect view(0, headerMargin, pPaintDevice->width(), pPaintDevice->height() - (headerMargin + footerMargin));
        QRect view1(0 * (columnWidth + columnDistance), view.top(), columnWidth, view.height());
        QRect view2(1 * (columnWidth + columnDistance), view.top(), columnWidth, view.height());
        QRect view3(2 * (columnWidth + columnDistance), view.top(), columnWidth, view.height());

        LineCount linesPerPage = view.height() / fm.lineSpacing();

        m_pEventLoopForPrinting = QPointer<QEventLoop>(new QEventLoop());
        if(m_pOptions->wordWrapOn())
        {
            // For printing the lines are wrapped differently (this invalidates the first line)
            recalcWordWrap(columnWidth);
            m_pEventLoopForPrinting->exec();
        }

        LineCount totalNofLines = std::max(m_pDiffTextWindow1->getNofLines(), m_pDiffTextWindow2->getNofLines());

        if(m_bTripleDiff && m_pDiffTextWindow3 != nullptr)
            totalNofLines = std::max(totalNofLines, m_pDiffTextWindow3->getNofLines());

        QList<qint32> pageList; // = printer.pageList();

        bool bPrintCurrentPage = false;
        bool bFirstPrintedPage = false;

        bool bPrintSelection = false;
        LineCount totalNofPages = (totalNofLines + linesPerPage - 1) / linesPerPage;
        LineRef line;
        LineRef selectionEndLine;

        if(printer.printRange() == QPrinter::AllPages) {
            pageList.clear();
            for(qint32 i = 0; i < totalNofPages; ++i)
            {
                pageList.push_back(i + 1);
            }
        }
        else if(printer.printRange() == QPrinter::PageRange)
        {
            pageList.clear();

            qint32 from = printer.fromPage(), to = printer.toPage();
            /*
                Per Qt docs QPrinter::fromPage and QPrinter::toPage return 0 to indicate they are not set.
                Account for this and other invalid settings the user may try.
            */
            if(from == 0) from = 1;
            if(from > totalNofPages) from = totalNofPages;
            if(to == 0 || to > totalNofPages) to = totalNofPages;
            if(from > to) to = from;

            for(qint32 i = from; i <= to; ++i)
            {
                pageList.push_back(i);
            }
        }
        else if(printer.printRange() == QPrinter::CurrentPage)
        {
            bPrintCurrentPage = true;
            totalNofPages = 1;
            // Detect the first visible line in the window.
            line = m_pDiffTextWindow1->convertDiff3LineIdxToLine(currentFirstD3LIdx);
        }
        else if(printer.printRange() == QPrinter::Selection)
        {
            bPrintSelection = true;
            if(firstSelectionD3LIdx.isValid())
            {
                line = m_pDiffTextWindow1->convertDiff3LineIdxToLine(firstSelectionD3LIdx);
                selectionEndLine = m_pDiffTextWindow1->convertDiff3LineIdxToLine(lastSelectionD3LIdx + 1);
                totalNofPages = (selectionEndLine - line + linesPerPage - 1) / linesPerPage;
            }
        }

        qint32 page = 1;

        ProgressScope pp;
        ProgressProxy::setMaxNofSteps(totalNofPages);
        QList<qint32>::iterator pageListIt = pageList.begin();
        for(;;) {
            ProgressProxy::setInformation(i18n("Printing page %1 of %2", page, totalNofPages), false);
            ProgressProxy::setCurrent(page - 1);
            if(ProgressProxy::wasCancelled())
            {
                printer.abort();
                break;
            }
            if(!bPrintSelection && !bPrintCurrentPage)
            {
                if(pageListIt == pageList.end())
                    break;
                page = *pageListIt;
                line = (page - 1) * linesPerPage;
            }
            else if(bPrintSelection)
            {
                if(line >= selectionEndLine) {
                    break;
                }
                else
                {
                    if(selectionEndLine - line < linesPerPage)
                        linesPerPage = selectionEndLine - line;
                }
            }
            if(line.isValid() && line < totalNofLines)
            {
                if(bFirstPrintedPage)
                    printer.newPage();

                painter.setClipping(true);

                painter.setPen(m_pOptions->aColor());
                QString headerText1 = m_sd1->getAliasName() + ", " + topLineText + ": " + QString::number(m_pDiffTextWindow1->calcTopLineInFile(line) + 1);
                m_pDiffTextWindow1->printWindow(painter, view1, headerText1, line, linesPerPage, m_pOptions->forgroundColor());

                painter.setPen(m_pOptions->bColor());
                QString headerText2 = m_sd2->getAliasName() + ", " + topLineText + ": " + QString::number(m_pDiffTextWindow2->calcTopLineInFile(line) + 1);
                m_pDiffTextWindow2->printWindow(painter, view2, headerText2, line, linesPerPage, m_pOptions->forgroundColor());

                if(m_bTripleDiff && m_pDiffTextWindow3 != nullptr)
                {
                    painter.setPen(m_pOptions->cColor());
                    QString headerText3 = m_sd3->getAliasName() + ", " + topLineText + ": " + QString::number(m_pDiffTextWindow3->calcTopLineInFile(line) + 1);
                    m_pDiffTextWindow3->printWindow(painter, view3, headerText3, line, linesPerPage, m_pOptions->forgroundColor());
                }
                painter.setClipping(false);

                painter.setPen(m_pOptions->forgroundColor());
                painter.drawLine(0, view.bottom() + 3, view.width(), view.bottom() + 3);
                QString s = bPrintCurrentPage ? QString("")
                                              : QString::number(page) + '/' + QString::number(totalNofPages);
                if(bPrintSelection) s += i18n(" (Selection)");
                painter.drawText((view.right() - Utils::getHorizontalAdvance(painter.fontMetrics(), s)) / 2,
                                 view.bottom() + painter.fontMetrics().ascent() + 5, s);

                bFirstPrintedPage = true;
                if(bPrintCurrentPage) break;
            }

            if(bPrintSelection)
            {
                line += linesPerPage;
                ++page;
            }
            else
            {
                ++pageListIt;
            }
        }

        painter.end();

        if(m_pOptions->wordWrapOn())
        {
            recalcWordWrap();
            m_pEventLoopForPrinting->exec();
            DiffTextWindow::mVScrollBar->setValue(m_pDiffTextWindow1->convertDiff3LineIdxToLine(currentFirstD3LIdx));
        }
        m_pEventLoopForPrinting.clear();

        slotStatusMsg(i18n("Printing completed."));
    }
    else
    {
        slotStatusMsg(i18n("Printing aborted."));
    }
#endif
}

void KDiff3App::slotFileQuit()
{
    slotStatusMsg(i18n("Exiting..."));

    if(!queryClose())
        return; // Don't quit

    QApplication::exit(isFileSaved() || isDirComparison() ? 0 : 1);
}

void KDiff3App::slotViewStatusBar()
{
    slotStatusMsg(i18n("Toggle the statusbar..."));
    m_pOptions->setStatusBarState(viewStatusBar->isChecked());
    ///////////////////////////////////////////////////////////////////
    //turn Statusbar on or off
    if(statusBar() != nullptr)
    {
        if(!viewStatusBar->isChecked())
        {
            statusBar()->hide();
        }
        else
        {
            statusBar()->show();
        }
    }

    slotStatusMsg(i18n("Ready."));
}

void KDiff3App::slotStatusMsg(const QString& text)
{
    ///////////////////////////////////////////////////////////////////
    // change status message permanently
    if(statusBar() != nullptr)
    {
        statusBar()->clearMessage();
        statusBar()->showMessage(text);
    }
}

//#include "kdiff3.moc"
