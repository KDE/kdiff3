/***************************************************************************
                          kdiff.cpp  -  description
                             -------------------
    begin                : Mon Mär 18 20:04:50 CET 2002
    copyright            : (C) 2002 by Joachim Eibl
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
 * Revision 1.1  2002/08/18 16:24:10  joachim99
 * Initial revision
 *                                                                   *
 ***************************************************************************/

#include <iostream>
#include <qaccel.h>

#include <kfiledialog.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <kfontdialog.h>
#include <kstatusbar.h>

#include <qclipboard.h>
#include <qscrollbar.h>
#include <qlayout.h>
#include <qsplitter.h>
#include <qlabel.h>
#include <qpushbutton.h>
#include <qlineedit.h>
#include <qcheckbox.h>
#include <qcombobox.h>
#include <minmax.h>
#include <assert.h>

#include "kdiff3.h"
#include "optiondialog.h"

using namespace std;

int g_tabSize = 8;
bool g_bIgnoreWhiteSpace = true;
bool g_bIgnoreTrivialMatches = true;

void KDiff3App::init( const char* filename1, const char* filename2, const char* filename3 )
{
   delete m_pBuf1;   m_pBuf1 = 0;
   delete m_pBuf2;   m_pBuf2 = 0;
   delete m_pBuf3;   m_pBuf3 = 0;

   g_bIgnoreWhiteSpace = m_pOptionDialog->m_bIgnoreWhiteSpace;
   g_bIgnoreTrivialMatches = m_pOptionDialog->m_bIgnoreTrivialMatches;

   m_diff3LineList.clear();

   // First get all input data.
   m_pBuf1 = readFile( filename1, m_size1 );
   m_v1size = preprocess( m_pBuf1, m_size1, m_v1 );

   m_pBuf2 = readFile( filename2, m_size2 );
   m_v2size = preprocess( m_pBuf2, m_size2, m_v2 );

   // Run the diff.
   if ( filename3 == 0 || filename3[0]=='\0' )
   {
      calcDiff( &m_v1[0], m_v1size, &m_v2[0], m_v2size, m_diffList12, 2 );

      calcDiff3LineListUsingAB( &m_diffList12, m_diff3LineList );
      fineDiff( m_diff3LineList, 1, &m_v1[0], &m_v2[0] );      
   }

   else
   {
      m_pBuf3 = readFile( filename3, m_size3 );

      m_v3size = preprocess( m_pBuf3, m_size3, m_v3 );

      calcDiff( &m_v1[0], m_v1size, &m_v2[0], m_v2size, m_diffList12, 2 );
      calcDiff( &m_v2[0], m_v2size, &m_v3[0], m_v3size, m_diffList23, 2 );
      calcDiff( &m_v1[0], m_v1size, &m_v3[0], m_v3size, m_diffList13, 2 );

      calcDiff3LineListUsingAB( &m_diffList12, m_diff3LineList );
      calcDiff3LineListUsingAC( &m_diffList13, m_diff3LineList );
      calcDiff3LineListTrim( m_diff3LineList, &m_v1[0], &m_v2[0], &m_v3[0] );

      calcDiff3LineListUsingBC( &m_diffList23, m_diff3LineList );
      calcDiff3LineListTrim( m_diff3LineList, &m_v1[0], &m_v2[0], &m_v3[0] );
      debugLineCheck( m_diff3LineList, m_v1size, 1 );
      debugLineCheck( m_diff3LineList, m_v2size, 2 );
      debugLineCheck( m_diff3LineList, m_v3size, 3 );

      fineDiff( m_diff3LineList, 1, &m_v1[0], &m_v2[0] );
      fineDiff( m_diff3LineList, 2, &m_v2[0], &m_v3[0] );
      fineDiff( m_diff3LineList, 3, &m_v3[0], &m_v1[0] );
   }

   // Calc needed lines for display
   m_neededLines = m_diff3LineList.size();

   // Calc max width for display
   m_maxWidth = 0;
   int j;
   for (j=0; j<m_v1size;++j )
   {
      m_maxWidth = max( m_v1[j].size, m_maxWidth );
   }
   for (j=0; j<m_v2size;++j )
   {
      m_maxWidth = max( m_v2[j].size, m_maxWidth );
   }

   if(filename3!=0 && filename3[0]!='\0')
   {
      for (j=0; j<m_v3size;++j )
      {
         m_maxWidth = max( m_v3[j].size, m_maxWidth );
      }
   }

   initView();
}


