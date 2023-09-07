/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
// clang-format on

#include "kdiff3_shell.h"
#include "kdiff3.h"
#include "kdiff3_part.h"

#include <QApplication>
#include <QCloseEvent>
#include <QStatusBar>

#include <KActionCollection>
#include <KConfig>
#include <KEditToolBar>
#include <KLocalizedString>
#include <KMessageBox>
#include <KShortcutsDialog>
#include <KStandardAction>
#include <KToolBar>


KDiff3Shell::KDiff3Shell(bool bCompleteInit)
{
    m_bUnderConstruction = true;
    // set the shell's ui resource file
    setXMLFile("kdiff3_shell.rc");

    // and a status bar
    statusBar()->show();

    /*const QVector<KPluginMetaData> plugin_offers = KPluginLoader::findPlugins( "kf5/kdiff3part" );
    for( const KPluginMetaData & service: plugin_offers ) {
        KPluginFactory *factory = KPluginLoader( service.fileName() ).factory();
        m_part = factory->create<KDiff3Part>( this, QVariantList() << QVariant( QLatin1String( "KDiff3Part" ) ) );
        if( m_part )
            break;
    }*/

    //Avoid redudant init call.
    KDiff3Part::noInit();

    m_part = new KDiff3Part(this, this, {QVariant(u8"KDiff3Part")});

    if(m_part)
    {
        m_widget = qobject_cast<KDiff3App*>(m_part->widget());
        // and integrate the part's GUI with the shell's
        createGUI(m_part);
        setStandardToolBarMenuEnabled(true);
        KStandardAction::configureToolbars(this, &KDiff3Shell::configureToolbars, actionCollection());

        //toolBar()->setToolButtonStyle( Qt::ToolButtonIconOnly );

        // tell the KParts::MainWindow that this is indeed the main widget
        setCentralWidget(m_widget);

        if(bCompleteInit)
            m_widget->completeInit(QString());
        chk_connect(m_widget, &KDiff3App::createNewInstance, this, &KDiff3Shell::slotNewInstance);
    }
    else
    {
        // if we couldn't find our Part, we exit since the Shell by
        // itself can't do anything useful
        KMessageBox::error(this, i18n("Could not initialize the KDiff3 part.\n"
                                      "This usually happens due to an installation problem. "
                                      "Please read the README-file in the source package for details."));
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

KDiff3Shell::~KDiff3Shell() = default;

bool KDiff3Shell::queryClose()
{
    if(m_widget)
        return m_widget->queryClose();
    else
        return true;
}

bool KDiff3Shell::queryExit()
{
    return true;
}

void KDiff3Shell::closeEvent(QCloseEvent* e)
{
    if(queryClose())
    {
        e->accept();
        bool bFileSaved = m_widget->isFileSaved();
        bool bDirCompare = m_widget->isDirComparison();
        QApplication::exit(bFileSaved || bDirCompare ? 0 : 1);
    }
    else
        e->ignore();
}

void KDiff3Shell::optionsShowStatusbar()
{
    // this is all very cut and paste code for showing/hiding the
    // statusbar
    if(m_statusbarAction->isChecked())
        statusBar()->show();
    else
        statusBar()->hide();
}

void KDiff3Shell::optionsConfigureKeys()
{
    KShortcutsDialog::configure(actionCollection(), KShortcutsEditor::LetterShortcutsDisallowed /*, "kdiff3_shell.rc" */);
}

void KDiff3Shell::optionsConfigureToolbars()
{
    KConfigGroup mainWindowGroup(KSharedConfig::openConfig(), "MainWindow");
    saveMainWindowSettings(mainWindowGroup);

    // use the standard toolbar editor
    KEditToolBar dlg(factory());
    chk_connect(&dlg, &KEditToolBar::newToolBarConfig, this, &KDiff3Shell::applyNewToolbarConfig);
    dlg.exec();
}

void KDiff3Shell::applyNewToolbarConfig()
{
    KConfigGroup mainWindowGroup(KSharedConfig::openConfig(), "MainWindow");
    applyMainWindowSettings(mainWindowGroup);
}

void KDiff3Shell::slotNewInstance(const QString& fn1, const QString& fn2, const QString& fn3)
{
    static KDiff3Shell* pKDiff3Shell = new KDiff3Shell(false);
    pKDiff3Shell->m_widget->completeInit(fn1, fn2, fn3);
}

//#include "kdiff3_shell.moc"
