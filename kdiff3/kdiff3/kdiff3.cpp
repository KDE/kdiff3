/***************************************************************************
                          kdiff3.cpp  -  description
                             -------------------
    begin                : Don Jul 11 12:31:29 CEST 2002
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
 * Revision 1.1  2002/08/18 16:24:16  joachim99
 * Initial revision
 *                                                                   *
 ***************************************************************************/

// include files for QT
#include <qdir.h>
#include <qprinter.h>
#include <qpainter.h>

// include files for KDE
#include <kiconloader.h>
#include <kmessagebox.h>
#include <kfiledialog.h>
#include <kmenubar.h>
#include <kstatusbar.h>
#include <klocale.h>
#include <kconfig.h>
#include <kstdaction.h>
#include <kcmdlineargs.h>

// application specific includes
#include "kdiff3.h"
#include "optiondialog.h"

#define ID_STATUS_MSG 1

KDiff3App::KDiff3App(QWidget* , const char* name):KMainWindow(0, name)
{
   config=kapp->config();

   m_pBuf1 = 0;
   m_pBuf2 = 0;
   m_pBuf3 = 0;

   // Option handling
   KCmdLineArgs *args = KCmdLineArgs::parsedArgs();

   m_outputFilename = args->getOption("output");

   if ( m_outputFilename.isEmpty()  && args->isSet("merge") )
   {
      m_outputFilename = "unnamed.txt";
      m_bDefaultFilename = true;
   }
   else
      m_bDefaultFilename = false;

   if ( args->count() > 0 ) m_filename1 = args->arg(0);
   if ( args->count() > 1 ) m_filename2 = args->arg(1);
   if ( args->count() > 2 ) m_filename3 = args->arg(2);

   args->clear(); // Free up some memory.


//   setCaption(tr( APP_NAME " " VERSION));

   ///////////////////////////////////////////////////////////////////
   // call inits to invoke all other construction parts
   initActions();
   initStatusBar();

   // All default values must be set before calling readOptions().
   m_pOptionDialog = new OptionDialog( this );
   connect( m_pOptionDialog, SIGNAL(applyClicked()), this, SLOT(slotRefresh()) );

   readOptions();

   init( m_filename1, m_filename2, m_filename3 );

   statusBar()->setSizeGripEnabled(false);


   ///////////////////////////////////////////////////////////////////
   // disable actions at startup
   fileSave->setEnabled(false);
   fileSaveAs->setEnabled(false);

   editCut->setEnabled(false);
   editCopy->setEnabled(false);
   editPaste->setEnabled(false);

   bool bFileOpenError = false;
   if ( ! m_filename1.isEmpty() && m_pBuf1==0  ||
        ! m_filename2.isEmpty() && m_pBuf2==0  ||
        ! m_filename3.isEmpty() && m_pBuf3==0 )
   {
      QString text( i18n("Opening of these files failed:") );
      text += "\n\n";
      if ( ! m_filename1.isEmpty() && m_pBuf1==0 )
         text += " - " + m_filename1 + "\n";
      if ( ! m_filename2.isEmpty() && m_pBuf2==0 )
         text += " - " + m_filename2 + "\n";
      if ( ! m_filename3.isEmpty() && m_pBuf3==0 )
         text += " - " + m_filename3 + "\n";

      KMessageBox::sorry( this, text, i18n("File open error") );

      bFileOpenError = true;
   }

   if ( m_filename1.isEmpty() || m_filename2.isEmpty() || bFileOpenError )
      slotFileOpen();

   slotClipboardChanged(); // For initialisation.
}

KDiff3App::~KDiff3App()
{

}

