/*
 *   kdiff3 - Text Diff And Merge Tool
 *   This file only: Copyright (C) 2002  Joachim Eibl, joachim.eibl@gmx.de
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
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

/***************************************************************************
 * $Log$
 * Revision 1.2  2003/10/11 13:59:39  joachim99
 * Use Courier New as default font under WIN32
 *
 * Revision 1.1  2003/10/06 18:38:48  joachim99
 * KDiff3 version 0.9.70
 ***************************************************************************/

#include <qcheckbox.h>
#include <qcombobox.h>
#include <qfont.h>
#include <qframe.h>
#include <qlayout.h>
#include <qlabel.h>
#include <qlineedit.h> 
#include <qvbox.h>
#include <qvalidator.h>
#include <qtooltip.h>

#include <kapplication.h>
#include <kcolorbtn.h>
#include <kfontdialog.h> // For KFontChooser
#include <kiconloader.h>
#include <klocale.h>
#include <kconfig.h>
#include <kmessagebox.h>
//#include <kkeydialog.h>

#include "optiondialog.h"
#include "diff.h"

void OptionDialog::addOptionItem(OptionItem* p)
{
   m_optionItemList.push_back(p);
}

class OptionItem
{
public:
   OptionItem( OptionDialog* pOptionDialog, QString saveName )
   {
      assert(pOptionDialog!=0);
      pOptionDialog->addOptionItem( this );
      m_saveName = saveName;
   }
   virtual ~OptionItem(){}
   virtual void setToDefault()=0;
   virtual void setToCurrent()=0;
   virtual void apply()=0;
   virtual void write(KConfig*)=0;
   virtual void read(KConfig*)=0;
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
   void write(KConfig* config){ config->writeEntry(m_saveName, *m_pbVar );   }
   void read (KConfig* config){ *m_pbVar = config->readBoolEntry( m_saveName, *m_pbVar ); }
private:
   OptionCheckBox( const OptionCheckBox& ); // private copy constructor without implementation
   bool* m_pbVar;
   bool m_bDefaultVal;
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
   void write(KConfig* config){ config->writeEntry(m_saveName, *m_pVar );   }
   void read (KConfig* config){ *m_pVar = config->readColorEntry( m_saveName, m_pVar ); }
private:
   OptionColorButton( const OptionColorButton& ); // private copy constructor without implementation
   QColor* m_pVar;
   QColor m_defaultVal;
};

class OptionLineEdit : public QLineEdit, public OptionItem
{
public:
   OptionLineEdit( const QString& defaultVal, const QString& saveName, QString* pVar,
                   QWidget* pParent, OptionDialog* pOD )
   : QLineEdit( pParent ), OptionItem( pOD, saveName )
   {
      m_pVar = pVar;
      m_defaultVal = defaultVal;
   }
   void setToDefault(){ setText( m_defaultVal );      }
   void setToCurrent(){ setText( *m_pVar );           }
   void apply()       { *m_pVar = text();                              }
   void write(KConfig* config){ config->writeEntry(m_saveName, *m_pVar );   }
   void read (KConfig* config){ *m_pVar = config->readEntry( m_saveName, *m_pVar ); }
private:
   OptionLineEdit( const OptionLineEdit& ); // private copy constructor without implementation
   QString* m_pVar;
   QString m_defaultVal;
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
   void write(KConfig* config){ config->writeEntry(m_saveName, *m_pVar );   }
   void read (KConfig* config){ *m_pVar = config->readNumEntry( m_saveName, *m_pVar ); }
private:
   OptionIntEdit( const OptionIntEdit& ); // private copy constructor without implementation
   int* m_pVar;
   int m_defaultVal;
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
  if (bShowDirMergeSettings)
     setupDirectoryMergePage();
  //setupKeysPage();

  // Initialize all values in the dialog
  resetToDefaults();
  slotApply();
}

OptionDialog::~OptionDialog( void )
{
}