void KDiff3App::resizeDiffTextWindow(int newWidth, int newHeight)
{
   m_DTWHeight = newHeight;
   m_pDiffVScrollBar->setRange(0, max(0, m_neededLines+1 - newHeight) );
   m_pDiffVScrollBar->setPageStep( newHeight );
   m_pOverview->setRange( m_pDiffVScrollBar->value(), m_pDiffVScrollBar->pageStep() );

   // The second window has a somewhat inverse width
   m_pHScrollBar->setRange(0, max(0, m_maxWidth - newWidth) );
   m_pHScrollBar->setPageStep( newWidth );
}

void KDiff3App::resizeMergeResultWindow()
{
   MergeResultWindow* p = m_pMergeResultWindow;
   m_pMergeVScrollBar->setRange(0, max(0, p->getNofLines() - p->getNofVisibleLines()) );
   m_pMergeVScrollBar->setPageStep( p->getNofVisibleLines() );

   // The second window has a somewhat inverse width
//   m_pHScrollBar->setRange(0, max(0, m_maxWidth - newWidth) );
//   m_pHScrollBar->setPageStep( newWidth );
}

void KDiff3App::scrollDiffTextWindow( int deltaX, int deltaY )
{
   if ( deltaY!= 0 )
   {
      m_pDiffVScrollBar->setValue( m_pDiffVScrollBar->value() + deltaY );
      m_pOverview->setRange( m_pDiffVScrollBar->value(), m_pDiffVScrollBar->pageStep() );
   }
   if ( deltaX!= 0)
      m_pHScrollBar->setValue( m_pHScrollBar->value() + deltaX );
}

void KDiff3App::scrollMergeResultWindow( int deltaX, int deltaY )
{
   if ( deltaY!= 0 )
      m_pMergeVScrollBar->setValue( m_pMergeVScrollBar->value() + deltaY );
   if ( deltaX!= 0)
      m_pHScrollBar->setValue( m_pHScrollBar->value() + deltaX );
}

void KDiff3App::setDiff3Line( int line )
{
   m_pDiffVScrollBar->setValue( line );
}

void KDiff3App::sourceMask( int srcMask, int enabledMask )
{
   chooseA->setChecked( (srcMask & 1) != 0 );

   chooseB->setChecked( (srcMask & 2) != 0 );
   chooseC->setChecked( (srcMask & 4) != 0 );
   chooseA->setEnabled( (enabledMask & 1) != 0 );
   chooseB->setEnabled( (enabledMask & 2) != 0 );
   chooseC->setEnabled( (enabledMask & 4) != 0 );
}



// Function uses setMinSize( sizeHint ) before adding the widget.
// void addWidget(QBoxLayout* layout, QWidget* widget);
template <class W, class L>
void addWidget( L* layout, W* widget)
{
   QSize s = widget->sizeHint();
   widget->setMinimumSize( QSize(max(s.width(),0),max(s.height(),0) ) );
   layout->addWidget( widget );
}

