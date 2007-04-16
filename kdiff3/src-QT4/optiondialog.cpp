/*
 *   kdiff3 - Text Diff And Merge Tool
 *   Copyright (C) 2002-2007  Joachim Eibl, joachim.eibl at gmx.de
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Steet, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLayout>
#include <QLineEdit> 
#include <QToolTip>
#include <QRadioButton>
#include <QGroupBox>
#include <QTextCodec>

#include <QSettings>
#include <QLocale>

#include <kapplication.h>
#include <kcolorbtn.h>
#include <kfontdialog.h> // For KFontChooser
#include <kiconloader.h>
#include <klocale.h>
#include <kconfig.h>
#include <kmessagebox.h>
#include <kmainwindow.h> //For ktoolbar.h

//#include <kkeydialog.h>
#include <map>

#include "optiondialog.h"
#include "diff.h"
#include "smalldialogs.h"

#include <iostream>

#ifndef KREPLACEMENTS_H
#include <kglobalsettings.h>
#endif

static QString s_historyEntryStartRegExpToolTip;
static QString s_historyEntryStartSortKeyOrderToolTip;
static QString s_autoMergeRegExpToolTip;
static QString s_historyStartRegExpToolTip;

void OptionDialog::addOptionItem(OptionItem* p)
{
   m_optionItemList.push_back(p);
}

class OptionItem
{
public:
   OptionItem( OptionDialog* pOptionDialog, const QString& saveName )
   {
      assert(pOptionDialog!=0);
      pOptionDialog->addOptionItem( this );
      m_saveName = saveName;
   }
   virtual ~OptionItem(){}
   virtual void setToDefault()=0;
   virtual void setToCurrent()=0;
   virtual void apply()=0;
   virtual void write(ValueMap*)=0;
   virtual void read(ValueMap*)=0;
   QString getSaveName(){return m_saveName;}
protected:
   QString m_saveName;
};

class OptionCheckBox : public QCheckBox, public OptionItem
{
public:
   OptionCheckBox( QString text, bool bDefaultVal, const QString& saveName, bool* pbVar,
                   QWidget* pParent, OptionDialog* pOD )
   : QCheckBox( text, pParent ), OptionItem( pOD, saveName )
   {
      m_pbVar = pbVar;
      m_bDefaultVal = bDefaultVal;
   }
   void setToDefault(){ setChecked( m_bDefaultVal );      }
   void setToCurrent(){ setChecked( *m_pbVar );           }
   void apply()       { *m_pbVar = isChecked();                              }
   void write(ValueMap* config){ config->writeEntry(m_saveName, *m_pbVar );   }
   void read (ValueMap* config){ *m_pbVar = config->readBoolEntry( m_saveName, *m_pbVar ); }
private:
   OptionCheckBox( const OptionCheckBox& ); // private copy constructor without implementation
   bool* m_pbVar;
   bool m_bDefaultVal;
};

class OptionRadioButton : public QRadioButton, public OptionItem
{
public:
   OptionRadioButton( QString text, bool bDefaultVal, const QString& saveName, bool* pbVar,
                   QWidget* pParent, OptionDialog* pOD )
   : QRadioButton( text, pParent ), OptionItem( pOD, saveName )
   {
      m_pbVar = pbVar;
      m_bDefaultVal = bDefaultVal;
   }
   void setToDefault(){ setChecked( m_bDefaultVal );      }
   void setToCurrent(){ setChecked( *m_pbVar );           }
   void apply()       { *m_pbVar = isChecked();                              }
   void write(ValueMap* config){ config->writeEntry(m_saveName, *m_pbVar );   }
   void read (ValueMap* config){ *m_pbVar = config->readBoolEntry( m_saveName, *m_pbVar ); }
private:
   OptionRadioButton( const OptionRadioButton& ); // private copy constructor without implementation
   bool* m_pbVar;
   bool m_bDefaultVal;
};


template<class T>
class OptionT : public OptionItem
{
public:
   OptionT( const T& defaultVal, const QString& saveName, T* pVar, OptionDialog* pOD )
   : OptionItem( pOD, saveName )
   {
      m_pVar = pVar;
      *m_pVar = defaultVal;
   }
   OptionT( const QString& saveName, T* pVar, OptionDialog* pOD )
   : OptionItem( pOD, saveName )
   {
      m_pVar = pVar;
   }
   void setToDefault(){}
   void setToCurrent(){}
   void apply()       {}
   void write(ValueMap* vm){ writeEntry( vm, m_saveName, *m_pVar ); }
   void read (ValueMap* vm){ readEntry ( vm, m_saveName, *m_pVar ); }
private:
   OptionT( const OptionT& ); // private copy constructor without implementation
   T* m_pVar;
};

template <class T> void writeEntry(ValueMap* vm, const QString& saveName, const T& v ) {   vm->writeEntry( saveName, v ); }
static void writeEntry(ValueMap* vm, const QString& saveName, const QStringList& v )   {   vm->writeEntry( saveName, v, '|' ); }

static void readEntry(ValueMap* vm, const QString& saveName, bool& v )       {   v = vm->readBoolEntry( saveName, v ); }
static void readEntry(ValueMap* vm, const QString& saveName, int&  v )       {   v = vm->readNumEntry( saveName, v ); }
static void readEntry(ValueMap* vm, const QString& saveName, QSize& v )      {   v = vm->readSizeEntry( saveName, &v ); }
static void readEntry(ValueMap* vm, const QString& saveName, QPoint& v )     {   v = vm->readPointEntry( saveName, &v ); }
static void readEntry(ValueMap* vm, const QString& saveName, QStringList& v ){   v = vm->readListEntry( saveName, QStringList(), '|' ); }

typedef OptionT<bool> OptionToggleAction;
typedef OptionT<int>  OptionNum;
typedef OptionT<QPoint> OptionPoint;
typedef OptionT<QSize> OptionSize;
typedef OptionT<QStringList> OptionStringList;

class OptionFontChooser : public KFontChooser, public OptionItem
{
public:
   OptionFontChooser( const QFont& defaultVal, const QString& saveName, QFont* pbVar, QWidget* pParent, OptionDialog* pOD )
   :KFontChooser( pParent,"font",true/*onlyFixed*/,QStringList(),false,6 ),
    OptionItem( pOD, saveName )
   {
      m_pbVar = pbVar;
      *m_pbVar = defaultVal;
      m_default = defaultVal;
   }
   void setToDefault(){ setFont( m_default, true /*only fixed*/ ); }
   void setToCurrent(){ setFont( *m_pbVar, true /*only fixed*/ ); }
   void apply()       { *m_pbVar = font();}
   void write(ValueMap* config){ config->writeEntry(m_saveName, *m_pbVar );   }
   void read (ValueMap* config){ *m_pbVar = config->readFontEntry( m_saveName, m_pbVar ); }
private:
   OptionFontChooser( const OptionToggleAction& ); // private copy constructor without implementation
   QFont* m_pbVar;
   QFont m_default;
};

class OptionColorButton : public KColorButton, public OptionItem
{
public:
   OptionColorButton( QColor defaultVal, const QString& saveName, QColor* pVar, QWidget* pParent, OptionDialog* pOD )
   : KColorButton( pParent ), OptionItem( pOD, saveName )
   {
      m_pVar = pVar;
      m_defaultVal = defaultVal;
   }
   void setToDefault(){ setColor( m_defaultVal );      }
   void setToCurrent(){ setColor( *m_pVar );           }
   void apply()       { *m_pVar = color();                              }
   void write(ValueMap* config){ config->writeEntry(m_saveName, *m_pVar );   }
   void read (ValueMap* config){ *m_pVar = config->readColorEntry( m_saveName, m_pVar ); }
private:
   OptionColorButton( const OptionColorButton& ); // private copy constructor without implementation
   QColor* m_pVar;
   QColor m_defaultVal;
};

class OptionLineEdit : public QComboBox, public OptionItem
{
public:
   OptionLineEdit( const QString& defaultVal, const QString& saveName, QString* pVar,
                   QWidget* pParent, OptionDialog* pOD )
   : QComboBox( pParent ), OptionItem( pOD, saveName )
   {
      setMinimumWidth(50);
      setEditable(true);
      m_pVar = pVar;
      m_defaultVal = defaultVal;
      m_list.push_back(defaultVal);
      insertText();
   }
   void setToDefault(){ setEditText( m_defaultVal );   }
   void setToCurrent(){ setEditText( *m_pVar );        }
   void apply()       { *m_pVar = currentText(); insertText();            }
   void write(ValueMap* config){ config->writeEntry( m_saveName, m_list, '|' );      }
   void read (ValueMap* config){ 
      m_list = config->readListEntry( m_saveName, QStringList(m_defaultVal), '|' ); 
      if ( !m_list.empty() ) *m_pVar = m_list.front();
      clear();
      insertItems(0,m_list);
   }
private:
   void insertText()
   {  // Check if the text exists. If yes remove it and push it in as first element
      QString current = currentText();
      m_list.removeAll( current );
      m_list.push_front( current );
      clear();
      if ( m_list.size()>10 ) 
         m_list.erase( m_list.begin()+10, m_list.end() );
      insertItems(0,m_list);
   }
   OptionLineEdit( const OptionLineEdit& ); // private copy constructor without implementation
   QString* m_pVar;
   QString m_defaultVal;
   QStringList m_list;
};

#if defined QT_NO_VALIDATOR
#error No validator
#endif
class OptionIntEdit : public QLineEdit, public OptionItem
{
public:
   OptionIntEdit( int defaultVal, const QString& saveName, int* pVar, int rangeMin, int rangeMax,
                   QWidget* pParent, OptionDialog* pOD )
   : QLineEdit( pParent ), OptionItem( pOD, saveName )
   {
      m_pVar = pVar;
      m_defaultVal = defaultVal;
      QIntValidator* v = new QIntValidator(this);
      v->setRange( rangeMin, rangeMax );
      setValidator( v );
   }
   void setToDefault(){ QString s;  s.setNum(m_defaultVal); setText( s );  }
   void setToCurrent(){ QString s;  s.setNum(*m_pVar);      setText( s );  }
   void apply()       { const QIntValidator* v=static_cast<const QIntValidator*>(validator());
                        *m_pVar = minMaxLimiter( text().toInt(), v->bottom(), v->top());
                        setText( QString::number(*m_pVar) );  }
   void write(ValueMap* config){ config->writeEntry(m_saveName, *m_pVar );   }
   void read (ValueMap* config){ *m_pVar = config->readNumEntry( m_saveName, *m_pVar ); }
private:
   OptionIntEdit( const OptionIntEdit& ); // private copy constructor without implementation
   int* m_pVar;
   int m_defaultVal;
};

