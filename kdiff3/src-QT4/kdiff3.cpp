/***************************************************************************
                          kdiff3.cpp  -  description
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

#include "difftextwindow.h"
#include "mergeresultwindow.h"

#include <cassert>

// include files for QT
#include <QDir>
#include <QPrinter>
#include <QPainter>
#include <QSplitter>
#include <QLineEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QMenu>
#include <QLabel>
#include <QTextEdit>
#include <QLayout>
#include <QPaintDevice>
#include <QStatusBar>
#include <QDesktopWidget>
#include <QPrinter>
#include <QPrintDialog>

// include files for KDE
#include <kiconloader.h>
#include <kmessagebox.h>
#include <kfiledialog.h>
#include <kmenubar.h>
#include <kstatusbar.h>
#include <klocale.h>
#include <kconfig.h>
#include <kstandardaction.h>
#include <kcmdlineargs.h>
//#include <kkeydialog.h>
#include <kglobal.h>
#include <ktoggleaction.h>
#include <ktoolbar.h>
#include <kiconloader.h>
#include <kactioncollection.h>

// application specific includes
#include "kdiff3.h"
#include "optiondialog.h"
#include "fileaccess.h"
#include "progress.h"
#include "kdiff3_part.h"
#include "directorymergewindow.h"
#include "smalldialogs.h"
#include "guiutils.h" // namespace KDiff3

#define ID_STATUS_MSG 1
#define MAIN_TOOLBAR_NAME "mainToolBar"

KActionCollection* KDiff3App::actionCollection()
{
   if ( m_pKDiff3Shell==0 )
      return m_pKDiff3Part->actionCollection();
   else
      return m_pKDiff3Shell->actionCollection();
}

KStatusBar* KDiff3App::statusBar()
{
   if ( m_pKDiff3Shell==0 )
      return 0;
   else
      return m_pKDiff3Shell->statusBar();
}

KToolBar* KDiff3App::toolBar(const char* toolBarId )
{
   if ( m_pKDiff3Shell==0 )
      return 0;
   else
      return m_pKDiff3Shell->toolBar( toolBarId );
//return m_pKDiff3Shell->toolBar();
}

bool KDiff3App::isPart()
{
   return m_pKDiff3Shell==0;
}

bool KDiff3App::isFileSaved()
{
   return m_bFileSaved;
}

bool KDiff3App::isDirComparison()
{
   return m_bDirCompare;
}

KDiff3App::KDiff3App(QWidget* pParent, const char* /*name*/, KDiff3Part* pKDiff3Part )
   : QSplitter(pParent)  //previously KMainWindow
{
   setObjectName( "KDiff3App" );
   m_pKDiff3Part = pKDiff3Part;
   m_pKDiff3Shell = qobject_cast<KParts::MainWindow*>(pParent);

   setWindowTitle( "KDiff3" );
   setOpaqueResize(false); // faster resizing
   setUpdatesEnabled(false);

   m_pMainSplitter = 0;
   m_pDirectoryMergeSplitter = 0;
   m_pDirectoryMergeWindow = 0;
   m_pCornerWidget = 0;
   m_pMainWidget = 0;
   m_pDiffTextWindow1 = 0;
   m_pDiffTextWindow2 = 0;
   m_pDiffTextWindow3 = 0;
   m_pDiffTextWindowFrame1 = 0;
   m_pDiffTextWindowFrame2 = 0;
   m_pDiffTextWindowFrame3 = 0;
   m_pDiffWindowSplitter = 0;
   m_pOverview        = 0;
   m_bTripleDiff = false;
   m_pMergeResultWindow = 0;
   m_pMergeWindowFrame = 0;
   m_bOutputModified = false;
   m_bFileSaved = false;
   m_bTimerBlock = false;
   m_pHScrollBar = 0;
   m_pDiffVScrollBar = 0;
   m_pMergeVScrollBar = 0;
   viewToolBar = 0;

   // Needed before any file operations via FileAccess happen.
   if (!g_pProgressDialog)
   {
      g_pProgressDialog = new ProgressDialog(0);
      g_pProgressDialog->setStayHidden( true );
   }

   // All default values must be set before calling readOptions().
   m_pOptionDialog = new OptionDialog( m_pKDiff3Shell!=0, this );
   connect( m_pOptionDialog, SIGNAL(applyDone()), this, SLOT(slotRefresh()) );

   // This is just a convenience variable to make code that accesses options more readable
   m_pOptions = &m_pOptionDialog->m_options;

   m_pOptionDialog->readOptions( isPart() ? m_pKDiff3Part->componentData().config() : KGlobal::config() );

   // Option handling: Only when pParent==0 (no parent)
   KCmdLineArgs *args = isPart() ? 0 : KCmdLineArgs::parsedArgs();

   if (args)
   {
      QString s;
      QString title;
      if ( args->isSet("confighelp") )
      {
         s = m_pOptionDialog->calcOptionHelp();
         title = i18n("Current Configuration:");
      }
      else
      {
         s = m_pOptionDialog->parseOptions( args->getOptionList("cs") );
         title = i18n("Config Option Error:");
      }
      if (!s.isEmpty())
      {
#if defined(_WIN32) || defined(Q_OS_OS2) 
         // A windows program has no console
         //KMessageBox::information(0, s,i18n("KDiff3-Usage"));
         QDialog* pDialog = new QDialog(this);
         pDialog->setAttribute( Qt::WA_DeleteOnClose );
         pDialog->setModal( true );
         pDialog->setWindowTitle(title);
         QVBoxLayout* pVBoxLayout = new QVBoxLayout( pDialog );
         QTextEdit* pTextEdit = new QTextEdit(pDialog);
         pTextEdit->setText(s);
         pTextEdit->setReadOnly(true);
         pTextEdit->setWordWrapMode(QTextOption::NoWrap);
         pVBoxLayout->addWidget(pTextEdit);
         pDialog->resize(600,400);
         pDialog->exec();
#else
         fprintf(stderr, "%s\n", title.toLatin1().constData());
         fprintf(stderr, "%s\n", s.toLatin1().constData());
#endif
         exit(1);
      }
   }

   m_sd1.setOptions(m_pOptions);
   m_sd2.setOptions(m_pOptions);
   m_sd3.setOptions(m_pOptions);

   if (args!=0)
   {
      m_outputFilename = args->getOption("output");
      if ( m_outputFilename.isEmpty() )
         m_outputFilename = args->getOption("out");
   }

   m_bAutoFlag = args!=0  && args->isSet("auto");
   m_bAutoMode = m_bAutoFlag || m_pOptions->m_bAutoSaveAndQuitOnMergeWithoutConflicts;
   if ( m_bAutoMode && m_outputFilename.isEmpty() )
   {
      if ( m_bAutoFlag )
      {
         //KMessageBox::information(this, i18n("Option --auto used, but no output file specified."));
         fprintf(stderr, "%s\n", (const char*)i18n("Option --auto used, but no output file specified.").toLatin1());
      }
      m_bAutoMode = false;
   }
   g_pProgressDialog->setStayHidden( m_bAutoMode );

   if ( m_outputFilename.isEmpty() && args!=0 && args->isSet("merge") )
   {
      m_outputFilename = "unnamed.txt";
      m_bDefaultFilename = true;
   }
   else
      m_bDefaultFilename = false;

   g_bAutoSolve = args!=0 && !args->isSet("qall");  // Note that this is effective only once.

   if ( args!=0 )
   {
      m_sd1.setFilename( args->getOption("base") );
      if ( m_sd1.isEmpty() )
      {
         if ( args->count() > 0 ) m_sd1.setFilename( args->url(0).url() ); // args->arg(0)
         if ( args->count() > 1 ) m_sd2.setFilename( args->url(1).url() );
         if ( args->count() > 2 ) m_sd3.setFilename( args->url(2).url() );
      }
      else
      {
         if ( args->count() > 0 ) m_sd2.setFilename( args->url(0).url() );
         if ( args->count() > 1 ) m_sd3.setFilename( args->url(1).url() );
      }


      QStringList aliasList = args->getOptionList("fname");
      QStringList::Iterator ali = aliasList.begin();

      QString an1 = args->getOption("L1");
      if ( !an1.isEmpty() )              { m_sd1.setAliasName(an1);         }
      else if ( ali != aliasList.end() ) { m_sd1.setAliasName(*ali); ++ali; }

      QString an2 = args->getOption("L2");
      if ( !an2.isEmpty() )              { m_sd2.setAliasName(an2);         }
      else if ( ali != aliasList.end() ) { m_sd2.setAliasName(*ali); ++ali; }

      QString an3 = args->getOption("L3");
      if ( !an3.isEmpty() )              { m_sd3.setAliasName(an3);         }
      else if ( ali != aliasList.end() ) { m_sd3.setAliasName(*ali); ++ali; }
   }

   ///////////////////////////////////////////////////////////////////
   // call inits to invoke all other construction parts
   initActions(actionCollection());
   initStatusBar();

   m_pFindDialog = new FindDialog( this );
   connect( m_pFindDialog, SIGNAL(findNext()), this, SLOT(slotEditFindNext()));

   autoAdvance->setChecked( m_pOptions->m_bAutoAdvance );
   showWhiteSpaceCharacters->setChecked( m_pOptions->m_bShowWhiteSpaceCharacters );
   showWhiteSpace->setChecked( m_pOptions->m_bShowWhiteSpace );
   showWhiteSpaceCharacters->setEnabled( m_pOptions->m_bShowWhiteSpace );
   showLineNumbers->setChecked( m_pOptions->m_bShowLineNumbers );
   wordWrap->setChecked( m_pOptions->m_bWordWrap );
   if ( ! isPart() )
   {
//  TODO 
//       viewToolBar->setChecked( m_pOptions->m_bShowToolBar );
//       slotViewToolBar();

      viewStatusBar->setChecked( m_pOptions->m_bShowStatusBar );
      slotViewStatusBar();
/*
   vrusu - commented out as new KToolBar does not hase setParPos
   TODO figure out how to restore bar position
      if( toolBar(MAIN_TOOLBAR_NAME)!=0 )
         toolBar(MAIN_TOOLBAR_NAME)->setBarPos( (KToolBar::BarPosition) m_pOptions->m_toolBarPos );*/
/*      QSize size = m_pOptions->m_geometry;
      QPoint pos = m_pOptions->m_position;
      if(!size.isEmpty())
      {
         m_pKDiff3Shell->resize( size );
         QRect visibleRect = QRect( pos, size ) & QApplication::desktop()->rect();
         if ( visibleRect.width()>100 && visibleRect.height()>100 )
            m_pKDiff3Shell->move( pos );
      }*/
   }
   slotRefresh();

   m_pMainSplitter = this; //new QSplitter(this);
   m_pMainSplitter->setOrientation( Qt::Vertical );
//   setCentralWidget( m_pMainSplitter );
   m_pDirectoryMergeSplitter = new QSplitter( m_pMainSplitter );
   m_pDirectoryMergeSplitter->setObjectName("DirectoryMergeSplitter");
   m_pMainSplitter->addWidget( m_pDirectoryMergeSplitter );
   m_pDirectoryMergeSplitter->setOrientation( Qt::Horizontal );
   m_pDirectoryMergeWindow = new DirectoryMergeWindow( m_pDirectoryMergeSplitter, m_pOptions,
      KIconLoader::global() );
   m_pDirectoryMergeSplitter->addWidget(m_pDirectoryMergeWindow);
   m_pDirectoryMergeInfo = new DirectoryMergeInfo( m_pDirectoryMergeSplitter );
   m_pDirectoryMergeWindow->setDirectoryMergeInfo( m_pDirectoryMergeInfo );
   m_pDirectoryMergeSplitter->addWidget(m_pDirectoryMergeInfo);

   connect( m_pDirectoryMergeWindow, SIGNAL(startDiffMerge(QString,QString,QString,QString,QString,QString,QString,TotalDiffStatus*)),
            this, SLOT( slotFileOpen2(QString,QString,QString,QString,QString,QString,QString,TotalDiffStatus*)));
   connect( m_pDirectoryMergeWindow->selectionModel(), SIGNAL(selectionChanged(const QItemSelection&,const QItemSelection&)), this, SLOT(slotUpdateAvailabilities()));
   connect( m_pDirectoryMergeWindow->selectionModel(), SIGNAL(currentChanged(const QModelIndex&,const QModelIndex&)), this, SLOT(slotUpdateAvailabilities()));
   connect( m_pDirectoryMergeWindow, SIGNAL(checkIfCanContinue(bool*)), this, SLOT(slotCheckIfCanContinue(bool*)));
   connect( m_pDirectoryMergeWindow, SIGNAL(updateAvailabilities()), this, SLOT(slotUpdateAvailabilities()));
   connect( m_pDirectoryMergeWindow, SIGNAL(statusBarMessage(const QString&)), this, SLOT(slotStatusMsg(const QString&)));

   m_pDirectoryMergeWindow->initDirectoryMergeActions( this, actionCollection() );

   if ( args!=0 )  args->clear(); // Free up some memory.

   if (m_pKDiff3Shell==0)
   {
      completeInit();
   }
}


