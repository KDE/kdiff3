/***************************************************************************
                          kreplacements.cpp  -  description
                             -------------------
    begin                : Sat Aug 3 2002
    copyright            : (C) 2002-2003 by Joachim Eibl
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
 * Revision 1.1  2003/10/06 18:48:54  joachim99
 * KDiff3 version 0.9.70
 *
 ***************************************************************************/

#include "kreplacements.h"

#include <assert.h>

#include <qnamespace.h>
#include <qmessagebox.h>
#include <qpopupmenu.h>
#include <qmenubar.h>
#include <qpainter.h>
#include <qcolordialog.h>
#include <qfontdialog.h>
#include <qlabel.h>
#include <qtextbrowser.h>
#include <qtextstream.h>
#include <qlayout.h>

#include <vector>
#include <iostream>
#include <algorithm>

static QString s_copyright;
static QString s_email;
static QString s_description;
static QString s_appName;
static QString s_version;
static QString s_homepage;

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
      if ( ! helpFile.exists() )
      {
         helpFile.setFile( exePath + "\\..\\doc\\en\\index.html" );
         if ( ! helpFile.exists() )
         {
            QMessageBox::warning( 0, "KDiff3 Helpfile not found", 
               "Couldn't find the helpfile at: \n\n    "
               + helpFile.absFilePath() + "\n\n"
               "The documentation can also be found at the homepage:\n\n " 
               "    http://kdiff3.sourceforge.net/");   
            return;
         }
      }

      HINSTANCE hi = FindExecutableA( helpFile.fileName(), helpFile.dirPath(true), buf );
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
         _spawnlp( _P_NOWAIT , prog.filePath(), prog.fileName(), (const char*)helpFile.absFilePath(), NULL );
      }

   #else
      static QTextBrowser* pBrowser = 0;
      if (pBrowser==0)
      {
         pBrowser = new QTextBrowser( 0 );
         pBrowser->setMinimumSize( 600, 400 );
      }
      pBrowser->setSource("/usr/local/share/doc/kdiff3/en/index.html");
      pBrowser->show();
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
   return  0 == QMessageBox::warning( parent, caption, text, button1, button2 ) ? Yes : No;
}

int KMessageBox::warningYesNoCancel( QWidget* parent, const QString& text, const QString& caption,
   const QString& button1, const QString& button2 )
{
   int val = QMessageBox::warning( parent, caption, text, 
      button1, button2, "Cancel" );
   if ( val==0 ) return Yes;
   if ( val==1 ) return No;
   else return Cancel;
}


KDialogBase::KDialogBase( int, const QString& caption, int, int, QWidget* parent, const QString& name,
  bool /*modal*/, bool )