void KDiff3App::initView()
{
   // set the main widget here
   QWidget* oldCentralWidget = centralWidget();
   if ( oldCentralWidget != 0 )
   {
      delete oldCentralWidget;
   }
   QFrame* pMainWidget = new QFrame(this);
   pMainWidget->setFrameStyle( QFrame::Panel | QFrame::Sunken );
   pMainWidget->setLineWidth(1);

   bool bVisibleMergeResultWindow = ! m_outputFilename.isEmpty();
   bool bTripleDiff = ! m_filename3.isEmpty();

   QVBoxLayout* pVLayout = new QVBoxLayout(pMainWidget);
   QSplitter* pVSplitter = 0;
   QSplitter* pDiffHSplitter = 0;
   QHBoxLayout* pDiffHLayout = 0;

   QWidget* pDiffWindowFrame = 0;

   if ( ! bVisibleMergeResultWindow )
   {
      pDiffWindowFrame = pMainWidget;
      pDiffHLayout = new QHBoxLayout( pVLayout );
   }
   else
   {
      pVSplitter = new QSplitter( pMainWidget );
      pVSplitter->setOrientation( Vertical );
      pVLayout->addWidget( pVSplitter );

      pDiffWindowFrame = new QFrame( pVSplitter );
      pDiffHLayout = new QHBoxLayout( pDiffWindowFrame );
   }

   pDiffHSplitter = new QSplitter( pDiffWindowFrame ); // horizontal
   pDiffHLayout->addWidget( pDiffHSplitter );

   m_pOverview = new Overview( pDiffWindowFrame, &m_diff3LineList, m_pOptionDialog, bTripleDiff );
   pDiffHLayout->addWidget(m_pOverview);
   connect( m_pOverview, SIGNAL(setLine(int)), this, SLOT(setDiff3Line(int)) );

   m_pDiffVScrollBar = new QScrollBar( Vertical, pDiffWindowFrame );
   pDiffHLayout->addWidget( m_pDiffVScrollBar );


   m_pDiffTextWindow1 = new DiffTextWindow( pDiffHSplitter, statusBar(), m_pOptionDialog );
   m_pDiffTextWindow1->init( m_filename1,
      &m_v1[0], m_v1size, &m_diff3LineList, 1, bTripleDiff );


   m_pDiffTextWindow2 = new DiffTextWindow( pDiffHSplitter, statusBar(), m_pOptionDialog );
   m_pDiffTextWindow2->init( m_filename2,
      &m_v2[0], m_v2size, &m_diff3LineList, 2, bTripleDiff );

   if ( bTripleDiff )
   {
      m_pDiffTextWindow3 = new DiffTextWindow( pDiffHSplitter, statusBar(), m_pOptionDialog );
      m_pDiffTextWindow3->init( m_filename3,
         &m_v3[0], m_v3size, &m_diff3LineList, 3, bTripleDiff );

   }
   else
   {
      m_pDiffTextWindow3=0;
   }

   if ( bVisibleMergeResultWindow   &&   pVSplitter !=0 )
   {
      // Merge window
      QFrame* pMergeWindowFrame = new QFrame( pVSplitter );
      QHBoxLayout* pMergeHLayout = new QHBoxLayout( pMergeWindowFrame );

      m_pMergeResultWindow = new MergeResultWindow( pMergeWindowFrame,
         &m_v1[0], &m_v2[0],
         bTripleDiff ? &m_v3[0] : 0,
         &m_diff3LineList, m_outputFilename,
         m_pOptionDialog
         );
      pMergeHLayout->addWidget( m_pMergeResultWindow );

      m_pMergeVScrollBar = new QScrollBar( Vertical, pMergeWindowFrame );
      pMergeHLayout->addWidget( m_pMergeVScrollBar );

      QValueList<int> sizes = pVSplitter->sizes();
      int total = sizes[0] + sizes[1];
      sizes[0]=total/2; sizes[1]=total/2;
      pVSplitter->setSizes( sizes );
   }
   else
   {
      m_pMergeResultWindow = new MergeResultWindow( pMainWidget,
         &m_v1[0], &m_v2[0],
         bTripleDiff ? &m_v3[0] : 0,
         &m_diff3LineList, m_outputFilename,
         m_pOptionDialog
         );
      m_pMergeResultWindow->hide();
      m_pMergeVScrollBar = 0;
   }

   QHBoxLayout* pHScrollBarLayout = new QHBoxLayout( pVLayout );
   m_pHScrollBar = new QScrollBar( Horizontal, pMainWidget );
   pHScrollBarLayout->addWidget( m_pHScrollBar );
   m_pCornerWidget = new QWidget( pMainWidget );
   pHScrollBarLayout->addWidget( m_pCornerWidget );


   setCentralWidget(pMainWidget);

   connect( m_pDiffVScrollBar, SIGNAL(valueChanged(int)), m_pOverview, SLOT(setFirstLine(int)));

   connect( m_pDiffVScrollBar, SIGNAL(valueChanged(int)), m_pDiffTextWindow1, SLOT(setFirstLine(int)));
   connect( m_pHScrollBar, SIGNAL(valueChanged(int)), m_pDiffTextWindow1, SLOT(setFirstColumn(int)));
   connect( m_pDiffTextWindow1, SIGNAL(newSelection()), this, SLOT(slotSelectionStart()));
   connect( m_pDiffTextWindow1, SIGNAL(selectionEnd()), this, SLOT(slotSelectionEnd()));
   connect( m_pDiffTextWindow1, SIGNAL(scroll(int,int)), this, SLOT(scrollDiffTextWindow(int,int)));
   m_pDiffTextWindow1->installEventFilter( this );

   connect( m_pDiffVScrollBar, SIGNAL(valueChanged(int)), m_pDiffTextWindow2, SLOT(setFirstLine(int)));
   connect( m_pHScrollBar, SIGNAL(valueChanged(int)), m_pDiffTextWindow2, SLOT(setFirstColumn(int)));
   connect( m_pDiffTextWindow2, SIGNAL(newSelection()), this, SLOT(slotSelectionStart()));
   connect( m_pDiffTextWindow2, SIGNAL(selectionEnd()), this, SLOT(slotSelectionEnd()));
   connect( m_pDiffTextWindow2, SIGNAL(scroll(int,int)), this, SLOT(scrollDiffTextWindow(int,int)));
   m_pDiffTextWindow2->installEventFilter( this );

   if ( m_pDiffTextWindow3 != 0 )
   {
      connect( m_pDiffVScrollBar, SIGNAL(valueChanged(int)), m_pDiffTextWindow3, SLOT(setFirstLine(int)));
      connect( m_pHScrollBar, SIGNAL(valueChanged(int)), m_pDiffTextWindow3, SLOT(setFirstColumn(int)));
      connect( m_pDiffTextWindow3, SIGNAL(newSelection()), this, SLOT(slotSelectionStart()));
      connect( m_pDiffTextWindow3, SIGNAL(selectionEnd()), this, SLOT(slotSelectionEnd()));
      connect( m_pDiffTextWindow3, SIGNAL(scroll(int,int)), this, SLOT(scrollDiffTextWindow(int,int)));
      m_pDiffTextWindow3->installEventFilter( this );
   }

   MergeResultWindow* p = m_pMergeResultWindow;
   if ( bVisibleMergeResultWindow )
   {
      connect( m_pMergeVScrollBar, SIGNAL(valueChanged(int)), p, SLOT(setFirstLine(int)));

      connect( m_pHScrollBar,      SIGNAL(valueChanged(int)), p, SLOT(setFirstColumn(int)));
      connect( p, SIGNAL(scroll(int,int)), this, SLOT(scrollMergeResultWindow(int,int)));
      connect( p, SIGNAL(sourceMask(int,int)), this, SLOT(sourceMask(int,int)));
      connect( p, SIGNAL( resizeSignal() ),this, SLOT(resizeMergeResultWindow()));
      connect( p, SIGNAL( selectionEnd() ), this, SLOT( slotSelectionEnd() ) );
      connect( p, SIGNAL( newSelection() ), this, SLOT( slotSelectionStart() ) );
      connect( p, SIGNAL( modified() ), this, SLOT( slotOutputModified() ) );
      connect( p, SIGNAL( savable(bool)), this, SLOT(slotOutputSavable(bool)));
   }
   else
   {
      sourceMask(0,0);
   }

   m_bOutputModified = false;


   connect( p, SIGNAL(setFastSelectorRange(int,int)), m_pDiffTextWindow1, SLOT(setFastSelectorRange(int,int)));
   connect( p, SIGNAL(setFastSelectorRange(int,int)), m_pDiffTextWindow2, SLOT(setFastSelectorRange(int,int)));
   connect(m_pDiffTextWindow1, SIGNAL(setFastSelectorLine(int)), p, SLOT(slotSetFastSelectorLine(int)));
   connect(m_pDiffTextWindow2, SIGNAL(setFastSelectorLine(int)), p, SLOT(slotSetFastSelectorLine(int)));
   if ( m_pDiffTextWindow3 != 0 )
   {
      connect(m_pDiffTextWindow3, SIGNAL(setFastSelectorLine(int)), p, SLOT(slotSetFastSelectorLine(int)));
      connect( p, SIGNAL(setFastSelectorRange(int,int)), m_pDiffTextWindow3, SLOT(setFastSelectorRange(int,int)));
   }



   connect( m_pDiffTextWindow1, SIGNAL( resizeSignal(int,int) ),this, SLOT(resizeDiffTextWindow(int,int)));

   m_pDiffTextWindow1->setFocus();
   show();
   pMainWidget->show();
   m_pCornerWidget->setFixedSize( m_pDiffVScrollBar->width(), m_pHScrollBar->height() );

   // TODO: This won't work correctly if the widgets are not yet visible.
   //       The show() commands above don't help.

   m_pDiffVScrollBar->setRange(0, max(0, m_neededLines+1 - m_pDiffTextWindow1->getNofLines() ) );
   m_pDiffVScrollBar->setPageStep( m_pDiffTextWindow1->getNofLines() );
   m_pOverview->setRange( 0, m_pDiffTextWindow1->getNofLines() );
}

