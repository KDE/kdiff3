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
 * Revision 1.1  2002/08/18 16:24:09  joachim99
 * Initial revision
 *                                                                   *
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

#include <kapp.h>
#include <kcolorbtn.h>
#include <kfontdialog.h> // For KFontChooser
#include <kiconloader.h>
#include <klocale.h>
#include <kconfig.h>
#include <kmessagebox.h>

#include "optiondialog.h"



OptionDialog::OptionDialog( QWidget *parent, char *name )
  :KDialogBase( IconList, i18n("Configure"), Help|Default|Apply|Ok|Cancel,
                Ok, parent, name, true /*modal*/, true )
{
  setHelp( "kdiff3/index.html", QString::null );

  setupFontPage();
  setupColorPage();
  setupEditPage();
  setupDiffPage();

  // Initialize all values in the dialog
  slotDefault();
  slotApply();
}

OptionDialog::~OptionDialog( void )
{
}


void OptionDialog::setupFontPage( void )
{
   QVBox *page = addVBoxPage( i18n("Font"), i18n("Editor and diff output font" ),
                             BarIcon("fonts", KIcon::SizeMedium ) );
   m_fontChooser = new KFontChooser( page,"font",true/*onlyFixed*/,QStringList(),false,6 );
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

  m_pFgColor = new KColorButton( page );
  label = new QLabel( m_pFgColor, i18n("Foreground color:"), page );
  gbox->addWidget( label, line, 0 );
  gbox->addWidget( m_pFgColor, line, 1 );
  ++line;

  m_pBgColor = new KColorButton( page );
  label = new QLabel( m_pBgColor, i18n("Background color:"), page );
  gbox->addWidget( label, line, 0 );
  gbox->addWidget( m_pBgColor, line, 1 );
  ++line;

  m_pDiffBgColor = new KColorButton( page );
  label = new QLabel( m_pDiffBgColor, i18n("Diff background color:"), page );
  gbox->addWidget( label, line, 0 );
  gbox->addWidget( m_pDiffBgColor, line, 1 );
  ++line;

  m_pColorA = new KColorButton( page );
  label = new QLabel( m_pColorA, i18n("Color A:"), page );
  gbox->addWidget( label, line, 0 );
  gbox->addWidget( m_pColorA, line, 1 );
  ++line;

  m_pColorB = new KColorButton( page );
  label = new QLabel( m_pColorB, i18n("Color B:"), page );
  gbox->addWidget( label, line, 0 );
  gbox->addWidget( m_pColorB, line, 1 );
  ++line;

  m_pColorC = new KColorButton( page );
  label = new QLabel( m_pColorC, i18n("Color C:"), page );
  gbox->addWidget( label, line, 0 );
  gbox->addWidget( m_pColorC, line, 1 );
  ++line;

  m_pColorForConflict = new KColorButton( page );
  label = new QLabel( m_pColorForConflict, i18n("Conflict Color:"), page );
  gbox->addWidget( label, line, 0 );
  gbox->addWidget( m_pColorForConflict, line, 1 );
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

   m_pReplaceTabs = new QCheckBox( i18n("Tab inserts spaces"), page );
   gbox->addMultiCellWidget( m_pReplaceTabs, line, line, 0, 1 );
   QToolTip::add( m_pReplaceTabs, i18n(
      "On: Pressing tab generates the appropriate number of spaces.\n"
      "Off: A Tab-character will be inserted.")
      );
   ++line;

   m_pTabSize = new QLineEdit( page );
   label = new QLabel( m_pTabSize, i18n("Tab size:"), page );
   QIntValidator* v = new QIntValidator( m_pTabSize );
   v->setRange(0,100);
   m_pTabSize->setValidator( v );
   gbox->addWidget( label, line, 0 );
   gbox->addWidget( m_pTabSize, line, 1 );
   ++line;

   m_pAutoIndentation = new QCheckBox( i18n("Auto Indentation"), page );
   gbox->addMultiCellWidget( m_pAutoIndentation, line, line, 0, 1 );
   QToolTip::add( m_pAutoIndentation, i18n(
      "On: The indentation of the previous line is used for a new line.\n"
      ));
   ++line;

   m_pAutoCopySelection = new QCheckBox( i18n("Auto Copy Selection"), page );
   gbox->addMultiCellWidget( m_pAutoCopySelection, line, line, 0, 1 );
   QToolTip::add( m_pAutoCopySelection, i18n(
      "On: Any selection is immediately written to the clipboard.\n"
      "Off: You must explicitely copy e.g. via Ctrl-C."
      ));
   ++line;

   topLayout->addStretch(10);
}