void KDiff3App::initActions()
{
   fileOpen = KStdAction::open(this, SLOT(slotFileOpen()), actionCollection());
   fileSave = KStdAction::save(this, SLOT(slotFileSave()), actionCollection());
   fileSaveAs = KStdAction::saveAs(this, SLOT(slotFileSaveAs()), actionCollection());
   fileQuit = KStdAction::quit(this, SLOT(slotFileQuit()), actionCollection());
   editCut = KStdAction::cut(this, SLOT(slotEditCut()), actionCollection());
   editCopy = KStdAction::copy(this, SLOT(slotEditCopy()), actionCollection());
   editPaste = KStdAction::paste(this, SLOT(slotEditPaste()), actionCollection());
   viewToolBar = KStdAction::showToolbar(this, SLOT(slotViewToolBar()), actionCollection());
   viewStatusBar = KStdAction::showStatusbar(this, SLOT(slotViewStatusBar()), actionCollection());
   KStdAction::preferences(this, SLOT(slotConfigure()), actionCollection() );

   fileOpen->setStatusText(i18n("Opens documents for comparison ..."));
   fileSave->setStatusText(i18n("Saves the merge result. All conflicts must be solved!"));
   fileSaveAs->setStatusText(i18n("Saves the current document as..."));
   fileQuit->setStatusText(i18n("Quits the application"));
   editCut->setStatusText(i18n("Cuts the selected section and puts it to the clipboard"));
   editCopy->setStatusText(i18n("Copies the selected section to the clipboard"));
   editPaste->setStatusText(i18n("Pastes the clipboard contents to actual position"));
   viewToolBar->setStatusText(i18n("Enables/disables the toolbar"));
   viewStatusBar->setStatusText(i18n("Enables/disables the statusbar"));

#include "downend.xpm"
#include "down1arrow.xpm"
#include "down2arrow.xpm"
#include "upend.xpm"
#include "up1arrow.xpm"
#include "up2arrow.xpm"
#include "iconA.xpm"
#include "iconB.xpm"
#include "iconC.xpm"

   goTop = new KAction(i18n("Go to Top"), QIconSet(QPixmap(upend)), 0, this, SLOT(slotGoTop()), actionCollection(), "go_top");
   goBottom = new KAction(i18n("Go to Bottom"), QIconSet(QPixmap(downend)), 0, this, SLOT(slotGoBottom()), actionCollection(), "go_bottom");
   goPrevDelta = new KAction(i18n("Go to PrevDelta"), QIconSet(QPixmap(up1arrow)), 0, this, SLOT(slotGoPrevDelta()), actionCollection(), "go_prev_delta");
   goNextDelta = new KAction(i18n("Go to NextDelta"), QIconSet(QPixmap(down1arrow)), 0, this, SLOT(slotGoNextDelta()), actionCollection(), "go_next_delta");
   goPrevConflict = new KAction(i18n("Go to Previous Conflict"), QIconSet(QPixmap(up2arrow)), 0, this, SLOT(slotGoPrevConflict()), actionCollection(), "go_prev_conflict");
   goNextConflict = new KAction(i18n("Go to Next Conflict"), QIconSet(QPixmap(down2arrow)), 0, this, SLOT(slotGoNextConflict()), actionCollection(), "go_next_conflict");
   chooseA = new KToggleAction(i18n("Select line(s) from A"), QIconSet(QPixmap(iconA)), 0, this, SLOT(slotChooseA()), actionCollection(), "choose_a");
   chooseB = new KToggleAction(i18n("Select line(s) from B"), QIconSet(QPixmap(iconB)), 0, this, SLOT(slotChooseB()), actionCollection(), "choose_b");
   chooseC = new KToggleAction(i18n("Select line(s) from C"), QIconSet(QPixmap(iconC)), 0, this, SLOT(slotChooseC()), actionCollection(), "choose_c");

   // use the absolute path to your kdiff3ui.rc file for testing purpose in createGUI();
   createGUI();

}


void KDiff3App::initStatusBar()
{
  ///////////////////////////////////////////////////////////////////
  // STATUSBAR
  // TODO: add your own items you need for displaying current application status.
  statusBar()->insertItem(i18n("Ready."), ID_STATUS_MSG);
}

void KDiff3App::saveOptions()
{
   config->setGroup("General Options");
   config->writeEntry("Geometry", size());
   config->writeEntry("Show Toolbar", viewToolBar->isChecked());
   config->writeEntry("Show Statusbar",viewStatusBar->isChecked());
   config->writeEntry("ToolBarPos", (int) toolBar("mainToolBar")->barPos());

   m_pOptionDialog->saveOptions( config );
}


