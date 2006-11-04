/***************************************************************************
                          kreplacements.cpp  -  description
                             -------------------
    begin                : Sat Aug 3 2002
    copyright            : (C) 2002-2006 by Joachim Eibl
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


#include "kreplacements.h"
#include "common.h"

#include <assert.h>

#include <qnamespace.h>
#include <qmessagebox.h>
#include <qmenu.h>
#include <qmenubar.h>
#include <qpainter.h>
#include <qcolordialog.h>
#include <qfontdialog.h>
#include <qlabel.h>
#include <qtextbrowser.h>
#include <qtextstream.h>
#include <qlayout.h>
#include <QTabWidget>
#include <QPaintEvent>
#include <QPixmap>
#include <QProcess>

#include <QTabWidget>
#include <QFileInfo>
#include <QFileDialog>

#include <vector>
#include <iostream>
#include <algorithm>


static QString s_copyright;
static QString s_email;
static QString s_description;
static QString s_appName;
static QString s_version;
static QString s_homepage;
static KAboutData* s_pAboutData;


#ifdef _WIN32
#include <process.h>
#include <windows.h>
#endif

static void showHelp()
{
   #ifdef _WIN32
      char buf[200];
      int r= SearchPathA( 0, ".",  0, sizeof(buf), buf, 0 );

      QString exePath;
      if (r!=0)  {  exePath = buf; }
      else       {  exePath = "."; }

      QFileInfo helpFile( exePath + "\\doc\\en\\index.html" );
      if ( ! helpFile.exists() ) {  helpFile.setFile( exePath + "\\..\\doc\\en\\index.html" );     }
      if ( ! helpFile.exists() ) {  helpFile.setFile( exePath + "\\doc\\index.html"         );     }
      if ( ! helpFile.exists() ) {  helpFile.setFile( exePath + "\\..\\doc\\index.html"     );     }
      if ( ! helpFile.exists() )
      {
         QMessageBox::warning( 0, "KDiff3 documentation not found",
            "Couldn't find the documentation. \n\n"
            "The documentation can also be found at the homepage:\n\n "
            "    http://kdiff3.sourceforge.net/");
         return;
      }

      HINSTANCE hi = FindExecutableA( helpFile.fileName().toAscii(), helpFile.absolutePath().toAscii(), buf );
      if ( int(hi)<=32 )
      {
         static QTextBrowser* pBrowser = 0;
         if (pBrowser==0)
         {
            pBrowser = new QTextBrowser( 0 );
            pBrowser->setMinimumSize( 600, 400 );
         }
         pBrowser->setSource(helpFile.filePath());
         pBrowser->show();
      }
      else
      {
         QFileInfo prog( buf );
         //_spawnlp( _P_NOWAIT , prog.filePath().toAscii(), prog.fileName().toAscii(), (""+.absFilePath()).toAscii(), NULL );
         QProcess::startDetached ( prog.filePath() + " \"file:///" + helpFile.absolutePath() + "\"" );
      }

   #else
      static QTextBrowser* pBrowser = 0;
      if (pBrowser==0)
      {
         pBrowser = new QTextBrowser( 0 );
         pBrowser->setMinimumSize( 600, 400 );
      }
      pBrowser->setSource(QUrl("file://usr/local/share/doc/kdiff3/en/index.html"));
      pBrowser->show();
   #endif
}

QString getTranslationDir()
{
   #ifdef _WIN32
      char buf[200];
      int r= SearchPathA( 0, ".",  0, sizeof(buf), buf, 0 );

      QString exePath;
      if (r!=0)  {  exePath = buf; }
      else       {  exePath = "."; }
      return exePath+"/translations";
   #else
      return ".";
   #endif
}

// static
void KMessageBox::error( QWidget* parent, const QString& text, const QString& caption )
{
   QMessageBox::critical( parent, caption, text );
}

int KMessageBox::warningContinueCancel( QWidget* parent, const QString& text, const QString& caption,
   const QString& button1 )
{
   return  0 == QMessageBox::warning( parent, caption, text, button1, "Cancel" ) ? Continue : Cancel;
}

void KMessageBox::sorry(  QWidget* parent, const QString& text, const QString& caption )
{
   QMessageBox::information( parent, caption, text );
}

void KMessageBox::information(  QWidget* parent, const QString& text, const QString& caption )
{
   QMessageBox::information( parent, caption, text );
}

int  KMessageBox::warningYesNo( QWidget* parent, const QString& text, const QString& caption,
   const QString& button1, const QString& button2 )
{
   return  0 == QMessageBox::warning( parent, caption, text, button1, button2, QString::null, 1, 1 ) ? Yes : No;
}

int KMessageBox::warningYesNoCancel( QWidget* parent, const QString& text, const QString& caption,
   const QString& button1, const QString& button2 )
{
   int val = QMessageBox::warning( parent, caption, text,
      button1, button2, i18n("Cancel") );
   if ( val==0 ) return Yes;
   if ( val==1 ) return No;
   else return Cancel;
}


KDialogBase::KDialogBase( int, const QString& caption, int, int, QWidget* parent, const char* name,
  bool /*modal*/, bool )