void KDiff3App::resizeEvent(QResizeEvent* e)
{
   KMainWindow::resizeEvent(e);
   m_pCornerWidget->setFixedSize( m_pDiffVScrollBar->width(), m_pHScrollBar->height() );
}


bool KDiff3App::eventFilter( QObject* o, QEvent* e )
{
   if ( e->type() == QEvent::KeyPress ) {  // key press
       QKeyEvent *k = (QKeyEvent*)e;




       int deltaX=0;
       int deltaY=0;
       int pageSize =  m_DTWHeight;
       switch( k->key() )
       {
       case Key_Down:   ++deltaY; break;
       case Key_Up:     --deltaY; break;
       case Key_PageDown: deltaY+=pageSize; break;
       case Key_PageUp:   deltaY-=pageSize; break;
       case Key_Left:     --deltaX;  break;
       case Key_Right:    ++deltaX;  break;
       case Key_Home: if ( k->state() & ControlButton )  m_pDiffVScrollBar->setValue( 0 );
                      else                               m_pHScrollBar->setValue( 0 );
                      break;
       case Key_End:  if ( k->state() & ControlButton )  m_pDiffVScrollBar->setValue( m_pDiffVScrollBar->maxValue() );
                      else                               m_pHScrollBar->setValue( m_pHScrollBar->maxValue() );
                      break;
       default: break;
       }

       scrollDiffTextWindow( deltaX, deltaY );



       return TRUE;                        // eat event
   }
   return KMainWindow::eventFilter( o, e );    // standard event processing
}