void KDiff3App::readOptions()
{
   config->setGroup("General Options");

   // bar status settings
   bool bViewToolbar = config->readBoolEntry("Show Toolbar", true);
   viewToolBar->setChecked(bViewToolbar);
   slotViewToolBar();

   bool bViewStatusbar = config->readBoolEntry("Show Statusbar", true);
   viewStatusBar->setChecked(bViewStatusbar);
   slotViewStatusBar();


   // bar position settings
   KToolBar::BarPosition toolBarPos;
   toolBarPos=(KToolBar::BarPosition) config->readNumEntry("ToolBarPos", KToolBar::Top);
   toolBar("mainToolBar")->setBarPos(toolBarPos);

   QSize size=config->readSizeEntry("Geometry");
   if(!size.isEmpty())
   {
     resize(size);
   }

   m_pOptionDialog->readOptions( config );
}


bool KDiff3App::queryClose()
{
   if(m_bOutputModified)
   {
      int result = KMessageBox::warningYesNo(this,
         i18n("The output file has been modified.\n"
              "If you continue your changes will be lost."),
         i18n("Warning"), i18n("Continue"), i18n("Cancel"));
      if ( result==KMessageBox::No )
         return false;
      else
         return true;   // Finished because user doesn't want to save.
   }

   return true;
}

bool KDiff3App::queryExit()
{
  saveOptions();
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

      bool bSuccess = m_pMergeResultWindow->saveDocument( m_outputFilename );
      if ( bSuccess )
         m_bOutputModified = false;

      slotStatusMsg(i18n("Ready."));
   }
}

void KDiff3App::slotFileSaveAs()
{
  slotStatusMsg(i18n("Saving file with a new filename..."));

  QString s = KFileDialog::getSaveFileName( QDir::currentDirPath(),
        i18n("*|All files"), this, i18n("Save as...") );
  if(!s.isEmpty())
  {
     m_outputFilename = s;
     bool bSuccess = m_pMergeResultWindow->saveDocument( m_outputFilename );
     if ( bSuccess )
        m_bOutputModified = false;
     //setCaption(url.fileName(),doc->isModified());

     m_bDefaultFilename = false;
  }

  slotStatusMsg(i18n("Ready."));
}

void KDiff3App::slotFileClose()
{
  slotStatusMsg(i18n("Closing file..."));

  close();

  slotStatusMsg(i18n("Ready."));
}


void KDiff3App::slotFileQuit()
{
   slotStatusMsg(i18n("Exiting..."));
   saveOptions();
   // close the first window, the list makes the next one the first again.
   // This ensures that queryClose() is called on each window to ask for closing
   KMainWindow* w;
   if(memberList)
   {
     for(w=memberList->first(); w!=0; w=memberList->first())
     {
       // only close the window if the closeEvent is accepted. If the user presses Cancel on the saveModified() dialog,
       // the window and the application stay open.
       if(!w->close())
          break;
     }
   }
}



void KDiff3App::slotViewToolBar()
{
  slotStatusMsg(i18n("Toggling toolbar..."));
  ///////////////////////////////////////////////////////////////////
  // turn Toolbar on or off
  if(!viewToolBar->isChecked())
  {
    toolBar("mainToolBar")->hide();
  }
  else
  {
    toolBar("mainToolBar")->show();
  }

  slotStatusMsg(i18n("Ready."));
}

void KDiff3App::slotViewStatusBar()
{
  slotStatusMsg(i18n("Toggle the statusbar..."));
  ///////////////////////////////////////////////////////////////////
  //turn Statusbar on or off
  if(!viewStatusBar->isChecked())
  {
    statusBar()->hide();
  }
  else
  {
    statusBar()->show();
  }

  slotStatusMsg(i18n("Ready."));
}


void KDiff3App::slotStatusMsg(const QString &text)
{
  ///////////////////////////////////////////////////////////////////
  // change status message permanently
  statusBar()->clear();
  statusBar()->changeItem(text, ID_STATUS_MSG);
}
