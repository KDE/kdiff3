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


#include "kreplacements.h"
#include "common.h"

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
#include <qdockarea.h>

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

      HINSTANCE hi = FindExecutableA( helpFile.fileName().ascii(), helpFile.dirPath(true).ascii(), buf );
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
         _spawnlp( _P_NOWAIT , prog.filePath().ascii(), prog.fileName().ascii(), ("file:///"+helpFile.absFilePath()).ascii(), NULL );
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

QString getTranslationDir()
{
   #ifdef _WIN32
      char buf[200];
      int r= SearchPathA( 0, ".",  0, sizeof(buf), buf, 0 );

      QString exePath;
      if (r!=0)  {  exePath = buf; }
      else       {  exePath = "."; }
      return exePath;
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
   QVBox* p = new QVBox(this, name.ascii());
   addTab( p, name );
   return p;
}

QFrame* KDialogBase::addPage(  const QString& name, const QString& /*info*/, int )
{
   QFrame* p = new QFrame( this, name.ascii() );
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
   QString s = QFileDialog::getSaveFileName(startDir, filter, parent, 0, caption);
   return KURL(s);
}

KURL KFileDialog::getOpenURL( const QString &  startDir,
                           const QString &  filter,
                           QWidget *  parent,
                           const QString &  caption )
{
   QString s = QFileDialog::getOpenFileName(startDir, filter, parent, 0, caption);
   return KURL(s);
}

KURL KFileDialog::getExistingURL( const QString &  startDir,
                               QWidget *  parent,
                               const QString &  caption)
{
   QString s = QFileDialog::getExistingDirectory(startDir, parent, 0, caption);
   return KURL(s);
}


KToolBar::BarPosition KToolBar::barPos()
{
   if ( m_pMainWindow->leftDock()->hasDockWindow(this) ) return Left;
   if ( m_pMainWindow->rightDock()->hasDockWindow(this) ) return Right;
   if ( m_pMainWindow->topDock()->hasDockWindow(this) ) return Top;
   if ( m_pMainWindow->bottomDock()->hasDockWindow(this) ) return Bottom;
   return Top;
}

void KToolBar::setBarPos(BarPosition bp)
{
   if ( bp == Left ) m_pMainWindow->moveDockWindow( this, DockLeft );
   else if ( bp == Right ) m_pMainWindow->moveDockWindow( this, DockRight );
   else if ( bp == Bottom ) m_pMainWindow->moveDockWindow( this, DockBottom );
   else if ( bp == Top ) m_pMainWindow->moveDockWindow( this, DockTop );
}

KToolBar::KToolBar( QMainWindow* parent )
: QToolBar( parent )
{
   m_pMainWindow = parent;
}