class OptionComboBox : public QComboBox, public OptionItem
{
public:
   OptionComboBox( int defaultVal, const QString& saveName, int* pVarNum,
                   QWidget* pParent, OptionDialog* pOD )
   : QComboBox( pParent ), OptionItem( pOD, saveName )
   {
      setMinimumWidth(50);
      m_pVarNum = pVarNum;
      m_pVarStr = 0;
      m_defaultVal = defaultVal;
      setEditable(false);
   }
   OptionComboBox( int defaultVal, const QString& saveName, QString* pVarStr,
                   QWidget* pParent, OptionDialog* pOD )
   : QComboBox( pParent ), OptionItem( pOD, saveName )
   {
      m_pVarNum = 0;
      m_pVarStr = pVarStr;
      m_defaultVal = defaultVal;
      setEditable(false);
   }
   void setToDefault()
   { 
      setCurrentIndex( m_defaultVal ); 
      if (m_pVarStr!=0){ *m_pVarStr=currentText(); } 
   }
   void setToCurrent()
   { 
      if (m_pVarNum!=0) setCurrentIndex( *m_pVarNum );
      else              setText( *m_pVarStr );
   }
   void apply()
   { 
      if (m_pVarNum!=0){ *m_pVarNum = currentIndex(); }
      else             { *m_pVarStr = currentText(); }
   }
   void write(ValueMap* config)
   { 
      if (m_pVarStr!=0) config->writeEntry(m_saveName, *m_pVarStr );
      else              config->writeEntry(m_saveName, *m_pVarNum );   
   }
   void read (ValueMap* config)
   {
      if (m_pVarStr!=0)  setText( config->readEntry( m_saveName, currentText() ) );
      else               *m_pVarNum = config->readNumEntry( m_saveName, *m_pVarNum ); 
   }
private:
   OptionComboBox( const OptionIntEdit& ); // private copy constructor without implementation
   int* m_pVarNum;
   QString* m_pVarStr;
   int m_defaultVal;
   
   void setText(const QString& s)
   {
      // Find the string in the combobox-list, don't change the value if nothing fits.
      for( int i=0; i<count(); ++i )
      {
         if ( itemText(i)==s )
         {
            if (m_pVarNum!=0) *m_pVarNum = i;
            if (m_pVarStr!=0) *m_pVarStr = s;
            setCurrentIndex(i);
            return;
         }
      }
   }
};

class OptionEncodingComboBox : public QComboBox, public OptionItem
{
   std::vector<QTextCodec*> m_codecVec;
   QTextCodec** m_ppVarCodec;
public:
   OptionEncodingComboBox( const QString& saveName, QTextCodec** ppVarCodec,
                   QWidget* pParent, OptionDialog* pOD )
   : QComboBox( pParent ), OptionItem( pOD, saveName )
   {
      m_ppVarCodec = ppVarCodec;
      insertCodec( i18n("Unicode, 8 bit"),  QTextCodec::codecForName("UTF-8") );
      insertCodec( i18n("Unicode"), QTextCodec::codecForName("iso-10646-UCS-2") );
      insertCodec( i18n("Latin1"), QTextCodec::codecForName("iso 8859-1") );

      // First sort codec names:
      std::map<QString, QTextCodec*> names;
      QList<int> mibs = QTextCodec::availableMibs();
      foreach(int i, mibs)
      {
         QTextCodec* c = QTextCodec::codecForMib(i);
         if ( c!=0 )
            names[QString(c->name()).toUpper()]=c;
      }

      std::map<QString, QTextCodec*>::iterator it;
      for(it=names.begin();it!=names.end();++it)
      {
         insertCodec( "", it->second );
      }

      this->setToolTip( i18n(
         "Change this if non-ASCII characters are not displayed correctly."
         ));
   }
   void insertCodec( const QString& visibleCodecName, QTextCodec* c )
   {
      if (c!=0)
      {
         for( unsigned int i=0; i<m_codecVec.size(); ++i )
         {
            if ( c==m_codecVec[i] )
               return;  // don't insert any codec twice
         }
         addItem( visibleCodecName.isEmpty() ? QString(c->name()) : visibleCodecName+" ("+c->name()+")", (int)m_codecVec.size() );
         m_codecVec.push_back( c );
      }
   }
   void setToDefault()
   {
      QString defaultName = QTextCodec::codecForLocale()->name();
      for(int i=0;i<count();++i)
      {
         if (defaultName==itemText(i) &&
             m_codecVec[i]==QTextCodec::codecForLocale())
         {
            setCurrentIndex(i);
            if (m_ppVarCodec!=0){ *m_ppVarCodec=m_codecVec[i]; }
            return;
         }
      }

      setCurrentIndex( 0 );
      if (m_ppVarCodec!=0){ *m_ppVarCodec=m_codecVec[0]; }
   }
   void setToCurrent()
   {
      if (m_ppVarCodec!=0)
      {
         for(unsigned int i=0; i<m_codecVec.size(); ++i)
         {
            if ( *m_ppVarCodec==m_codecVec[i] )
            {
               setCurrentIndex( i );
               break;
            }
         }
      }
   }
   void apply()
   {
      if (m_ppVarCodec!=0){ *m_ppVarCodec = m_codecVec[ currentIndex() ]; }
   }
   void write(ValueMap* config)
   {
      if (m_ppVarCodec!=0) config->writeEntry(m_saveName, QString((*m_ppVarCodec)->name()) );
   }
   void read (ValueMap* config)
   {
      QString codecName = config->readEntry( m_saveName, m_codecVec[ currentIndex() ]->name() );
      for(unsigned int i=0; i<m_codecVec.size(); ++i)
      {
         if ( codecName == m_codecVec[i]->name() )
         {
            setCurrentIndex( i );
            if (m_ppVarCodec!=0) *m_ppVarCodec = m_codecVec[i];
            break;
         }
      }
   }
};


OptionDialog::OptionDialog( bool bShowDirMergeSettings, QWidget *parent, char *name )
  :KDialogBase( IconList, i18n("Configure"), Help|Default|Apply|Ok|Cancel,
                Ok, parent, name, true /*modal*/, true )
{
   setHelp( "kdiff3/index.html", QString::null );

   setupFontPage();
   setupColorPage();
   setupEditPage();
   setupDiffPage();
   setupMergePage();
   setupOtherOptions();
   if (bShowDirMergeSettings)
      setupDirectoryMergePage();

   setupRegionalPage();
   setupIntegrationPage();

   //setupKeysPage();

   // Initialize all values in the dialog
   resetToDefaults();
   slotApply();
}

OptionDialog::~OptionDialog( void )
{
}

void OptionDialog::setupOtherOptions()
{
   new OptionToggleAction( false, "AutoAdvance", &m_bAutoAdvance, this );
   new OptionToggleAction( true,  "ShowWhiteSpaceCharacters", &m_bShowWhiteSpaceCharacters, this );
   new OptionToggleAction( true,  "ShowWhiteSpace", &m_bShowWhiteSpace, this );
   new OptionToggleAction( false, "ShowLineNumbers", &m_bShowLineNumbers, this );
   new OptionToggleAction( true,  "HorizDiffWindowSplitting", &m_bHorizDiffWindowSplitting, this );
   new OptionToggleAction( false, "WordWrap", &m_bWordWrap, this );

   new OptionToggleAction( true,  "ShowIdenticalFiles", &m_bDmShowIdenticalFiles, this );

   new OptionToggleAction( true,  "Show Toolbar", &m_bShowToolBar, this );
   new OptionToggleAction( true,  "Show Statusbar", &m_bShowStatusBar, this );

   new OptionNum( (int)KToolBar::Top, "ToolBarPos", &m_toolBarPos, this );
   new OptionSize( QSize(600,400),"Geometry", &m_geometry, this );
   new OptionPoint( QPoint(0,22), "Position", &m_position, this );
   new OptionToggleAction( false, "WindowStateMaximised", &m_bMaximised, this );

   new OptionStringList( "RecentAFiles", &m_recentAFiles, this );
   new OptionStringList( "RecentBFiles", &m_recentBFiles, this );
   new OptionStringList( "RecentCFiles", &m_recentCFiles, this );
   new OptionStringList( "RecentOutputFiles", &m_recentOutputFiles, this );
}

void OptionDialog::setupFontPage( void )
{
   QFrame *page = addPage( i18n("Font"), i18n("Editor & Diff Output Font" ),
                             BarIcon("fonts", KIcon::SizeMedium ) );

   QVBoxLayout *topLayout = new QVBoxLayout( page );
   topLayout->setMargin( 5 );
   topLayout->setSpacing( spacingHint() );

   QFont defaultFont =
#ifdef _WIN32
      QFont("Courier New", 10 );
#elif defined( KREPLACEMENTS_H )
      QFont("Courier", 10 );
#else
      KGlobalSettings::fixedFont();
#endif

   OptionFontChooser* pFontChooser = new OptionFontChooser( defaultFont, "Font", &m_font, page, this );
   topLayout->addWidget( pFontChooser );

   QGridLayout *gbox = new QGridLayout();
   topLayout->addLayout( gbox );
   int line=0;

   OptionCheckBox* pItalicDeltas = new OptionCheckBox( i18n("Italic font for deltas"), false, "ItalicForDeltas", &m_bItalicForDeltas, page, this );
   gbox->addWidget( pItalicDeltas, line, 0, 1, 2 );
   pItalicDeltas->setToolTip( i18n(
      "Selects the italic version of the font for differences.\n"
      "If the font doesn't support italic characters, then this does nothing.")
      );
}


