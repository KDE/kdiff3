/***************************************************************************
                          kreplacements.h  -  description
                             -------------------
    begin                : Sat Aug 3 2002
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

#ifndef KREPLACEMENTS_H
#define KREPLACEMENTS_H

#include "common.h"

#include <qobject.h>
#include <qtabdialog.h>
#include <qmainwindow.h>
#include <qaction.h>
#include <qfiledialog.h>
#include <qapplication.h>
#include <qvbox.h>
#include <qpushbutton.h>
#include <qstatusbar.h>
#include <qtoolbar.h>
#include <qprogressbar.h>
#include <qpopupmenu.h>
#include <qstringlist.h>
#include <qprinter.h>

#include <map>
#include <list>

QString getTranslationDir();

class KMainWindow;

class KURL
{
public:
   KURL(){}
   KURL(const QString& s){ m_s = s; }
   static KURL fromPathOrURL( const QString& s ){ return KURL(s); }
   QString url() const { return m_s; }
   bool isEmpty() const { return m_s.isEmpty(); }
   QString prettyURL() const { return m_s; }
   bool isLocalFile() const { return true; }
   bool isValid() const { return true; }
   QString path() const { return m_s; }
   void setPath( const QString& s ){ m_s=s; }
   QString fileName() const { return m_s; } // not really needed
   void addPath( const QString& s ){ m_s += "/" + s; }
private:
   QString m_s;
};

class KMessageBox
{
public:
   static void error( QWidget* parent, const QString& text, const QString& caption=QString() );
   static int warningContinueCancel( QWidget* parent, const QString& text, const QString& caption=QString(),
      const QString& button1=QString("Continue") );
   static void sorry(  QWidget* parent, const QString& text, const QString& caption=QString() );
   static void information(  QWidget* parent, const QString& text, const QString& caption=QString() );
   static int  warningYesNo( QWidget* parent, const QString& text, const QString& caption,
      const QString& button1, const QString& button2 );
   static int warningYesNoCancel(
         QWidget* parent, const QString& text, const QString& caption,
         const QString& button1, const QString& button2 );

   enum {Cancel=-1, No=0, Yes=1, Continue=1};
};

#define i18n(x) QObject::tr(x)
#define I18N_NOOP(x) x
#define RESTORE(x)
#define _UNLOAD(x)

typedef QPopupMenu KPopupMenu;

class KDialogBase : public QTabDialog
{
   Q_OBJECT
public:
   KDialogBase( int, const QString& caption, int, int, QWidget* parent, const char* name,
     bool /*modal*/, bool );
   ~KDialogBase();

   void incInitialSize ( const QSize& );
   void setHelp(const QString& helpfilename, const QString& );
   enum {IconList, Help, Default, Apply, Ok, Cancel };

   int BarIcon(const QString& iconName, int );

   QVBox* addVBoxPage( const QString& name, const QString& info, int );
   QFrame* addPage(  const QString& name, const QString& info, int );
   int spacingHint();

   virtual void accept();
signals:
   void applyClicked();

protected slots:
    virtual void slotOk( void );
    virtual void slotApply( void );
    virtual void slotHelp( void );
    virtual void slotCancel( void );
    virtual void slotDefault( void );
};

class KFileDialog : public QFileDialog
{
public:
   static KURL getSaveURL( const QString &startDir=QString::null,
                           const QString &filter=QString::null,
                           QWidget *parent=0, const QString &caption=QString::null);
   static KURL getOpenURL( const QString &  startDir = QString::null,
                           const QString &  filter = QString::null,
                           QWidget *  parent = 0,
                           const QString &  caption = QString::null );
   static KURL getExistingURL( const QString &  startDir = QString::null,
                               QWidget *  parent = 0,
                               const QString &  caption = QString::null );
   static QString getSaveFileName (const QString &startDir=QString::null, 
                                   const QString &filter=QString::null, 
                                   QWidget *parent=0, 
                                   const QString &caption=QString::null);
};

typedef QStatusBar KStatusBar;

class KToolBar : public QToolBar
{
public:
   KToolBar(QMainWindow* parent);

   enum BarPosition {Top, Bottom, Left, Right};
   BarPosition barPos();
   void setBarPos(BarPosition);
private:
   QMainWindow* m_pMainWindow;
};

class KActionCollection
{
public:
   KMainWindow* m_pMainWindow;
   KActionCollection( KMainWindow* p){ m_pMainWindow=p; }
};

class KKeyDialog
{
public:
   static void configure(void*, QWidget*){}
   static void configureKeys(KActionCollection*, const QString&){}
   static void configure(KActionCollection*, const QString&){}
};

namespace KParts
{
   class ReadWritePart;
}