: QDialog( parent )
{
   setObjectName(name);
   setModal(true);
   QVBoxLayout *pMainLayout = new QVBoxLayout(this);
   m_pTabWidget = new QTabWidget();
   pMainLayout->addWidget(m_pTabWidget,1);

   QHBoxLayout* pButtonLayout = new QHBoxLayout();
   pMainLayout->addLayout( pButtonLayout );

   pButtonLayout->addStretch(1);
   QPushButton* pOk = new QPushButton( i18n("Ok") );
   connect( pOk, SIGNAL( clicked() ), this, SLOT(accept()) );
   pButtonLayout->addWidget( pOk );

   QPushButton* pHelp = new QPushButton( i18n("Help") );
   connect( pHelp, SIGNAL( clicked() ), this, SLOT(slotHelp()));
   pButtonLayout->addWidget( pHelp );

   QPushButton* pDefaults = new QPushButton( i18n("Defaults") );
   connect( pDefaults, SIGNAL( clicked() ), this, SLOT(slotDefault()) );
   pButtonLayout->addWidget( pDefaults );

   QPushButton* pCancel = new QPushButton( i18n("Cancel") );
   connect( pCancel, SIGNAL( clicked() ), this, SLOT(reject()));
   pButtonLayout->addWidget( pCancel );

   setWindowTitle( caption );
}

KDialogBase::~KDialogBase()
{
}

void KDialogBase::incInitialSize ( const QSize& )
{
}

void KDialogBase::setHelp(const QString&, const QString& )
{
}


int KDialogBase::BarIcon(const QString& /*iconName*/, int )
{
   return 0; // Not used for replacement.
}


QFrame* KDialogBase::addPage(  const QString& name, const QString& /*info*/, int )
{
   QFrame* p = new QFrame();
   p->setObjectName( name );
   m_pTabWidget->addTab( p, name );
   return p;
}

int KDialogBase::spacingHint()
{
   return 3;
}

static bool s_inAccept = false;
static bool s_bAccepted = false;
void KDialogBase::accept()
{
   if( ! s_inAccept )
   {
      s_bAccepted = false;
      s_inAccept = true;
      slotOk();
      s_inAccept = false;
      if ( s_bAccepted )
         QDialog::accept();
   }
   else
   {
      s_bAccepted = true;   
   }   
}

void KDialogBase::slotDefault( )
{
}
void KDialogBase::slotOk()
{
}
void KDialogBase::slotCancel( )
{
}
void KDialogBase::slotApply( )
{
   emit applyClicked();
}
void KDialogBase::slotHelp( )
{
   showHelp();
}

KURL KFileDialog::getSaveURL( const QString &startDir,
                              const QString &filter,
                              QWidget *parent, const QString &caption)
{
   QString s = QFileDialog::getSaveFileName(parent, caption, startDir, filter, 0/*, QFileDialog::DontUseNativeDialog*/);
   return KURL(s);
}

KURL KFileDialog::getOpenURL( const QString &  startDir,
                           const QString &  filter,
                           QWidget *  parent,
                           const QString &  caption )
{
   QString s = QFileDialog::getOpenFileName(parent, caption, startDir, filter );
   return KURL(s);
}

KURL KFileDialog::getExistingURL( const QString &  startDir,
                               QWidget *  parent,
                               const QString &  caption)
{
   QString s = QFileDialog::getExistingDirectory(parent, caption, startDir);
   return KURL(s);
}

QString KFileDialog::getSaveFileName (const QString &startDir, 
                        const QString &filter, 
                        QWidget *parent, 
                        const QString &caption)
{
   return QFileDialog::getSaveFileName( parent, caption, startDir, filter );
}


KToolBar::BarPosition KToolBar::barPos()
{
   if ( m_pMainWindow->toolBarArea(this)==Qt::LeftToolBarArea ) return Left;
   if ( m_pMainWindow->toolBarArea(this)==Qt::RightToolBarArea ) return Right;
   if ( m_pMainWindow->toolBarArea(this)==Qt::BottomToolBarArea ) return Bottom;
   if ( m_pMainWindow->toolBarArea(this)==Qt::TopToolBarArea ) return Top;
   return Top;
}

void KToolBar::setBarPos(BarPosition bp)
{
   if ( bp == Left ) m_pMainWindow->addToolBar ( Qt::LeftToolBarArea, this );
   else if ( bp == Right ) m_pMainWindow->addToolBar ( Qt::RightToolBarArea, this );
   else if ( bp == Bottom ) m_pMainWindow->addToolBar ( Qt::BottomToolBarArea, this );
   else if ( bp == Top ) m_pMainWindow->addToolBar ( Qt::TopToolBarArea, this );
}

KToolBar::KToolBar( QMainWindow* parent )
: QToolBar( parent )
{
   m_pMainWindow = parent;
}