void KDiff3App::completeInit( const QString& fn1, const QString& fn2, const QString& fn3 )
{
   if (m_pKDiff3Shell!=0)
   {
      QSize size=m_pOptions->m_geometry;
      QPoint pos=m_pOptions->m_position;
      if(!size.isEmpty())
      {
         m_pKDiff3Shell->resize( size );

         QRect visibleRect = QRect( pos, size ) & QApplication::desktop()->rect();
         if ( visibleRect.width()>100 && visibleRect.height()>100 )
            m_pKDiff3Shell->move( pos );
         if (!m_bAutoMode)
         {
            if ( m_pOptions->m_bMaximised )
               m_pKDiff3Shell->showMaximized();
            else
               m_pKDiff3Shell->show();
         }
      }
   }
   if ( ! fn1.isEmpty() )  { m_sd1.setFilename(fn1); }
   if ( ! fn2.isEmpty() )  { m_sd2.setFilename(fn2); }
   if ( ! fn3.isEmpty() )  { m_sd3.setFilename(fn3); }

   bool bSuccess = improveFilenames(false);

   if ( m_bAutoFlag && m_bAutoMode && m_bDirCompare )
   {
      fprintf(stderr, "%s\n", (const char*)i18n("Option --auto ignored for directory comparison.").toLatin1());
      m_bAutoMode = false;
   }
   if (!m_bDirCompare)
   {
      m_pDirectoryMergeSplitter->hide();

      init( m_bAutoMode );
      if ( m_bAutoMode )
      {
         SourceData* pSD=0;
         if ( m_sd3.isEmpty() )
         {
            if ( m_totalDiffStatus.bBinaryAEqB ){ pSD = &m_sd1; }
         }
         else
         {
            if      ( m_totalDiffStatus.bBinaryBEqC ){ pSD = &m_sd3; } // B==C (assume A is old)
            else if ( m_totalDiffStatus.bBinaryAEqB ){ pSD = &m_sd3; } // assuming C has changed
            else if ( m_totalDiffStatus.bBinaryAEqC ){ pSD = &m_sd2; } // assuming B has changed
         }

         if ( pSD!=0 )
         {
            // Save this file directly, not via the merge result window.
            bool bSuccess = false;
            FileAccess fa( m_outputFilename );
            if ( m_pOptions->m_bDmCreateBakFiles && fa.exists() )
            {
               QString newName = m_outputFilename + ".orig";
               if ( FileAccess::exists( newName ) ) FileAccess::removeFile( newName );
               if ( !FileAccess::exists( newName ) ) fa.rename( newName );
            }

            bSuccess = pSD->saveNormalDataAs( m_outputFilename );
            if ( bSuccess ) ::exit(0);
            else KMessageBox::error( this, i18n("Saving failed.") );
         }
         else if ( m_pMergeResultWindow->getNrOfUnsolvedConflicts() == 0 )
         {
            bool bSuccess = m_pMergeResultWindow->saveDocument( m_pMergeResultWindowTitle->getFileName(), m_pMergeResultWindowTitle->getEncoding(), m_pMergeResultWindowTitle->getLineEndStyle() );
            if ( bSuccess ) ::exit(0);
         }
      }
   }
   m_bAutoMode = false;

   if (m_pKDiff3Shell)
   {
      if ( m_pOptions->m_bMaximised )
         m_pKDiff3Shell->showMaximized();
      else
         m_pKDiff3Shell->show();
   }

   g_pProgressDialog->setStayHidden( false );

   if (statusBar() !=0 )
      statusBar()->setSizeGripEnabled(true);

   slotClipboardChanged(); // For initialisation.

   slotUpdateAvailabilities();

   if ( ! m_bDirCompare  &&  m_pKDiff3Shell!=0 )
   {
      bool bFileOpenError = false;
      if ( (! m_sd1.isEmpty() && !m_sd1.hasData())  ||
           (! m_sd2.isEmpty() && !m_sd2.hasData())  ||
           (! m_sd3.isEmpty() && !m_sd3.hasData()) )
      {
         QString text( i18n("Opening of these files failed:") );
         text += "\n\n";
         if ( ! m_sd1.isEmpty() && !m_sd1.hasData() )
            text += " - " + m_sd1.getAliasName() + "\n";
         if ( ! m_sd2.isEmpty() && !m_sd2.hasData() )
            text += " - " + m_sd2.getAliasName() + "\n";
         if ( ! m_sd3.isEmpty() && !m_sd3.hasData() )
            text += " - " + m_sd3.getAliasName() + "\n";

         KMessageBox::sorry( this, text, i18n("File Open Error") );
         bFileOpenError = true;
      }

      if ( m_sd1.isEmpty() || m_sd2.isEmpty() || bFileOpenError )
         slotFileOpen();
   }
   else if ( !bSuccess )  // Directory open failed
   {
      slotFileOpen();
   }
}