void OptionDialog::setupFontPage( void )
{
   QFrame *page = addPage( i18n("Font"), i18n("Editor and diff output font" ),
                             BarIcon("fonts", KIcon::SizeMedium ) );

   QVBoxLayout *topLayout = new QVBoxLayout( page, 0, spacingHint() );
   
   m_fontChooser = new KFontChooser( page,"font",true/*onlyFixed*/,QStringList(),false,6 );
   topLayout->addWidget( m_fontChooser );

   QGridLayout *gbox = new QGridLayout( 1, 2 );
   topLayout->addLayout( gbox );
   int line=0;

   OptionCheckBox* pItalicDeltas = new OptionCheckBox( i18n("Italic Font For Deltas"), false, "ItalicForDeltas", &m_bItalicForDeltas, page, this );
   gbox->addMultiCellWidget( pItalicDeltas, line, line, 0, 1 );
   QToolTip::add( pItalicDeltas, i18n(
      "Selects the italic version of the font for differences.\n"
      "If the font doesn't support italic characters, then this does nothing.")
      );
}


void OptionDialog::setupColorPage( void )
{
  QFrame *page = addPage( i18n("Color"), i18n("Colors in editor and diff output"),
     BarIcon("colorize", KIcon::SizeMedium ) );
  QVBoxLayout *topLayout = new QVBoxLayout( page, 0, spacingHint() );

  QGridLayout *gbox = new QGridLayout( 7, 2 );
  topLayout->addLayout(gbox);

  QLabel* label;
  int line = 0;

  int depth = QColor::numBitPlanes();
  bool bLowColor = depth<=8;

  OptionColorButton* pFgColor = new OptionColorButton( black,"FgColor", &m_fgColor, page, this );
  label = new QLabel( pFgColor, i18n("Foreground color:"), page );
  gbox->addWidget( label, line, 0 );
  gbox->addWidget( pFgColor, line, 1 );
  ++line;

  OptionColorButton* pBgColor = new OptionColorButton( white, "BgColor", &m_bgColor, page, this );
  label = new QLabel( pBgColor, i18n("Background color:"), page );
  gbox->addWidget( label, line, 0 );
  gbox->addWidget( pBgColor, line, 1 );

  ++line;

  OptionColorButton* pDiffBgColor = new OptionColorButton( lightGray, "DiffBgColor", &m_diffBgColor, page, this );
  label = new QLabel( pDiffBgColor, i18n("Diff background color:"), page );
  gbox->addWidget( label, line, 0 );
  gbox->addWidget( pDiffBgColor, line, 1 );
  ++line;

  OptionColorButton* pColorA = new OptionColorButton(
     bLowColor ? qRgb(0,0,255) : qRgb(0,0,200)/*blue*/, "ColorA", &m_colorA, page, this );
  label = new QLabel( pColorA, i18n("Color A:"), page );
  gbox->addWidget( label, line, 0 );
  gbox->addWidget( pColorA, line, 1 );
  ++line;

  OptionColorButton* pColorB = new OptionColorButton(
     bLowColor ? qRgb(0,128,0) : qRgb(0,150,0)/*green*/, "ColorB", &m_colorB, page, this );
  label = new QLabel( pColorB, i18n("Color B:"), page );
  gbox->addWidget( label, line, 0 );
  gbox->addWidget( pColorB, line, 1 );
  ++line;

  OptionColorButton* pColorC = new OptionColorButton(
     bLowColor ? qRgb(128,0,128) : qRgb(150,0,150)/*magenta*/, "ColorC", &m_colorC, page, this );
  label = new QLabel( pColorC, i18n("Color C:"), page );
  gbox->addWidget( label, line, 0 );
  gbox->addWidget( pColorC, line, 1 );
  ++line;

  OptionColorButton* pColorForConflict = new OptionColorButton( red, "ColorForConflict", &m_colorForConflict, page, this );
  label = new QLabel( pColorForConflict, i18n("Conflict Color:"), page );
  gbox->addWidget( label, line, 0 );
  gbox->addWidget( pColorForConflict, line, 1 );
  ++line;

  OptionColorButton* pColor = new OptionColorButton( 
     bLowColor ? qRgb(192,192,192) : qRgb(220,220,100), "CurrentRangeBgColor", &m_currentRangeBgColor, page, this );
  label = new QLabel( pColor, i18n("Current range background color:"), page );
  gbox->addWidget( label, line, 0 );
  gbox->addWidget( pColor, line, 1 );
  ++line;

  pColor = new OptionColorButton( 
     bLowColor ? qRgb(255,255,0) : qRgb(255,255,150), "CurrentRangeDiffBgColor", &m_currentRangeDiffBgColor, page, this );
  label = new QLabel( pColor, i18n("Current range diff background color:"), page );
  gbox->addWidget( label, line, 0 );
  gbox->addWidget( pColor, line, 1 );
  ++line;

  topLayout->addStretch(10);
}