KMainWindow::KMainWindow( QWidget* parent, const char* name )
: QMainWindow( parent, name ), m_actionCollection(this)
{
   fileMenu = new QPopupMenu();
   menuBar()->insertItem(i18n("&File"), fileMenu);
   editMenu = new QPopupMenu();
   menuBar()->insertItem(i18n("&Edit"), editMenu);
   directoryMenu = new QPopupMenu();
   menuBar()->insertItem(i18n("&Directory"), directoryMenu);
   dirCurrentItemMenu = 0;
   dirCurrentSyncItemMenu = 0;
   movementMenu = new QPopupMenu();
   menuBar()->insertItem(i18n("&Movement"), movementMenu);
   diffMenu = new QPopupMenu();
   menuBar()->insertItem(i18n("D&iffview"), diffMenu);
   mergeMenu = new QPopupMenu();
   menuBar()->insertItem(i18n("&Merge"), mergeMenu);
   windowsMenu = new QPopupMenu();
   menuBar()->insertItem(i18n("&Window"), windowsMenu);
   settingsMenu = new QPopupMenu();
   menuBar()->insertItem(i18n("&Settings"), settingsMenu);
   helpMenu = new QPopupMenu();
   menuBar()->insertItem(i18n("&Help"), helpMenu);

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
   QTabDialog d;
   d.setCaption("About " + s_appName);
   QTextBrowser* tb1 = new QTextBrowser(&d);
   tb1->setWordWrap( QTextEdit::NoWrap );
   tb1->setText(
      s_appName + " Version " + s_version +
      "\n\n" + s_description + 
      "\n\n" + s_copyright +
      "\n\nHomepage: " + s_homepage +
      "\n\nLicence: GNU GPL Version 2"
      );
   d.addTab(tb1,i18n("&About"));
      
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
   QTextBrowser* tb2 = new QTextBrowser(&d);
   tb2->setWordWrap( QTextEdit::NoWrap );
   tb2->setText(s2);
   d.addTab(tb2,i18n("A&uthor"));
   
   QString s3;
   for( i=s_pAboutData->m_creditList.begin(); i!=s_pAboutData->m_creditList.end(); ++i )
   {
      if ( !i->m_name.isEmpty() )    s3 +=         i->m_name    + "\n";
      if ( !i->m_task.isEmpty() )    s3 += "   " + i->m_task    + "\n";
      if ( !i->m_email.isEmpty() )   s3 += "   " + i->m_email   + "\n";
      if ( !i->m_weblink.isEmpty() ) s3 += "   " + i->m_weblink + "\n";
      s3 += "\n";
   }
   QTextBrowser* tb3 = new QTextBrowser(&d);
   tb3->setWordWrap( QTextEdit::NoWrap );
   tb3->setText(s3);
   d.addTab(tb3,i18n("&Thanks To"));
   
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

// safeStringJoin and safeStringSplit allow to convert a stringlist into a string and back
// safely, even if the individual strings in the list contain the separator character.
static QString safeStringJoin(const QStringList& sl, char sepChar=',', char metaChar='\\' )
{
   // Join the strings in the list, using the separator ','
   // If a string contains the separator character, it will be replaced with "\,".
   // Any occurances of "\" (one backslash) will be replaced with "\\" (2 backslashes)
   
   assert(sepChar!=metaChar);
   
   QString sep;
   sep += sepChar;
   QString meta;
   meta += metaChar;   
   
   QString safeString;
   
   QStringList::const_iterator i;
   for (i=sl.begin(); i!=sl.end(); ++i)
   {
      QString s = *i;
      s.replace(meta, meta+meta);   //  "\" -> "\\"
      s.replace(sep, meta+sep);     //  "," -> "\,"
      if ( i==sl.begin() )
         safeString = s;
      else
         safeString += sep + s;
   }
   return safeString;
}

// Split a string that was joined with safeStringJoin
static QStringList safeStringSplit(const QString& s, char sepChar=',', char metaChar='\\' )
{
   assert(sepChar!=metaChar);
   QStringList sl;
   // Miniparser
   int i=0;
   int len=s.length();
   QString b;
   for(i=0;i<len;++i)
   {
      if      ( i+1<len && s[i]==metaChar && s[i+1]==metaChar ){ b+=metaChar; ++i; }
      else if ( i+1<len && s[i]==metaChar && s[i+1]==sepChar ){ b+=sepChar; ++i; }
      else if ( s[i]==sepChar )  // real separator
      {
         sl.push_back(b);
         b="";
      }
      else { b+=s[i]; }     
   }
   if ( !b.isEmpty() )
      sl.push_back(b);

   return sl;
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
   m_map[k] = v.family() + "," + QString::number(v.pointSize()) + "," + (v.bold() ? "bold" : "normal");
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
   m_map[k] = safeStringJoin(v, separator);
}


QFont KConfig::readFontEntry(const QString& k, QFont* defaultVal )
{
   QFont f = *defaultVal;
   std::map<QString,QString>::iterator i = m_map.find( k );
   if ( i!=m_map.end() )
   {
      f.setFamily( subSection( i->second, 0, ',' ) );
      f.setPointSize( subSection( i->second, 1, ',' ).toInt() );
      f.setBold( subSection( i->second, 2, ',' )=="bold" );
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

QStringList KConfig::readListEntry(const QString& k, char separator )
{
   QStringList strList;

   std::map<QString,QString>::iterator i = m_map.find( k );
   if ( i!=m_map.end() )
   {
      strList = safeStringSplit( i->second, separator );
   }
   return strList;
}

void KAction::init(QObject* receiver, const char* slot, KActionCollection* actionCollection, 
                   const char* name, bool bToggle, bool bMenu)
{
   QString n(name);
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
      if( n[0]=='g')       addTo( p->movementMenu );
      else if( n.left(16)=="dir_current_sync")
      {
         if ( p->dirCurrentItemMenu==0 )
         {
            p->dirCurrentItemMenu = new QPopupMenu();
            p->directoryMenu->insertItem(i18n("Current Item Merge Operation"), p->dirCurrentItemMenu);
            p->dirCurrentSyncItemMenu = new QPopupMenu();
            p->directoryMenu->insertItem(i18n("Current Item Sync Operation"), p->dirCurrentSyncItemMenu);
         }
         addTo( p->dirCurrentItemMenu );
      }
      else if( n.left(11)=="dir_current")
      {
         if ( p->dirCurrentItemMenu==0 )
         {
            p->dirCurrentItemMenu = new QPopupMenu();
            p->directoryMenu->insertItem(i18n("Current Item Merge Operation"), p->dirCurrentItemMenu);
            p->dirCurrentSyncItemMenu = new QPopupMenu();
            p->directoryMenu->insertItem(i18n("Current Item Sync Operation"), p->dirCurrentSyncItemMenu);
         }
         addTo( p->dirCurrentSyncItemMenu );
      }
      else if( n.left(4)=="diff")  addTo( p->diffMenu );
      else if( name[0]=='d')  addTo( p->directoryMenu );
      else if( name[0]=='f')  addTo( p->fileMenu );
      else if( name[0]=='w')  addTo( p->windowsMenu );
      else                    addTo( p->mergeMenu );
   }
}


KAction::KAction(const QString& text, const QIconSet& icon, int accel,
 QObject* receiver, const char* slot, KActionCollection* actionCollection,
 const char* name, bool bToggle, bool bMenu
 )
: QAction ( text, icon, text, accel, actionCollection->m_pMainWindow, name, bToggle )
{
   KMainWindow* p = actionCollection->m_pMainWindow;
   if ( !icon.isNull() && p ) this->addTo( p->m_pToolBar );

   init(receiver,slot,actionCollection,name,bToggle,bMenu);
}

KAction::KAction(const QString& text, int accel,
 QObject* receiver, const char* slot, KActionCollection* actionCollection,
 const char* name, bool bToggle, bool bMenu
 )
: QAction ( text, text, accel, actionCollection->m_pMainWindow, name, bToggle )
{
   init(receiver,slot,actionCollection,name,bToggle,bMenu);
}

void KAction::setStatusText(const QString&)
{
}

void KAction::plug(QPopupMenu* menu)
{
   addTo(menu);
}


KToggleAction::KToggleAction(const QString& text, const QIconSet& icon, int accel, QObject* receiver, const char* slot, KActionCollection* actionCollection, const char* name, bool bMenu)
: KAction( text, icon, accel, receiver, slot, actionCollection, name, true, bMenu)
{
}

KToggleAction::KToggleAction(const QString& text, int accel, QObject* receiver, const char* slot, KActionCollection* actionCollection, const char* name, bool bMenu)
: KAction( text, accel, receiver, slot, actionCollection, name, true, bMenu)
{
}

KToggleAction::KToggleAction(const QString& text, const QIconSet& icon, int accel, KActionCollection* actionCollection, const char* name, bool bMenu)
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
   KAction* a = new KAction( i18n("Open"), QIconSet(QPixmap(fileopen)), Qt::CTRL+Qt::Key_O, parent, slot, actionCollection, "open", false, false);
   if(p){ a->addTo( p->fileMenu ); }
   return a;
}

KAction* KStdAction::save( QWidget* parent, const char* slot, KActionCollection* actionCollection )
{
   #include "../xpm/filesave.xpm"
   KMainWindow* p = actionCollection->m_pMainWindow;
   KAction* a = new KAction( i18n("Save"), QIconSet(QPixmap(filesave)), Qt::CTRL+Qt::Key_S, parent, slot, actionCollection, "save", false, false);
   if(p){ a->addTo( p->fileMenu ); }
   return a;
}

KAction* KStdAction::saveAs( QWidget* parent, const char* slot, KActionCollection* actionCollection)
{
   KMainWindow* p = actionCollection->m_pMainWindow;
   KAction* a = new KAction( i18n("Save As..."), 0, parent, slot, actionCollection, "saveas", false, false);
   if(p) a->addTo( p->fileMenu );
   return a;
}

KAction* KStdAction::quit( QWidget* parent, const char* slot, KActionCollection* actionCollection)
{
   KMainWindow* p = actionCollection->m_pMainWindow;
   KAction* a = new KAction( i18n("Quit"), Qt::CTRL+Qt::Key_Q, parent, slot, actionCollection, "quit", false, false);
   if(p) a->addTo( p->fileMenu );
   return a;
}

KAction* KStdAction::cut( QWidget* parent, const char* slot, KActionCollection* actionCollection)
{
   KMainWindow* p = actionCollection->m_pMainWindow;
   KAction* a = new KAction( i18n("Cut"), Qt::CTRL+Qt::Key_X, parent, slot, actionCollection, "cut", false, false );
   if(p) a->addTo( p->editMenu );
   return a;
}

KAction* KStdAction::copy( QWidget* parent, const char* slot, KActionCollection* actionCollection)
{
   KMainWindow* p = actionCollection->m_pMainWindow;
   KAction* a = new KAction( i18n("Copy"), Qt::CTRL+Qt::Key_C, parent, slot, actionCollection, "copy", false, false );
   if(p) a->addTo( p->editMenu );
   return a;
}

KAction* KStdAction::paste( QWidget* parent, const char* slot, KActionCollection* actionCollection)
{
   KMainWindow* p = actionCollection->m_pMainWindow;
   KAction* a = new KAction( i18n("Paste"), Qt::CTRL+Qt::Key_V, parent, slot, actionCollection, "paste", false, false );
   if(p) a->addTo( p->editMenu );
   return a;
}

KToggleAction* KStdAction::showToolbar( QWidget* parent, const char* slot, KActionCollection* actionCollection)
{
   KMainWindow* p = actionCollection->m_pMainWindow;
   KToggleAction* a = new KToggleAction( i18n("Show Toolbar"), 0, parent, slot, actionCollection, "showtoolbar", false );
   if(p) a->addTo( p->settingsMenu );
   return a;
}

KToggleAction* KStdAction::showStatusbar( QWidget* parent, const char* slot, KActionCollection* actionCollection)
{
   KMainWindow* p = actionCollection->m_pMainWindow;
   KToggleAction* a = new KToggleAction( i18n("Show &Statusbar"), 0, parent, slot, actionCollection, "showstatusbar", false );
   if(p) a->addTo( p->settingsMenu );
   return a;
}

KAction* KStdAction::preferences( QWidget* parent, const char* slot, KActionCollection* actionCollection)
{
   KMainWindow* p = actionCollection->m_pMainWindow;
   KAction* a = new KAction( i18n("&Configure %1...").arg("KDiff3"), 0, parent, slot, actionCollection, "settings", false, false );
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
   KAction* a = new KAction( i18n("About"), 0, parent, slot, actionCollection, "about", false, false );
   if(p) a->addTo( p->helpMenu );
   return a;
}

KAction* KStdAction::help( QWidget* parent, const char* slot, KActionCollection* actionCollection)
{
   KMainWindow* p = actionCollection->m_pMainWindow;
   KAction* a = new KAction( i18n("Help"), Qt::Key_F1, parent, slot, actionCollection, "help", false, false );
   if(p) a->addTo( p->helpMenu );
   return a;
}
KAction* KStdAction::find( QWidget* parent, const char* slot, KActionCollection* actionCollection)
{
   KMainWindow* p = actionCollection->m_pMainWindow;
   KAction* a = new KAction( i18n("Find"), Qt::CTRL+Qt::Key_F, parent, slot, actionCollection, "find", false, false );
   if(p) a->addTo( p->editMenu );
   return a;
}

KAction* KStdAction::findNext( QWidget* parent, const char* slot, KActionCollection* actionCollection)
{
   KMainWindow* p = actionCollection->m_pMainWindow;
   KAction* a = new KAction( i18n("Find Next"), Qt::Key_F3, parent, slot, actionCollection, "findNext", false, false );
   if(p) a->addTo( p->editMenu );
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
            QString s;
            s = QString("Unknown option: ") +  s_argv[i] + "\n";

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
               if ( s_pOptions[j].longName!=0 )
               {
                  if (s_pOptions[j].shortName[0]!='+')
                  {
                     s += "-";
                     if ( strlen(s_pOptions[j].shortName)>1 ) s += "-";
                  }
                  s += s_pOptions[j].shortName;
                  s += QString().fill(' ', minMaxLimiter( 20 - ((int)s.length()-pos), 3, 20 ) );
                  s += s_pOptions[j].longName;
                  s +="\n";
                  pos=s.length();
               }
               else
               {
                  s += "-";
                  if ( strlen(s_pOptions[j].shortName)>1 ) s += "-";
                  s += s_pOptions[j].shortName;
                  s += ", ";
               }
            }
            
            s += "\nFor more documentation, see the help-menu or the subdirectory doc.\n";
#ifdef _WIN32 
            // A windows program has no console
            KMessageBox::information(0, s,i18n("KDiff3-Usage"));
#else
            std::cerr << s.latin1() << std::endl;
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
      return f->createPartObject( (QWidget*)pParent, name.ascii(),
                                            pParent, name.ascii(),
                                            classname.ascii(),  QStringList() );
   else
      return 0;
}




#include "kreplacements.moc"