KDiff3App::~KDiff3App()
{

}

/**
 * Helper function used to create actions into the ac collection
 */

void KDiff3App::initActions( KActionCollection* ac )
{
   if (ac==0)
      KMessageBox::error(0, "actionCollection==0");

   fileOpen = KStandardAction::open(this, SLOT(slotFileOpen()), ac);
   fileOpen->setStatusTip(i18n("Opens documents for comparison..."));

   fileReload = KDiff3::createAction< KAction >( i18n("Reload"), KShortcut( Qt::Key_F5 ), this, SLOT(slotReload()), ac, "file_reload" );

   fileSave = KStandardAction::save(this, SLOT(slotFileSave()), ac);
   fileSave->setStatusTip(i18n("Saves the merge result. All conflicts must be solved!"));
   fileSaveAs = KStandardAction::saveAs(this, SLOT(slotFileSaveAs()), ac);
   fileSaveAs->setStatusTip(i18n("Saves the current document as..."));
#ifndef QT_NO_PRINTER
   filePrint = KStandardAction::print(this, SLOT(slotFilePrint()), ac);
   filePrint->setStatusTip(i18n("Print the differences"));
#endif
   fileQuit = KStandardAction::quit(this, SLOT(slotFileQuit()), ac);
   fileQuit->setStatusTip(i18n("Quits the application"));
   editCut = KStandardAction::cut(this, SLOT(slotEditCut()), ac);
   editCut->setStatusTip(i18n("Cuts the selected section and puts it to the clipboard"));
   editCopy = KStandardAction::copy(this, SLOT(slotEditCopy()), ac);
   editCopy->setStatusTip(i18n("Copies the selected section to the clipboard"));
   editPaste = KStandardAction::paste(this, SLOT(slotEditPaste()), ac);
   editPaste->setStatusTip(i18n("Pastes the clipboard contents to current position"));
   editSelectAll = KStandardAction::selectAll(this, SLOT(slotEditSelectAll()), ac);
   editSelectAll->setStatusTip(i18n("Select everything in current window"));
   editFind = KStandardAction::find(this, SLOT(slotEditFind()), ac);
   editFind->setStatusTip(i18n("Search for a string"));
   editFindNext = KStandardAction::findNext(this, SLOT(slotEditFindNext()), ac);
   editFindNext->setStatusTip(i18n("Search again for the string"));
/*   FIXME figure out how to implement this action
   viewToolBar = KStandardAction::showToolbar(this, SLOT(slotViewToolBar()), ac);
   viewToolBar->setStatusTip(i18n("Enables/disables the toolbar")); */
   viewStatusBar = KStandardAction::showStatusbar(this, SLOT(slotViewStatusBar()), ac);
   viewStatusBar->setStatusTip(i18n("Enables/disables the statusbar"));
   KStandardAction::keyBindings(this, SLOT(slotConfigureKeys()), ac);
   KAction* pAction = KStandardAction::preferences(this, SLOT(slotConfigure()), ac );
   if ( isPart() )
      pAction->setText(i18n("Configure KDiff3..."));


#include "xpm/downend.xpm"
#include "xpm/currentpos.xpm"
#include "xpm/down1arrow.xpm"
#include "xpm/down2arrow.xpm"
#include "xpm/upend.xpm"
#include "xpm/up1arrow.xpm"
#include "xpm/up2arrow.xpm"
#include "xpm/prevunsolved.xpm"
#include "xpm/nextunsolved.xpm"
#include "xpm/iconA.xpm"
#include "xpm/iconB.xpm"
#include "xpm/iconC.xpm"
#include "xpm/autoadvance.xpm"
#include "xpm/showwhitespace.xpm"
#include "xpm/showwhitespacechars.xpm"
#include "xpm/showlinenumbers.xpm"
//#include "reload.xpm"

   goCurrent = KDiff3::createAction< KAction >( i18n("Go to Current Delta"), QIcon(QPixmap(currentpos)), i18n("Current\nDelta"), KShortcut( Qt::CTRL+Qt::Key_Space ), this, SLOT(slotGoCurrent()), ac, "go_current");

   goTop = KDiff3::createAction< KAction >(i18n("Go to First Delta"), QIcon(QPixmap(upend)), i18n("First\nDelta"), this, SLOT(slotGoTop()), ac, "go_top");

   goBottom = KDiff3::createAction< KAction >(i18n("Go to Last Delta"), QIcon(QPixmap(downend)), i18n("Last\nDelta"), this, SLOT(slotGoBottom()), ac, "go_bottom");

   QString omitsWhitespace = ".\n" + i18n("(Skips white space differences when \"Show White Space\" is disabled.)");
   QString includeWhitespace = ".\n" + i18n("(Does not skip white space differences even when \"Show White Space\" is disabled.)");
   goPrevDelta = KDiff3::createAction< KAction >(i18n("Go to Previous Delta"), QIcon(QPixmap(up1arrow)), i18n("Prev\nDelta"), KShortcut( Qt::CTRL+Qt::Key_Up), this, SLOT(slotGoPrevDelta()), ac, "go_prev_delta");
   goPrevDelta->setToolTip( goPrevDelta->text() + omitsWhitespace );
   goNextDelta = KDiff3::createAction< KAction >(i18n("Go to Next Delta"), QIcon(QPixmap(down1arrow)), i18n("Next\nDelta"), KShortcut( Qt::CTRL+Qt::Key_Down ), this, SLOT(slotGoNextDelta()), ac, "go_next_delta");
   goNextDelta->setToolTip( goNextDelta->text() + omitsWhitespace );
   goPrevConflict = KDiff3::createAction< KAction >(i18n("Go to Previous Conflict"), QIcon(QPixmap(up2arrow)), i18n("Prev\nConflict"), KShortcut( Qt::CTRL+Qt::Key_PageUp ), this, SLOT(slotGoPrevConflict()), ac, "go_prev_conflict");
   goPrevConflict->setToolTip( goPrevConflict->text() + omitsWhitespace );
   goNextConflict = KDiff3::createAction< KAction >(i18n("Go to Next Conflict"), QIcon(QPixmap(down2arrow)), i18n("Next\nConflict"), KShortcut( Qt::CTRL+Qt::Key_PageDown ), this, SLOT(slotGoNextConflict()), ac, "go_next_conflict");
   goNextConflict->setToolTip( goNextConflict->text() + omitsWhitespace );
   goPrevUnsolvedConflict = KDiff3::createAction< KAction >(i18n("Go to Previous Unsolved Conflict"), QIcon(QPixmap(prevunsolved)), i18n("Prev\nUnsolved"), this, SLOT(slotGoPrevUnsolvedConflict()), ac, "go_prev_unsolved_conflict");
   goPrevUnsolvedConflict->setToolTip( goPrevUnsolvedConflict->text() + includeWhitespace );
   goNextUnsolvedConflict = KDiff3::createAction< KAction >(i18n("Go to Next Unsolved Conflict"), QIcon(QPixmap(nextunsolved)), i18n("Next\nUnsolved"), this, SLOT(slotGoNextUnsolvedConflict()), ac, "go_next_unsolved_conflict");
   goNextUnsolvedConflict->setToolTip( goNextUnsolvedConflict->text() + includeWhitespace );
   chooseA = KDiff3::createAction< KToggleAction >(i18n("Select Line(s) From A"), QIcon(QPixmap(iconA)), i18n("Choose\nA"), KShortcut( Qt::CTRL+Qt::Key_1 ), this, SLOT(slotChooseA()), ac, "merge_choose_a");
   chooseB = KDiff3::createAction< KToggleAction >(i18n("Select Line(s) From B"), QIcon(QPixmap(iconB)), i18n("Choose\nB"), KShortcut( Qt::CTRL+Qt::Key_2 ), this, SLOT(slotChooseB()), ac, "merge_choose_b");
   chooseC = KDiff3::createAction< KToggleAction >(i18n("Select Line(s) From C"), QIcon(QPixmap(iconC)), i18n("Choose\nC"), KShortcut( Qt::CTRL+Qt::Key_3 ), this, SLOT(slotChooseC()), ac, "merge_choose_c");
   autoAdvance = KDiff3::createAction< KToggleAction >(i18n("Automatically Go to Next Unsolved Conflict After Source Selection"), QIcon(QPixmap(autoadvance)), i18n("Auto\nNext"), this, SLOT(slotAutoAdvanceToggled()), ac, "merge_autoadvance");

   showWhiteSpaceCharacters = KDiff3::createAction< KToggleAction >(i18n("Show Space && Tabulator Characters for Differences"), QIcon(QPixmap(showwhitespacechars)), i18n("White\nCharacters"), this, SLOT(slotShowWhiteSpaceToggled()), ac, "diff_show_whitespace_characters");
   showWhiteSpace = KDiff3::createAction< KToggleAction >(i18n("Show White Space"), QIcon(QPixmap(showwhitespace)), i18n("White\nDeltas"), this, SLOT(slotShowWhiteSpaceToggled()), ac, "diff_show_whitespace");

   showLineNumbers = KDiff3::createAction< KToggleAction >(i18n("Show Line Numbers"), QIcon(QPixmap(showlinenumbers)), i18n("Line\nNumbers"), this, SLOT(slotShowLineNumbersToggled()), ac, "diff_showlinenumbers");
   chooseAEverywhere = KDiff3::createAction< KAction >(i18n("Choose A Everywhere"), KShortcut( Qt::CTRL+Qt::SHIFT+Qt::Key_1 ), this, SLOT(slotChooseAEverywhere()), ac, "merge_choose_a_everywhere");
   chooseBEverywhere = KDiff3::createAction< KAction >(i18n("Choose B Everywhere"), KShortcut( Qt::CTRL+Qt::SHIFT+Qt::Key_2 ), this, SLOT(slotChooseBEverywhere()), ac, "merge_choose_b_everywhere");
   chooseCEverywhere = KDiff3::createAction< KAction >(i18n("Choose C Everywhere"), KShortcut( Qt::CTRL+Qt::SHIFT+Qt::Key_3 ), this, SLOT(slotChooseCEverywhere()), ac, "merge_choose_c_everywhere");
   chooseAForUnsolvedConflicts = KDiff3::createAction< KAction >(i18n("Choose A for All Unsolved Conflicts"), this, SLOT(slotChooseAForUnsolvedConflicts()), ac, "merge_choose_a_for_unsolved_conflicts");
   chooseBForUnsolvedConflicts = KDiff3::createAction< KAction >(i18n("Choose B for All Unsolved Conflicts"), this, SLOT(slotChooseBForUnsolvedConflicts()), ac, "merge_choose_b_for_unsolved_conflicts");
   chooseCForUnsolvedConflicts = KDiff3::createAction< KAction >(i18n("Choose C for All Unsolved Conflicts"), this, SLOT(slotChooseCForUnsolvedConflicts()), ac, "merge_choose_c_for_unsolved_conflicts");
   chooseAForUnsolvedWhiteSpaceConflicts = KDiff3::createAction< KAction >(i18n("Choose A for All Unsolved Whitespace Conflicts"), this, SLOT(slotChooseAForUnsolvedWhiteSpaceConflicts()), ac, "merge_choose_a_for_unsolved_whitespace_conflicts");
   chooseBForUnsolvedWhiteSpaceConflicts = KDiff3::createAction< KAction >(i18n("Choose B for All Unsolved Whitespace Conflicts"), this, SLOT(slotChooseBForUnsolvedWhiteSpaceConflicts()), ac, "merge_choose_b_for_unsolved_whitespace_conflicts");
   chooseCForUnsolvedWhiteSpaceConflicts = KDiff3::createAction< KAction >(i18n("Choose C for All Unsolved Whitespace Conflicts"), this, SLOT(slotChooseCForUnsolvedWhiteSpaceConflicts()), ac, "merge_choose_c_for_unsolved_whitespace_conflicts");
   autoSolve    = KDiff3::createAction< KAction >(i18n("Automatically Solve Simple Conflicts"),  this, SLOT(slotAutoSolve()),    ac, "merge_autosolve");
   unsolve      = KDiff3::createAction< KAction >(i18n("Set Deltas to Conflicts"),               this, SLOT(slotUnsolve()),      ac, "merge_autounsolve");
   mergeRegExp  = KDiff3::createAction< KAction >(i18n("Run Regular Expression Auto Merge"),     this, SLOT(slotRegExpAutoMerge()),ac, "merge_regexp_automerge" );
   mergeHistory = KDiff3::createAction< KAction >(i18n("Automatically Solve History Conflicts"), this, SLOT(slotMergeHistory()), ac, "merge_versioncontrol_history" );
   splitDiff    = KDiff3::createAction< KAction >(i18n("Split Diff At Selection"),               this, SLOT(slotSplitDiff()),    ac, "merge_splitdiff");
   joinDiffs    = KDiff3::createAction< KAction >(i18n("Join Selected Diffs"),                   this, SLOT(slotJoinDiffs()),    ac, "merge_joindiffs");

   showWindowA = KDiff3::createAction< KToggleAction >(i18n("Show Window A"), this, SLOT(slotShowWindowAToggled()), ac, "win_show_a");
   showWindowB = KDiff3::createAction< KToggleAction >(i18n("Show Window B"), this, SLOT(slotShowWindowBToggled()), ac, "win_show_b");
   showWindowC = KDiff3::createAction< KToggleAction >(i18n("Show Window C"), this, SLOT(slotShowWindowCToggled()), ac, "win_show_c");
   winFocusNext = KDiff3::createAction< KAction >(i18n("Focus Next Window"), KShortcut( Qt::ALT+Qt::Key_Right ), this, SLOT(slotWinFocusNext()), ac, "win_focus_next");

   overviewModeNormal = KDiff3::createAction< KToggleAction >(i18n("Normal Overview"), this, SLOT(slotOverviewNormal()), ac, "diff_overview_normal");
   overviewModeAB     = KDiff3::createAction< KToggleAction >(i18n("A vs. B Overview"), this, SLOT(slotOverviewAB()), ac, "diff_overview_ab");
   overviewModeAC     = KDiff3::createAction< KToggleAction >(i18n("A vs. C Overview"), this, SLOT(slotOverviewAC()), ac, "diff_overview_ac");
   overviewModeBC     = KDiff3::createAction< KToggleAction >(i18n("B vs. C Overview"), this, SLOT(slotOverviewBC()), ac, "diff_overview_bc");
   wordWrap     = KDiff3::createAction< KToggleAction >(i18n("Word Wrap Diff Windows"), this, SLOT(slotWordWrapToggled()), ac, "diff_wordwrap");
   addManualDiffHelp  = KDiff3::createAction< KAction >(i18n("Add Manual Diff Alignment"), KShortcut( Qt::CTRL+Qt::Key_Y ), this, SLOT(slotAddManualDiffHelp()), ac, "diff_add_manual_diff_help");
   clearManualDiffHelpList  = KDiff3::createAction< KAction >(i18n("Clear All Manual Diff Alignments"), KShortcut( Qt::CTRL+Qt::SHIFT+Qt::Key_Y ), this, SLOT(slotClearManualDiffHelpList()), ac, "diff_clear_manual_diff_help_list");

#if defined(_WIN32) || defined(Q_OS_OS2)
   KDiff3::createAction< KAction >(i18n("Focus Next Window"), KShortcut(Qt::CTRL+Qt::Key_Tab), this, SLOT(slotWinFocusNext()), ac, "win_focus_next");
#endif
   winFocusPrev = KDiff3::createAction< KAction >(i18n("Focus Prev Window"), KShortcut( Qt::ALT+Qt::Key_Left ), this, SLOT(slotWinFocusPrev()), ac, "win_focus_prev");
   winToggleSplitOrientation = KDiff3::createAction< KAction >(i18n("Toggle Split Orientation"), this, SLOT(slotWinToggleSplitterOrientation()), ac, "win_toggle_split_orientation");

   dirShowBoth = KDiff3::createAction< KToggleAction >(i18n("Dir && Text Split Screen View"), this, SLOT(slotDirShowBoth()), ac, "win_dir_show_both");
   dirShowBoth->setChecked( true );
   dirViewToggle = KDiff3::createAction< KAction >(i18n("Toggle Between Dir && Text View"), this, SLOT(slotDirViewToggle()), ac, "win_dir_view_toggle");

   m_pMergeEditorPopupMenu = new QMenu( this );
/*   chooseA->plug( m_pMergeEditorPopupMenu );
   chooseB->plug( m_pMergeEditorPopupMenu );
   chooseC->plug( m_pMergeEditorPopupMenu );*/
   m_pMergeEditorPopupMenu->addAction( chooseA );
   m_pMergeEditorPopupMenu->addAction( chooseB );
   m_pMergeEditorPopupMenu->addAction( chooseC );
}