void OptionDialog::setupEditPage( void )
{
   QFrame *page = addPage( i18n("Editor Settings"), i18n("Editor behaviour"),
                           BarIcon("edit", KIcon::SizeMedium ) );
   QVBoxLayout *topLayout = new QVBoxLayout( page, 0, spacingHint() );

   QGridLayout *gbox = new QGridLayout( 4, 2 );
   topLayout->addLayout( gbox );
   QLabel* label;
   int line=0;

   OptionCheckBox* pReplaceTabs = new OptionCheckBox( i18n("Tab inserts spaces"), false, "ReplaceTabs", &m_bReplaceTabs, page, this );
   gbox->addMultiCellWidget( pReplaceTabs, line, line, 0, 1 );
   QToolTip::add( pReplaceTabs, i18n(
      "On: Pressing tab generates the appropriate number of spaces.\n"
      "Off: A Tab-character will be inserted.")
      );
   ++line;

   OptionIntEdit* pTabSize = new OptionIntEdit( 8, "TabSize", &m_tabSize, 1, 100, page, this );
   label = new QLabel( pTabSize, i18n("Tab size:"), page );
   gbox->addWidget( label, line, 0 );
   gbox->addWidget( pTabSize, line, 1 );
   ++line;

   OptionCheckBox* pAutoIndentation = new OptionCheckBox( i18n("Auto Indentation"), true, "AutoIndentation", &m_bAutoIndentation, page, this  );
   gbox->addMultiCellWidget( pAutoIndentation, line, line, 0, 1 );
   QToolTip::add( pAutoIndentation, i18n(
      "On: The indentation of the previous line is used for a new line.\n"
      ));
   ++line;

   OptionCheckBox* pAutoCopySelection = new OptionCheckBox( i18n("Auto Copy Selection"), false, "AutoCopySelection", &m_bAutoCopySelection, page, this );
   gbox->addMultiCellWidget( pAutoCopySelection, line, line, 0, 1 );
   QToolTip::add( pAutoCopySelection, i18n(
      "On: Any selection is immediately written to the clipboard.\n"
      "Off: You must explicitely copy e.g. via Ctrl-C."
      ));
   ++line;

   topLayout->addStretch(10);
}