class KMainWindow : public QMainWindow
{
   Q_OBJECT
private:
   KStatusBar m_statusBar;
   KActionCollection m_actionCollection;
protected:
   virtual bool queryClose() = 0;
   virtual bool queryExit() = 0;
public:
   QPopupMenu* fileMenu;
   QPopupMenu* editMenu;
   QPopupMenu* directoryMenu;
   QPopupMenu* dirCurrentItemMenu;
   QPopupMenu* dirCurrentSyncItemMenu;
   QPopupMenu* movementMenu;
   QPopupMenu* mergeMenu;
   QPopupMenu* diffMenu;
   QPopupMenu* windowsMenu;
   QPopupMenu* settingsMenu;
   QPopupMenu* helpMenu;

   KToolBar*  m_pToolBar;

   KMainWindow( QWidget* parent, const char* name );
   KToolBar* toolBar(const QString& s = QString::null);
   KActionCollection* actionCollection();
   void createGUI();
   void createGUI(KParts::ReadWritePart*){createGUI();}

   QList<KMainWindow>* memberList;
public slots:
   void slotHelp();
   void slotAbout();
};

class KConfig : public ValueMap
{
   QString m_fileName;
public:
   KConfig();
   ~KConfig();
   void readConfigFile(const QString& configFileName);

   void setGroup(const QString&);
};

class KAction : public QAction
{
   Q_OBJECT
public:
   KAction(const QString& text, const QIconSet& icon, int accel, QObject* receiver, const char* slot, KActionCollection* actionCollection, const char* name, bool bToggle=false, bool bMenu=true);
   KAction(const QString& text, int accel, QObject* receiver, const char* slot, KActionCollection* actionCollection, const char* name, bool bToggle=false, bool bMenu=true);
   void init(QObject* receiver, const char* slot, KActionCollection* actionCollection, 
        const char* name, bool bToggle, bool bMenu);
   void setStatusText(const QString&);
   void plug(QPopupMenu*);
};

class KToggleAction : public KAction
{
public:
   KToggleAction(const QString& text, const QIconSet& icon, int accel, QObject* receiver, const char* slot, KActionCollection* actionCollection, const char* name, bool bMenu=true);
   KToggleAction(const QString& text, int accel, QObject* receiver, const char* slot, KActionCollection* actionCollection, const char* name, bool bMenu=true);
   KToggleAction(const QString& text, const QIconSet& icon, int accel, KActionCollection* actionCollection, const char* name, bool bMenu=true);
   void setChecked(bool);
   bool isChecked();
};


class KStdAction
{
public:
   static KAction* open( QWidget* parent, const char* slot, KActionCollection* );
   static KAction* save( QWidget* parent, const char* slot, KActionCollection* );
   static KAction* saveAs( QWidget* parent, const char* slot, KActionCollection* );
   static KAction* print( QWidget* parent, const char* slot, KActionCollection* );
   static KAction* quit( QWidget* parent, const char* slot, KActionCollection* );
   static KAction* cut( QWidget* parent, const char* slot, KActionCollection* );
   static KAction* copy( QWidget* parent, const char* slot, KActionCollection* );
   static KAction* paste( QWidget* parent, const char* slot, KActionCollection* );
   static KAction* selectAll( QWidget* parent, const char* slot, KActionCollection* );
   static KToggleAction* showToolbar( QWidget* parent, const char* slot, KActionCollection* );
   static KToggleAction* showStatusbar( QWidget* parent, const char* slot, KActionCollection* );
   static KAction* preferences( QWidget* parent, const char* slot, KActionCollection* );
   static KAction* about( QWidget* parent, const char* slot, KActionCollection* );
   static KAction* aboutQt( KActionCollection* );
   static KAction* help( QWidget* parent, const char* slot, KActionCollection* );
   static KAction* find( QWidget* parent, const char* slot, KActionCollection* );
   static KAction* findNext( QWidget* parent, const char* slot, KActionCollection* );
   static KAction* keyBindings( QWidget* parent, const char* slot, KActionCollection* );
};

class KIcon
{
public:
   enum {SizeMedium,Small};
};

class KFontChooser : public QWidget
{
   Q_OBJECT
   QFont m_font;
   QPushButton* m_pSelectFont;
   QLabel* m_pLabel;
   QWidget* m_pParent;
public:
   KFontChooser( QWidget* pParent, const QString& name, bool, const QStringList&, bool, int );
   QFont font();
   void setFont( const QFont&, bool );
private slots:
   void slotSelectFont();
};

class KColorButton : public QPushButton
{
   Q_OBJECT
   QColor m_color;
public:
   KColorButton(QWidget* parent);
   QColor color();
   void setColor(const QColor&);
   virtual void paintEvent(QPaintEvent* e);
public slots:
   void slotClicked();
};

class KPrinter : public QPrinter
{
public:
   KPrinter();
   enum e_PageSelection {ApplicationSide};
   QValueList<int> pageList();
   void setCurrentPage(int);
   void setPageSelection(e_PageSelection);
};

class KStandardDirs
{
public:
   QString findResource(const QString& resource, const QString& appName);
};   

struct KCmdLineOptions
{
   const char* name;
   const char* description;
   int def;
};