void KDiff3App::showPopupMenu( const QPoint& point )
{
   m_pMergeEditorPopupMenu->popup( point );
}

void KDiff3App::initStatusBar()
{
  ///////////////////////////////////////////////////////////////////
  // STATUSBAR
  if (statusBar() !=0 )
     statusBar()->showMessage( i18n("Ready.") );
}

void KDiff3App::saveOptions( KSharedConfigPtr config )
{
   if ( !m_bAutoMode )
   {
      if (!isPart())
      {
         m_pOptions->m_bMaximised = m_pKDiff3Shell->isMaximized();
         if( ! m_pKDiff3Shell->isMaximized() && m_pKDiff3Shell->isVisible() )
         {
            m_pOptions->m_geometry = m_pKDiff3Shell->size();
            m_pOptions->m_position = m_pKDiff3Shell->pos();
         }
/*  TODO change this option as now KToolbar uses QToolbar positioning style
         if ( toolBar(MAIN_TOOLBAR_NAME)!=0 )
            m_pOptionDialog->m_toolBarPos = (int) toolBar(MAIN_TOOLBAR_NAME)->allowedAreas();*/
      }

      m_pOptionDialog->saveOptions( config );
   }
}




bool KDiff3App::queryClose()
{
   saveOptions( isPart() ? m_pKDiff3Part->componentData().config() : KGlobal::config() );

   if(m_bOutputModified)
   {
      int result = KMessageBox::warningYesNoCancel(this,
         i18n("The merge result hasn't been saved."),
         i18n("Warning"), 
         KGuiItem( i18n("Save && Quit") ), 
         KGuiItem( i18n("Quit Without Saving") ) );
      if ( result==KMessageBox::Cancel )
         return false;
      else if ( result==KMessageBox::Yes )
      {
         slotFileSave();
         if ( m_bOutputModified )
         {
            KMessageBox::sorry(this, i18n("Saving the merge result failed."), i18n("Warning") );
            return false;
         }
      }
   }

   m_bOutputModified = false;

   if ( m_pDirectoryMergeWindow->isDirectoryMergeInProgress() )
   {
      int result = KMessageBox::warningYesNo(this,
         i18n("You are currently doing a directory merge. Are you sure, you want to abort?"),
         i18n("Warning"), 
         KStandardGuiItem::quit(), 
         KStandardGuiItem::cont() /* i18n("Continue Merging") */ );
      if ( result!=KMessageBox::Yes )
         return false;
   }

   return true;
}