KMainWindow::KMainWindow( QWidget* parent, const char* name )
: QMainWindow( parent ), m_actionCollection(this)
{
   setObjectName(name);
   fileMenu =      menuBar()->addMenu( i18n("&File") );
   editMenu =      menuBar()->addMenu(i18n("&Edit") );
   directoryMenu = menuBar()->addMenu(i18n("&Directory") );
   dirCurrentItemMenu = 0;
   dirCurrentSyncItemMenu = 0;
   movementMenu = menuBar()->addMenu(i18n("&Movement") );
   diffMenu =     menuBar()->addMenu(i18n("D&iffview") );
   mergeMenu =    menuBar()->addMenu(i18n("&Merge") );
   windowsMenu =  menuBar()->addMenu(i18n("&Window") );
   settingsMenu = menuBar()->addMenu(i18n("&Settings") );
   helpMenu =     menuBar()->addMenu(i18n("&Help") );

   m_pToolBar = new KToolBar(this);
      
   memberList = new QList<KMainWindow*>;
   memberList->append(this);
}

KToolBar* KMainWindow::toolBar(const QString&)
{
   return m_pToolBar;
}

KActionCollection* KMainWindow::actionCollection()
{
   return &m_actionCollection;
}

void KMainWindow::createGUI()
{
   KStdAction::help(this, SLOT(slotHelp()), actionCollection());
   KStdAction::about(this, SLOT(slotAbout()), actionCollection());
   KStdAction::aboutQt(actionCollection());
}

void KMainWindow::slotAbout()
{
   QDialog d;
   QVBoxLayout* l = new QVBoxLayout( &d );
   QTabWidget* pTabWidget = new QTabWidget;
   l->addWidget( pTabWidget );

   QPushButton* pOkButton = new QPushButton(i18n("Ok"));
   connect( pOkButton, SIGNAL(clicked()), &d, SLOT(accept()));
   l->addWidget( pOkButton );

   d.setWindowTitle("About " + s_appName);
   QTextBrowser* tb1 = new QTextBrowser();
   tb1->setWordWrapMode( QTextOption::NoWrap );
   tb1->setText(
      s_appName + " Version " + s_version +
      "\n\n" + s_description + 
      "\n\n" + s_copyright +
      "\n\nHomepage: " + s_homepage +
      "\n\nLicence: GNU GPL Version 2"
      );
   pTabWidget->addTab(tb1,i18n("&About"));
      
   std::list<KAboutData::AboutDataEntry>::iterator i;
   
   QString s2;   
   for( i=s_pAboutData->m_authorList.begin(); i!=s_pAboutData->m_authorList.end(); ++i )
   {
      if ( !i->m_name.isEmpty() )    s2 +=         i->m_name    + "\n";
      if ( !i->m_task.isEmpty() )    s2 += "   " + i->m_task    + "\n";
      if ( !i->m_email.isEmpty() )   s2 += "   " + i->m_email   + "\n";
      if ( !i->m_weblink.isEmpty() ) s2 += "   " + i->m_weblink + "\n";
      s2 += "\n";
   }
   QTextBrowser* tb2 = new QTextBrowser();
   tb2->setWordWrapMode( QTextOption::NoWrap );
   tb2->setText(s2);
   pTabWidget->addTab(tb2,i18n("A&uthor"));
   
   QString s3;
   for( i=s_pAboutData->m_creditList.begin(); i!=s_pAboutData->m_creditList.end(); ++i )
   {
      if ( !i->m_name.isEmpty() )    s3 +=         i->m_name    + "\n";
      if ( !i->m_task.isEmpty() )    s3 += "   " + i->m_task    + "\n";
      if ( !i->m_email.isEmpty() )   s3 += "   " + i->m_email   + "\n";
      if ( !i->m_weblink.isEmpty() ) s3 += "   " + i->m_weblink + "\n";
      s3 += "\n";
   }
   QTextBrowser* tb3 = new QTextBrowser();
   tb3->setWordWrapMode( QTextOption::NoWrap );
   tb3->setText(s3);
   pTabWidget->addTab(tb3,i18n("&Thanks To"));
   
   d.resize(400,300);
   d.exec();
/*
   QMessageBox::information(
      this,
      "About " + s_appName,
      s_appName + " Version " + s_version +
      "\n\n" + s_description + 
      "\n\n" + s_copyright +
      "\n\nHomepage: " + s_homepage +
      "\n\nLicence: GNU GPL Version 2"
      );
*/
}

void KMainWindow::slotHelp()
{
   showHelp();
}


QString KStandardDirs::findResource(const QString& resource, const QString& /*appName*/)
{
   if (resource=="config")
   {
      QString home = QDir::homePath();
      return home + "/.kdiff3rc";
   }
   return QString();
}

KConfig::KConfig()
{
}