void OptionDialog::setupColorPage( void )
{
   QFrame *page = addPage( i18n("Color"), i18n("Colors Settings"),
      BarIcon("colorize", KIcon::SizeMedium ) );
   QVBoxLayout *topLayout = new QVBoxLayout( page );
   topLayout->setMargin( 5 );
   topLayout->setSpacing( spacingHint() );


  QGridLayout *gbox = new QGridLayout();
  gbox->setColumnStretch(1,5);
  topLayout->addLayout(gbox);

  QLabel* label;
  int line = 0;

  int depth = QPixmap::defaultDepth();
  bool bLowColor = depth<=8;

  label = new QLabel( i18n("Editor and Diff Views:"), page );
  gbox->addWidget( label, line, 0 );
  QFont f( label->font() );
  f.setBold(true);
  label->setFont(f);
  ++line;

  OptionColorButton* pFgColor = new OptionColorButton( Qt::black,"FgColor", &m_fgColor, page, this );
  label = new QLabel( i18n("Foreground color:"), page );
  label->setBuddy(pFgColor);
  gbox->addWidget( label, line, 0 );
  gbox->addWidget( pFgColor, line, 1 );
  ++line;

  OptionColorButton* pBgColor = new OptionColorButton( Qt::white, "BgColor", &m_bgColor, page, this );
  label = new QLabel( i18n("Background color:"), page );
  label->setBuddy(pBgColor);
  gbox->addWidget( label, line, 0 );
  gbox->addWidget( pBgColor, line, 1 );

  ++line;

  OptionColorButton* pDiffBgColor = new OptionColorButton( 
     bLowColor ? QColor(Qt::lightGray) : qRgb(224,224,224), "DiffBgColor", &m_diffBgColor, page, this );
  label = new QLabel( i18n("Diff background color:"), page );
  label->setBuddy(pDiffBgColor);
  gbox->addWidget( label, line, 0 );
  gbox->addWidget( pDiffBgColor, line, 1 );
  ++line;

  OptionColorButton* pColorA = new OptionColorButton(
     bLowColor ? qRgb(0,0,255) : qRgb(0,0,200)/*blue*/, "ColorA", &m_colorA, page, this );
  label = new QLabel( i18n("Color A:"), page );
  label->setBuddy(pColorA);
  gbox->addWidget( label, line, 0 );
  gbox->addWidget( pColorA, line, 1 );
  ++line;

  OptionColorButton* pColorB = new OptionColorButton(
     bLowColor ? qRgb(0,128,0) : qRgb(0,150,0)/*green*/, "ColorB", &m_colorB, page, this );
  label = new QLabel( i18n("Color B:"), page );
  label->setBuddy(pColorB);
  gbox->addWidget( label, line, 0 );
  gbox->addWidget( pColorB, line, 1 );
  ++line;

  OptionColorButton* pColorC = new OptionColorButton(
     bLowColor ? qRgb(128,0,128) : qRgb(150,0,150)/*magenta*/, "ColorC", &m_colorC, page, this );
  label = new QLabel( i18n("Color C:"), page );
  label->setBuddy(pColorC);
  gbox->addWidget( label, line, 0 );
  gbox->addWidget( pColorC, line, 1 );
  ++line;

  OptionColorButton* pColorForConflict = new OptionColorButton( Qt::red, "ColorForConflict", &m_colorForConflict, page, this );
  label = new QLabel( i18n("Conflict color:"), page );
  label->setBuddy(pColorForConflict);
  gbox->addWidget( label, line, 0 );
  gbox->addWidget( pColorForConflict, line, 1 );
  ++line;

  OptionColorButton* pColor = new OptionColorButton( 
     bLowColor ? qRgb(192,192,192) : qRgb(220,220,100), "CurrentRangeBgColor", &m_currentRangeBgColor, page, this );
  label = new QLabel( i18n("Current range background color:"), page );
  label->setBuddy(pColor);
  gbox->addWidget( label, line, 0 );
  gbox->addWidget( pColor, line, 1 );
  ++line;

  pColor = new OptionColorButton( 
     bLowColor ? qRgb(255,255,0) : qRgb(255,255,150), "CurrentRangeDiffBgColor", &m_currentRangeDiffBgColor, page, this );
  label = new QLabel( i18n("Current range diff background color:"), page );
  label->setBuddy(pColor);
  gbox->addWidget( label, line, 0 );
  gbox->addWidget( pColor, line, 1 );
  ++line;

  pColor = new OptionColorButton( qRgb(0xff,0xd0,0x80), "ManualAlignmentRangeColor", &m_manualHelpRangeColor, page, this );
  label = new QLabel( i18n("Color for manually aligned difference ranges:"), page );
  label->setBuddy(pColor);
  gbox->addWidget( label, line, 0 );
  gbox->addWidget( pColor, line, 1 );
  ++line;

  label = new QLabel( i18n("Directory Comparison View:"), page );
  gbox->addWidget( label, line, 0 );
  label->setFont(f);
  ++line;

  pColor = new OptionColorButton( qRgb(0,0xd0,0), "NewestFileColor", &m_newestFileColor, page, this );
  label = new QLabel( i18n("Newest file color:"), page );
  label->setBuddy(pColor);
  gbox->addWidget( label, line, 0 );
  gbox->addWidget( pColor, line, 1 );
  QString dirColorTip = i18n( "Changing this color will only be effective when starting the next directory comparison.");
  label->setToolTip( dirColorTip );
  ++line;

  pColor = new OptionColorButton( qRgb(0xf0,0,0), "OldestFileColor", &m_oldestFileColor, page, this );
  label = new QLabel( i18n("Oldest file color:"), page );
  label->setBuddy(pColor);
  gbox->addWidget( label, line, 0 );
  gbox->addWidget( pColor, line, 1 );
  label->setToolTip( dirColorTip );
  ++line;

  pColor = new OptionColorButton( qRgb(0xc0,0xc0,0), "MidAgeFileColor", &m_midAgeFileColor, page, this );
  label = new QLabel( i18n("Middle age file color:"), page );
  label->setBuddy(pColor);
  gbox->addWidget( label, line, 0 );
  gbox->addWidget( pColor, line, 1 );
  label->setToolTip( dirColorTip );
  ++line;

  pColor = new OptionColorButton( qRgb(0,0,0), "MissingFileColor", &m_missingFileColor, page, this );
  label = new QLabel( i18n("Color for missing files:"), page );
  label->setBuddy(pColor);
  gbox->addWidget( label, line, 0 );
  gbox->addWidget( pColor, line, 1 );
  label->setToolTip( dirColorTip );
  ++line;

  topLayout->addStretch(10);
}


void OptionDialog::setupEditPage( void )
{
   QFrame *page = addPage( i18n("Editor"), i18n("Editor Behavior"),
                           BarIcon("edit", KIcon::SizeMedium ) );
   QVBoxLayout *topLayout = new QVBoxLayout( page );
   topLayout->setMargin( 5 );
   topLayout->setSpacing( spacingHint() );


   QGridLayout *gbox = new QGridLayout();
   gbox->setColumnStretch(1,5);
   topLayout->addLayout( gbox );
   QLabel* label;
   int line=0;

   OptionCheckBox* pReplaceTabs = new OptionCheckBox( i18n("Tab inserts spaces"), false, "ReplaceTabs", &m_bReplaceTabs, page, this );
   gbox->addWidget( pReplaceTabs, line, 0, 1, 2 );
   pReplaceTabs->setToolTip( i18n(
      "On: Pressing tab generates the appropriate number of spaces.\n"
      "Off: A Tab-character will be inserted.")
      );
   ++line;

   OptionIntEdit* pTabSize = new OptionIntEdit( 8, "TabSize", &m_tabSize, 1, 100, page, this );
   label = new QLabel( i18n("Tab size:"), page );
   label->setBuddy( pTabSize );
   gbox->addWidget( label, line, 0 );
   gbox->addWidget( pTabSize, line, 1 );
   ++line;

   OptionCheckBox* pAutoIndentation = new OptionCheckBox( i18n("Auto indentation"), true, "AutoIndentation", &m_bAutoIndentation, page, this  );
   gbox->addWidget( pAutoIndentation, line, 0, 1, 2 );
   pAutoIndentation->setToolTip( i18n(
      "On: The indentation of the previous line is used for a new line.\n"
      ));
   ++line;

   OptionCheckBox* pAutoCopySelection = new OptionCheckBox( i18n("Auto copy selection"), false, "AutoCopySelection", &m_bAutoCopySelection, page, this );
   gbox->addWidget( pAutoCopySelection, line, 0, 1, 2 );
   pAutoCopySelection->setToolTip( i18n(
      "On: Any selection is immediately written to the clipboard.\n"
      "Off: You must explicitely copy e.g. via Ctrl-C."
      ));
   ++line;
   
   label = new QLabel( i18n("Line end style:"), page );
   gbox->addWidget( label, line, 0 );
   #ifdef _WIN32
   int defaultLineEndStyle = eLineEndDos;
   #else
   int defaultLineEndStyle = eLineEndUnix;
   #endif
   OptionComboBox* pLineEndStyle = new OptionComboBox( defaultLineEndStyle, "LineEndStyle", &m_lineEndStyle, page, this );
   gbox->addWidget( pLineEndStyle, line, 1 );
   pLineEndStyle->insertItem( eLineEndUnix, "Unix" );
   pLineEndStyle->insertItem( eLineEndDos, "Dos/Windows" );
   label->setToolTip( i18n(
      "Sets the line endings for when an edited file is saved.\n"
      "DOS/Windows: CR+LF; UNIX: LF; with CR=0D, LF=0A")
      );
   ++line;
      
   topLayout->addStretch(10);
}