: QTabDialog( parent, name, true /* modal */ )
{
   setCaption( caption );
   setDefaultButton();
   setHelpButton();
   setCancelButton();
   //setApplyButton();
   setOkButton();
   setDefaultButton();

   connect( this, SIGNAL( defaultButtonPressed() ), this, SLOT(slotDefault()) );
   connect( this, SIGNAL( helpButtonPressed() ), this, SLOT(slotHelp()));
   connect( this, SIGNAL( applyButtonPressed() ), this, SLOT( slotApply() ));
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


QVBox* KDialogBase::addVBoxPage( const QString& name, const QString& /*info*/, int )
{
   QVBox* p = new QVBox(this, name);
   addTab( p, name );
   return p;
}

QFrame* KDialogBase::addPage(  const QString& name, const QString& /*info*/, int )
{
   QFrame* p = new QFrame( this, name );
   addTab( p, name );
   return p;
}

int KDialogBase::spacingHint()
{
   return 5;
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
         QTabDialog::accept();
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
   QString s = QFileDialog::getSaveFileName(startDir, filter, parent, caption);
   return KURL(s);
}

KURL KFileDialog::getOpenURL( const QString &  startDir,
                           const QString &  filter,
                           QWidget *  parent,
                           const QString &  caption )
{
   QString s = QFileDialog::getOpenFileName(startDir, filter, parent, caption);
   return KURL(s);
}

KURL KFileDialog::getExistingURL( const QString &  startDir,
                               QWidget *  parent,
                               const QString &  caption)
{
   QString s = QFileDialog::getExistingDirectory(startDir, parent, caption);
   return KURL(s);
}


KToolBar::BarPosition KToolBar::barPos()
{
   return Top;
}

void KToolBar::setBarPos(BarPosition)
{
}

KToolBar::KToolBar( QMainWindow* parent )
: QToolBar( parent )
{
}


KMainWindow::KMainWindow( QWidget* parent, const QString& name )
: QMainWindow( parent, name ), m_actionCollection(this)
{
   fileMenu = new QPopupMenu();
   menuBar()->insertItem(tr("&File"), fileMenu);
   editMenu = new QPopupMenu();
   menuBar()->insertItem(tr("&Edit"), editMenu);
   directoryMenu = new QPopupMenu();
   menuBar()->insertItem(tr("&Directory"), directoryMenu);
   movementMenu = new QPopupMenu();
   menuBar()->insertItem(tr("&Movement"), movementMenu);
   mergeMenu = new QPopupMenu();
   menuBar()->insertItem(tr("&Merge"), mergeMenu);
   windowsMenu = new QPopupMenu();
   menuBar()->insertItem(tr("&Windows"), windowsMenu);
   settingsMenu = new QPopupMenu();
   menuBar()->insertItem(tr("&Settings"), settingsMenu);
   helpMenu = new QPopupMenu();
   menuBar()->insertItem(tr("&Help"), helpMenu);

   m_pToolBar = new KToolBar(this);
      
   memberList = new QList<KMainWindow>;
   memberList->append(this);
   connect( qApp, SIGNAL(lastWindowClosed()), this, SLOT(quit())); 
}

void KMainWindow::closeEvent(QCloseEvent*e)
{
   if ( queryClose() )
   {
      e->accept();
   }
   else
      e->ignore();
}

bool KMainWindow::event( QEvent* e )
{
   return QMainWindow::event(e);
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
}

void KMainWindow::quit()
{
   if ( queryExit() )
   {
      qApp->quit();
   }
}

void KMainWindow::slotAbout()
{
   QMessageBox::information(
      this,
      "About " + s_appName,
      s_appName + " Version " + s_version +
      "\n\n" + s_description + 
      "\n\n" + s_copyright +
      "\n\nHomepage: " + s_homepage +
      "\n\nLicence: GNU GPL Version 2"
      );
}

void KMainWindow::slotHelp()
{
   showHelp();
}

KConfig::KConfig()
{
   QString home = QDir::homeDirPath();
   m_fileName = home + "/.kdiff3rc";

   QFile f( m_fileName );
   if ( f.open(IO_ReadOnly) )
   {                               // file opened successfully
      QTextStream t( &f );        // use a text stream
      while ( !t.eof() )
      {                                 // until end of file...	   
         QString s = t.readLine();         // line of text excluding '\n'
         int pos = s.find('=');
         if( pos > 0 )                     // seems not to have a tag
         {
            QString key = s.left(pos);
            QString val = s.mid(pos+1);
            m_map[key] = val;
         }	   
      }
      f.close();
   }
}

KConfig::~KConfig()
{
   QFile f(m_fileName);
   if ( f.open( IO_WriteOnly | IO_Translate ) )
   {                               // file opened successfully
       QTextStream t( &f );        // use a text stream
       std::map<QString,QString>::iterator i;
       for( i=m_map.begin(); i!=m_map.end(); ++i)
       {
          QString key = i->first;
          QString val = i->second;
          t << key << "=" << val << "\n";
       }
       f.close();
   }
}

void KConfig::setGroup(const QString&)
{
}

static QString numStr(int n)
{
   QString s;
   s.setNum( n );
   return s;
}

static QString subSection( const QString& s, int idx, char sep )
{
   int pos=0;
   while( idx>0 )
   {
      pos = s.find( sep, pos );
      --idx;
      if (pos<0) break;
      ++pos;
   }
   if ( pos>=0 )
   {
      int pos2 = s.find( sep, pos );
      if ( pos2>0 )
         return s.mid(pos, pos2-pos);
      else
         return s.mid(pos);
   }

   return "";
}

static int num( QString& s, int idx )
{
   return subSection( s, idx, ',').toInt();

   //return s.section(',', idx, idx).toInt();
}

void KConfig::writeEntry(const QString& k, const QFont& v )
{
   //m_map[k] = v.toString();
   m_map[k] = v.family() + "," + QString::number(v.pointSize());
}

void KConfig::writeEntry(const QString& k, const QColor& v )
{
   m_map[k] = numStr(v.red()) + "," + numStr(v.green()) + "," + numStr(v.blue());
}

void KConfig::writeEntry(const QString& k, const QSize& v )
{
   m_map[k] = numStr(v.width()) + "," + numStr(v.height());
}

void KConfig::writeEntry(const QString& k, const QPoint& v )
{
   m_map[k] = numStr(v.x()) + "," + numStr(v.y());
}

void KConfig::writeEntry(const QString& k, int v )
{
   m_map[k] = numStr(v);
}

void KConfig::writeEntry(const QString& k, bool v )
{
   m_map[k] = numStr(v);
}

void KConfig::writeEntry(const QString& k, const QString& v )
{
   m_map[k] = v;
}

void KConfig::writeEntry(const QString& k, const QStringList& v, char separator )
{
   QString s;

   QStringList::ConstIterator i = v.begin();
   for( i=v.begin(); i!= v.end(); ++i )
   {
      s += *i;

      if ( !(*i).isEmpty() )
         s += separator;
   }

   m_map[k] = s;
}


QFont KConfig::readFontEntry(const QString& k, QFont* defaultVal )
{
   QFont f = *defaultVal;
   std::map<QString,QString>::iterator i = m_map.find( k );
   if ( i!=m_map.end() )
   {
      f.setFamily( subSection( i->second, 0, ',' ) );
      f.setPointSize( subSection( i->second, 1, ',' ).toInt() );
      //f.fromString(i->second);
   }

   return f;
}

QColor KConfig::readColorEntry(const QString& k, QColor* defaultVal )
{
   QColor c= *defaultVal;
   std::map<QString,QString>::iterator i = m_map.find( k );
   if ( i!=m_map.end() )
   {
      QString s = i->second;
      c = QColor( num(s,0),num(s,1),num(s,2) );
   }

   return c;
}

QSize KConfig::readSizeEntry(const QString& k)
{
   QSize size(640,400);
   std::map<QString,QString>::iterator i = m_map.find( k );
   if ( i!=m_map.end() )
   {

      QString s = i->second;
      size = QSize( num(s,0),num(s,1) );
   }

   return size;
}

QPoint KConfig::readPointEntry(const QString& k)
{
   QPoint point(0,0);
   std::map<QString,QString>::iterator i = m_map.find( k );
   if ( i!=m_map.end() )
   {
      QString s = i->second;
      point = QPoint( num(s,0),num(s,1) );
   }

   return point;
}

bool KConfig::readBoolEntry(const QString& k, bool bDefault )
{
   bool b = bDefault;
   std::map<QString,QString>::iterator i = m_map.find( k );
   if ( i!=m_map.end() )
   {
      QString s = i->second;
      b = (bool)num(s,0);
   }

   return b;
}

int KConfig::readNumEntry(const QString& k, int iDefault )
{
   int ival = iDefault;
   std::map<QString,QString>::iterator i = m_map.find( k );
   if ( i!=m_map.end() )
   {
      QString s = i->second;
      ival = num(s,0);
   }

   return ival;
}

QString KConfig::readEntry(const QString& k, const QString& sDefault )
{
   QString sval = sDefault;
   std::map<QString,QString>::iterator i = m_map.find( k );
   if ( i!=m_map.end() )
   {
      sval = i->second;
   }

   return sval;
}

QStringList KConfig::readListEntry(const QString& k, char /*separator*/ )
{
   QStringList strList;

   std::map<QString,QString>::iterator i = m_map.find( k );
   if ( i!=m_map.end() )
   {
      QString s = i->second;
      int idx=0;
      for(;;)
      {
         QString sec = subSection( s, idx, '|' ); //s.section('|',idx,idx);
         if ( sec.isEmpty() )
            break;
         else
            strList.append(sec);
         ++idx;
      }
   }
   return strList;
}


KAction::KAction(const QString& text, const QIconSet& icon, int accel,
 QWidget* receiver, const char* slot, KActionCollection* actionCollection,
 const QString& name, bool bToggle, bool bMenu
 )
: QAction ( text, icon, text, accel, actionCollection->m_pMainWindow, name, bToggle )
{
   KMainWindow* p = actionCollection->m_pMainWindow;
   if( slot!=0 )
   {
      if (!bToggle)
         connect(this, SIGNAL(activated()), receiver, slot);
      else
      {
         connect(this, SIGNAL(toggled(bool)), receiver, slot);
      }
   }

   if ( !icon.isNull() && p ) this->addTo( p->m_pToolBar );

   if (bMenu)
   {
      if( name[0]=='g')       addTo( p->movementMenu );
      else if( name[0]=='d')  addTo( p->directoryMenu );
      else if( name[0]=='f')  addTo( p->fileMenu );
      else if( name[0]=='w')  addTo( p->windowsMenu );
      else                    addTo( p->mergeMenu );
   }
}

KAction::KAction(const QString& text, int accel,
 QWidget* receiver, const char* slot, KActionCollection* actionCollection,
 const QString& name, bool bToggle, bool bMenu
 )
: QAction ( text, text, accel, actionCollection->m_pMainWindow, name, bToggle )
{
   KMainWindow* p = actionCollection->m_pMainWindow;
   if( slot!=0 )
   {
      if (!bToggle)
         connect(this, SIGNAL(activated()), receiver, slot);
      else
      {
         connect(this, SIGNAL(toggled(bool)), receiver, slot);
      }
   }
   if (bMenu)
   {
      if( name[0]=='g') addTo( p->movementMenu );
      else if( name[0]=='d')  addTo( p->directoryMenu );
      else if( name[0]=='f')  addTo( p->fileMenu );
      else if( name[0]=='w')  addTo( p->windowsMenu );
      else              addTo( p->mergeMenu );
   }
}

void KAction::setStatusText(const QString&)
{
}

void KAction::plug(QPopupMenu* menu)
{
   addTo(menu);
}


KToggleAction::KToggleAction(const QString& text, const QIconSet& icon, int accel, QWidget* parent, const char* slot, KActionCollection* actionCollection, const QString& name, bool bMenu)
: KAction( text, icon, accel, parent, slot, actionCollection, name, true, bMenu)
{
}

KToggleAction::KToggleAction(const QString& text, int accel, QWidget* parent, const char* slot, KActionCollection* actionCollection, const QString& name, bool bMenu)
: KAction( text, accel, parent, slot, actionCollection, name, true, bMenu)
{
}

KToggleAction::KToggleAction(const QString& text, const QIconSet& icon, int accel, KActionCollection* actionCollection, const QString& name, bool bMenu)
: KAction( text, icon, accel, 0, 0, actionCollection, name, true, bMenu)
{
}

void KToggleAction::setChecked(bool bChecked)
{
   blockSignals( true );
   setOn( bChecked );
   blockSignals( false );
}

bool KToggleAction::isChecked()
{
   return isOn();
}



//static
KAction* KStdAction::open( QWidget* parent, const char* slot, KActionCollection* actionCollection)
{
   #include "../xpm/fileopen.xpm"
   KMainWindow* p = actionCollection->m_pMainWindow;
   KAction* a = new KAction( "Open", QIconSet(QPixmap(fileopen)), Qt::CTRL+Qt::Key_O, parent, slot, actionCollection, "open", false, false);
   if(p){ a->addTo( p->fileMenu ); }
   return a;
}

KAction* KStdAction::save( QWidget* parent, const char* slot, KActionCollection* actionCollection )
{
   #include "../xpm/filesave.xpm"
   KMainWindow* p = actionCollection->m_pMainWindow;
   KAction* a = new KAction( "Save", QIconSet(QPixmap(filesave)), Qt::CTRL+Qt::Key_S, parent, slot, actionCollection, "save", false, false);
   if(p){ a->addTo( p->fileMenu ); }
   return a;
}

KAction* KStdAction::saveAs( QWidget* parent, const char* slot, KActionCollection* actionCollection)
{
   KMainWindow* p = actionCollection->m_pMainWindow;
   KAction* a = new KAction( "SaveAs", 0, parent, slot, actionCollection, "saveas", false, false);
   if(p) a->addTo( p->fileMenu );
   return a;
}

KAction* KStdAction::quit( QWidget* parent, const char* slot, KActionCollection* actionCollection)
{
   KMainWindow* p = actionCollection->m_pMainWindow;
   KAction* a = new KAction( "Quit", Qt::CTRL+Qt::Key_Q, parent, slot, actionCollection, "quit", false, false);
   if(p) a->addTo( p->fileMenu );
   return a;
}

KAction* KStdAction::cut( QWidget* parent, const char* slot, KActionCollection* actionCollection)
{
   KMainWindow* p = actionCollection->m_pMainWindow;
   KAction* a = new KAction( "Cut", Qt::CTRL+Qt::Key_X, parent, slot, actionCollection, "cut", false, false );
   if(p) a->addTo( p->editMenu );
   return a;
}

KAction* KStdAction::copy( QWidget* parent, const char* slot, KActionCollection* actionCollection)
{
   KMainWindow* p = actionCollection->m_pMainWindow;
   KAction* a = new KAction( "Copy", Qt::CTRL+Qt::Key_C, parent, slot, actionCollection, "copy", false, false );
   if(p) a->addTo( p->editMenu );
   return a;
}

KAction* KStdAction::paste( QWidget* parent, const char* slot, KActionCollection* actionCollection)
{
   KMainWindow* p = actionCollection->m_pMainWindow;
   KAction* a = new KAction( "Paste", Qt::CTRL+Qt::Key_V, parent, slot, actionCollection, "paste", false, false );
   if(p) a->addTo( p->editMenu );
   return a;
}

KToggleAction* KStdAction::showToolbar( QWidget* parent, const char* slot, KActionCollection* actionCollection)
{
   KMainWindow* p = actionCollection->m_pMainWindow;
   KToggleAction* a = new KToggleAction( "ShowToolBar", 0, parent, slot, actionCollection, "showtoolbar", false );
   if(p) a->addTo( p->settingsMenu );
   return a;
}

KToggleAction* KStdAction::showStatusbar( QWidget* parent, const char* slot, KActionCollection* actionCollection)
{
   KMainWindow* p = actionCollection->m_pMainWindow;
   KToggleAction* a = new KToggleAction( "ShowStatusBar", 0, parent, slot, actionCollection, "showstatusbar", false );
   if(p) a->addTo( p->settingsMenu );
   return a;
}

KAction* KStdAction::preferences( QWidget* parent, const char* slot, KActionCollection* actionCollection)
{
   KMainWindow* p = actionCollection->m_pMainWindow;
   KAction* a = new KAction( "Settings ...", 0, parent, slot, actionCollection, "settings", false, false );
   if(p) a->addTo( p->settingsMenu );
   return a;
}
KAction* KStdAction::keyBindings( QWidget*, const char*, KActionCollection*)
{
   return 0;
}

KAction* KStdAction::about( QWidget* parent, const char* slot, KActionCollection* actionCollection)
{
   KMainWindow* p = actionCollection->m_pMainWindow;
   KAction* a = new KAction( "About", 0, parent, slot, actionCollection, "about", false, false );
   if(p) a->addTo( p->helpMenu );
   return a;
}

KAction* KStdAction::help( QWidget* parent, const char* slot, KActionCollection* actionCollection)
{
   KMainWindow* p = actionCollection->m_pMainWindow;
   KAction* a = new KAction( "Help", Qt::Key_F1, parent, slot, actionCollection, "help", false, false );
   if(p) a->addTo( p->helpMenu );
   return a;
}
KAction* KStdAction::find( QWidget* parent, const char* slot, KActionCollection* actionCollection)
{
   KMainWindow* p = actionCollection->m_pMainWindow;
   KAction* a = new KAction( "Find", Qt::CTRL+Qt::Key_F, parent, slot, actionCollection, "find", false, false );
   if(p) a->addTo( p->editMenu );
   return a;
}

KAction* KStdAction::findNext( QWidget* parent, const char* slot, KActionCollection* actionCollection)
{
   KMainWindow* p = actionCollection->m_pMainWindow;
   KAction* a = new KAction( "Find Next", Qt::Key_F3, parent, slot, actionCollection, "findNext", false, false );
   if(p) a->addTo( p->editMenu );
   return a;
}




KFontChooser::KFontChooser( QWidget* pParent, const QString& /*name*/, bool, const QStringList&, bool, int )
: QWidget(pParent)
{
   m_pParent = pParent;
   QVBoxLayout* pLayout = new QVBoxLayout( this );
   m_pSelectFont = new QPushButton("Select Font", this );
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

KAboutData::KAboutData( const QString& /*name*/, const QString& appName, const QString& version )
{
}

void KAboutData::addAuthor(const QString& /*name*/, int, const QString& /*email*/)
{
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

void KCmdLineArgs::init( int argc, char**argv, KAboutData* )  // static
{
   s_argc = argc;
   s_argv = argv;
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
   return QString(s_vArg[idx]);
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
      const char* optName = s_pOptions[j].shortName;
      const char* pos = strchr( optName,' ' );
      int len = pos==0 ? strlen( optName ) : pos - optName;

      if( s == (const char*)( QCString( optName, len+1) ) )
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
      const char* optName = s_pOptions[j].shortName;
      const char* pos = strchr( optName,' ' );
      int len = pos==0 ? strlen( optName ) : pos - optName;

      if( s == (const char*)( QCString( optName, len+1) ) )
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
      const char* optName = s_pOptions[j].shortName;
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
   while( s_pOptions[i].shortName != 0 )
   {
      if ( s_pOptions[i].shortName[0]=='[' )
         nofArgs++;
      else
         nofOptions++;

      ++i;
   }

   s_vOption.resize(nofOptions);

   for( i=1; i<s_argc; ++i )
   {
      if ( s_argv[i][0]=='-' )  // An option
      {
         // Find the option
         int j=0;
         for( j=0; j<nofOptions; ++j )
         {
            const char* optName = s_pOptions[j].shortName;
            const char* pos = strchr( optName,' ' );
            int len = pos==0 ? strlen( optName ) : pos - optName;

            if( len>0 && ( s_argv[i][1]=='-' && memcmp( &s_argv[i][2], optName, len )==0  ||
                                                memcmp( &s_argv[i][1], optName, len )==0  ))
            {
               if (s_pOptions[j].longName == 0)  // alias, because without description.
               {
                  ++j;
                  optName = s_pOptions[j].shortName;
                  pos = strchr( optName,' ' );
               }
               if (pos!=0){ ++i; s_vOption[j].append(s_argv[i]); } //use param
               else       { s_vOption[j].append("1"); }           //set state
               break;
            }
         }
         if (j==nofOptions)
         {
            using std::cerr;
            using std::endl;
            cerr << "Unknown option: " << s_argv[i] << endl<<endl;

            cerr << "Usage when starting via commandline: "                      << endl;
            cerr << "- Comparing 2 files:     kdiff3 file1 file2  "              << endl;
            cerr << "- Merging 2 files:       kdiff3 file1 file2 -o outputfile " << endl;
            cerr << "- Comparing 3 files:     kdiff3 file1 file2 file3         " << endl;
            cerr << "- Merging 3 files:       kdiff3 file1 file2 file3 -o outputfile " << endl;
            cerr << "     Note that file1 will be treated as base of file2 and file3." << endl;
            cerr << endl;
            cerr << "If you start without arguments, then a dialog will appear"        << endl;
            cerr << "where you can select your files via a filebrowser."               << endl;
            cerr << endl;
            cerr << "For more documentation, see the help-menu or the subdirectory doc. " << endl;
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

QObject* KLibFactory::create(QObject* pParent, QString const& name, QString const& classname )
{
   KParts::Factory* f = dynamic_cast<KParts::Factory*>(this);
   if (f!=0)
      return f->createPartObject( (QWidget*)pParent, name,
                                            pParent, name,
                                            classname,  QStringList() );
   else
      return 0;
}



#include "kreplacements.moc"