void KConfig::readConfigFile( const QString& configFileName )
{
   if ( !configFileName.isEmpty() )
   {
      m_fileName = configFileName;
   }
   else
   {
      m_fileName = KStandardDirs().findResource("config","kdiff3rc");
   }

   QFile f( m_fileName );
   if ( f.open(QIODevice::ReadOnly) )
   {                               // file opened successfully
      QTextStream t( &f );         // use a text stream
      load(t);
      f.close();
   }
}

KConfig::~KConfig()
{
   QFile f(m_fileName);
   if ( f.open( QIODevice::WriteOnly | QIODevice::Text ) )
   {                               // file opened successfully
       QTextStream t( &f );        // use a text stream
       save(t);
       f.close();
   }
}

void KConfig::setGroup(const QString&)
{
}

void KAction::init(QObject* receiver, const char* slot, KActionCollection* actionCollection, 
                   const char* name, bool bToggle, bool bMenu)
{
   QString n(name);
   KMainWindow* p = actionCollection->m_pMainWindow;
   if( slot!=0 )
   {
      if (!bToggle)
         connect(this, SIGNAL(triggered()), receiver, slot);
      else
      {
         connect(this, SIGNAL(toggled(bool)), receiver, slot);
      }
   }

   if (bMenu)
   {
      if( n[0]=='g')       p->movementMenu->addAction( this );
      else if( n.left(16)=="dir_current_sync")
      {
         if ( p->dirCurrentItemMenu==0 )
         {
            p->dirCurrentItemMenu = p->directoryMenu->addMenu( i18n("Current Item Merge Operation") );
            p->dirCurrentSyncItemMenu = p->directoryMenu->addMenu( i18n("Current Item Sync Operation") );
         }
         p->dirCurrentItemMenu->addAction( this );
      }
      else if( n.left(11)=="dir_current")
      {
         if ( p->dirCurrentItemMenu==0 )
         {
            p->dirCurrentItemMenu = p->directoryMenu->addMenu( i18n("Current Item Merge Operation") );
            p->dirCurrentSyncItemMenu = p->directoryMenu->addMenu( i18n("Current Item Sync Operation") );
         }
         p->dirCurrentSyncItemMenu->addAction( this );
      }
      else if( n.left(4)=="diff")  p->diffMenu->addAction( this );
      else if( name[0]=='d')  p->directoryMenu->addAction( this );
      else if( name[0]=='f')  p->fileMenu->addAction( this );
      else if( name[0]=='w')  p->windowsMenu->addAction( this );
      else                    p->mergeMenu->addAction( this );
   }
}


KAction::KAction(const QString& text, const QIcon& icon, int accel,
 QObject* receiver, const char* slot, KActionCollection* actionCollection,
 const char* name, bool bToggle, bool bMenu
 )
: QAction ( icon, text, actionCollection->m_pMainWindow )
{
   setObjectName(name);
   setShortcut( accel );
   setCheckable( bToggle );
   KMainWindow* p = actionCollection->m_pMainWindow;
   if ( !icon.isNull() && p ) p->m_pToolBar->addAction( this );

   init(receiver,slot,actionCollection,name,bToggle,bMenu);
}

KAction::KAction(const QString& text, int accel,
 QObject* receiver, const char* slot, KActionCollection* actionCollection,
 const char* name, bool bToggle, bool bMenu
 )
: QAction ( text, actionCollection->m_pMainWindow )
{
   setObjectName(name);
   setShortcut( accel );
   setCheckable( bToggle );
   init(receiver,slot,actionCollection,name,bToggle,bMenu);
}

void KAction::setStatusText(const QString&)
{
}

void KAction::plug(QMenu* menu)
{
   menu->addAction( this );
}


KToggleAction::KToggleAction(const QString& text, const QIcon& icon, int accel, QObject* receiver, const char* slot, KActionCollection* actionCollection, const char* name, bool bMenu)
: KAction( text, icon, accel, receiver, slot, actionCollection, name, true, bMenu)
{
}

KToggleAction::KToggleAction(const QString& text, int accel, QObject* receiver, const char* slot, KActionCollection* actionCollection, const char* name, bool bMenu)
: KAction( text, accel, receiver, slot, actionCollection, name, true, bMenu)
{
}

KToggleAction::KToggleAction(const QString& text, const QIcon& icon, int accel, KActionCollection* actionCollection, const char* name, bool bMenu)
: KAction( text, icon, accel, 0, 0, actionCollection, name, true, bMenu)
{
}

void KToggleAction::setChecked(bool bChecked)
{
   blockSignals( true );
   QAction::setChecked( bChecked );
   blockSignals( false );
}


//static
KAction* KStdAction::open( QWidget* parent, const char* slot, KActionCollection* actionCollection)
{
   #include "../xpm/fileopen.xpm"
   KMainWindow* p = actionCollection->m_pMainWindow;
   KAction* a = new KAction( i18n("Open"), QIcon(QPixmap(fileopen)), Qt::CTRL+Qt::Key_O, parent, slot, actionCollection, "open", false, false);
   if(p){ p->fileMenu->addAction( a ); }
   return a;
}