void OptionDialog::setupDiffPage( void )
{
   QFrame *page = addPage( i18n("Diff"), i18n("Diff Settings"),
                           BarIcon("misc", KIcon::SizeMedium ) );
   QVBoxLayout *topLayout = new QVBoxLayout( page );
   topLayout->setMargin( 5 );
   topLayout->setSpacing( spacingHint() );

   QGridLayout *gbox = new QGridLayout();
   gbox->setColumnStretch(1,5);
   topLayout->addLayout( gbox );
   int line=0;

   QLabel* label=0;

   m_bPreserveCarriageReturn = false;
   //OptionCheckBox* pPreserveCarriageReturn = new OptionCheckBox( i18n("Preserve carriage return"), false, "PreserveCarriageReturn", &m_bPreserveCarriageReturn, page, this );
   //gbox->addWidget( pPreserveCarriageReturn, line, 0, 1, 2 );
   //pPreserveCarriageReturn->setToolTip( i18n(
   //   "Show carriage return characters '\\r' if they exist.\n"
   //   "Helps to compare files that were modified under different operating systems.")
   //   );
   //++line;
   QString treatAsWhiteSpace = " ("+i18n("Treat as white space.")+")";

   OptionCheckBox* pIgnoreNumbers = new OptionCheckBox( i18n("Ignore numbers")+treatAsWhiteSpace, false, "IgnoreNumbers", &m_bIgnoreNumbers, page, this );
   gbox->addWidget( pIgnoreNumbers, line, 0, 1, 2 );
   pIgnoreNumbers->setToolTip( i18n(
      "Ignore number characters during line matching phase. (Similar to Ignore white space.)\n"
      "Might help to compare files with numeric data.")
      );
   ++line;

   OptionCheckBox* pIgnoreComments = new OptionCheckBox( i18n("Ignore C/C++ comments")+treatAsWhiteSpace, false, "IgnoreComments", &m_bIgnoreComments, page, this );
   gbox->addWidget( pIgnoreComments, line, 0, 1, 2 );
   pIgnoreComments->setToolTip( i18n( "Treat C/C++ comments like white space.")
      );
   ++line;

   OptionCheckBox* pIgnoreCase = new OptionCheckBox( i18n("Ignore case")+treatAsWhiteSpace, false, "IgnoreCase", &m_bIgnoreCase, page, this );
   gbox->addWidget( pIgnoreCase, line, 0, 1, 2 );
   pIgnoreCase->setToolTip( i18n(
      "Treat case differences like white space changes. ('a'<=>'A')")
      );
   ++line;

   label = new QLabel( i18n("Preprocessor command:"), page );
   gbox->addWidget( label, line, 0 );
   OptionLineEdit* pLE = new OptionLineEdit( "", "PreProcessorCmd", &m_PreProcessorCmd, page, this );
   gbox->addWidget( pLE, line, 1 );
   label->setToolTip( i18n("User defined pre-processing. (See the docs for details.)") );
   ++line;

   label = new QLabel( i18n("Line-matching preprocessor command:"), page );
   gbox->addWidget( label, line, 0 );
   pLE = new OptionLineEdit( "", "LineMatchingPreProcessorCmd", &m_LineMatchingPreProcessorCmd, page, this );
   gbox->addWidget( pLE, line, 1 );
   label->setToolTip( i18n("This pre-processor is only used during line matching.\n(See the docs for details.)") );
   ++line;

   OptionCheckBox* pTryHard = new OptionCheckBox( i18n("Try hard (slower)"), true, "TryHard", &m_bTryHard, page, this );
   gbox->addWidget( pTryHard, line, 0, 1, 2 );
   pTryHard->setToolTip( i18n(
      "Enables the --minimal option for the external diff.\n"
      "The analysis of big files will be much slower.")
      );
   ++line;

   topLayout->addStretch(10);
}

void OptionDialog::setupMergePage( void )
{
   QFrame *page = addPage( i18n("Merge"), i18n("Merge Settings"),
                           BarIcon("misc", KIcon::SizeMedium ) );
   QVBoxLayout *topLayout = new QVBoxLayout( page );
   topLayout->setMargin( 5 );
   topLayout->setSpacing( spacingHint() );

   QGridLayout *gbox = new QGridLayout();
   gbox->setColumnStretch(1,5);
   topLayout->addLayout( gbox );
   int line=0;

   QLabel* label=0;

   label = new QLabel( i18n("Auto advance delay (ms):"), page );
   gbox->addWidget( label, line, 0 );
   OptionIntEdit* pAutoAdvanceDelay = new OptionIntEdit( 500, "AutoAdvanceDelay", &m_autoAdvanceDelay, 0, 2000, page, this );
   gbox->addWidget( pAutoAdvanceDelay, line, 1 );
   label->setToolTip(i18n(
      "When in Auto-Advance mode the result of the current selection is shown \n"
      "for the specified time, before jumping to the next conflict. Range: 0-2000 ms")
      );
   ++line;

   label = new QLabel( i18n("White space 2-file merge default:"), page );
   gbox->addWidget( label, line, 0 );
   OptionComboBox* pWhiteSpace2FileMergeDefault = new OptionComboBox( 0, "WhiteSpace2FileMergeDefault", &m_whiteSpace2FileMergeDefault, page, this );
   gbox->addWidget( pWhiteSpace2FileMergeDefault, line, 1 );
   pWhiteSpace2FileMergeDefault->insertItem( 0, i18n("Manual Choice") );
   pWhiteSpace2FileMergeDefault->insertItem( 1, "A" );
   pWhiteSpace2FileMergeDefault->insertItem( 2, "B" );
   label->setToolTip( i18n(
         "Allow the merge algorithm to automatically select an input for "
         "white-space-only changes." )
                    );
   ++line;

   label = new QLabel( i18n("White space 3-file merge default:"), page );
   gbox->addWidget( label, line, 0 );
   OptionComboBox* pWhiteSpace3FileMergeDefault = new OptionComboBox( 0, "WhiteSpace3FileMergeDefault", &m_whiteSpace3FileMergeDefault, page, this );
   gbox->addWidget( pWhiteSpace3FileMergeDefault, line, 1 );
   pWhiteSpace3FileMergeDefault->insertItem( 0, i18n("Manual Choice") );
   pWhiteSpace3FileMergeDefault->insertItem( 1, "A" );
   pWhiteSpace3FileMergeDefault->insertItem( 2, "B" );
   pWhiteSpace3FileMergeDefault->insertItem( 3, "C" );
   label->setToolTip( i18n(
         "Allow the merge algorithm to automatically select an input for "
         "white-space-only changes." )
                    );
   ++line;

   QGroupBox* pGroupBox = new QGroupBox( i18n("Automatic Merge Regular Expression") );
   gbox->addWidget( pGroupBox, line, 0, 1, 2 );
   ++line;
   {
      QGridLayout* gbox = new QGridLayout( pGroupBox );
      gbox->setMargin(spacingHint());
      gbox->setColumnStretch(1,10);
      int line = 0;

      label = new QLabel( i18n("Auto merge regular expression:"), page );
      gbox->addWidget( label, line, 0 );
      m_pAutoMergeRegExpLineEdit = new OptionLineEdit( ".*\\$(Version|Header|Date|Author).*\\$.*", "AutoMergeRegExp", &m_autoMergeRegExp, page, this );
      gbox->addWidget( m_pAutoMergeRegExpLineEdit, line, 1 );
      s_autoMergeRegExpToolTip = i18n("Regular expression for lines where KDiff3 should automatically choose one source.\n"
            "When a line with a conflict matches the regular expression then\n"
            "- if available - C, otherwise B will be chosen.");
      label->setToolTip( s_autoMergeRegExpToolTip );
      ++line;

      OptionCheckBox* pAutoMergeRegExp = new OptionCheckBox( i18n("Run regular expression auto merge on merge start"), false, "RunRegExpAutoMergeOnMergeStart", &m_bRunRegExpAutoMergeOnMergeStart, page, this );
      gbox->addWidget( pAutoMergeRegExp, line, 0, 1, 2 );
      pAutoMergeRegExp->setToolTip( i18n( "Run the merge for auto merge regular expressions\n"
            "immediately when a merge starts.\n"));
      ++line;
   }

   pGroupBox = new QGroupBox( i18n("Version Control History Merging") );
   gbox->addWidget( pGroupBox, line, 0, 1, 2 );
   ++line;
   {
      QGridLayout* gbox = new QGridLayout( pGroupBox );
      gbox->setMargin( spacingHint() );
      gbox->setColumnStretch(1,10);
      int line = 0;

      label = new QLabel( i18n("History start regular expression:"), page );
      gbox->addWidget( label, line, 0 );
      m_pHistoryStartRegExpLineEdit = new OptionLineEdit( ".*\\$Log.*\\$.*", "HistoryStartRegExp", &m_historyStartRegExp, page, this );
      gbox->addWidget( m_pHistoryStartRegExpLineEdit, line, 1 );
      s_historyStartRegExpToolTip = i18n("Regular expression for the start of the version control history entry.\n"
            "Usually this line contains the \"$Log$\"-keyword.\n"
            "Default value: \".*\\$Log.*\\$.*\"");
      label->setToolTip( s_historyStartRegExpToolTip );
      ++line;
   
      label = new QLabel( i18n("History entry start regular expression:"), page );
      gbox->addWidget( label, line, 0 );
      // Example line:  "** \main\rolle_fsp_dev_008\1   17 Aug 2001 10:45:44   rolle"
      QString historyEntryStartDefault =
            "\\s*\\\\main\\\\(\\S+)\\s+"  // Start with  "\main\"
            "([0-9]+) "          // day
            "(Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec) " //month
            "([0-9][0-9][0-9][0-9]) " // year
            "([0-9][0-9]:[0-9][0-9]:[0-9][0-9])\\s+(.*)";  // time, name
   
      m_pHistoryEntryStartRegExpLineEdit = new OptionLineEdit( historyEntryStartDefault, "HistoryEntryStartRegExp", &m_historyEntryStartRegExp, page, this );
      gbox->addWidget( m_pHistoryEntryStartRegExpLineEdit, line, 1 );
      s_historyEntryStartRegExpToolTip = i18n("A version control history entry consists of several lines.\n"
            "Specify the regular expression to detect the first line (without the leading comment).\n"
            "Use parentheses to group the keys you want to use for sorting.\n"
            "If left empty, then KDiff3 assumes that empty lines separate history entries.\n"
            "See the documentation for details.");
      label->setToolTip( s_historyEntryStartRegExpToolTip );
      ++line;
   
      m_pHistoryMergeSorting = new OptionCheckBox( i18n("History merge sorting"), false, "HistoryMergeSorting", &m_bHistoryMergeSorting, page, this );
      gbox->addWidget( m_pHistoryMergeSorting, line, 0, 1, 2 );
      m_pHistoryMergeSorting->setToolTip( i18n("Sort version control history by a key.") );
      ++line;
            //QString branch = newHistoryEntry.cap(1);
            //int day    = newHistoryEntry.cap(2).toInt();
            //int month  = QString("Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec").find(newHistoryEntry.cap(3))/4 + 1;
            //int year   = newHistoryEntry.cap(4).toInt();
            //QString time = newHistoryEntry.cap(5);
            //QString name = newHistoryEntry.cap(6);
      QString defaultSortKeyOrder = "4,3,2,5,1,6"; //QDate(year,month,day).toString(Qt::ISODate) +" "+ time + " " + branch + " " + name;

      label = new QLabel( i18n("History entry start sort key order:"), page );
      gbox->addWidget( label, line, 0 );
      m_pHistorySortKeyOrderLineEdit = new OptionLineEdit( defaultSortKeyOrder, "HistoryEntryStartSortKeyOrder", &m_historyEntryStartSortKeyOrder, page, this );
      gbox->addWidget( m_pHistorySortKeyOrderLineEdit, line, 1 );
      s_historyEntryStartSortKeyOrderToolTip = i18n("Each parentheses used in the regular expression for the history start entry\n"
            "groups a key that can be used for sorting.\n"
            "Specify the list of keys (that are numbered in order of occurrence\n"
            "starting with 1) using ',' as separator (e.g. \"4,5,6,1,2,3,7\").\n"
            "If left empty, then no sorting will be done.\n"
            "See the documentation for details.");
      label->setToolTip( s_historyEntryStartSortKeyOrderToolTip );
      m_pHistorySortKeyOrderLineEdit->setEnabled(false);
      connect( m_pHistoryMergeSorting, SIGNAL(toggled(bool)), m_pHistorySortKeyOrderLineEdit, SLOT(setEnabled(bool)));
      ++line;

      m_pHistoryAutoMerge = new OptionCheckBox( i18n("Merge version control history on merge start"), false, "RunHistoryAutoMergeOnMergeStart", &m_bRunHistoryAutoMergeOnMergeStart, page, this );
      gbox->addWidget( m_pHistoryAutoMerge, line, 0, 1, 2 );
      m_pHistoryAutoMerge->setToolTip( i18n("Run version control history automerge on merge start.") );
      ++line;

      OptionIntEdit* pMaxNofHistoryEntries = new OptionIntEdit( -1, "MaxNofHistoryEntries", &m_maxNofHistoryEntries, -1, 1000, page, this );
      label = new QLabel( i18n("Max number of history entries:"), page );
      gbox->addWidget( label, line, 0 );
      gbox->addWidget( pMaxNofHistoryEntries, line, 1 );
      pMaxNofHistoryEntries->setToolTip( i18n("Cut off after specified number. Use -1 for infinite number of entries.") );
      ++line;
   }

   QPushButton* pButton = new QPushButton( i18n("Test your regular expressions"), page );
   gbox->addWidget( pButton, line, 0 );
   connect( pButton, SIGNAL(clicked()), this, SLOT(slotHistoryMergeRegExpTester()));
   ++line;

   label = new QLabel( i18n("Irrelevant merge command:"), page );
   gbox->addWidget( label, line, 0 );
   OptionLineEdit* pLE = new OptionLineEdit( "", "IrrelevantMergeCmd", &m_IrrelevantMergeCmd, page, this );
   gbox->addWidget( pLE, line, 1 );
   label->setToolTip( i18n("If specified this script is run after automerge\n"
         "when no other relevant changes were detected.\n"
         "Called with the parameters: filename1 filename2 filename3") );
   ++line;


   OptionCheckBox* pAutoSaveAndQuit = new OptionCheckBox( i18n("Auto save and quit on merge without conflicts"), false,
      "AutoSaveAndQuitOnMergeWithoutConflicts", &m_bAutoSaveAndQuitOnMergeWithoutConflicts, page, this );
   gbox->addWidget( pAutoSaveAndQuit, line, 0, 1, 2 );
   pAutoSaveAndQuit->setToolTip( i18n("When KDiff3 was started for a file-merge from the commandline and all\n" 
                                         "conflicts are solvable without user interaction then automatically save and quit.\n"
                                         "(Similar to command line option \"--auto\".") );
   ++line;

   topLayout->addStretch(10);
}