void OptionDialog::setupDiffPage( void )
{
   QFrame *page = addPage( i18n("Diff & Merge Settings"), i18n("Diff & Merge Settings"),
                           BarIcon("misc", KIcon::SizeMedium ) );
   QVBoxLayout *topLayout = new QVBoxLayout( page, 0, spacingHint() );

   QGridLayout *gbox = new QGridLayout( 3, 2 );
   topLayout->addLayout( gbox );
   int line=0;

   OptionCheckBox* pIgnoreWhiteSpace = new OptionCheckBox( i18n("Ignore white space"), true, "IgnoreWhiteSpace", &m_bIgnoreWhiteSpace, page, this );
   gbox->addMultiCellWidget( pIgnoreWhiteSpace, line, line, 0, 1 );
   QToolTip::add( pIgnoreWhiteSpace, i18n(
      "On: Text that differs only in white space will match and\n"
      "be shown on the same line in the different output windows.\n"
      "Off is useful when whitespace is very important.\n"
      "On is good for C/C++ and similar languages." )
      );
   ++line;

   OptionCheckBox* pPreserveCarriageReturn = new OptionCheckBox( i18n("Preserve Carriage Return"), false, "PreserveCarriageReturn", &m_bPreserveCarriageReturn, page, this );
   gbox->addMultiCellWidget( pPreserveCarriageReturn, line, line, 0, 1 );
   QToolTip::add( pPreserveCarriageReturn,
      "Show carriage return characters '\\r' if they exist.\n"
      "Helps to compare files that were modified under different operating systems."
      );
   ++line;

   OptionCheckBox* pIgnoreNumbers = new OptionCheckBox( i18n("Ignore Numbers"), false, "IgnoreNumbers", &m_bIgnoreNumbers, page, this );
   gbox->addMultiCellWidget( pIgnoreNumbers, line, line, 0, 1 );
   QToolTip::add( pIgnoreNumbers,
      "Ignore number characters during line matching phase. (Similar to Ignore white space.)\n"
      "Might help to compare files with numeric data."
      );
   ++line;

   OptionCheckBox* pUpCase = new OptionCheckBox( i18n("Convert to Upper Case"), false, "UpCase", &m_bUpCase, page, this );
   gbox->addMultiCellWidget( pUpCase, line, line, 0, 1 );
   QToolTip::add( pUpCase,
      "Turn all lower case characters to upper case on reading. (e.g.: 'a'->'A')"
      );
   ++line;

   QLabel* label = new QLabel( i18n("Preprocessor-Command"), page );
   gbox->addWidget( label, line, 0 );
   OptionLineEdit* pLE = new OptionLineEdit( "", "PreProcessorCmd", &m_PreProcessorCmd, page, this );
   gbox->addWidget( pLE, line, 1 );
   QToolTip::add( label, i18n("User defined pre-processing. (See the docs for details.)") );
   ++line;

   label = new QLabel( i18n("Line-Matching Preprocessor-Command"), page );
   gbox->addWidget( label, line, 0 );
   pLE = new OptionLineEdit( "", "LineMatchingPreProcessorCmd", &m_LineMatchingPreProcessorCmd, page, this );
   gbox->addWidget( pLE, line, 1 );
   QToolTip::add( label, i18n("This pre-processor is only used during line matching.\n(See the docs for details.)") );
   ++line;

   OptionCheckBox* pUseExternalDiff = new OptionCheckBox( i18n("Use external diff"), true, "UseExternalDiff", &m_bUseExternalDiff, page, this );
   gbox->addMultiCellWidget( pUseExternalDiff, line, line, 0, 1 );
   QToolTip::add( pUseExternalDiff,
      "Since for complicated files the internal algorithm is not so good yet,\n"
      "you probably want to use the normal diff tool as line matcher."
      );
   ++line;

   OptionCheckBox* pTryHard = new OptionCheckBox( i18n("Try Hard (slow)"), true, "TryHard", &m_bTryHard, page, this );
   gbox->addMultiCellWidget( pTryHard, line, line, 0, 1 );
   QToolTip::add( pTryHard,
      "Enables the --minimal option for the external diff.\n"
      "The analysis of big files will be much slower."
      );
   ++line;

   OptionCheckBox* pIgnoreTrivialMatches = new OptionCheckBox( i18n("Ignore trivial matches"), true, "IgnoreTrivialMatches", &m_bIgnoreTrivialMatches, page, this );
   gbox->addMultiCellWidget( pIgnoreTrivialMatches, line, line, 0, 1 );
   QToolTip::add( pIgnoreTrivialMatches, i18n(
      "When a difference was found, the algorithm searches for matching lines\n"
      "Short or trivial lines match even when the differences still continue.\n"
      "Ignoring trivial lines avoids this. Good for C/C++ and similar languages." )
      );
   ++line;

   label = new QLabel( i18n("Max search length"), page );
   gbox->addWidget( label, line, 0 );
   OptionIntEdit* pMaxSearchLength = new OptionIntEdit( 1000, "MaxSearchLength", &m_maxSearchLength, 100, 100000, page, this );
   gbox->addWidget( pMaxSearchLength, line, 1 );
   QToolTip::add( label, i18n(
      "Diff might fail for too small values but might take too long for big values.\n" )
      );
   ++line;

   connect( pUseExternalDiff, SIGNAL( toggled(bool)), pTryHard, SLOT(setEnabled(bool)));
   connect( pUseExternalDiff, SIGNAL( toggled(bool)), pMaxSearchLength, SLOT(setDisabled(bool)));

   connect( pUseExternalDiff, SIGNAL( toggled(bool)), label, SLOT(setDisabled(bool)));
   connect( pUseExternalDiff, SIGNAL( toggled(bool)), pIgnoreTrivialMatches, SLOT(setDisabled(bool)));

   label = new QLabel( i18n("Auto Advance Delay (ms)"), page );
   gbox->addWidget( label, line, 0 );
   OptionIntEdit* pAutoAdvanceDelay = new OptionIntEdit( 500, "AutoAdvanceDelay", &m_autoAdvanceDelay, 0, 2000, page, this );
   gbox->addWidget( pAutoAdvanceDelay, line, 1 );
   QToolTip::add( label,i18n(
      "When in Auto-Advance mode the result of the current selection is shown \n"
      "for the specified time, before jumping to the next conflict. Range: 0-2000 ms")
      );
   ++line;

   topLayout->addStretch(10);
}