KAction* KStdAction::save( QWidget* parent, const char* slot, KActionCollection* actionCollection )
{
   #include "../xpm/filesave.xpm"
   KMainWindow* p = actionCollection->m_pMainWindow;
   KAction* a = new KAction( i18n("Save"), QIcon(QPixmap(filesave)), Qt::CTRL+Qt::Key_S, parent, slot, actionCollection, "save", false, false);
   if(p){ p->fileMenu->addAction( a ); }
   return a;
}

KAction* KStdAction::saveAs( QWidget* parent, const char* slot, KActionCollection* actionCollection)
{
   KMainWindow* p = actionCollection->m_pMainWindow;
   KAction* a = new KAction( i18n("Save As..."), 0, parent, slot, actionCollection, "saveas", false, false);
   if(p) p->fileMenu->addAction( a );
   return a;
}

KAction* KStdAction::print( QWidget* parent, const char* slot, KActionCollection* actionCollection)
{
   #include "../xpm/fileprint.xpm"
   KMainWindow* p = actionCollection->m_pMainWindow;
   KAction* a = new KAction( i18n("Print..."), QIcon(QPixmap(fileprint)),Qt::CTRL+Qt::Key_P, parent, slot, actionCollection, "print", false, false);
   if(p) p->fileMenu->addAction( a );
   return a;
}

KAction* KStdAction::quit( QWidget* parent, const char* slot, KActionCollection* actionCollection)
{
   KMainWindow* p = actionCollection->m_pMainWindow;
   KAction* a = new KAction( i18n("Quit"), Qt::CTRL+Qt::Key_Q, parent, slot, actionCollection, "quit", false, false);
   if(p) p->fileMenu->addAction( a );
   return a;
}

KAction* KStdAction::cut( QWidget* parent, const char* slot, KActionCollection* actionCollection)
{
   KMainWindow* p = actionCollection->m_pMainWindow;
   KAction* a = new KAction( i18n("Cut"), Qt::CTRL+Qt::Key_X, parent, slot, actionCollection, "cut", false, false );
   if(p) p->editMenu->addAction( a );
   return a;
}

KAction* KStdAction::copy( QWidget* parent, const char* slot, KActionCollection* actionCollection)
{
   KMainWindow* p = actionCollection->m_pMainWindow;
   KAction* a = new KAction( i18n("Copy"), Qt::CTRL+Qt::Key_C, parent, slot, actionCollection, "copy", false, false );
   if(p) p->editMenu->addAction( a );
   return a;
}

KAction* KStdAction::paste( QWidget* parent, const char* slot, KActionCollection* actionCollection)
{
   KMainWindow* p = actionCollection->m_pMainWindow;
   KAction* a = new KAction( i18n("Paste"), Qt::CTRL+Qt::Key_V, parent, slot, actionCollection, "paste", false, false );
   if(p) p->editMenu->addAction( a );
   return a;
}

KAction* KStdAction::selectAll( QWidget* parent, const char* slot, KActionCollection* actionCollection)
{
   KMainWindow* p = actionCollection->m_pMainWindow;
   KAction* a = new KAction( i18n("Select All"), Qt::CTRL+Qt::Key_A, parent, slot, actionCollection, "selectall", false, false );
   if(p) p->editMenu->addAction( a );
   return a;
}

KToggleAction* KStdAction::showToolbar( QWidget* parent, const char* slot, KActionCollection* actionCollection)
{
   KMainWindow* p = actionCollection->m_pMainWindow;
   KToggleAction* a = new KToggleAction( i18n("Show Toolbar"), 0, parent, slot, actionCollection, "showtoolbar", false );
   if(p) p->settingsMenu->addAction( a );
   return a;
}

KToggleAction* KStdAction::showStatusbar( QWidget* parent, const char* slot, KActionCollection* actionCollection)
{
   KMainWindow* p = actionCollection->m_pMainWindow;
   KToggleAction* a = new KToggleAction( i18n("Show &Statusbar"), 0, parent, slot, actionCollection, "showstatusbar", false );
   if(p) p->settingsMenu->addAction( a );
   return a;
}

KAction* KStdAction::preferences( QWidget* parent, const char* slot, KActionCollection* actionCollection)
{
   KMainWindow* p = actionCollection->m_pMainWindow;
   KAction* a = new KAction( i18n("&Configure %1...").arg("KDiff3"), 0, parent, slot, actionCollection, "settings", false, false );
   if(p) p->settingsMenu->addAction( a );
   return a;
}
KAction* KStdAction::keyBindings( QWidget*, const char*, KActionCollection*)
{
   return 0;
}

KAction* KStdAction::about( QWidget* parent, const char* slot, KActionCollection* actionCollection)
{
   KMainWindow* p = actionCollection->m_pMainWindow;
   KAction* a = new KAction( i18n("About")+" KDiff3", 0, parent, slot, actionCollection, "about_kdiff3", false, false );
   if(p) p->helpMenu->addAction( a );
   return a;
}