void OptionDialog::setupDirectoryMergePage( void )
{
   QFrame *page = addPage( i18n("Directory Merge"), i18n("Directory Merge"),
                           BarIcon("folder", KIcon::SizeMedium ) );
   QVBoxLayout *topLayout = new QVBoxLayout( page );
   topLayout->setMargin( 5 );
   topLayout->setSpacing( spacingHint() );

   QGridLayout *gbox = new QGridLayout();
   gbox->setColumnStretch(1,5);
   topLayout->addLayout( gbox );
   int line=0;

   OptionCheckBox* pRecursiveDirs = new OptionCheckBox( i18n("Recursive directories"), true, "RecursiveDirs", &m_bDmRecursiveDirs, page, this );
   gbox->addWidget( pRecursiveDirs, line, 0, 1, 2 );
   pRecursiveDirs->setToolTip( i18n("Whether to analyze subdirectories or not.") );
   ++line;
   QLabel* label = new QLabel( i18n("File pattern(s):"), page );
   gbox->addWidget( label, line, 0 );
   OptionLineEdit* pFilePattern = new OptionLineEdit( "*", "FilePattern", &m_DmFilePattern, page, this );
   gbox->addWidget( pFilePattern, line, 1 );
   label->setToolTip( i18n(
      "Pattern(s) of files to be analyzed. \n"
      "Wildcards: '*' and '?'\n"
      "Several Patterns can be specified by using the separator: ';'"
      ));
   ++line;

   label = new QLabel( i18n("File-anti-pattern(s):"), page );
   gbox->addWidget( label, line, 0 );
   OptionLineEdit* pFileAntiPattern = new OptionLineEdit( "*.orig;*.o;*.obj", "FileAntiPattern", &m_DmFileAntiPattern, page, this );
   gbox->addWidget( pFileAntiPattern, line, 1 );
   label->setToolTip( i18n(
      "Pattern(s) of files to be excluded from analysis. \n"
      "Wildcards: '*' and '?'\n"
      "Several Patterns can be specified by using the separator: ';'"
      ));
   ++line;

   label = new QLabel( i18n("Dir-anti-pattern(s):"), page );
   gbox->addWidget( label, line, 0 );
   OptionLineEdit* pDirAntiPattern = new OptionLineEdit( "CVS;.deps;.svn", "DirAntiPattern", &m_DmDirAntiPattern, page, this );
   gbox->addWidget( pDirAntiPattern, line, 1 );
   label->setToolTip( i18n(
      "Pattern(s) of directories to be excluded from analysis. \n"
      "Wildcards: '*' and '?'\n"
      "Several Patterns can be specified by using the separator: ';'"
      ));
   ++line;

   OptionCheckBox* pUseCvsIgnore = new OptionCheckBox( i18n("Use .cvsignore"), false, "UseCvsIgnore", &m_bDmUseCvsIgnore, page, this );
   gbox->addWidget( pUseCvsIgnore, line, 0, 1, 2 );
   pUseCvsIgnore->setToolTip( i18n(
      "Extends the antipattern to anything that would be ignored by CVS.\n"
      "Via local \".cvsignore\"-files this can be directory specific."
      ));
   ++line;

   OptionCheckBox* pFindHidden = new OptionCheckBox( i18n("Find hidden files and directories"), true, "FindHidden", &m_bDmFindHidden, page, this );
   gbox->addWidget( pFindHidden, line, 0, 1, 2 );
#ifdef _WIN32
   pFindHidden->setToolTip( i18n("Finds files and directories with the hidden attribute.") );
#else
   pFindHidden->setToolTip( i18n("Finds files and directories starting with '.'.") );
#endif
   ++line;

   OptionCheckBox* pFollowFileLinks = new OptionCheckBox( i18n("Follow file links"), false, "FollowFileLinks", &m_bDmFollowFileLinks, page, this );
   gbox->addWidget( pFollowFileLinks, line, 0, 1, 2 );
   pFollowFileLinks->setToolTip( i18n(
      "On: Compare the file the link points to.\n"
      "Off: Compare the links."
      ));
   ++line;

   OptionCheckBox* pFollowDirLinks = new OptionCheckBox( i18n("Follow directory links"), false, "FollowDirLinks", &m_bDmFollowDirLinks, page, this );
   gbox->addWidget( pFollowDirLinks, line, 0, 1, 2 );
   pFollowDirLinks->setToolTip(    i18n(
      "On: Compare the directory the link points to.\n"
      "Off: Compare the links."
      ));
   ++line;

   //OptionCheckBox* pShowOnlyDeltas = new OptionCheckBox( i18n("List only deltas"),false,"ListOnlyDeltas", &m_bDmShowOnlyDeltas, page, this );
   //gbox->addWidget( pShowOnlyDeltas, line, 0, 1, 2 );
   //pShowOnlyDeltas->setToolTip( i18n(
   //              "Files and directories without change will not appear in the list."));
   //++line;

#ifdef _WIN32
   bool bCaseSensitiveFilenameComparison = false;
#else
   bool bCaseSensitiveFilenameComparison = true;
#endif
   OptionCheckBox* pCaseSensitiveFileNames = new OptionCheckBox( i18n("Case sensitive filename comparison"),bCaseSensitiveFilenameComparison,"CaseSensitiveFilenameComparison", &m_bDmCaseSensitiveFilenameComparison, page, this );
   gbox->addWidget( pCaseSensitiveFileNames, line, 0, 1, 2 );
   pCaseSensitiveFileNames->setToolTip( i18n(
                 "The directory comparison will compare files or directories when their names match.\n"
                 "Set this option if the case of the names must match. (Default for Windows is off, otherwise on.)"));
   ++line;

   QGroupBox* pBG = new QGroupBox( i18n("File Comparison Mode") );
   gbox->addWidget( pBG, line, 0, 1, 2 );

   QVBoxLayout* pBGLayout = new QVBoxLayout( pBG );
   pBGLayout->setMargin(spacingHint());
   
   OptionRadioButton* pBinaryComparison = new OptionRadioButton( i18n("Binary comparison"), true, "BinaryComparison", &m_bDmBinaryComparison, pBG, this );
   pBinaryComparison->setToolTip( i18n("Binary comparison of each file. (Default)") );
   pBGLayout->addWidget( pBinaryComparison );
   
   OptionRadioButton* pFullAnalysis = new OptionRadioButton( i18n("Full analysis"), false, "FullAnalysis", &m_bDmFullAnalysis, pBG, this );
   pFullAnalysis->setToolTip( i18n("Do a full analysis and show statistics information in extra columns.\n"
                                      "(Slower than a binary comparison, much slower for binary files.)") );
   pBGLayout->addWidget( pFullAnalysis );
   
   OptionRadioButton* pTrustDate = new OptionRadioButton( i18n("Trust the size and modification date (unsafe)"), false, "TrustDate", &m_bDmTrustDate, pBG, this );
   pTrustDate->setToolTip( i18n("Assume that files are equal if the modification date and file length are equal.\n"
                                   "Files with equal contents but different modification dates will appear as different.\n"
                                     "Useful for big directories or slow networks.") );
   pBGLayout->addWidget( pTrustDate );
                                     
   OptionRadioButton* pTrustDateFallbackToBinary = new OptionRadioButton( i18n("Trust the size and date, but use binary comparison if date doesn't match (unsafe)"), false, "TrustDateFallbackToBinary", &m_bDmTrustDateFallbackToBinary, pBG, this );
   pTrustDateFallbackToBinary->setToolTip( i18n("Assume that files are equal if the modification date and file length are equal.\n"
                                     "If the date isn't equal but the sizes are, use binary comparison.\n"
                                     "Useful for big directories or slow networks.") );
   pBGLayout->addWidget( pTrustDateFallbackToBinary );

   OptionRadioButton* pTrustSize = new OptionRadioButton( i18n("Trust the size (unsafe)"), false, "TrustSize", &m_bDmTrustSize, pBG, this );
   pTrustSize->setToolTip( i18n("Assume that files are equal if their file lengths are equal.\n"
                                   "Useful for big directories or slow networks when the date is modified during download.") );
   pBGLayout->addWidget( pTrustSize );

   ++line;
   

   // Some two Dir-options: Affects only the default actions.
   OptionCheckBox* pSyncMode = new OptionCheckBox( i18n("Synchronize directories"), false,"SyncMode", &m_bDmSyncMode, page, this );
   gbox->addWidget( pSyncMode, line, 0, 1, 2 );
   pSyncMode->setToolTip( i18n(
                  "Offers to store files in both directories so that\n"
                  "both directories are the same afterwards.\n"
                  "Works only when comparing two directories without specifying a destination."  ) );
   ++line;

   // Allow white-space only differences to be considered equal
   OptionCheckBox* pWhiteSpaceDiffsEqual = new OptionCheckBox( i18n("White space differences considered equal"), true,"WhiteSpaceEqual", &m_bDmWhiteSpaceEqual, page, this );
   gbox->addWidget( pWhiteSpaceDiffsEqual, line, 0, 1, 2 );
   pWhiteSpaceDiffsEqual->setToolTip( i18n(
                  "If files differ only by white space consider them equal.\n"
                  "This is only active when full analysis is chosen."  ) );
   connect(pFullAnalysis, SIGNAL(toggled(bool)), pWhiteSpaceDiffsEqual, SLOT(setEnabled(bool)));
   pWhiteSpaceDiffsEqual->setEnabled(false);
   ++line;

   OptionCheckBox* pCopyNewer = new OptionCheckBox( i18n("Copy newer instead of merging (unsafe)"), false, "CopyNewer", &m_bDmCopyNewer, page, this );
   gbox->addWidget( pCopyNewer, line, 0, 1, 2 );
   pCopyNewer->setToolTip( i18n(
                  "Don't look inside, just take the newer file.\n"
                  "(Use this only if you know what you are doing!)\n"
                  "Only effective when comparing two directories."  ) );
   ++line;

   OptionCheckBox* pCreateBakFiles = new OptionCheckBox( i18n("Backup files (.orig)"), true, "CreateBakFiles", &m_bDmCreateBakFiles, page, this );
   gbox->addWidget( pCreateBakFiles, line, 0, 1, 2 );
   pCreateBakFiles->setToolTip( i18n(
                 "When a file would be saved over an old file, then the old file\n"
                 "will be renamed with a '.orig'-extension instead of being deleted."));
   ++line;

   topLayout->addStretch(10);
}
/*
static void insertCodecs(OptionComboBox* p)
{
   std::multimap<QString,QString> m;  // Using the multimap for case-insensitive sorting.
   int i;
   for(i=0;;++i)
   {
      QTextCodec* pCodec = QTextCodec::codecForIndex ( i );
      if ( pCodec != 0 )  m.insert( std::make_pair( QString(pCodec->mimeName()).upper(), pCodec->mimeName()) );
      else                break;
   }
   
   p->insertItem( i18n("Auto"), 0 );
   std::multimap<QString,QString>::iterator mi;
   for(mi=m.begin(), i=0; mi!=m.end(); ++mi, ++i)
      p->insertItem(mi->second, i+1);
}
*/