void OptionDialog::setupDirectoryMergePage( void )
{
   QFrame *page = addPage( i18n("Directory Merge Settings"), i18n("Directory Merge"),
                           BarIcon("folder", KIcon::SizeMedium ) );
   QVBoxLayout *topLayout = new QVBoxLayout( page, 0, spacingHint() );

   QGridLayout *gbox = new QGridLayout( 11, 2 );
   topLayout->addLayout( gbox );
   int line=0;

   OptionCheckBox* pRecursiveDirs = new OptionCheckBox( i18n("Recursive Directories"), true, "RecursiveDirs", &m_bDmRecursiveDirs, page, this );
   gbox->addMultiCellWidget( pRecursiveDirs, line, line, 0, 1 );
   QToolTip::add( pRecursiveDirs, i18n("Whether to analyze subdirectories or not.") );
   ++line;
   QLabel* label = new QLabel( i18n("File Pattern(s)"), page );
   gbox->addWidget( label, line, 0 );
   OptionLineEdit* pFilePattern = new OptionLineEdit( "*", "FilePattern", &m_DmFilePattern, page, this );
   gbox->addWidget( pFilePattern, line, 1 );
   QToolTip::add( label, i18n(
      "Pattern(s) of files to be analyzed. \n"
      "Wildcards: '*' and '?'\n"
      "Several Patterns can be specified by using the separator: ';'"
      ));
   ++line;

   label = new QLabel( i18n("File-Anti-Pattern(s)"), page );
   gbox->addWidget( label, line, 0 );
   OptionLineEdit* pFileAntiPattern = new OptionLineEdit( "*.orig;*.o", "FileAntiPattern", &m_DmFileAntiPattern, page, this );
   gbox->addWidget( pFileAntiPattern, line, 1 );
   QToolTip::add( label, i18n(
      "Pattern(s) of files to be excluded from analysis. \n"
      "Wildcards: '*' and '?'\n"
      "Several Patterns can be specified by using the separator: ';'"
      ));
   ++line;

   label = new QLabel( i18n("Dir-Anti-Pattern(s)"), page );
   gbox->addWidget( label, line, 0 );
   OptionLineEdit* pDirAntiPattern = new OptionLineEdit( "CVS;.deps", "DirAntiPattern", &m_DmDirAntiPattern, page, this );
   gbox->addWidget( pDirAntiPattern, line, 1 );
   QToolTip::add( label, i18n(
      "Pattern(s) of directories to be excluded from analysis. \n"
      "Wildcards: '*' and '?'\n"
      "Several Patterns can be specified by using the separator: ';'"
      ));
   ++line;

   OptionCheckBox* pUseCvsIgnore = new OptionCheckBox( i18n("Use CVS-Ignore"), false, "UseCvsIgnore", &m_bDmUseCvsIgnore, page, this );
   gbox->addMultiCellWidget( pUseCvsIgnore, line, line, 0, 1 );
   QToolTip::add( pUseCvsIgnore, i18n(
      "Extends the antipattern to anything that would be ignored by CVS.\n"
      "Via local \".cvsignore\"-files this can be directory specific."
      ));
   ++line;

   OptionCheckBox* pFindHidden = new OptionCheckBox( i18n("Find Hidden Files and Directories"), true, "FindHidden", &m_bDmFindHidden, page, this );
   gbox->addMultiCellWidget( pFindHidden, line, line, 0, 1 );
#ifdef _WIN32
   QToolTip::add( pFindHidden, i18n("Finds files and directories with the hidden attribute.") );
#else
   QToolTip::add( pFindHidden, i18n("Finds files and directories starting with '.'.") );
#endif
   ++line;

   OptionCheckBox* pFollowFileLinks = new OptionCheckBox( i18n("Follow File Links"), false, "FollowFileLinks", &m_bDmFollowFileLinks, page, this );
   gbox->addMultiCellWidget( pFollowFileLinks, line, line, 0, 1 );
   QToolTip::add( pFollowFileLinks, i18n(
      "On: Compare the file the link points to.\n"
      "Off: Compare the links."
      ));
   ++line;

   OptionCheckBox* pFollowDirLinks = new OptionCheckBox( i18n("Follow Directory Links"), false, "FollowDirLinks", &m_bDmFollowDirLinks, page, this );
   gbox->addMultiCellWidget( pFollowDirLinks, line, line, 0, 1 );
   QToolTip::add( pFollowDirLinks,    i18n(
      "On: Compare the directory the link points to.\n"
      "Off: Compare the links."
      ));
   ++line;

   OptionCheckBox* pShowOnlyDeltas = new OptionCheckBox( i18n("List only deltas"),false,"ListOnlyDeltas", &m_bDmShowOnlyDeltas, page, this );
   gbox->addMultiCellWidget( pShowOnlyDeltas, line, line, 0, 1 );
   QToolTip::add( pShowOnlyDeltas, i18n(
                 "Files and directories without change will not appear in the list."));
   ++line;

   OptionCheckBox* pTrustDate = new OptionCheckBox( i18n("Trust the modification date. (Unsafe)."), false, "TrustDate", &m_bDmTrustDate, page, this );
   gbox->addMultiCellWidget( pTrustDate, line, line, 0, 1 );
   QToolTip::add( pTrustDate, i18n("Assume that files are equal if the modification date and file length are equal.\n"
                                     "Useful for big directories or slow networks.") );
   ++line;

   // Some two Dir-options: Affects only the default actions.
   OptionCheckBox* pSyncMode = new OptionCheckBox( i18n("Synchronize Directories."), false,"SyncMode", &m_bDmSyncMode, page, this );
   gbox->addMultiCellWidget( pSyncMode, line, line, 0, 1 );
   QToolTip::add( pSyncMode, i18n(
                  "Offers to store files in both directories so that\n"
                  "both directories are the same afterwards.\n"
                  "Works only when comparing two directories without specifying a destination."  ) );
   ++line;

   OptionCheckBox* pCopyNewer = new OptionCheckBox( i18n("Copy newer instead of merging. (Unsafe)."), false, "CopyNewer", &m_bDmCopyNewer, page, this );
   gbox->addMultiCellWidget( pCopyNewer, line, line, 0, 1 );
   QToolTip::add( pCopyNewer, i18n(
                  "Don't look inside, just take the newer file.\n"
                  "(Use this only if you know what you are doing!)\n"
                  "Only effective when comparing two directories."  ) );
   ++line;
                                               
   OptionCheckBox* pCreateBakFiles = new OptionCheckBox( i18n("Backup files (.orig)"), true, "CreateBakFiles", &m_bDmCreateBakFiles, page, this );
   gbox->addMultiCellWidget( pCreateBakFiles, line, line, 0, 1 );
   QToolTip::add( pCreateBakFiles, i18n(
                 "When a file would be saved over an old file, then the old file\n"
                 "will be renamed with a '.orig'-extension instead of being deleted."));
   ++line;

   topLayout->addStretch(10);
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
         i18n("Incompatible font."),
         i18n("Continue at my own risk"), i18n("Select another font"));
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

   // FontConfigDlg
   m_font = m_fontChooser->font();
   
   emit applyClicked();   
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