/////////////////////////////////////////////////////////////////////
// SLOT IMPLEMENTATION
/////////////////////////////////////////////////////////////////////


void KDiff3App::slotFileSave()
{
   if ( m_bDefaultFilename )
   {
      slotFileSaveAs();
   }
   else
   {
      slotStatusMsg(i18n("Saving file..."));

      bool bSuccess = m_pMergeResultWindow->saveDocument( m_outputFilename, m_pMergeResultWindowTitle->getEncoding(), m_pMergeResultWindowTitle->getLineEndStyle() );
      if ( bSuccess )
      {
         m_bFileSaved = true;
         m_bOutputModified = false;
         if ( m_bDirCompare )
            m_pDirectoryMergeWindow->mergeResultSaved( m_outputFilename );
      }

      slotStatusMsg(i18n("Ready."));
   }
}

void KDiff3App::slotFileSaveAs()
{
  slotStatusMsg(i18n("Saving file with a new filename..."));

  QString s = KFileDialog::getSaveUrl( QDir::currentPath(), 0, this, i18n("Save As...") ).url();
  if(!s.isEmpty())
  {
     m_outputFilename = s;
     m_pMergeResultWindowTitle->setFileName( m_outputFilename );
     bool bSuccess = m_pMergeResultWindow->saveDocument( m_outputFilename, m_pMergeResultWindowTitle->getEncoding(), m_pMergeResultWindowTitle->getLineEndStyle() );
     if ( bSuccess )
     {
        m_bOutputModified = false;
        if ( m_bDirCompare )
           m_pDirectoryMergeWindow->mergeResultSaved( m_outputFilename );
     }
     //setCaption(url.fileName(),doc->isModified());

     m_bDefaultFilename = false;
  }

  slotStatusMsg(i18n("Ready."));
}