// UTF8-Codec that saves a BOM
// UTF8-Codec that saves a BOM
class Utf8BOMCodec : public QTextCodec
{
   public:
   QByteArray name () const { return "UTF-8-BOM"; }
   int mibEnum () const { return 2123; }
   QByteArray convertFromUnicode ( const QChar * input, int number, ConverterState * pState ) const
   {
      QByteArray r;
      if ( pState && pState->state_data[0]==0)
      {
        r += "\xEF\xBB\xBF";
        pState->state_data[0]=1;
      }

      r += QString::fromRawData( input, number ).toUtf8();
      return r;
   }
   QString convertToUnicode ( const char * p, int len, ConverterState* ) const
   {
      return QString::fromUtf8 ( p, len );
   }

   class UTF8BOMEncoder : public QTextEncoder
   {
      bool bBOMAdded;
   public:
      UTF8BOMEncoder(const QTextCodec* pTC):QTextEncoder(pTC)  {  bBOMAdded=false;  }
      QByteArray fromUnicode(const QString& uc, int& lenInOut )
      {
         QByteArray r;
         if (!bBOMAdded)
         {
            r += "\xEF\xBB\xBF";
            bBOMAdded=true;
         }
         r += uc.toUtf8();
         lenInOut = r.length();
         return r;
      }
   };
   QTextEncoder* makeEncoder() const
   {
      return new UTF8BOMEncoder(this);
   }

   class UTF8BOMDecoder : public QTextDecoder
   {
      QTextDecoder *m_pDecoder;
   public:
      UTF8BOMDecoder(const QTextCodec* pTC) : QTextDecoder(pTC)  {  m_pDecoder = QTextCodec::codecForName("UTF-8")->makeDecoder();  }
      ~UTF8BOMDecoder() {
         delete m_pDecoder;
      }
      QString toUnicode( const char* p, int len)
      {
         return m_pDecoder->toUnicode( p, len );
      }
   };
   QTextDecoder* makeDecoder() const
   {
      return new UTF8BOMDecoder(this);
   }
};

