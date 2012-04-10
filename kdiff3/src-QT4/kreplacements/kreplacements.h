/***************************************************************************
                          kreplacements.h  -  description
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

#ifndef KREPLACEMENTS_H
#define KREPLACEMENTS_H
#ifndef __OS2__
#pragma once
#endif

#include "common.h"

#include <QSharedData>
#include <QMainWindow>
#include <QAction>
#include <QDialog>
#include <QApplication>
#include <QPushButton>
#include <QToolBar>
#include <QProgressBar>
#include <QPrinter>
#include <QExplicitlySharedDataPointer>
//Added by qt3to4:
#include <QLabel>
#include <QPixmap>
#include <QFrame>
#include <QPaintEvent>

class QTabWidget;
class QLabel;

#include <map>
#include <list>

QString getTranslationDir(const QString&);

class KMainWindow;
class KAction;
class KIcon;

typedef QString KGuiItem;

inline QString i18n( const char* x ){ return QObject::tr(x); }

template <typename A1>
inline QString i18n (const char *text, const A1 &a1)
{ return QObject::tr(text).arg(a1); }

template <typename A1, typename A2>
inline QString i18n (const char *text, const A1 &a1, const A2 &a2)
{ return QObject::tr(text).arg(a1).arg(a2); }

template <typename A1, typename A2, typename A3>
inline QString i18n (const char *text, const A1 &a1, const A2 &a2, const A3 &a3)
{ return QObject::tr(text).arg(a1).arg(a2).arg(a3); }

template <typename A1, typename A2, typename A3, typename A4>
inline QString i18n (const char *text, const A1 &a1, const A2 &a2, const A3 &a3, const A4 &a4)
{ return QObject::tr(text).arg(a1).arg(a2).arg(a3).arg(a4); }


typedef QString KLocalizedString;
#define ki18n(x) QObject::tr(x)
#define I18N_NOOP(x) x
#define RESTORE(x)
#define _UNLOAD(x)

class KUrl
{
public:
   KUrl(){}
   KUrl(const QString& s){ m_s = s; }
   static KUrl fromPathOrUrl( const QString& s ){ return KUrl(s); }
   QString url() const { return m_s; }
   bool isEmpty() const { return m_s.isEmpty(); }
   QString prettyUrl() const { return m_s; }
   bool isLocalFile() const { return true; }
   bool isRelative() const { return true; }
   bool isValid() const { return true; }
   QString path() const { return m_s; }
   void setPath( const QString& s ){ m_s=s; }
   QString fileName() const { return m_s; } // not really needed
   void addPath( const QString& s ){ m_s += "/" + s; }
private:
   QString m_s;
};

typedef QString KGuiItem;

class KStandardGuiItem
{
public:
   static QString cont() { return i18n("Continue"); }
   static QString cancel() { return i18n("Cancel"); }
   static QString quit() { return i18n("Quit"); }
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



typedef QMenu KMenu;

class KPageWidgetItem : public QObject
{
public:
   QWidget* m_pWidget;
   QString m_title;

   KPageWidgetItem( QWidget* pPage, const QString& title )
   {
      m_pWidget = pPage;
      m_title = title;
   }
   void setHeader( const QString& ) {}
   void setIcon( const KIcon& ) {}
};


class KPageDialog : public QDialog
{
   Q_OBJECT
   QTabWidget* m_pTabWidget;
public:
   KPageDialog( QWidget* parent );
   ~KPageDialog();

   void incrementInitialSize ( const QSize& );
   void setHelp(const QString& helpfilename, const QString& );
   enum {IconList, Help, Default, Apply, Ok, Cancel };

   int BarIcon(const QString& iconName, int );

   void addPage( KPageWidgetItem* );
   QFrame* addPage(  const QString& name, const QString& info, int );
   int spacingHint();

   enum FaceType { List };
   void setFaceType(FaceType){}
   void setButtons(int){}
   void setDefaultButton(int){}
   void showButtonSeparator(bool){}
private slots:
   void slotHelpClicked();
signals:
   void applyClicked();
   void okClicked();
   void helpClicked();
   void defaultClicked();
};

class KFileDialog //: public QFileDialog
{
public:
   static KUrl getSaveUrl( const QString &startDir=QString::null,
                           const QString &filter=QString::null,
                           QWidget *parent=0, const QString &caption=QString::null);
   static KUrl getOpenUrl( const QString &  startDir = QString::null,
                           const QString &  filter = QString::null,
                           QWidget *  parent = 0,
                           const QString &  caption = QString::null );
   static KUrl getExistingDirectoryUrl( const QString &  startDir = QString::null,
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
   void addAction(const QString& name, QAction* );
   KAction* addAction(const QString& name );
};

typedef QKeySequence KShortcut;

class KShortcutsEditor
{
public:
   enum { LetterShortcutsAllowed };
};

class KShortcutsDialog
{
public:
   static void configure(KActionCollection*){}
   static void configureKeys(KActionCollection*, const QString&){}
   static void configure(KActionCollection*, int, QWidget*){}
};

namespace KParts
{
   class ReadWritePart;
}

class KMainWindow : public QMainWindow
{
   Q_OBJECT
private:
   KActionCollection m_actionCollection;
protected:
   virtual bool queryClose() = 0;
   virtual bool queryExit() = 0;
public:
   QMenu* fileMenu;
   QMenu* editMenu;
   QMenu* directoryMenu;
   QMenu* dirCurrentItemMenu;
   QMenu* dirCurrentSyncItemMenu;
   QMenu* movementMenu;
   QMenu* mergeMenu;
   QMenu* diffMenu;
   QMenu* windowsMenu;
   QMenu* settingsMenu;
   QMenu* helpMenu;

   KToolBar*  m_pToolBar;

   KMainWindow( QWidget* parent );
   KToolBar* toolBar(const QString& s = QString::null);
   KActionCollection* actionCollection();
   void createGUI();
   void createGUI(KParts::ReadWritePart*){createGUI();}

   QList<KMainWindow*>* memberList;
public slots:
   void appHelpActivated();
   void slotAbout();
};

class KConfigGroupData : public ValueMap, public QSharedData
{
public:
   QString m_fileName;
   ~KConfigGroupData();
};

class KConfigGroup
{
private:
   QExplicitlySharedDataPointer<KConfigGroupData> d;
public:
   KConfigGroup(const KConfigGroup*, const QString& ){}
   KConfigGroup();
   ~KConfigGroup();
   void readConfigFile(const QString& configFileName);

   void setGroup(const QString&);
   KConfigGroup& group( const QString& groupName );

   template <class T>  void writeEntry(const QString& s, const T& v){ d->writeEntry(s,v); }
   void writeEntry(const QString& s, const QStringList& v, char separator ){ d->writeEntry(s,v,separator); }
   void writeEntry(const QString& s, const char* v){ d->writeEntry(s,v); }

   template <class T>  T readEntry (const QString& s, const T& defaultVal ){ return d->readEntry(s,defaultVal); }
   QString     readEntry (const QString& s, const char* defaultVal ){ return d->readEntry(s,defaultVal); }
   QStringList readEntry (const QString& s, const QStringList& defaultVal, char separator='|' ){ return d->readEntry(s,defaultVal,separator); }
};

typedef KConfigGroup* KSharedConfigPtr;

class KAction : public QAction
{
   Q_OBJECT
public:
   KAction( KActionCollection* actionCollection );
   KAction(const QString& text, KActionCollection* actionCollection );
   KAction(const QString& text, const QIcon& icon, int accel, QObject* receiver, const char* slot, KActionCollection* actionCollection, const char* name, bool bToggle=false, bool bMenu=true);
   KAction(const QString& text, int accel, QObject* receiver, const char* slot, KActionCollection* actionCollection, const char* name, bool bToggle=false, bool bMenu=true);
   void setStatusText(const QString&);
   void plug(QMenu*);
   void setIcon( const QIcon& icon );
};

class KToggleAction : public KAction
{
public:
   KToggleAction(KActionCollection* actionCollection);
   KToggleAction(const QString& text, const QIcon& icon, int accel, QObject* receiver, const char* slot, KActionCollection* actionCollection, const char* name, bool bMenu=true);
   KToggleAction(const QString& text, int accel, QObject* receiver, const char* slot, KActionCollection* actionCollection, const char* name, bool bMenu=true);
   KToggleAction(const QString& text, const QIcon& icon, int accel, KActionCollection* actionCollection, const char* name, bool bMenu=true);
   void setChecked(bool);
};


class KStandardAction
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
   KIcon( const QString& ) {}
};

class KFontChooser : public QWidget
{
   Q_OBJECT
   QFont m_font;
   QPushButton* m_pSelectFont;
   QLabel* m_pLabel;
   QWidget* m_pParent;
public:
   KFontChooser( QWidget* pParent );
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

#ifndef QT_NO_PRINTER
class KPrinter : public QPrinter
{
public:
   KPrinter();
   enum e_PageSelection {ApplicationSide};
   QList<int> pageList();
   void setCurrentPage(int);
   void setPageSelection(e_PageSelection);
};
#endif

class KStandardDirs
{
public:
   QString findResource(const QString& resource, const QString& appName);
};   

class KCmdLineOptions
{
public:
   KCmdLineOptions& add( const QString& name, const QString& description = 0 );
};

#define KCmdLineLastOption {0,0,0}

class KAboutData
{
public:
   enum LicenseKey { License_GPL, License_GPL_V2, License_Unknown };

   //KAboutData( const QString& name, const QString& appName, const QString& version,
   //   const QString& description, int licence,
   //   const QString& copyright, int w, const QString& homepage, const QString& email);

   KAboutData (const QByteArray &appName, const QByteArray &catalogName, const KLocalizedString &programName, 
      const QByteArray &version, const KLocalizedString &shortDescription, LicenseKey licenseType, 
      const KLocalizedString &copyrightStatement, const KLocalizedString &text, 
      const QByteArray &homePageAddress, const QByteArray &bugsEmailAddress);
   KAboutData( const QString& name, const QString& appName, const QString& appName2, const QString& version );
   void addAuthor(const QString& name, const QString& task=0, const QString& email=0, const QString& weblink=0);
   void addCredit(const QString& name, const QString& task=0, const QString& email=0, const QString& weblink=0);
   
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

typedef QList<QString> QCStringList;

class KCmdLineArgs
{
public:
   static KCmdLineArgs* parsedArgs();
   static void init( int argc, char**argv, KAboutData* );
   static void addCmdLineOptions( const KCmdLineOptions& options ); // Add our own options.

   int count();
   QString arg(int);
   KUrl url(int i){ return KUrl(arg(i)); }
   void clear();
   QString getOption(const QString&);
   QStringList getOptionList( const QString& );
   bool isSet(const QString&);
};

class KIconLoader
{
public:
   enum { Small, NoGroup };
   QPixmap loadIcon(const QString& name, int, int =0);
   static KIconLoader* global() { return 0; }
};

class KApplication : public QApplication
{
   KConfigGroup m_config;
   KIconLoader m_iconLoader;
public:
   KApplication();
   static KApplication* kApplication();
   KIconLoader* iconLoader();
   KConfigGroup* config();
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

class KEditToolBar : public QDialog
{
public:
   KEditToolBar( int ){}
};

class KGlobal
{
public:
   static KConfigGroup* config() { return 0; }
};

class KJobUiDelegate
{
public:
   void showErrorMessage() {}
};

class KJob : public QObject
{
public:
   bool error() {return false;}
   enum KillVerbosity { Quietly };
   bool kill( KillVerbosity ){return false;}
   KJobUiDelegate* uiDelegate() {return 0;}
};

namespace KIO
{
   enum { Overwrite, DefaultFlags, Resume, HideProgressInfo, NoReload };
   enum UDSEntry {};
   typedef QList<UDSEntry> UDSEntryList;
   class Job : public KJob
   {   
   };
   class SimpleJob : public KJob {};
   SimpleJob* mkdir( KUrl );
   SimpleJob* rmdir( KUrl );
   SimpleJob* file_delete( KUrl, int );
   class FileCopyJob : public KJob {};
   FileCopyJob* file_move(  KUrl, KUrl, int, int );
   FileCopyJob* file_copy(  KUrl, KUrl, int, int );
   class CopyJob : public KJob {};
   CopyJob* link( KUrl, KUrl, bool );
   class ListJob : public KJob {};
   ListJob* listRecursive( KUrl, bool, bool );
   ListJob* listDir( KUrl, bool, bool );
   class StatJob : public KJob {
      public: 
         enum {SourceSide,DestinationSide};
         UDSEntry statResult(){ return (UDSEntry)0; }
   };
   StatJob* stat( KUrl, bool, int, int );
   class TransferJob : public KJob {};
   TransferJob* get( KUrl, int );
   TransferJob* put( KUrl, int, int );
};

typedef QProgressBar KProgress;

class KInstance : public QObject
{
public:
   KInstance(KAboutData*){}
};

class KComponentData : public QObject
{
public:
   KComponentData(KAboutData*){}
   KConfigGroup* config() {return 0;}
};

namespace KParts
{
   class MainWindow : public KMainWindow
   {
      Q_OBJECT
   public:
      MainWindow( QWidget* parent=0 ) : KMainWindow(parent) {}
      void setXMLFile(const QString&){}
      void setAutoSaveSettings(){}
      void saveMainWindowSettings(KConfigGroup&){}
      void applyMainWindowSettings(KConfigGroup&){}
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
   ReadOnlyPart(QObject*,const QString&){}
   QString localFilePath() {return QString(); }
   void setComponentData(const KComponentData&){} // actually member of PartBase
   KComponentData& componentData() { return *(KComponentData*)0;}
   QString m_file;
   };

   class ReadWritePart : public ReadOnlyPart
   {
   public:
   ReadWritePart(QObject*){}
   void setReadWrite(bool){}
   };

   class Factory : public KLibFactory
   {
      Q_OBJECT
   public:
   virtual KParts::Part* createPartObject( QWidget* /*parentWidget*/, const char * /*widgetName*/,
                                            QObject* /*parent*/, const char * /*name*/,
                                            const char* /*classname*/, const QStringList& /*args*/ ){return 0;}
   };
};
#endif