#define KCmdLineLastOption {0,0,0}

class KAboutData
{
public:
   KAboutData( const QString& name, const QString& appName, const QString& version,
      const QString& description, int licence,
      const QString& copyright, int w, const QString& homepage, const QString& email);
   KAboutData( const QString& name, const QString& appName, const QString& version );
   void addAuthor(const char* name=0, const char* task=0, const char* email=0, const char* weblink=0);
   void addCredit(const char* name=0, const char* task=0, const char* email=0, const char* weblink=0);
   enum { License_GPL };
   
   struct AboutDataEntry
   {
      AboutDataEntry(const QString& name, const QString& task, const QString& email, const QString& weblink)
      : m_name(name), m_task(task), m_email(email), m_weblink(weblink)  
      {}
      QString m_name;
      QString m_task;
      QString m_email;
      QString m_weblink;
   };
   
   std::list<AboutDataEntry> m_authorList;
   std::list<AboutDataEntry> m_creditList;
};

typedef QValueList<QCString> QCStringList;

class KCmdLineArgs
{
public:
   static KCmdLineArgs* parsedArgs();
   static void init( int argc, char**argv, KAboutData* );
   static void addCmdLineOptions( KCmdLineOptions* options ); // Add our own options.

   int count();
   QString arg(int);
   KURL url(int i){ return KURL(arg(i)); }
   void clear();
   QString getOption(const QString&);
   QCStringList getOptionList( const QString& );
   bool isSet(const QString&);
};

class KIconLoader
{
public:
   QPixmap loadIcon(const QString& name, int);
};

class KApplication : public QApplication
{
   KConfig m_config;
   KIconLoader m_iconLoader;
public:
   KApplication();
   static KApplication* kApplication();
   KIconLoader* iconLoader();
   KConfig* config();
   bool isRestored();
};

extern KApplication* kapp;

class KLibFactory : public QObject
{
   Q_OBJECT
public:
   QObject* create(QObject*,const QString&,const QString&);
};

class KLibLoader
{
public:
   static KLibLoader* self();
   KLibFactory* factory(const QString&);
};

class KEditToolbar : public QDialog
{
public:
   KEditToolbar( int ){}
};

class KGlobal
{
public:
   static KConfig* config() { return 0; }
};

namespace KIO
{
   enum UDSEntry {};
   typedef QValueList<UDSEntry> UDSEntryList;
   class Job : public QObject
   {
   public:
      void kill(bool){}
      bool error() {return false;}
      void showErrorDialog( QWidget* ) {}
   };
   class SimpleJob : public Job {};
   SimpleJob* mkdir( KURL );
   SimpleJob* rmdir( KURL );
   SimpleJob* file_delete( KURL, bool );
   class FileCopyJob : public Job {};
   FileCopyJob* file_move(  KURL, KURL, int, bool, bool, bool );
   FileCopyJob* file_copy(  KURL, KURL, int, bool, bool, bool );
   class CopyJob : public Job {};
   CopyJob* link( KURL, KURL, bool );
   class ListJob : public Job {};
   ListJob* listRecursive( KURL, bool, bool );
   ListJob* listDir( KURL, bool, bool );
   class StatJob : public Job {
      public: UDSEntry statResult(){ return (UDSEntry)0; }
   };
   StatJob* stat( KURL, bool, int, bool );
   class TransferJob : public Job {};
   TransferJob* get( KURL, bool, bool );
   TransferJob* put( KURL, int, bool, bool, bool );
};

typedef QProgressBar KProgress;

class KInstance : public QObject
{
public:
   KInstance(KAboutData*){}
};

namespace KParts
{
   class MainWindow : public KMainWindow
   {
   public:
      MainWindow( QWidget* parent, const char* name ) : KMainWindow(parent,name) {}
      void setXMLFile(const QString&){}
      void setAutoSaveSettings(){}
      void saveMainWindowSettings(KConfig*){}
      void applyMainWindowSettings(KConfig*){}
      int factory(){return 0;}
   };

   class Part : public QObject
   {
   public:
      KActionCollection* actionCollection();
      KApplication* instance();
      void setWidget( QWidget* w ){ m_pWidget=w; }
      QWidget* widget(){return m_pWidget;}
      void setXMLFile(const QString&){}
   private:
      QWidget* m_pWidget;
   };

   class ReadOnlyPart : public Part
   {
   public:
   ReadOnlyPart(){}
   ReadOnlyPart(QObject*,const QCString&){}
   void setInstance( KInstance* ){}
   QString m_file;
   };

   class ReadWritePart : public ReadOnlyPart
   {
   public:
   ReadWritePart(QObject*,const QCString&){}
   void setReadWrite(bool){}
   };

   class Factory : public KLibFactory
   {
      Q_OBJECT
   public:
   virtual KParts::Part* createPartObject( QWidget *parentWidget, const char *widgetName,
                                            QObject *parent, const char *name,
                                            const char *classname, const QStringList &args )=0;
   };
};
#endif


