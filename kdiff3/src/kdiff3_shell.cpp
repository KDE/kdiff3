/***************************************************************************
 * Copyright (C) 2003-2007 Joachim Eibl <joachim.eibl at gmx.de>           *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Steet, Fifth Floor, Boston, MA 02110-1301, USA.           *
 ***************************************************************************/

#include "kdiff3_shell.h"
#include "kdiff3.h"

#include <kkeydialog.h>
#include <kfiledialog.h>
#include <kconfig.h>
#include <kurl.h>

#include <kedittoolbar.h>

#include <kaction.h>
#include <kstdaction.h>

#include <klibloader.h>
#include <kmessagebox.h>
#include <kstatusbar.h>
#include <klocale.h>

#include <iostream>

KDiff3Shell::KDiff3Shell(bool bCompleteInit)
    : KParts::MainWindow( 0L, "kdiff3" )
{
    m_bUnderConstruction = true;
    // set the shell's ui resource file
    setXMLFile("kdiff3_shell.rc");

    // and a status bar
    statusBar()->show();

    // this routine will find and load our Part.  it finds the Part by
    // name which is a bad idea usually.. but it's alright in this
    // case since our Part is made for this Shell
    KLibFactory *factory = KLibLoader::self()->factory("libkdiff3part");
    if (factory)
    {
        // now that the Part is loaded, we cast it to a Part to get
        // our hands on it
        m_part = static_cast<KParts::ReadWritePart *>(factory->create(this,
                                "kdiff3_part", "KParts::ReadWritePart" ));

        if (m_part)
        {
            // and integrate the part's GUI with the shell's
            createGUI(m_part);

            // tell the KParts::MainWindow that this is indeed the main widget
            setCentralWidget(m_part->widget());

            if (bCompleteInit)
               ((KDiff3App*)m_part->widget())->completeInit();
            connect(((KDiff3App*)m_part->widget()), SIGNAL(createNewInstance(const QString&, const QString&, const QString&)), this, SLOT(slotNewInstance(const QString&, const QString&, const QString&)));
        }
    }
    else
    {
        // if we couldn't find our Part, we exit since the Shell by
        // itself can't do anything useful
        KMessageBox::error(this, i18n("Could not find our part!\n"
           "This usually happens due to an installation problem. "
           "Please read the README-file in the source package for details.")
           );
        //kapp->quit();
        
        ::exit(-1); //kapp->quit() doesn't work here yet.

        // we return here, cause kapp->quit() only means "exit the
        // next time we enter the event loop...
        
        return;
    }

    // apply the saved mainwindow settings, if any, and ask the mainwindow
    // to automatically save settings if changed: window size, toolbar
    // position, icon size, etc.
    setAutoSaveSettings();
    m_bUnderConstruction = false;
}

KDiff3Shell::~KDiff3Shell()
{
}

bool KDiff3Shell::queryClose()
{
   if (m_part)
      return ((KDiff3App*)m_part->widget())->queryClose();
   else
      return true;
}

bool KDiff3Shell::queryExit()
{
   return true;
}

void KDiff3Shell::closeEvent(QCloseEvent*e)
{
   if ( queryClose() )
   {
      e->accept();
      bool bFileSaved = ((KDiff3App*)m_part->widget())->isFileSaved();
      KApplication::exit( bFileSaved ? 0 : 1 );
   }
   else
      e->ignore();
}

void KDiff3Shell::optionsShowToolbar()
{
    // this is all very cut and paste code for showing/hiding the
    // toolbar
    if (m_toolbarAction->isChecked())
        toolBar()->show();
    else
        toolBar()->hide();
}

void KDiff3Shell::optionsShowStatusbar()
{
    // this is all very cut and paste code for showing/hiding the
    // statusbar
    if (m_statusbarAction->isChecked())
        statusBar()->show();
    else
        statusBar()->hide();
}

void KDiff3Shell::optionsConfigureKeys()
{
    KKeyDialog::configure(actionCollection(), "kdiff3_shell.rc");
}

void KDiff3Shell::optionsConfigureToolbars()
{
#if defined(KDE_MAKE_VERSION)
# if KDE_VERSION >= KDE_MAKE_VERSION(3,1,0)
    saveMainWindowSettings(KGlobal::config(), autoSaveGroup());
# else
    saveMainWindowSettings(KGlobal::config() );
# endif
#else
    saveMainWindowSettings(KGlobal::config() );
#endif

    // use the standard toolbar editor
    KEditToolbar dlg(factory());
    connect(&dlg, SIGNAL(newToolbarConfig()),
            this, SLOT(applyNewToolbarConfig()));
    dlg.exec();
}

void KDiff3Shell::applyNewToolbarConfig()
{
#if defined(KDE_MAKE_VERSION)
# if KDE_VERSION >= KDE_MAKE_VERSION(3,1,0)
    applyMainWindowSettings(KGlobal::config(), autoSaveGroup());
# else
    applyMainWindowSettings(KGlobal::config());
# endif
#else
    applyMainWindowSettings(KGlobal::config());
#endif
}

void KDiff3Shell::slotNewInstance( const QString& fn1, const QString& fn2, const QString& fn3 )
{
   KDiff3Shell* pKDiff3Shell = new KDiff3Shell(false);
   ((KDiff3App*)pKDiff3Shell->m_part->widget())->completeInit(fn1,fn2,fn3);
}

#include "kdiff3_shell.moc"