void OptionDialog::setupRegionalPage( void )
{
   new Utf8BOMCodec();

   QFrame *page = addPage( i18n("Regional Settings"), i18n("Regional Settings"),
                           BarIcon("locale"/*"charset"*/, KIcon::SizeMedium ) );
   QVBoxLayout *topLayout = new QVBoxLayout( page );
   topLayout->setMargin( 5 );
   topLayout->setSpacing( spacingHint() );

   QGridLayout *gbox = new QGridLayout();
   gbox->setColumnStretch(1,5);
   topLayout->addLayout( gbox );
   int line=0;

   QLabel* label;

#ifdef KREPLACEMENTS_H

static char* countryMap[]={
"af Afrikaans",
"ar Arabic",
"az Azerbaijani",
"be Belarusian",
"bg Bulgarian",
"bn Bengali",
"bo Tibetan",
"br Breton",
"bs Bosnian",
"ca Catalan",
"cs Czech",
"cy Welsh",
"da Danish",
"de German",
"el Greek",
"en_GB British English",
"eo Esperanto",
"es Spanish",
"et Estonian",
"eu Basque",
"fa Farsi (Persian)",
"fi Finnish",
"fo Faroese",
"fr French",
"ga Irish Gaelic",
"gl Galician",
"gu Gujarati",
"he Hebrew",
"hi Hindi",
"hr Croatian",
"hsb Upper Sorbian",
"hu Hungarian",
"id Indonesian",
"is Icelandic",
"it Italian",
"ja Japanese",
"ka Georgian",
"ko Korean",
"ku Kurdish",
"lo Lao",
"lt Lithuanian",
"lv Latvian",
"mi Maori",
"mk Macedonian",
"mn Mongolian",
"ms Malay",
"mt Maltese",
"nb Norwegian Bookmal",
"nds Low Saxon",
"nl Dutch",
"nn Norwegian Nynorsk",
"nso Northern Sotho",
"oc Occitan",
"pl Polish",
"pt Portuguese",
"pt_BR Brazilian Portuguese",
"ro Romanian",
"ru Russian",
"rw Kinyarwanda",
"se Northern Sami",
"sk Slovak",
"sl Slovenian",
"sq Albanian",
"sr Serbian",
"sr@Latn Serbian",
"ss Swati",
"sv Swedish",
"ta Tamil",
"tg Tajik",
"th Thai",
"tr Turkish",
"uk Ukrainian",
"uz Uzbek",
"ven Venda",
"vi Vietnamese",
"wa Walloon",
"xh Xhosa",
"zh_CN Chinese Simplified",
"zh_TW Chinese Traditional",
"zu Zulu"
};

   label = new QLabel( i18n("Language (restart required)"), page );
   gbox->addWidget( label, line, 0 );
   OptionComboBox* pLanguage = new OptionComboBox( 0, "Language", &m_language, page, this );
   gbox->addWidget( pLanguage, line, 1 );
   pLanguage->addItem( "Auto" );  // Must not translate, won't work otherwise!
   pLanguage->addItem( "en_orig" );
      
   // Read directory: Find all kdiff3_*.qm-files and insert the found files here selection   
   FileAccess fa( getTranslationDir() );
   t_DirectoryList dirList;
   fa.listDir( &dirList, false, false, "kdiff3_*.qm", "", "*", false, false );
   t_DirectoryList::iterator i;
   for( i=dirList.begin(); i!=dirList.end(); ++i)
   {      
      QString fileName = i->fileName();
      // Skip the "kdiff3_" and omit the .qm
      QString languageId = fileName.mid(7, fileName.length()-10 );
      
      unsigned int countryIdx=0;
      for(countryIdx=0; countryIdx< sizeof(countryMap)/sizeof(countryMap[0]); ++countryIdx )
      {
         QString fullName = countryMap[countryIdx];
         if ( languageId+" " == fullName.left(languageId.length()+1) )
         {
            languageId += " (" + fullName.mid(languageId.length()+1) + ")";
         }
      }
      
      pLanguage->addItem( languageId );
   }
   
   label->setToolTip( i18n(
      "Choose the language of the GUI-strings or \"Auto\".\n" 
      "For a change of language to take place, quit and restart KDiff3.") 
      );
   ++line;
/*
   label = new QLabel( i18n("Codec for file contents"), page );
   gbox->addWidget( label, line, 0 );
   OptionComboBox* pFileCodec = new OptionComboBox( 0, "FileCodec", &m_fileCodec, page, this );
   gbox->addWidget( pFileCodec, line, 1 );
   insertCodecs( pFileCodec );
   label->setToolTip( i18n(
      "Choose the codec that should be used for your input files\n"
      "or \"Auto\" if unsure." ) 
      );
   ++line;
*/      
#endif

   m_pSameEncoding = new OptionCheckBox( i18n("Use the same encoding for everything:"), true, "SameEncoding", &m_bSameEncoding, page, this );
   gbox->addWidget( m_pSameEncoding, line, 0, 1, 2 );
   m_pSameEncoding->setToolTip( i18n(
                  "Enable this allows to change all encodings by changing the first only.\n"
                  "Disable this if different individual settings are needed."
                  ) );
   ++line;

   label = new QLabel( i18n("Note: Local Encoding is ") + "\"" + QTextCodec::codecForLocale()->name() + "\"", page );
   gbox->addWidget( label, line, 0 );
   ++line;

   label = new QLabel( i18n("File Encoding for A:"), page );
   gbox->addWidget( label, line, 0 );
   m_pEncodingAComboBox = new OptionEncodingComboBox( "EncodingForA", &m_pEncodingA, page, this );
   gbox->addWidget( m_pEncodingAComboBox, line, 1 );

   QString autoDetectToolTip = i18n(
      "If enabled then Unicode (UTF-16 or UTF-8) encoding will be detected.\n"
      "If the file is not Unicode then the selected encoding will be used as fallback.\n"
      "(Unicode detection depends on the first bytes of a file.)"
      );
   m_pAutoDetectUnicodeA = new OptionCheckBox( i18n("Auto Detect Unicode"), true, "AutoDetectUnicodeA", &m_bAutoDetectUnicodeA, page, this );
   gbox->addWidget( m_pAutoDetectUnicodeA, line, 2 );
   m_pAutoDetectUnicodeA->setToolTip( autoDetectToolTip );
   ++line;

   label = new QLabel( i18n("File Encoding for B:"), page );
   gbox->addWidget( label, line, 0 );
   m_pEncodingBComboBox = new OptionEncodingComboBox( "EncodingForB", &m_pEncodingB, page, this );
   gbox->addWidget( m_pEncodingBComboBox, line, 1 );
   m_pAutoDetectUnicodeB = new OptionCheckBox( i18n("Auto Detect Unicode"), true, "AutoDetectUnicodeB", &m_bAutoDetectUnicodeB, page, this );
   gbox->addWidget( m_pAutoDetectUnicodeB, line, 2 );
   m_pAutoDetectUnicodeB->setToolTip( autoDetectToolTip );
   ++line;

   label = new QLabel( i18n("File Encoding for C:"), page );
   gbox->addWidget( label, line, 0 );
   m_pEncodingCComboBox = new OptionEncodingComboBox( "EncodingForC", &m_pEncodingC, page, this );
   gbox->addWidget( m_pEncodingCComboBox, line, 1 );
   m_pAutoDetectUnicodeC = new OptionCheckBox( i18n("Auto Detect Unicode"), true, "AutoDetectUnicodeC", &m_bAutoDetectUnicodeC, page, this );
   gbox->addWidget( m_pAutoDetectUnicodeC, line, 2 );
   m_pAutoDetectUnicodeC->setToolTip( autoDetectToolTip );
   ++line;

   label = new QLabel( i18n("File Encoding for Merge Output and Saving:"), page );
   gbox->addWidget( label, line, 0 );
   m_pEncodingOutComboBox = new OptionEncodingComboBox( "EncodingForOutput", &m_pEncodingOut, page, this );
   gbox->addWidget( m_pEncodingOutComboBox, line, 1 );
   m_pAutoSelectOutEncoding = new OptionCheckBox( i18n("Auto Select"), true, "AutoSelectOutEncoding", &m_bAutoSelectOutEncoding, page, this );
   gbox->addWidget( m_pAutoSelectOutEncoding, line, 2 );
   m_pAutoSelectOutEncoding->setToolTip( i18n(
      "If enabled then the encoding from the input files is used.\n"
      "In ambiguous cases a dialog will ask the user to choose the encoding for saving."
      ) );
   ++line;
   label = new QLabel( i18n("File Encoding for Preprocessor Files:"), page );
   gbox->addWidget( label, line, 0 );
   m_pEncodingPPComboBox = new OptionEncodingComboBox( "EncodingForPP", &m_pEncodingPP, page, this );
   gbox->addWidget( m_pEncodingPPComboBox, line, 1 );   
   ++line;

   connect(m_pSameEncoding, SIGNAL(toggled(bool)), this, SLOT(slotEncodingChanged()));
   connect(m_pEncodingAComboBox, SIGNAL(activated(int)), this, SLOT(slotEncodingChanged()));
   connect(m_pAutoDetectUnicodeA, SIGNAL(toggled(bool)), this, SLOT(slotEncodingChanged()));
   connect(m_pAutoSelectOutEncoding, SIGNAL(toggled(bool)), this, SLOT(slotEncodingChanged()));

   OptionCheckBox* pRightToLeftLanguage = new OptionCheckBox( i18n("Right To Left Language"), false, "RightToLeftLanguage", &m_bRightToLeftLanguage, page, this );
   gbox->addWidget( pRightToLeftLanguage, line, 0, 1, 2 );
   pRightToLeftLanguage->setToolTip( i18n(
                 "Some languages are read from right to left.\n"
                 "This setting will change the viewer and editor accordingly."));
   ++line;   


   topLayout->addStretch(10);
}

#ifdef _WIN32
#include "ccInstHelper.cpp"
#endif

void OptionDialog::setupIntegrationPage( void )
{
   QFrame *page = addPage( i18n("Integration"), i18n("Integration Settings"),
                           BarIcon("launch"/*"charset"*/, KIcon::SizeMedium ) );
   QVBoxLayout *topLayout = new QVBoxLayout( page );
   topLayout->setMargin( 5 );
   topLayout->setSpacing( spacingHint() );

   QGridLayout *gbox = new QGridLayout();
   gbox->setColumnStretch(2,5);
   topLayout->addLayout( gbox );
   int line=0;

   QLabel* label;
   label = new QLabel( i18n("Command line options to ignore:"), page );
   gbox->addWidget( label, line, 0 );
   OptionLineEdit* pIgnorableCmdLineOptions = new OptionLineEdit( "-u;-query;-html;-abort", "IgnorableCmdLineOptions", &m_ignorableCmdLineOptions, page, this );
   gbox->addWidget( pIgnorableCmdLineOptions, line, 1, 1, 2 );
   label->setToolTip( i18n(
      "List of command line options that should be ignored when KDiff3 is used by other tools.\n"
      "Several values can be specified if separated via ';'\n"
      "This will suppress the \"Unknown option\"-error."
      ));
   ++line;

#ifdef _WIN32
   QPushButton* pIntegrateWithClearCase = new QPushButton( i18n("Integrate with ClearCase"), page);
   gbox->addWidget( pIntegrateWithClearCase, line, 0 );
   pIntegrateWithClearCase->setToolTip( i18n(
                 "Integrate with Rational ClearCase from IBM.\n"
                 "Modifies the \"map\" file in ClearCase-subdir \"lib/mgrs\"\n"
                 "(Only enabled when ClearCase \"bin\" directory is in the path.)"));
   connect(pIntegrateWithClearCase, SIGNAL(clicked()),this, SLOT(slotIntegrateWithClearCase()) );
   pIntegrateWithClearCase->setEnabled( integrateWithClearCase( "existsClearCase", "" )!=0 );

   QPushButton* pRemoveClearCaseIntegration = new QPushButton( i18n("Remove ClearCase Integration"), page);
   gbox->addWidget( pRemoveClearCaseIntegration, line, 1 );
   pRemoveClearCaseIntegration->setToolTip( i18n(
                 "Restore the old \"map\" file from before doing the Clearcase integration."));
   connect(pRemoveClearCaseIntegration, SIGNAL(clicked()),this, SLOT(slotRemoveClearCaseIntegration()) );
   pRemoveClearCaseIntegration->setEnabled( integrateWithClearCase( "existsClearCase", "" )!=0 );

   ++line;
#endif

   topLayout->addStretch(10);
}

void OptionDialog::slotIntegrateWithClearCase()
{
#ifdef _WIN32
   char kdiff3CommandPath[1000];
   GetModuleFileNameA( 0, kdiff3CommandPath, sizeof(kdiff3CommandPath)-1 );
   integrateWithClearCase( "install", kdiff3CommandPath );
#endif
}

void OptionDialog::slotRemoveClearCaseIntegration()
{
#ifdef _WIN32
   char kdiff3CommandPath[1000];
   GetModuleFileNameA( 0, kdiff3CommandPath, sizeof(kdiff3CommandPath)-1 );
   integrateWithClearCase( "uninstall", kdiff3CommandPath );
#endif
}