#ifdef _WIN32
   m_fontChooser->setFont( QFont("Courier New", 10 ), true /*only fixed*/ );
#else
   m_fontChooser->setFont( QFont("Courier", 10 ), true /*only fixed*/ );
#endif

   m_bAutoAdvance = false;
   m_bShowWhiteSpace = true;
   m_bShowLineNumbers = false;
   m_bHorizDiffWindowSplitting = true;
}

/** Initialise the widgets using the values in the public varibles. */
void OptionDialog::setState()
{
   std::list<OptionItem*>::iterator i;
   for(i=m_optionItemList.begin(); i!=m_optionItemList.end(); ++i)
   {
      (*i)->setToCurrent();
   }

   m_fontChooser->setFont( m_font, true /*only fixed*/ );
}

void OptionDialog::saveOptions( KConfig* config )
{
   // No i18n()-Translations here!

   config->setGroup("KDiff3 Options");

   std::list<OptionItem*>::iterator i;
   for(i=m_optionItemList.begin(); i!=m_optionItemList.end(); ++i)
   {
      (*i)->write(config);
   }

   // FontConfigDlg
   config->writeEntry("Font",  m_font );

   config->writeEntry("AutoAdvance", m_bAutoAdvance );
   config->writeEntry("ShowWhiteSpace", m_bShowWhiteSpace );
   config->writeEntry("ShowLineNumbers", m_bShowLineNumbers );
   config->writeEntry("HorizDiffWindowSplitting", m_bHorizDiffWindowSplitting );

   // Recent files (selectable in the OpenDialog)
   config->writeEntry( "RecentAFiles", m_recentAFiles, '|' );
   config->writeEntry( "RecentBFiles", m_recentBFiles, '|' );
   config->writeEntry( "RecentCFiles", m_recentCFiles, '|' );
   config->writeEntry( "RecentOutputFiles", m_recentOutputFiles, '|' );
}