KAction* KStdAction::aboutQt( KActionCollection* actionCollection )
{
   KMainWindow* p = actionCollection->m_pMainWindow;
   KAction* a = new KAction( i18n("About")+" Qt", 0, qApp, SLOT(aboutQt()), actionCollection, "about_qt", false, false );
   if(p) p->helpMenu->addAction( a );
   return a;
}

KAction* KStdAction::help( QWidget* parent, const char* slot, KActionCollection* actionCollection)
{
   KMainWindow* p = actionCollection->m_pMainWindow;
   KAction* a = new KAction( i18n("Help"), Qt::Key_F1, parent, slot, actionCollection, "help", false, false );
   if(p) p->helpMenu->addAction( a );
   return a;
}
KAction* KStdAction::find( QWidget* parent, const char* slot, KActionCollection* actionCollection)
{
   KMainWindow* p = actionCollection->m_pMainWindow;
   KAction* a = new KAction( i18n("Find"), Qt::CTRL+Qt::Key_F, parent, slot, actionCollection, "find", false, false );
   if(p) p->editMenu->addAction( a );
   return a;
}

KAction* KStdAction::findNext( QWidget* parent, const char* slot, KActionCollection* actionCollection)
{
   KMainWindow* p = actionCollection->m_pMainWindow;
   KAction* a = new KAction( i18n("Find Next"), Qt::Key_F3, parent, slot, actionCollection, "findNext", false, false );
   if(p) p->editMenu->addAction( a );
   return a;
}




KFontChooser::KFontChooser( QWidget* pParent, const QString& /*name*/, bool, const QStringList&, bool, int )
: QWidget(pParent)
{
   m_pParent = pParent;
   QVBoxLayout* pLayout = new QVBoxLayout( this );
   m_pSelectFont = new QPushButton(i18n("Select Font"), this );
   connect(m_pSelectFont, SIGNAL(clicked()), this, SLOT(slotSelectFont()));
   pLayout->addWidget(m_pSelectFont);

   m_pLabel = new QLabel( "", this );
   m_pLabel->setFont( m_font );
   m_pLabel->setMinimumWidth(200);
   m_pLabel->setText( "The quick brown fox jumps over the river\n"
                      "but the little red hen escapes with a shiver.\n"
                      ":-)");
   pLayout->addWidget(m_pLabel);
}

QFont KFontChooser::font()
{
   return m_font;//QFont("courier",10);
}

void KFontChooser::setFont( const QFont& font, bool )
{
   m_font = font;
   m_pLabel->setFont( m_font );
   //update();
}

void KFontChooser::slotSelectFont()
{
   for(;;)
   {
      bool bOk;
      m_font = QFontDialog::getFont(&bOk, m_font );
      m_pLabel->setFont( m_font );   
      QFontMetrics fm(m_font);

      // Variable width font.
      if ( fm.width('W')!=fm.width('i') )
      {
         int result = KMessageBox::warningYesNo(m_pParent, i18n(
            "You selected a variable width font.\n\n"
            "Because this program doesn't handle variable width fonts\n"
            "correctly, you might experience problems while editing.\n\n"
            "Do you want to continue or do you want to select another font."),
            i18n("Incompatible font."),
            i18n("Continue at my own risk"), i18n("Select another font"));
         if (result==KMessageBox::Yes)
            return;
      }
      else
         return;
   }
}


KColorButton::KColorButton(QWidget* parent)
: QPushButton(parent)
{
   connect( this, SIGNAL(clicked()), this, SLOT(slotClicked()));
}

QColor KColorButton::color()
{
   return m_color;
}

void KColorButton::setColor( const QColor& color )
{
   m_color = color;
   update();
}

void KColorButton::paintEvent( QPaintEvent* e )
{
   QPushButton::paintEvent(e);
   QPainter p(this);

   int w = width();
   int h = height();
   p.fillRect( 10, 5, w-20, h-10, m_color );
   p.drawRect( 10, 5, w-20, h-10 );
}

void KColorButton::slotClicked()
{
   // Under Windows ChooseColor() should be used. (Nicer if few colors exist.)
   QColor c = QColorDialog::getColor ( m_color, this );
   if ( c.isValid() ) m_color = c;
   update();
}

KPrinter::KPrinter()
{
}
QList<int> KPrinter::pageList()
{
   QList<int> vl;
   int to = toPage();
   for(int i=fromPage(); i<=to; ++i)
   {
      vl.push_back(i);
   }
   return vl;
}
void KPrinter::setCurrentPage(int)
{
}
void KPrinter::setPageSelection(e_PageSelection)
{
}


QPixmap KIconLoader::loadIcon( const QString&, int )
{
   return QPixmap();
}

KAboutData::KAboutData( const QString& /*name*/, const QString& appName, const QString& version,
      const QString& description, int,
      const QString& copyright, int, const QString& homepage, const QString& email)
{
   s_copyright = copyright;
   s_email = email;
   s_appName = appName;
   s_description = description;
   s_version = version;
   s_homepage = homepage;
}