void OptionDialog::slotEncodingChanged()
{
   if ( m_pSameEncoding->isChecked() )
   {
      m_pEncodingBComboBox->setEnabled( false );
      m_pEncodingBComboBox->setCurrentIndex( m_pEncodingAComboBox->currentIndex() );
      m_pEncodingCComboBox->setEnabled( false );
      m_pEncodingCComboBox->setCurrentIndex( m_pEncodingAComboBox->currentIndex() );
      m_pEncodingOutComboBox->setEnabled( false );
      m_pEncodingOutComboBox->setCurrentIndex( m_pEncodingAComboBox->currentIndex() );
      m_pEncodingPPComboBox->setEnabled( false );
      m_pEncodingPPComboBox->setCurrentIndex( m_pEncodingAComboBox->currentIndex() );
      m_pAutoDetectUnicodeB->setEnabled( false );
      m_pAutoDetectUnicodeB->setCheckState( m_pAutoDetectUnicodeA->checkState() );
      m_pAutoDetectUnicodeC->setEnabled( false );
      m_pAutoDetectUnicodeC->setCheckState( m_pAutoDetectUnicodeA->checkState() );
      m_pAutoSelectOutEncoding->setEnabled( false );
      m_pAutoSelectOutEncoding->setCheckState( m_pAutoDetectUnicodeA->checkState() );
   }
   else
   {
      m_pEncodingBComboBox->setEnabled( true );
      m_pEncodingCComboBox->setEnabled( true );
      m_pEncodingOutComboBox->setEnabled( true );
      m_pEncodingPPComboBox->setEnabled( true );
      m_pAutoDetectUnicodeB->setEnabled( true );
      m_pAutoDetectUnicodeC->setEnabled( true );
      m_pAutoSelectOutEncoding->setEnabled( true );
      m_pEncodingOutComboBox->setEnabled( m_pAutoSelectOutEncoding->checkState()==Qt::Unchecked );
   }
}

void OptionDialog::setupKeysPage( void )
{
   //QVBox *page = addVBoxPage( i18n("Keys"), i18n("KeyDialog" ),
   //                          BarIcon("fonts", KIcon::SizeMedium ) );

   //QVBoxLayout *topLayout = new QVBoxLayout( page, 0, spacingHint() );
    //           new KFontChooser( page,"font",false/*onlyFixed*/,QStringList(),false,6 );
   //m_pKeyDialog=new KKeyDialog( false, 0 );
   //topLayout->addWidget( m_pKeyDialog );
}

void OptionDialog::slotOk( void )
{
   slotApply();

   // My system returns variable width fonts even though I
   // disabled this. Even QFont::fixedPitch() doesn't work.
   QFontMetrics fm(m_font);
   if ( fm.width('W')!=fm.width('i') )
   {
      int result = KMessageBox::warningYesNo(this, i18n(
         "You selected a variable width font.\n\n"
         "Because this program doesn't handle variable width fonts\n"
         "correctly, you might experience problems while editing.\n\n"
         "Do you want to continue or do you want to select another font."),
         i18n("Incompatible Font"),
         i18n("Continue at Own Risk"), i18n("Select Another Font"));
      if (result==KMessageBox::No)
         return;
   }

   accept();
}


/** Copy the values from the widgets to the public variables.*/
void OptionDialog::slotApply( void )
{
   std::list<OptionItem*>::iterator i;
   for(i=m_optionItemList.begin(); i!=m_optionItemList.end(); ++i)
   {
      (*i)->apply();
   }

   emit applyClicked();   

#ifdef _WIN32
   QString locale = m_language;
   if ( locale == "Auto" || locale.isEmpty() )
      locale = QLocale::system().name().left(2);
   int spacePos = locale.indexOf(' ');
   if (spacePos>0) locale = locale.left(spacePos);
   QSettings settings("HKEY_CURRENT_USER\\Software\\KDiff3\\diff-ext", QSettings::NativeFormat);
   settings.setValue( "Language", locale );
#endif
}

/** Set the default values in the widgets only, while the
    public variables remain unchanged. */
void OptionDialog::slotDefault()
{
   int result = KMessageBox::warningContinueCancel(this, i18n("This resets all options. Not only those of the current topic.") );
   if ( result==KMessageBox::Cancel ) return;
   else resetToDefaults();
}

void OptionDialog::resetToDefaults()
{
   std::list<OptionItem*>::iterator i;
   for(i=m_optionItemList.begin(); i!=m_optionItemList.end(); ++i)
   {
      (*i)->setToDefault();
   }

   slotEncodingChanged();
}

/** Initialise the widgets using the values in the public varibles. */
void OptionDialog::setState()
{
   std::list<OptionItem*>::iterator i;
   for(i=m_optionItemList.begin(); i!=m_optionItemList.end(); ++i)
   {
      (*i)->setToCurrent();
   }

   slotEncodingChanged();
}

class ConfigValueMap : public ValueMap
{
private:
   KConfig* m_pConfig;
public:
   ConfigValueMap( KConfig* pConfig ) { m_pConfig = pConfig; }

   void writeEntry(const QString& s, const QFont&  v ){ m_pConfig->writeEntry(s,v); }
   void writeEntry(const QString& s, const QColor& v ){ m_pConfig->writeEntry(s,v); }
   void writeEntry(const QString& s, const QSize&  v ){ m_pConfig->writeEntry(s,v); }
   void writeEntry(const QString& s, const QPoint& v ){ m_pConfig->writeEntry(s,v); }
   void writeEntry(const QString& s, int v )          { m_pConfig->writeEntry(s,v); }
   void writeEntry(const QString& s, bool v )         { m_pConfig->writeEntry(s,v); }
   void writeEntry(const QString& s, const QStringList& v, char separator ){ m_pConfig->writeEntry(s,v,separator); }
   void writeEntry(const QString& s, const QString& v ){ m_pConfig->writeEntry(s,v); }
   void writeEntry(const QString& s, const char* v )   { m_pConfig->writeEntry(s,v); }

   QFont       readFontEntry (const QString& s, QFont* defaultVal ) { return m_pConfig->readFontEntry(s,defaultVal); }
   QColor      readColorEntry(const QString& s, QColor* defaultVal ){ return m_pConfig->readColorEntry(s,defaultVal); }
   QSize       readSizeEntry (const QString& s, QSize* defaultVal ) { return m_pConfig->readSizeEntry(s,defaultVal); }
   QPoint      readPointEntry(const QString& s, QPoint* defaultVal) { return m_pConfig->readPointEntry(s,defaultVal); }
   bool        readBoolEntry (const QString& s, bool defaultVal )   { return m_pConfig->readBoolEntry(s,defaultVal); }
   int         readNumEntry  (const QString& s, int defaultVal )    { return m_pConfig->readNumEntry(s,defaultVal); }
   QStringList readListEntry (const QString& s, const QStringList& def, char separator )    { return m_pConfig->readListEntry(s.toLatin1(),def,separator); }
   QString     readEntry     (const QString& s, const QString& defaultVal){ return m_pConfig->readEntry(s,defaultVal); }
};

void OptionDialog::saveOptions( KConfig* config )
{
   // No i18n()-Translations here!

   config->setGroup("KDiff3 Options");

   ConfigValueMap cvm(config);
   std::list<OptionItem*>::iterator i;
   for(i=m_optionItemList.begin(); i!=m_optionItemList.end(); ++i)
   {
      (*i)->write(&cvm);
   }
}

void OptionDialog::readOptions( KConfig* config )
{
   // No i18n()-Translations here!

   config->setGroup("KDiff3 Options");

   ConfigValueMap cvm(config);
   std::list<OptionItem*>::iterator i;
   for(i=m_optionItemList.begin(); i!=m_optionItemList.end(); ++i)
   {
      (*i)->read(&cvm);
   }

   setState();
}

void OptionDialog::slotHelp( void )
{
   KDialogBase::slotHelp();
}

QString OptionDialog::parseOptions( const QCStringList& optionList )
{
   QString result;
   QCStringList::const_iterator i;
   for ( i=optionList.begin(); i!=optionList.end(); ++i )
   {
      QString s = *i;

      int pos = s.indexOf('=');
      if( pos > 0 )                     // seems not to have a tag
      {
         QString key = s.left(pos);
         QString val = s.mid(pos+1);
         std::list<OptionItem*>::iterator j;
         bool bFound = false;
         for(j=m_optionItemList.begin(); j!=m_optionItemList.end(); ++j)
         {
            if ( (*j)->getSaveName()==key )
            {
               ValueMap config;
               config.writeEntry( key, val );  // Write the value as a string and
               (*j)->read(&config);       // use the internal conversion from string to the needed value.
               bFound = true;
               break;
            }
         }
         if ( ! bFound )
         {
            result += "No config item named \"" + key + "\"\n";
         }
      }
      else
      {
         result += "No '=' found in \"" + s + "\"\n";
      }
   }
   return result;
}

QString OptionDialog::calcOptionHelp()
{
   ValueMap config;
   std::list<OptionItem*>::iterator j;
   for(j=m_optionItemList.begin(); j!=m_optionItemList.end(); ++j)
   {
      (*j)->write( &config );
   }
   return config.getAsString();
}

void OptionDialog::slotHistoryMergeRegExpTester()
{
   RegExpTester dlg(this, s_autoMergeRegExpToolTip, s_historyStartRegExpToolTip, 
                          s_historyEntryStartRegExpToolTip, s_historyEntryStartSortKeyOrderToolTip );
   dlg.init(m_pAutoMergeRegExpLineEdit->currentText(), m_pHistoryStartRegExpLineEdit->currentText(), 
            m_pHistoryEntryStartRegExpLineEdit->currentText(), m_pHistorySortKeyOrderLineEdit->currentText());
   if ( dlg.exec() )
   {
      m_pAutoMergeRegExpLineEdit->setEditText( dlg.autoMergeRegExp() );
      m_pHistoryStartRegExpLineEdit->setEditText( dlg.historyStartRegExp() );
      m_pHistoryEntryStartRegExpLineEdit->setEditText( dlg.historyEntryStartRegExp() );
      m_pHistorySortKeyOrderLineEdit->setEditText( dlg.historySortKeyOrder() );
   }
}


//#include "optiondialog.moc"