void OptionDialog::setupDiffPage( void )
{
   QFrame *page = addPage( i18n("Diff Settings"), i18n("Diff Settings"),
                           BarIcon("misc", KIcon::SizeMedium ) );
   QVBoxLayout *topLayout = new QVBoxLayout( page, 0, spacingHint() );

   QGridLayout *gbox = new QGridLayout( 2, 2 );
   topLayout->addLayout( gbox );
   int line=0;

   m_pIgnoreWhiteSpace = new QCheckBox( i18n("Ignore white space"), page );
   gbox->addMultiCellWidget( m_pIgnoreWhiteSpace, line, line, 0, 1 );
   QToolTip::add( m_pIgnoreWhiteSpace, i18n(
      "On: Text that differs only in white space will match and\n"
      "be shown on the same line in the different output windows.\n"
      "Off is useful when whitespace is very important.\n"
      "On is good for C/C++ and similar languages." )
   );
   ++line;

   m_pIgnoreTrivialMatches = new QCheckBox( i18n("Ignore trivial matches"), page );
   gbox->addMultiCellWidget( m_pIgnoreTrivialMatches, line, line, 0, 1 );
   QToolTip::add( m_pIgnoreTrivialMatches,
      "When a difference was found, the algorithm searches for matching lines\n"
      "Short or trivial lines match even when the differences still continue.\n"
      "Igoring trivial lines avoids this. Good for C/C++ and similar languages."
   );
   ++line;

   topLayout->addStretch(10);
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
   // FontConfigDlg
   m_font = m_fontChooser->font();

   // ColorConfigDlg
   m_fgColor = m_pFgColor->color();
   m_bgColor = m_pBgColor->color();
   m_diffBgColor = m_pDiffBgColor->color();
   m_colorA  = m_pColorA->color();
   m_colorB  = m_pColorB->color();
   m_colorC  = m_pColorC->color();
   m_colorForConflict = m_pColorForConflict->color();

   // EditConfigDlg
   m_bReplaceTabs = m_pReplaceTabs->isChecked();
   m_tabSize = m_pTabSize->text().toInt();
   m_bAutoIndentation = m_pAutoIndentation->isChecked();
   m_bAutoCopySelection = m_pAutoCopySelection->isChecked();

   // DiffConfigDlg
   m_bIgnoreWhiteSpace = m_pIgnoreWhiteSpace->isChecked();
   m_bIgnoreTrivialMatches = m_pIgnoreTrivialMatches->isChecked();

   emit applyClicked();
}

/** Set the default values in the widgets only, while the
    public variables remain unchanged. */
void OptionDialog::slotDefault( void )
{
    m_fontChooser->setFont( QFont("Courier", 10 ), true /*only fixed*/ );

    // ColorConfigDlg
    m_pFgColor->setColor( black );
    m_pBgColor->setColor( white );
    m_pDiffBgColor->setColor( lightGray );
    m_pColorA->setColor(qRgb(   0,   0, 200 ));  //  blue
    m_pColorB->setColor(qRgb(   0, 150,   0 ));  //  green
    m_pColorC->setColor(qRgb( 150,   0, 150 ));  //  magenta
    m_pColorForConflict->setColor( red );

    // EditConfigDlg
    m_pReplaceTabs->setChecked( false );
    m_pTabSize->setText("8");
    m_pAutoIndentation->setChecked( true );
    m_pAutoCopySelection->setChecked( false );

    // DiffConfigDlg
    m_pIgnoreWhiteSpace->setChecked( true );
    m_pIgnoreTrivialMatches->setChecked( true );
}