KAboutData::KAboutData( const QString& /*name*/, const QString& /*appName*/, const QString& /*version*/ )
{
}

void KAboutData::addAuthor(const char* name, const char* task, const char* email, const char* weblink)
{
   m_authorList.push_back( AboutDataEntry( name, task, email, weblink) );
}

void KAboutData::addCredit(const char* name, const char* task, const char* email, const char* weblink)
{
   m_creditList.push_back( AboutDataEntry( name, task, email, weblink) );
}

/*  Option structure: e.g.:
  { "m", 0, 0 },
  { "merge", I18N_NOOP("Automatically merge the input."), 0 },
  { "o", 0, 0 },
  { "output file", I18N_NOOP("Output file. Implies -m. E.g.: -o newfile.txt"), 0 },
  { "+[File1]", I18N_NOOP("file1 to open (base)"), 0 },
  { "+[File2]", I18N_NOOP("file2 to open"), 0 },
  { "+[File3]", I18N_NOOP("file3 to open"), 0 },
*/
////////////////
static KCmdLineArgs s_cmdLineArgs;
static int s_argc;
static char** s_argv;
static KCmdLineOptions* s_pOptions;

static std::vector<QCStringList> s_vOption;
static std::vector<const char*> s_vArg;

KCmdLineArgs* KCmdLineArgs::parsedArgs()  // static
{
   return &s_cmdLineArgs;
}

void KCmdLineArgs::init( int argc, char**argv, KAboutData* pAboutData )  // static
{
   s_argc = argc;
   s_argv = argv;
   s_pAboutData = pAboutData;
}

void KCmdLineArgs::addCmdLineOptions( KCmdLineOptions* options ) // static
{
   s_pOptions = options;
}

int KCmdLineArgs::count()
{
   return s_vArg.size();
}

QString KCmdLineArgs::arg(int idx)
{
   return QString::fromLocal8Bit( s_vArg[idx] );
}

void KCmdLineArgs::clear()
{
}

QString KCmdLineArgs::getOption( const QString& s )
{
   // Find the option
   int j=0;
   for( j=0; j<(int)s_vOption.size(); ++j )
   {
      const char* optName = s_pOptions[j].name;
      const char* pos = strchr( optName,' ' );
      int len = pos==0 ? strlen( optName ) : pos - optName;

      if( s == QString::fromLatin1( optName, len ) )
      {
         return s_vOption[j].isEmpty() ? QString() : s_vOption[j].last();
      }
   }
   assert(false);
   return QString();
}

QCStringList KCmdLineArgs::getOptionList( const QString& s )
{   
   // Find the option
   int j=0;
   for( j=0; j<(int)s_vOption.size(); ++j )
   {
      const char* optName = s_pOptions[j].name;
      const char* pos = strchr( optName,' ' );
      int len = pos==0 ? strlen( optName ) : pos - optName;

      if( s == QString::fromLatin1( optName, len) )
      {
         return s_vOption[j];
      }
   }

   assert(false);
   return QCStringList();
}

bool KCmdLineArgs::isSet(const QString& s)
{
   // Find the option
   int j=0;
   for( j=0; j<(int)s_vOption.size(); ++j )
   {
      const char* optName = s_pOptions[j].name;
      if( s == QString( optName ) )
      {
         return ! s_vOption[j].isEmpty();
      }
   }
   assert(false);
   return false;
}

///////////////////
KApplication* kapp;