void printDiffTextWindow( MyPainter& painter, const QRect& view, const QString& headerText, DiffTextWindow* pDiffTextWindow, int line, int linesPerPage, QColor fgColor )
{
   QRect clipRect = view;
   clipRect.setTop(0);
   painter.setClipRect( clipRect );
   painter.translate( view.left() , 0 );
   QFontMetrics fm = painter.fontMetrics();
   //if ( fm.width(headerText) > view.width() )
   {
      // A simple wrapline algorithm
      int l=0;
      for (int p=0; p<headerText.length(); )
      {
         QString s = headerText.mid(p);
         int i;
         for(i=2;i<s.length();++i) 
            if (fm.width(s,i)>view.width())
            {
               --i;
               break;
            }
         //QString s2 = s.left(i);
         painter.drawText( 0, l*fm.height() + fm.ascent(), s.left(i) );
         p+=i;
         ++l;
      }
      painter.setPen( fgColor );
      painter.drawLine( 0, view.top()-2, view.width(), view.top()-2 );
   }

   painter.translate( 0, view.top() );
   pDiffTextWindow->print( painter, view, line, linesPerPage );
   painter.resetMatrix();
}

void KDiff3App::slotFilePrint()
{
   if ( !m_pDiffTextWindow1 )
      return;
#ifdef QT_NO_PRINTER
   slotStatusMsg( i18n( "Printing not implemented." ) );
#endif
#ifndef QT_NO_PRINTER
   QPrinter printer;
   QPrintDialog printDialog(&printer, this);

   int firstSelectionD3LIdx = -1;
   int lastSelectionD3LIdx = -1;
   if (                           m_pDiffTextWindow1 ) { m_pDiffTextWindow1->getSelectionRange(&firstSelectionD3LIdx, &lastSelectionD3LIdx, eD3LLineCoords); }
   if ( firstSelectionD3LIdx<0 && m_pDiffTextWindow2 ) { m_pDiffTextWindow2->getSelectionRange(&firstSelectionD3LIdx, &lastSelectionD3LIdx, eD3LLineCoords); }
   if ( firstSelectionD3LIdx<0 && m_pDiffTextWindow3 ) { m_pDiffTextWindow3->getSelectionRange(&firstSelectionD3LIdx, &lastSelectionD3LIdx, eD3LLineCoords); }
#ifdef KREPLACEMENTS_H  // Currently PrintSelection is not supported in KDEs print dialog.
   if ( firstSelectionD3LIdx>=0 )
   {
      printDialog.addEnabledOption(QPrintDialog::PrintSelection);
      //printer.setOptionEnabled(QPrinter::PrintSelection,true);
      printDialog.setPrintRange(QAbstractPrintDialog::Selection);
   }
#endif

   if ( firstSelectionD3LIdx == -1 )
      printDialog.setPrintRange(QAbstractPrintDialog::AllPages);
   //printDialog.setMinMax(0,0);
   printDialog.setFromTo(0,0);

   int currentFirstLine = m_pDiffTextWindow1->getFirstLine();
   int currentFirstD3LIdx = m_pDiffTextWindow1->convertLineToDiff3LineIdx( currentFirstLine );

   // do some printer initialization
   printer.setFullPage( false );

   // initialize the printer using the print dialog
   if (printDialog.exec()== QDialog::Accepted)
   {
      slotStatusMsg( i18n( "Printing..." ) );
      // create a painter to paint on the printer object
      MyPainter painter( &printer, m_pOptions->m_bRightToLeftLanguage, width(), fontMetrics().width('W') );

      QPaintDevice* pPaintDevice = painter.device();
      int dpiy = pPaintDevice->logicalDpiY();
      int columnDistance = (int) ( (0.5/2.54)*dpiy ); // 0.5 cm between the columns

      int columns = m_bTripleDiff ? 3 : 2;
      int columnWidth = ( pPaintDevice->width()  - (columns-1)*columnDistance ) / columns;

      QFont f = m_pOptions->m_font;
      f.setPointSizeF(f.pointSizeF()-1); // Print with slightly smaller font.
      painter.setFont( f );
      QFontMetrics fm = painter.fontMetrics();

      QString topLineText = i18n("Top line"); 

      //int headerWidth = fm.width( m_sd1.getAliasName() + ", "+topLineText+": 01234567" );
      int headerLines = fm.width( m_sd1.getAliasName() + ", "+topLineText+": 01234567" )/columnWidth+1;

      int headerMargin = headerLines * fm.height() + 3; // Text + one horizontal line
      int footerMargin = fm.height() + 3;

      QRect view ( 0, headerMargin, pPaintDevice->width(), pPaintDevice->height() - (headerMargin + footerMargin) );
      QRect view1( 0*(columnWidth + columnDistance), view.top(), columnWidth, view.height() );
      QRect view2( 1*(columnWidth + columnDistance), view.top(), columnWidth, view.height() );
      QRect view3( 2*(columnWidth + columnDistance), view.top(), columnWidth, view.height() );

      int linesPerPage = view.height() / fm.height();
      int charactersPerLine = columnWidth / fm.width("W");
      if ( m_pOptions->m_bWordWrap )
      {
         // For printing the lines are wrapped differently (this invalidates the first line)
         recalcWordWrap( charactersPerLine );
      }

      int totalNofLines = max2(m_pDiffTextWindow1->getNofLines(), m_pDiffTextWindow2->getNofLines());
      if ( m_bTripleDiff && m_pDiffTextWindow3)
         totalNofLines = max2(totalNofLines, m_pDiffTextWindow3->getNofLines());

      QList<int> pageList;// = printer.pageList();

      bool bPrintCurrentPage=false;
      bool bFirstPrintedPage = false;

      bool bPrintSelection = false;
      int totalNofPages = (totalNofLines+linesPerPage-1) / linesPerPage;
      int line=-1;
      int selectionEndLine = -1;

#ifdef KREPLACEMENTS_H
      if ( printer.printRange()==QPrinter::AllPages )
      {
         pageList.clear();
         for(int i=0; i<totalNofPages; ++i)
         {
            pageList.push_back(i+1);
         }
      }
      else if ( printer.printRange()==QPrinter::PageRange )
      {
         pageList.clear();
         for(int i=printer.fromPage(); i<=printer.toPage(); ++i)
         {
            pageList.push_back(i);
         }
      }

      if ( printer.printRange()==QPrinter::Selection )
#else
      if ( !pageList.empty() && pageList.front()==9999 )
#endif
      {
         bPrintSelection = true;
         if ( firstSelectionD3LIdx >=0 )
         {
            line = m_pDiffTextWindow1->convertDiff3LineIdxToLine( firstSelectionD3LIdx );
            selectionEndLine = m_pDiffTextWindow1->convertDiff3LineIdxToLine( lastSelectionD3LIdx+1 );
            totalNofPages = (selectionEndLine-line+linesPerPage-1) / linesPerPage;
         }
      }

      int page = 1;

      QList<int>::iterator pageListIt = pageList.begin();
      for(;;)
      {
         if (!bPrintSelection)
         {
            if (pageListIt==pageList.end())
               break;
            page = *pageListIt;
            line = (page - 1) * linesPerPage;
            if (page==10000)  // This means "Print the current page"
            {
               bPrintCurrentPage=true;
               // Detect the first visible line in the window.
               line = m_pDiffTextWindow1->convertDiff3LineIdxToLine( currentFirstD3LIdx );
            }
         }
         else
         {
            if ( line>=selectionEndLine )
            {
               break;
            }
            else
            {
               if ( selectionEndLine-line < linesPerPage )
                  linesPerPage=selectionEndLine-line;
            }
         }
         if (line>=0 && line<totalNofLines )
         {

            if (bFirstPrintedPage)
               printer.newPage();

            painter.setClipping(true);

            painter.setPen( m_pOptions->m_colorA );
            QString headerText1 = m_sd1.getAliasName() + ", "+topLineText+": " + QString::number(m_pDiffTextWindow1->calcTopLineInFile(line)+1);
            printDiffTextWindow( painter, view1, headerText1, m_pDiffTextWindow1, line, linesPerPage, m_pOptions->m_fgColor );

            painter.setPen( m_pOptions->m_colorB );
            QString headerText2 = m_sd2.getAliasName() + ", "+topLineText+": " + QString::number(m_pDiffTextWindow2->calcTopLineInFile(line)+1);
            printDiffTextWindow( painter, view2, headerText2, m_pDiffTextWindow2, line, linesPerPage, m_pOptions->m_fgColor );

            if ( m_bTripleDiff && m_pDiffTextWindow3 )
            {
               painter.setPen( m_pOptions->m_colorC );
               QString headerText3 = m_sd3.getAliasName() + ", "+topLineText+": " + QString::number(m_pDiffTextWindow3->calcTopLineInFile(line)+1);
               printDiffTextWindow( painter, view3, headerText3, m_pDiffTextWindow3, line, linesPerPage, m_pOptions->m_fgColor );
            }
            painter.setClipping(false);

            painter.setPen( m_pOptions->m_fgColor );
            painter.drawLine( 0, view.bottom()+3, view.width(), view.bottom()+3 ); 
            QString s = bPrintCurrentPage ? QString("") 
                                          : QString::number( page ) + "/" + QString::number(totalNofPages);
            if ( bPrintSelection ) s+=" (" + i18n("Selection") + ")";
            painter.drawText( (view.right() - painter.fontMetrics().width( s ))/2,
                        view.bottom() + painter.fontMetrics().ascent() + 5, s );

            bFirstPrintedPage = true;
         }

         if ( bPrintSelection )
         {
            line+=linesPerPage;
            ++page;
         }
         else
         {
            ++pageListIt;
         }
      }

      painter.end();

      if ( m_pOptions->m_bWordWrap )
      {
         recalcWordWrap();
         m_pDiffVScrollBar->setValue( m_pDiffTextWindow1->convertDiff3LineIdxToLine( currentFirstD3LIdx ) );
      }

      slotStatusMsg( i18n( "Printing completed." ) );
   }
   else
   {
      slotStatusMsg( i18n( "Printing aborted." ) );
   }
#endif
}