/** Initialise the widgets using the values in the public varibles. */
void OptionDialog::setState()
{
    m_fontChooser->setFont( m_font, true /*only fixed*/ );

    // ColorConfigDlg
    m_pFgColor->setColor( m_fgColor );
    m_pBgColor->setColor( m_bgColor );
    m_pDiffBgColor->setColor( m_diffBgColor );
    m_pColorA->setColor(m_colorA);
    m_pColorB->setColor(m_colorB);
    m_pColorC->setColor(m_colorC);
    m_pColorForConflict->setColor( m_colorForConflict );

    // EditConfigDlg
    m_pReplaceTabs->setChecked( m_bReplaceTabs );
    QString s;
    s.setNum(m_tabSize);
    m_pTabSize->setText( s );
    m_pAutoIndentation->setChecked( m_bAutoIndentation );
    m_pAutoCopySelection->setChecked( m_bAutoCopySelection );

    // DiffConfigDlg
    m_pIgnoreWhiteSpace->setChecked( m_bIgnoreWhiteSpace );
    m_pIgnoreTrivialMatches->setChecked( m_bIgnoreTrivialMatches );
}

void OptionDialog::saveOptions( KConfig* config )
{
   // No i18n()-Translations here!

   config->setGroup("KDiff3 Options");

   // FontConfigDlg
   config->writeEntry("Font",  m_font );

   // ColorConfigDlg
   config->writeEntry("FgColor", m_fgColor );
   config->writeEntry("BgColor", m_bgColor );
   config->writeEntry("DiffBgColor", m_diffBgColor );
   config->writeEntry("ColorA", m_colorA );
   config->writeEntry("ColorB", m_colorB );
   config->writeEntry("ColorC", m_colorC );
   config->writeEntry("ColorForConflict", m_colorForConflict );

   // EditConfigDlg
   config->writeEntry("ReplaceTabs", m_colorForConflict );
   config->writeEntry("TabSize", m_tabSize );
   config->writeEntry("AutoIndentation", m_bAutoIndentation );
   config->writeEntry("AutoCopySelection", m_bAutoCopySelection );

   // DiffConfigDlg
   config->writeEntry("IgnoreWhiteSpace", m_bIgnoreWhiteSpace );
   config->writeEntry("IgnoreTrivialMatches", m_bIgnoreTrivialMatches );

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

   // Use the current values as default settings.

   // FontConfigDlg
   m_font = config->readFontEntry( "Font", &m_font);

   // ColorConfigDlg
   m_fgColor = config->readColorEntry("FgColor", &m_fgColor );
   m_bgColor = config->readColorEntry("BgColor", &m_bgColor );
   m_diffBgColor = config->readColorEntry("DiffBgColor", &m_diffBgColor );
   m_colorA = config->readColorEntry("ColorA", &m_colorA );
   m_colorB = config->readColorEntry("ColorB", &m_colorB );
   m_colorC = config->readColorEntry("ColorC", &m_colorC );
   m_colorForConflict = config->readColorEntry("ColorForConflict", &m_colorForConflict );

   // EditConfigDlg
   m_bReplaceTabs = config->readBoolEntry("ReplaceTabs", m_bReplaceTabs );
   m_tabSize = config->readNumEntry("TabSize", m_tabSize );
   m_bAutoIndentation = config->readBoolEntry("AutoIndentation", m_bAutoIndentation );
   m_bAutoCopySelection = config->readBoolEntry("AutoCopySelection", m_bAutoCopySelection );

   // DiffConfigDlg
   m_bIgnoreWhiteSpace = config->readBoolEntry("IgnoreWhiteSpace", m_bIgnoreWhiteSpace );
   m_bIgnoreTrivialMatches = config->readBoolEntry("IgnoreTrivialMatches", m_bIgnoreTrivialMatches );

   // Recent files (selectable in the OpenDialog)
   config->readListEntry( "RecentAFiles", m_recentAFiles, '|' );
   config->readListEntry( "RecentBFiles", m_recentBFiles, '|' );
   config->readListEntry( "RecentCFiles", m_recentCFiles, '|' );
   config->readListEntry( "RecentOutputFiles", m_recentOutputFiles, '|' );

   setState();
}

void OptionDialog::slotHelp( void )
{
   KDialogBase::slotHelp();
}