OpenDialog::OpenDialog(
   QWidget* pParent, const char* n1, const char* n2, const char* n3,
   bool bMerge, const char* outputName, const char* slotConfigure, OptionDialog* pOptions )
: QDialog( pParent, "OpenDialog", true /*modal*/ )
{
   m_pOptions = pOptions;

   QVBoxLayout* v = new QVBoxLayout( this, 5 );
   QGridLayout* h = new QGridLayout( v, 5, 3, 5 );

   QLabel* label  = new QLabel( "A (Base):", this );

   m_lineA = new QComboBox( true, this );
   m_lineA->insertStrList( m_pOptions->m_recentAFiles );
   m_lineA->setEditText( n1 );
   QPushButton * button = new QPushButton( "Select...", this );
   connect( button, SIGNAL(clicked()), this, SLOT( selectFileA() ) );

   h->addWidget( label, 0, 0 );
   h->addWidget( m_lineA,  0, 1 );
   h->addWidget( button, 0, 2 );

   label    = new QLabel( "B:", this );
   m_lineB  = new QComboBox( true, this );
   m_lineB->insertStrList( m_pOptions->m_recentBFiles );
   m_lineB->setEditText( n2 );
   button   = new QPushButton( "Select...", this );
   connect( button, SIGNAL(clicked()), this, SLOT( selectFileB() ) );

   h->addWidget( label,  1, 0 );
   h->addWidget( m_lineB,   1, 1 );
   h->addWidget( button, 1, 2 );

   label  = new QLabel( "C (Optional):", this );
   m_lineC= new QComboBox( true, this );
   m_lineC->insertStrList( m_pOptions->m_recentCFiles );
   m_lineC->setEditText( n3 );
   button = new QPushButton( "Select...", this );
   connect( button, SIGNAL(clicked()), this, SLOT( selectFileC() ) );

   h->addWidget( label,     2, 0 );
   h->addWidget( m_lineC,   2, 1 );
   h->addWidget( button,    2, 2 );

   m_pMerge = new QCheckBox( "Merge", this );
   h->addWidget( m_pMerge, 3, 0 );

   label  = new QLabel( "Output (Optional):", this );
   m_lineOutput = new QComboBox( true, this );
   m_lineOutput->insertStrList( m_pOptions->m_recentOutputFiles );
   m_lineOutput->setEditText( outputName );
   button = new QPushButton( "Select...", this );
   connect( button, SIGNAL(clicked()), this, SLOT( selectOutputName() ) );
   connect( m_pMerge, SIGNAL(stateChanged(int)), this, SLOT(internalSlot(int)) );
   connect( this, SIGNAL(internalSignal(bool)), m_lineOutput, SLOT(setEnabled(bool)) );
   connect( this, SIGNAL(internalSignal(bool)), button, SLOT(setEnabled(bool)) );
   
   m_pMerge->setChecked( bMerge );
   m_lineOutput->setEnabled( bMerge );

   button->setEnabled( bMerge );

   h->addWidget( label,          4, 0 );
   h->addWidget( m_lineOutput,   4, 1 );
   h->addWidget( button,         4, 2 );

   h->addColSpacing( 1, 200 );

   QHBoxLayout* l = new QHBoxLayout( v, 5 );
   button = new QPushButton( "Ok", this );
   connect( button, SIGNAL(clicked()), this, SLOT( accept() ) );
   l->addWidget( button );

   button = new QPushButton( "Cancel", this );
   connect( button, SIGNAL(clicked()), this, SLOT( reject() ) );
   l->addWidget( button );

   button = new QPushButton( "Configure", this );
   connect( button, SIGNAL(clicked()), pParent, slotConfigure );
   l->addWidget( button );

   QSize sh = sizeHint();
   setFixedHeight( sh.height() );
}

