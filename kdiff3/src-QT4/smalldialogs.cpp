/***************************************************************************
 *   Copyright (C) 2005-2007 by Joachim Eibl                               *
 *   joachim.eibl at gmx.de                                                *
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

#include "smalldialogs.h"
#include "options.h"
#include "diff.h"

#include <QComboBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QLabel>
#include <QLayout>
#include <QToolTip>
#include <QUrl>
#include <QDir>
#include <QDropEvent>
#include <QMenu>
#include <QPushButton>

#include <kfiledialog.h>
#include <klocale.h>

// OpenDialog **************************************************************

OpenDialog::OpenDialog(
   QWidget* pParent, const QString& n1, const QString& n2, const QString& n3,
   bool bMerge, const QString& outputName, const char* slotConfigure, Options* pOptions )
: QDialog( pParent )
{
   setObjectName("OpenDialog");
   setModal(true);
   m_pOptions = pOptions;

   QVBoxLayout* v = new QVBoxLayout( this );
   v->setMargin(5);
   QGridLayout* h = new QGridLayout();
   v->addLayout(h);
   h->setSpacing( 5 );
   h->setColumnStretch( 1, 10 );

   QLabel* label  = new QLabel( i18n("A (Base):"), this );

   m_pLineA = new QComboBox();
   m_pLineA->setEditable(true);
   m_pLineA->insertItems( 0, m_pOptions->m_recentAFiles );
   m_pLineA->setEditText( KUrl(n1).prettyUrl() );
   m_pLineA->setMinimumWidth( 200 );
   QPushButton * button = new QPushButton( i18n("File..."), this );
   connect( button, SIGNAL(clicked()), this, SLOT( selectFileA() ) );
   QPushButton * button2 = new QPushButton( i18n("Dir..."), this );
   connect( button2, SIGNAL(clicked()), this, SLOT( selectDirA() ) );
   connect( m_pLineA, SIGNAL(editTextChanged(const QString&)), this, SLOT(inputFilenameChanged() ) );

   h->addWidget( label,    0, 0 );
   h->addWidget( m_pLineA,  0, 1 );
   h->addWidget( button,   0, 2 );
   h->addWidget( button2,  0, 3 );

   label    = new QLabel( "B:", this );
   m_pLineB  = new QComboBox();
   m_pLineB->setEditable(true);
   m_pLineB->insertItems( 0, m_pOptions->m_recentBFiles );
   m_pLineB->setEditText( KUrl(n2).prettyUrl() );
   m_pLineB->setMinimumWidth( 200 );
   button   = new QPushButton( i18n("File..."), this );
   connect( button, SIGNAL(clicked()), this, SLOT( selectFileB() ) );
   button2   = new QPushButton( i18n("Dir..."), this );
   connect( button2, SIGNAL(clicked()), this, SLOT( selectDirB() ) );
   connect( m_pLineB, SIGNAL(editTextChanged(const QString&)), this, SLOT(inputFilenameChanged() ) );

   h->addWidget( label,     1, 0 );
   h->addWidget( m_pLineB,  1, 1 );
   h->addWidget( button,    1, 2 );
   h->addWidget( button2,   1, 3 );

   label  = new QLabel( i18n("C (Optional):"), this );
   m_pLineC= new QComboBox();
   m_pLineC->setEditable(true);
   m_pLineC->insertItems( 0, m_pOptions->m_recentCFiles );
   m_pLineC->setEditText( KUrl(n3).prettyUrl() );
   m_pLineC->setMinimumWidth( 200 );
   button = new QPushButton( i18n("File..."), this );
   connect( button, SIGNAL(clicked()), this, SLOT( selectFileC() ) );
   button2   = new QPushButton( i18n("Dir..."), this );
   connect( button2, SIGNAL(clicked()), this, SLOT( selectDirC() ) );
   connect( m_pLineC, SIGNAL(editTextChanged(const QString&)), this, SLOT(inputFilenameChanged() ) );

   h->addWidget( label,     2, 0 );
   h->addWidget( m_pLineC,   2, 1 );
   h->addWidget( button,    2, 2 );
   h->addWidget( button2,   2, 3 );

   m_pMerge = new QCheckBox( i18n("Merge"), this );
   h->addWidget( m_pMerge, 3, 0 );

   QHBoxLayout* hl = new QHBoxLayout();
   h->addLayout( hl, 3, 1 );
   hl->addStretch(2);
   button = new QPushButton(i18n("Swap/Copy Names ..."), this);
   //button->setToggleButton(false);
   hl->addWidget( button );

   QMenu* m = new QMenu(this);
   m->addAction( i18n("Swap %1<->%2", QString("A"),QString("B") ));
   m->addAction( i18n("Swap %1<->%2",QString("B"),QString("C") ));
   m->addAction( i18n("Swap %1<->%2",QString("C"),QString("A") ));
   m->addAction( i18n("Copy %1->Output",QString("A")  ));
   m->addAction( i18n("Copy %1->Output",QString("B")  ));
   m->addAction( i18n("Copy %1->Output",QString("C")  ));
   m->addAction( i18n("Swap %1<->Output",QString("A") ));
   m->addAction( i18n("Swap %1<->Output",QString("B") ));
   m->addAction( i18n("Swap %1<->Output",QString("C") ));
   connect( m, SIGNAL(triggered(QAction*)), this, SLOT(slotSwapCopyNames(QAction*)));
   button->setMenu(m);


   hl->addStretch(2);

   label  = new QLabel( i18n("Output (optional):"), this );
   m_pLineOut = new QComboBox();
   m_pLineOut->setEditable(true);
   m_pLineOut->insertItems( 0, m_pOptions->m_recentOutputFiles );
   m_pLineOut->setEditText( KUrl(outputName).prettyUrl() );
   m_pLineOut->setMinimumWidth( 200 );
   button = new QPushButton( i18n("File..."), this );
   connect( button, SIGNAL(clicked()), this, SLOT( selectOutputName() ) );
   button2   = new QPushButton( i18n("Dir..."), this );
   connect( button2, SIGNAL(clicked()), this, SLOT( selectOutputDir() ) );
   connect( m_pMerge, SIGNAL(stateChanged(int)), this, SLOT(internalSlot(int)) );
   connect( this, SIGNAL(internalSignal(bool)), m_pLineOut, SLOT(setEnabled(bool)) );
   connect( this, SIGNAL(internalSignal(bool)), button, SLOT(setEnabled(bool)) );
   connect( this, SIGNAL(internalSignal(bool)), button2, SLOT(setEnabled(bool)) );

   m_pMerge->setChecked( !bMerge );
   m_pMerge->setChecked( bMerge );
//   m_pLineOutput->setEnabled( bMerge );

//   button->setEnabled( bMerge );

   h->addWidget( label,          4, 0 );
   h->addWidget( m_pLineOut,      4, 1 );
   h->addWidget( button,         4, 2 );
   h->addWidget( button2,        4, 3 );

   h->addItem(new QSpacerItem(200, 0), 0, 1);

   QHBoxLayout* l = new QHBoxLayout();
   v->addLayout( l );
   l->setSpacing(5);

   button = new QPushButton( i18n("Configure..."), this );
   connect( button, SIGNAL(clicked()), pParent, slotConfigure );
   l->addWidget( button, 1 );

   l->addStretch(1);

   button = new QPushButton( i18n("&OK"), this );
   button->setDefault( true );
   connect( button, SIGNAL(clicked()), this, SLOT( accept() ) );
   l->addWidget( button, 1 );

   button = new QPushButton( i18n("&Cancel"), this );
   connect( button, SIGNAL(clicked()), this, SLOT( reject() ) );
   l->addWidget( button,1 );

   QSize sh = sizeHint();
   setFixedHeight( sh.height() );
   m_bInputFileNameChanged = false;

#ifdef KREPLACEMENTS_H
   m_pLineA->lineEdit()->installEventFilter( this );
   m_pLineB->lineEdit()->installEventFilter( this );
   m_pLineC->lineEdit()->installEventFilter( this );
   m_pLineOut->lineEdit()->installEventFilter( this );
#endif
}

// Eventfilter: Only needed under Windows.
// Without this, files dropped in the line edit have URL-encoding.
// This eventfilter decodes the filenames as needed by KDiff3.
bool OpenDialog::eventFilter(QObject* o, QEvent* e)
{
   if ( e->type()==QEvent::DragEnter )
   {
      QDragEnterEvent* d = static_cast<QDragEnterEvent*>(e);
      d->setAccepted( d->mimeData()->hasUrls() );
      return true;
   }
   if (e->type()==QEvent::Drop)
   {
      QDropEvent* d = static_cast<QDropEvent*>(e);

      if ( !d->mimeData()->hasUrls() )
         return false;

      QList<QUrl> lst = d->mimeData()->urls();

      if ( lst.count() > 0 )
      {
         static_cast<QLineEdit*>(o)->setText( QDir::toNativeSeparators( lst[0].toLocalFile() ) );
         static_cast<QLineEdit*>(o)->setFocus();
      }
       
      return true;
   }
   return false;
}


void OpenDialog::selectURL( QComboBox* pLine, bool bDir, int i, bool bSave )
{
   QString current = pLine->currentText();
   if (current.isEmpty() && i>3 ){  current = m_pLineC->currentText(); }
   if (current.isEmpty()        ){  current = m_pLineB->currentText(); }
   if (current.isEmpty()        ){  current = m_pLineA->currentText(); }
   KUrl newURL = bDir ? KFileDialog::getExistingDirectoryUrl( current, this)
                      : bSave ? KFileDialog::getSaveUrl( current, 0, this)
                              : KFileDialog::getOpenUrl( current, 0, this);
   if ( !newURL.isEmpty() )
   {
      pLine->setEditText( newURL.url() );
   }
   // newURL won't be modified if nothing was selected.
}

void OpenDialog::selectFileA()     {  selectURL( m_pLineA,    false, 1, false );  }
void OpenDialog::selectFileB()     {  selectURL( m_pLineB,    false, 2, false );  }
void OpenDialog::selectFileC()     {  selectURL( m_pLineC,    false, 3, false );  }
void OpenDialog::selectOutputName(){  selectURL( m_pLineOut,  false, 4, true );  }
void OpenDialog::selectDirA()      {  selectURL( m_pLineA,    true,  1, false );  }
void OpenDialog::selectDirB()      {  selectURL( m_pLineB,    true,  2, false );  }
void OpenDialog::selectDirC()      {  selectURL( m_pLineC,    true,  3, false );  }
void OpenDialog::selectOutputDir() {  selectURL( m_pLineOut,  true,  4, true );  }

void OpenDialog::internalSlot(int i)
{
   emit internalSignal(i!=0);
}

// Clear the output-filename when any input-filename changed, 
// because users forgot to change the output and accidently overwrote it with
// wrong data during a merge.
void OpenDialog::inputFilenameChanged()
{
   if(!m_bInputFileNameChanged)
   {
      m_bInputFileNameChanged=true;
      m_pLineOut->clearEditText();
   }
}

static void fixCurrentText( QComboBox* pCB )
{
   QString s = pCB->currentText();

   int pos = s.indexOf( '\n' );
   if ( pos>=0 )
      s=s.left(pos);
   pos = s.indexOf( '\r' );
   if ( pos>=0 )
      s=s.left(pos);

   pCB->setEditText( s );
}

void OpenDialog::accept()
{
   int maxNofRecentFiles = 10;
   fixCurrentText( m_pLineA );

   QString s = m_pLineA->currentText();
   s = QUrl::fromLocalFile(s).toLocalFile();
   QStringList* sl = &m_pOptions->m_recentAFiles;
   // If an item exist, remove it from the list and reinsert it at the beginning.
   sl->removeAll(s);
   if ( !s.isEmpty() ) sl->prepend( s );
   if (sl->count()>maxNofRecentFiles) sl->erase( sl->begin()+maxNofRecentFiles, sl->end() );

   fixCurrentText( m_pLineB );
   s = m_pLineB->currentText();
   s = QUrl::fromLocalFile(s).toLocalFile();
   sl = &m_pOptions->m_recentBFiles;
   sl->removeAll(s);
   if ( !s.isEmpty() ) sl->prepend( s );
   if (sl->count()>maxNofRecentFiles) sl->erase( sl->begin()+maxNofRecentFiles, sl->end() );

   fixCurrentText( m_pLineC );
   s = m_pLineC->currentText();
   s = QUrl::fromLocalFile(s).toLocalFile();
   sl = &m_pOptions->m_recentCFiles;
   sl->removeAll(s);
   if ( !s.isEmpty() ) sl->prepend( s );
   if (sl->count()>maxNofRecentFiles) sl->erase( sl->begin()+maxNofRecentFiles, sl->end() );

   fixCurrentText( m_pLineOut );
   s = m_pLineOut->currentText();
   s = QUrl::fromLocalFile(s).toLocalFile();
   sl = &m_pOptions->m_recentOutputFiles;
   sl->removeAll(s);
   if ( !s.isEmpty() ) sl->prepend( s );
   if (sl->count()>maxNofRecentFiles) sl->erase( sl->begin()+maxNofRecentFiles, sl->end() );

   QDialog::accept();
}

void OpenDialog::slotSwapCopyNames( QAction* pAction ) // id selected in the popup menu
{
   int id = pAction->parentWidget()->actions().indexOf(pAction);
   QComboBox* cb1=0;
   QComboBox* cb2=0;
   switch(id)
   {
   case 0:  cb1=m_pLineA; cb2=m_pLineB; break;
   case 1:  cb1=m_pLineB; cb2=m_pLineC; break;
   case 2:  cb1=m_pLineC; cb2=m_pLineA; break;
   case 3:  cb1=m_pLineA; cb2=m_pLineOut; break;
   case 4:  cb1=m_pLineB; cb2=m_pLineOut; break;
   case 5:  cb1=m_pLineC; cb2=m_pLineOut; break;
   case 6:  cb1=m_pLineA; cb2=m_pLineOut; break;
   case 7:  cb1=m_pLineB; cb2=m_pLineOut; break;
   case 8:  cb1=m_pLineC; cb2=m_pLineOut; break;
   }
   if ( cb1 && cb2 )
   {
      QString t1 = cb1->currentText();
      QString t2 = cb2->currentText();
      cb2->setEditText(t1);
      if ( id<=2 || id>=6 )
      {
         cb1->setEditText( t2 );
      }
   }
}

// FindDialog *********************************************

FindDialog::FindDialog(QWidget* pParent)
: QDialog( pParent )
{
   QGridLayout* layout = new QGridLayout( this );
   layout->setMargin(5);
   layout->setSpacing(5);

   int line=0;
   layout->addWidget( new QLabel(i18n("Search text:"),this), line, 0, 1, 2 );
   ++line;

   m_pSearchString = new QLineEdit( this );
   layout->addWidget( m_pSearchString, line, 0, 1, 2 );
   ++line;

   m_pCaseSensitive = new QCheckBox(i18n("Case sensitive"),this);
   layout->addWidget( m_pCaseSensitive, line, 1 );

   m_pSearchInA = new QCheckBox(i18n("Search A"),this);
   layout->addWidget( m_pSearchInA, line, 0 );
   m_pSearchInA->setChecked( true );
   ++line;

   m_pSearchInB = new QCheckBox(i18n("Search B"),this);
   layout->addWidget( m_pSearchInB, line, 0 );
   m_pSearchInB->setChecked( true );
   ++line;

   m_pSearchInC = new QCheckBox(i18n("Search C"),this);
   layout->addWidget( m_pSearchInC, line, 0 );
   m_pSearchInC->setChecked( true );
   ++line;

   m_pSearchInOutput = new QCheckBox(i18n("Search output"),this);
   layout->addWidget( m_pSearchInOutput, line, 0 );
   m_pSearchInOutput->setChecked( true );
   ++line;

   QPushButton* pButton = new QPushButton( i18n("&Search"), this );
   layout->addWidget( pButton, line, 0 );
   connect( pButton, SIGNAL(clicked()), this, SLOT(accept()));

   pButton = new QPushButton( i18n("&Cancel"), this );
   layout->addWidget( pButton, line, 1 );
   connect( pButton, SIGNAL(clicked()), this, SLOT(reject()));

   hide();
}

void FindDialog::setVisible( bool bVisible )
{
   QDialog::setVisible( bVisible );
   m_pSearchString->selectAll();
   m_pSearchString->setFocus();
}

RegExpTester::RegExpTester( QWidget* pParent, const QString& autoMergeRegExpToolTip,
   const QString& historyStartRegExpToolTip, const QString& historyEntryStartRegExpToolTip, const QString& historySortKeyOrderToolTip )
: QDialog( pParent)
{
   int line=0;
   setWindowTitle(i18n("Regular Expression Tester"));
   QGridLayout* pGrid = new QGridLayout( this );
   pGrid->setSpacing(5);
   pGrid->setMargin(5);

   QLabel* l = new QLabel(i18n("Auto merge regular expression:"), this);
   pGrid->addWidget(l,line,0);
   l->setToolTip( autoMergeRegExpToolTip );
   m_pAutoMergeRegExpEdit = new QLineEdit(this);
   pGrid->addWidget(m_pAutoMergeRegExpEdit,line,1);
   connect( m_pAutoMergeRegExpEdit, SIGNAL(textChanged(const QString&)), this, SLOT(slotRecalc()));
   ++line;

   l = new QLabel(i18n("Example auto merge line:"), this);
   pGrid->addWidget(l,line,0);
   l->setToolTip( i18n("To test auto merge, copy a line as used in your files.") );
   m_pAutoMergeExampleEdit = new QLineEdit(this);
   pGrid->addWidget(m_pAutoMergeExampleEdit,line,1);
   connect( m_pAutoMergeExampleEdit, SIGNAL(textChanged(const QString&)), this, SLOT(slotRecalc()));
   ++line;

   l = new QLabel(i18n("Match result:"), this);
   pGrid->addWidget(l,line,0);
   m_pAutoMergeMatchResult = new QLineEdit(this);
   m_pAutoMergeMatchResult->setReadOnly(true);
   pGrid->addWidget(m_pAutoMergeMatchResult,line,1);
   ++line;

   pGrid->addItem( new QSpacerItem(100,20), line, 0 );
   pGrid->setRowStretch( line, 5);
   ++line;

   l = new QLabel(i18n("History start regular expression:"), this);
   pGrid->addWidget(l,line,0);
   l->setToolTip( historyStartRegExpToolTip );
   m_pHistoryStartRegExpEdit = new QLineEdit(this);
   pGrid->addWidget(m_pHistoryStartRegExpEdit,line,1);
   connect( m_pHistoryStartRegExpEdit, SIGNAL(textChanged(const QString&)), this, SLOT(slotRecalc()));
   ++line;

   l = new QLabel(i18n("Example history start line (with leading comment):"), this);
   pGrid->addWidget(l,line,0);
   l->setToolTip( i18n("Copy a history start line as used in your files,\n"
                          "including the leading comment.") );
   m_pHistoryStartExampleEdit = new QLineEdit(this);
   pGrid->addWidget(m_pHistoryStartExampleEdit,line,1);
   connect( m_pHistoryStartExampleEdit, SIGNAL(textChanged(const QString&)), this, SLOT(slotRecalc()));
   ++line;

   l = new QLabel(i18n("Match result:"), this);
   pGrid->addWidget(l,line,0);
   m_pHistoryStartMatchResult = new QLineEdit(this);
   m_pHistoryStartMatchResult->setReadOnly(true);
   pGrid->addWidget(m_pHistoryStartMatchResult,line,1);
   ++line;

   pGrid->addItem( new QSpacerItem(100,20), line, 0 );
   pGrid->setRowStretch( line, 5);
   ++line;

   l = new QLabel(i18n("History entry start regular expression:"), this);
   pGrid->addWidget(l,line,0);
   l->setToolTip( historyEntryStartRegExpToolTip );
   m_pHistoryEntryStartRegExpEdit = new QLineEdit(this);
   pGrid->addWidget(m_pHistoryEntryStartRegExpEdit,line,1);
   connect( m_pHistoryEntryStartRegExpEdit, SIGNAL(textChanged(const QString&)), this, SLOT(slotRecalc()));
   ++line;

   l = new QLabel(i18n("History sort key order:"), this);
   pGrid->addWidget(l,line,0);
   l->setToolTip( historySortKeyOrderToolTip );
   m_pHistorySortKeyOrderEdit = new QLineEdit(this);
   pGrid->addWidget(m_pHistorySortKeyOrderEdit,line,1);
   connect( m_pHistorySortKeyOrderEdit, SIGNAL(textChanged(const QString&)), this, SLOT(slotRecalc()));
   ++line;

   l = new QLabel(i18n("Example history entry start line (without leading comment):"), this);
   pGrid->addWidget(l,line,0);
   l->setToolTip( i18n("Copy a history entry start line as used in your files,\n"
                          "but omit the leading comment.") );
   m_pHistoryEntryStartExampleEdit = new QLineEdit(this);
   pGrid->addWidget(m_pHistoryEntryStartExampleEdit,line,1);
   connect( m_pHistoryEntryStartExampleEdit, SIGNAL(textChanged(const QString&)), this, SLOT(slotRecalc()));
   ++line;

   l = new QLabel(i18n("Match result:"), this);
   pGrid->addWidget(l,line,0);
   m_pHistoryEntryStartMatchResult = new QLineEdit(this);
   m_pHistoryEntryStartMatchResult->setReadOnly(true);
   pGrid->addWidget(m_pHistoryEntryStartMatchResult,line,1);
   ++line;

   l = new QLabel(i18n("Sort key result:"), this);
   pGrid->addWidget(l,line,0);
   m_pHistorySortKeyResult = new QLineEdit(this);
   m_pHistorySortKeyResult->setReadOnly(true);
   pGrid->addWidget(m_pHistorySortKeyResult,line,1);
   ++line;

   QPushButton* pButton = new QPushButton(i18n("OK"), this);
   pGrid->addWidget(pButton,line,0);
   connect( pButton, SIGNAL(clicked()), this, SLOT(accept()));

   pButton = new QPushButton(i18n("Cancel"), this);
   pGrid->addWidget(pButton,line,1);
   connect( pButton, SIGNAL(clicked()), this, SLOT(reject()));

   resize( 800, sizeHint().height() );
}

void RegExpTester::init( const QString& autoMergeRegExp, const QString& historyStartRegExp, const QString& historyEntryStartRegExp, const QString historySortKeyOrder )
{
   m_pAutoMergeRegExpEdit->setText( autoMergeRegExp );
   m_pHistoryStartRegExpEdit->setText( historyStartRegExp );
   m_pHistoryEntryStartRegExpEdit->setText( historyEntryStartRegExp );
   m_pHistorySortKeyOrderEdit->setText( historySortKeyOrder );
}

QString RegExpTester::autoMergeRegExp()
{
   return m_pAutoMergeRegExpEdit->text();
}

QString RegExpTester::historyStartRegExp()
{
   return m_pHistoryStartRegExpEdit->text();
}

QString RegExpTester::historyEntryStartRegExp()
{
   return m_pHistoryEntryStartRegExpEdit->text();
}

QString RegExpTester::historySortKeyOrder()
{
   return m_pHistorySortKeyOrderEdit->text();
}

void RegExpTester::slotRecalc()
{
   QRegExp autoMergeRegExp( m_pAutoMergeRegExpEdit->text() );
   if ( autoMergeRegExp.exactMatch( m_pAutoMergeExampleEdit->text() ) )
   {
      m_pAutoMergeMatchResult->setText( i18n("Match success.") );
   }
   else
   {
      m_pAutoMergeMatchResult->setText( i18n("Match failed.") );
   }

   QRegExp historyStartRegExp( m_pHistoryStartRegExpEdit->text() );
   if ( historyStartRegExp.exactMatch( m_pHistoryStartExampleEdit->text() ) )
   {
      m_pHistoryStartMatchResult->setText( i18n("Match success.") );
   }
   else
   {
      m_pHistoryStartMatchResult->setText( i18n("Match failed.") );
   }


   QStringList parenthesesGroups;
   bool bSuccess = findParenthesesGroups( m_pHistoryEntryStartRegExpEdit->text(), parenthesesGroups );
   if ( ! bSuccess )
   {
      m_pHistoryEntryStartMatchResult->setText( i18n("Opening and closing parentheses do not match in regular expression.") );
      m_pHistorySortKeyResult->setText( "" );
      return;
   }
   QRegExp historyEntryStartRegExp( m_pHistoryEntryStartRegExpEdit->text() );
   QString s = m_pHistoryEntryStartExampleEdit->text();

   if ( historyEntryStartRegExp.exactMatch( s ) )
   {
      m_pHistoryEntryStartMatchResult->setText( i18n("Match success.") );
      QString key = calcHistorySortKey( m_pHistorySortKeyOrderEdit->text(),historyEntryStartRegExp,parenthesesGroups);
      m_pHistorySortKeyResult->setText(key);
   }
   else
   {
      m_pHistoryEntryStartMatchResult->setText( i18n("Match failed.") );
      m_pHistorySortKeyResult->setText( "" );
   }
}

//#include "smalldialogs.moc"