void OptionDialog::readOptions( KConfig* config )
{
   // No i18n()-Translations here!

   config->setGroup("KDiff3 Options");

   std::list<OptionItem*>::iterator i;

   for(i=m_optionItemList.begin(); i!=m_optionItemList.end(); ++i)
   {
      (*i)->read(config);
   }

   // Use the current values as default settings.

   // FontConfigDlg
   m_font = config->readFontEntry( "Font", &m_font);

   // DiffConfigDlg

   m_bAutoAdvance = config->readBoolEntry("AutoAdvance", m_bAutoAdvance );
   m_bShowWhiteSpace = config->readBoolEntry("ShowWhiteSpace", m_bShowWhiteSpace );
   m_bShowLineNumbers = config->readBoolEntry("ShowLineNumbers", m_bShowLineNumbers );
   m_bHorizDiffWindowSplitting = config->readBoolEntry("HorizDiffWindowSplitting", m_bHorizDiffWindowSplitting);

   // Recent files (selectable in the OpenDialog)
   m_recentAFiles = config->readListEntry( "RecentAFiles", '|' );
   m_recentBFiles = config->readListEntry( "RecentBFiles", '|' );
   m_recentCFiles = config->readListEntry( "RecentCFiles", '|' );
   m_recentOutputFiles = config->readListEntry( "RecentOutputFiles", '|' );

   setState();
}

void OptionDialog::slotHelp( void )
{
   KDialogBase::slotHelp();
}


#include "optiondialog.moc"