void KDiff3App::slotFileQuit()
{
   slotStatusMsg(i18n("Exiting..."));

   if( !queryClose() )
       return;      // Don't quit

   KApplication::exit( isFileSaved() || isDirComparison() ? 0 : 1 );
}



void KDiff3App::slotViewToolBar()
{
   assert( viewToolBar != 0 );
   slotStatusMsg(i18n("Toggling toolbar..."));
   m_pOptions->m_bShowToolBar = viewToolBar->isChecked();
   ///////////////////////////////////////////////////////////////////
   // turn Toolbar on or off
   if ( toolBar(MAIN_TOOLBAR_NAME) !=0 )
   {
      if(!m_pOptions->m_bShowToolBar)
      {
         toolBar(MAIN_TOOLBAR_NAME)->hide();
      }
      else
      {
         toolBar(MAIN_TOOLBAR_NAME)->show();
      }
   }

   slotStatusMsg(i18n("Ready."));
}

void KDiff3App::slotViewStatusBar()
{
   slotStatusMsg(i18n("Toggle the statusbar..."));
   m_pOptions->m_bShowStatusBar = viewStatusBar->isChecked();
   ///////////////////////////////////////////////////////////////////
   //turn Statusbar on or off
   if (statusBar() !=0 )
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


void KDiff3App::slotStatusMsg(const QString &text)
{
   ///////////////////////////////////////////////////////////////////
   // change status message permanently
   if (statusBar() !=0 )
   {
      statusBar()->clearMessage();
      statusBar()->showMessage( text );
   }
}




//#include "kdiff3.moc"