void OpenDialog::selectFileA()
{
   QString fileName = KFileDialog::getOpenFileName(0,0,this);
//   QString fileName = KFileDialog::getOpenURL(QString::null,
//        i18n("*|All files"), this, i18n("Open File...")).fileName();
   m_lineA->setEditText( fileName );
}
void OpenDialog::selectFileB()
{
   QString fileName = KFileDialog::getOpenFileName(0,0,this);
   m_lineB->setEditText( fileName );
}
void OpenDialog::selectFileC()
{

   QString fileName = KFileDialog::getOpenFileName(0,0,this);
   m_lineC->setEditText( fileName );
}

void OpenDialog::selectOutputName()
{
   QString fileName = KFileDialog::getOpenFileName(0,0,this);
   m_lineOutput->setEditText( fileName );
}

void OpenDialog::internalSlot(int i)
{
   emit internalSignal(i!=0);
}

void OpenDialog::accept()
{
   unsigned int maxNofRecentFiles = 6;

   QString s = m_lineA->currentText();
   QStrList* sl = &m_pOptions->m_recentAFiles;
   if ( sl->find( s ) != -1 )  // A new item
   {
      s = sl->current();
      sl->remove();
   }
   if ( !s.isEmpty() ) sl->prepend( s );
   while ( sl->count()>maxNofRecentFiles ) sl->removeLast();

   s = m_lineB->currentText();
   sl = &m_pOptions->m_recentBFiles;
   if ( sl->find( s ) != -1 )  // A new item
   {
      s = sl->current();
      sl->remove();
   }
   if ( !s.isEmpty() ) sl->prepend( s );
   while ( sl->count()>maxNofRecentFiles ) sl->removeLast();

   s = m_lineC->currentText();
   sl = &m_pOptions->m_recentCFiles;
   if ( sl->find( s ) != -1 )  // A new item
   {
      s = sl->current();
      sl->remove();
   }
   if ( !s.isEmpty() ) sl->prepend( s );
   while ( sl->count()>maxNofRecentFiles ) sl->removeLast();

   s = m_lineOutput->currentText();
   sl = &m_pOptions->m_recentOutputFiles;
   if ( sl->find( s ) != -1 )  // A new item
   {
      s = sl->current();
      sl->remove();
   }
   if ( !s.isEmpty() ) sl->prepend( s );
   while ( sl->count()>maxNofRecentFiles ) sl->removeLast();

   QDialog::accept();
}