KApplication::KApplication()
: QApplication( s_argc,s_argv )
{
   kapp = this;

   int nofOptions=0;
   int nofArgs=0;
   int i=0;
   while( s_pOptions[i].name != 0 )
   {
      if ( s_pOptions[i].name[0]=='[' )
         nofArgs++;
      else
         nofOptions++;

      ++i;
   }

   // First find the option "-config" or "--config" to allow loading of options
   QString configFileName;
   for( i=1; i<s_argc-1; ++i )
   {
      QString arg = s_argv[i];
      if ( arg == "-config" || arg == "--config" )
      {
         configFileName = s_argv[i+1];
      }
   }
   m_config.readConfigFile(configFileName);

   QStringList ignorableCmdLineOptionsList = m_config.readListEntry("IgnorableCmdLineOptions", QStringList("-u;-query;-html;-abort"), '|');
   QString ignorableCmdLineOptions;
   if ( !ignorableCmdLineOptionsList.isEmpty() ) 
      ignorableCmdLineOptions = ignorableCmdLineOptionsList.front() + ";";

   s_vOption.resize(nofOptions);

   for( i=1; i<s_argc; ++i )
   {
      if ( s_argv[i][0]=='-' )  // An option
      {
         if ( ignorableCmdLineOptions.contains(QString(s_argv[i])+";") )
            continue;
         // Find the option
         int j=0;
         for( j=0; j<nofOptions; ++j )
         {
            const char* optName = s_pOptions[j].name;
            const char* pos = strchr( optName,' ' );
            int len = pos==0 ? strlen( optName ) : pos - optName;
            int len2 = strlen(s_argv[i]);

            if( len>0 && ( s_argv[i][1]=='-' && len2-2==len && memcmp( &s_argv[i][2], optName, len )==0  ||
                                                len2-1==len && memcmp( &s_argv[i][1], optName, len )==0  ))
            {
               if (s_pOptions[j].description == 0)  // alias, because without description.
               {
                  ++j;
                  optName = s_pOptions[j].name;
                  pos = strchr( optName,' ' );
               }
               if (pos!=0){ ++i; s_vOption[j].append(s_argv[i]); } //use param
               else       { s_vOption[j].append("1"); }           //set state
               break;
            }
         }
         if (j==nofOptions)
         {
            QString s;
            s = QString("Unknown option: ") +  QString(s_argv[i]) + "\n";
            s += "If KDiff3 should ignore this option, run KDiff3 normally and edit\n"
                 "the \"Command line options to ignore\" in the \"Integration Settings\".\n\n";

            s += "KDiff3-Usage when starting via commandline: \n";
            s += "- Comparing 2 files:\t\tkdiff3 file1 file2\n";
            s += "- Merging 2 files:  \t\tkdiff3 file1 file2 -o outputfile\n";
            s += "- Comparing 3 files:\t\tkdiff3 file1 file2 file3\n";
            s += "- Merging 3 files:  \t\tkdiff3 file1 file2 file3 -o outputfile\n";
            s += "     Note that file1 will be treated as base of file2 and file3.\n";
            s += "\n";
            s += "If you start without arguments, then a dialog will appear\n";
            s += "where you can select your files via a filebrowser.\n";
            s += "\n";

            s += "Options:\n";

            j=0;
            int pos=s.length();
            for( j=0; j<nofOptions; ++j )
            {
               if ( s_pOptions[j].description!=0 )
               {
                  if (s_pOptions[j].name[0]!='+')
                  {
                     s += "-";
                     if ( strlen(s_pOptions[j].name)>1 ) s += "-";
                  }
                  s += s_pOptions[j].name;
                  s += QString().fill(' ', minMaxLimiter( 20 - ((int)s.length()-pos), 3, 20 ) );
                  s += s_pOptions[j].description;
                  s +="\n";
                  pos=s.length();
               }
               else
               {
                  s += "-";
                  if ( strlen(s_pOptions[j].name)>1 ) s += "-";
                  s += s_pOptions[j].name;
                  s += ", ";
               }
            }

            s += "\n"+i18n("For more documentation, see the help-menu or the subdirectory doc.")+"\n";
#ifdef _WIN32
            // A windows program has no console
            if ( 0==QMessageBox::information(0, i18n("KDiff3-Usage"), s, i18n("Ignore"),i18n("Exit") ) )
               continue;
#else
            std::cerr << s.toLatin1().constData() << std::endl;
#endif

            ::exit(-1);
         }
      }
      else
         s_vArg.push_back( s_argv[i] );
   }
}

KConfig* KApplication::config()
{
   return &m_config;
}

bool KApplication::isRestored()
{
   return false;
}

KApplication* KApplication::kApplication()
{
   return kapp;
}

KIconLoader* KApplication::iconLoader()
{
   return &m_iconLoader;
}


namespace KIO
{
   SimpleJob* mkdir( KURL ){return 0;}
   SimpleJob* rmdir( KURL ){return 0;}
   SimpleJob* file_delete( KURL, bool ){return 0;}
   FileCopyJob* file_move(  KURL, KURL, int, bool, bool, bool ) {return 0;}
   FileCopyJob* file_copy(  KURL, KURL, int, bool, bool, bool ) {return 0;}
   CopyJob* link(  KURL, KURL, bool ) {return 0;}
   ListJob* listRecursive( KURL, bool, bool ){return 0;}
   ListJob* listDir( KURL, bool, bool ){return 0;}
   StatJob* stat( KURL, bool, int, bool ){return 0;}
   TransferJob* get( KURL, bool, bool ){return (TransferJob*)0;}
   TransferJob* put( KURL, int, bool, bool, bool ){return (TransferJob*)0;}
};

KActionCollection* KParts::Part::actionCollection()
{
   return 0;
}

KApplication* KParts::Part::instance()
{
   return kapp;
}


KLibLoader* KLibLoader::self()
{
   static KLibLoader ll;
   return &ll;
}

extern "C" void* init_libkdiff3part();
KLibFactory* KLibLoader::factory(QString const&)
{
   return (KLibFactory*) init_libkdiff3part();
}

QObject* KLibFactory::create(QObject* pParent, const QString& name, const QString& classname )
{
   KParts::Factory* f = dynamic_cast<KParts::Factory*>(this);
   if (f!=0)
      return f->createPartObject( (QWidget*)pParent, name.toAscii(),
                                            pParent, name.toAscii(),
                                            classname.toAscii(),  QStringList() );
   else
      return 0;
}




#include "kreplacements.moc"