void KDiff3App::slotFileOpen()
{
   if(m_bOutputModified)
   {
      int result = KMessageBox::warningContinueCancel(this,
         i18n("The output file has been modified.\n"
              "If you continue your changes will be lost."),
         i18n("Warning"), i18n("Continue"), i18n("Cancel"));
      if ( result==KMessageBox::Cancel )
         return;

   }


   slotStatusMsg(i18n("Opening files..."));

   for(;;)
   {

      OpenDialog d(this, m_filename1, m_filename2, m_filename3,
         !m_outputFilename.isEmpty(),
         m_bDefaultFilename ? "" : (const char*)m_outputFilename,
         SLOT(slotConfigure()), m_pOptionDialog );
      int status = d.exec();
      if ( status == QDialog::Accepted )
      {
         m_filename1 = d.m_lineA->currentText();
         m_filename2 = d.m_lineB->currentText();
         m_filename3 = d.m_lineC->currentText();

         if ( d.m_pMerge->isChecked() )
         {
            if ( d.m_lineOutput->currentText().isEmpty() )
            {
               m_outputFilename = "unnamed.txt";
               m_bDefaultFilename = true;
            }
            else
            {
               m_outputFilename = d.m_lineOutput->currentText();
               m_bDefaultFilename = false;
            }
         }
         else
            m_outputFilename = QString();

         init( m_filename1, m_filename2, m_filename3 );
   //      setCaption(url.fileName(), false);

         if ( ! m_filename1.isEmpty() && m_pBuf1==0  ||
              ! m_filename2.isEmpty() && m_pBuf2==0  ||
              ! m_filename3.isEmpty() && m_pBuf3==0 )
         {
            QString text( i18n("Opening of these files failed:") );
            text += "\n\n";
            if ( ! m_filename1.isEmpty() && m_pBuf1==0 )
               text += " - " + m_filename1 + "\n";
            if ( ! m_filename2.isEmpty() && m_pBuf2==0 )
               text += " - " + m_filename2 + "\n";
            if ( ! m_filename3.isEmpty() && m_pBuf3==0 )
               text += " - " + m_filename3 + "\n";

            KMessageBox::sorry( this, text, i18n("File open error") );
            continue;
         }
      }
      break;
   }

   slotStatusMsg(i18n("Ready."));
}


void KDiff3App::slotEditCut()
{
   slotStatusMsg(i18n("Cutting selection..."));

   QString s;
   if ( m_pMergeResultWindow!=0 )
   {
      s = m_pMergeResultWindow->getSelection();
      m_pMergeResultWindow->deleteSelection();

      m_pMergeResultWindow->update();
   }

   if ( !s.isNull() )
   {
      QApplication::clipboard()->setText( s );
   }

   slotStatusMsg(i18n("Ready."));
}

void KDiff3App::slotEditCopy()
{
   slotStatusMsg(i18n("Copying selection to clipboard..."));

   QString s = m_pDiffTextWindow1->getSelection();
   if ( s.isNull() )
      s = m_pDiffTextWindow2->getSelection();
   if ( s.isNull() && m_pDiffTextWindow3!=0 )
      s = m_pDiffTextWindow3->getSelection();
   if ( s.isNull() && m_pMergeResultWindow!=0 )
      s = m_pMergeResultWindow->getSelection();
   if ( !s.isNull() )
   {
      QApplication::clipboard()->setText( s );
   }

   slotStatusMsg(i18n("Ready."));
}

void KDiff3App::slotEditPaste()
{
   slotStatusMsg(i18n("Inserting clipboard contents..."));

   if ( m_pMergeResultWindow!=0  )
   {
      m_pMergeResultWindow->pasteClipboard();
   }

   slotStatusMsg(i18n("Ready."));
}


void KDiff3App::slotGoTop()
{
   if (m_pMergeResultWindow)  m_pMergeResultWindow->slotGoTop();
}
void KDiff3App::slotGoBottom()
{
   if (m_pMergeResultWindow)  m_pMergeResultWindow->slotGoBottom();
}
void KDiff3App::slotGoPrevConflict()
{
   if (m_pMergeResultWindow)  m_pMergeResultWindow->slotGoPrevConflict();
}
void KDiff3App::slotGoNextConflict()
{
   if (m_pMergeResultWindow)  m_pMergeResultWindow->slotGoNextConflict();
}
void KDiff3App::slotGoPrevDelta()
{
   if (m_pMergeResultWindow)  m_pMergeResultWindow->slotGoPrevDelta();
}
void KDiff3App::slotGoNextDelta()
{
   if (m_pMergeResultWindow)  m_pMergeResultWindow->slotGoNextDelta();
}
void KDiff3App::slotChooseA()
{
   if (m_pMergeResultWindow)  m_pMergeResultWindow->slotChooseA();
}
void KDiff3App::slotChooseB()
{
   if (m_pMergeResultWindow)  m_pMergeResultWindow->slotChooseB();
}
void KDiff3App::slotChooseC()
{
   if (m_pMergeResultWindow)  m_pMergeResultWindow->slotChooseC();
}

void KDiff3App::slotConfigure()
{
   m_pOptionDialog->setState();
   m_pOptionDialog->incInitialSize ( QSize(0,40) );
   m_pOptionDialog->exec();
   slotRefresh();
}

void KDiff3App::slotRefresh()
{
   g_tabSize = m_pOptionDialog->m_tabSize;
   if (m_pDiffTextWindow1)   m_pDiffTextWindow1->update();
   if (m_pDiffTextWindow2)   m_pDiffTextWindow2->update();
   if (m_pDiffTextWindow3)   m_pDiffTextWindow3->update();
   if (m_pMergeResultWindow) m_pMergeResultWindow->update();
}

void KDiff3App::slotSelectionStart()
{
   editCopy->setEnabled( false );
   editCut->setEnabled( false );

   const QObject* s = sender();
   if (m_pDiffTextWindow1 && s!=m_pDiffTextWindow1)   m_pDiffTextWindow1->resetSelection();
   if (m_pDiffTextWindow2 && s!=m_pDiffTextWindow2)   m_pDiffTextWindow2->resetSelection();
   if (m_pDiffTextWindow3 && s!=m_pDiffTextWindow3)   m_pDiffTextWindow3->resetSelection();
   if (m_pMergeResultWindow && s!=m_pMergeResultWindow) m_pMergeResultWindow->resetSelection();
}

void KDiff3App::slotSelectionEnd()
{
   const QObject* s = sender();
   editCopy->setEnabled(true);
   editCut->setEnabled( s==m_pMergeResultWindow );
   if ( m_pOptionDialog->m_bAutoCopySelection )
   {
      slotEditCopy();
   }
}

void KDiff3App::slotClipboardChanged()
{
   QString s = QApplication::clipboard()->text();
   editPaste->setEnabled(!s.isEmpty());
}

void KDiff3App::slotOutputModified()
{
   m_bOutputModified=true;
}

void KDiff3App::slotOutputSavable( bool bSavable )
{
   fileSave->setEnabled( bSavable );
   fileSaveAs->setEnabled( bSavable );
}