/* This file is part of the KDiff3 project

   Copyright (C) 2008 Joachim Eibl <joachim dot eibl at gmx dot de>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; version 2
   of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#include "kdiff3plugin.h"

#include <kapplication.h>
#include <kstandarddirs.h>
#include <kaction.h>
#include <kactionmenu.h>
#include <klocale.h>
#include <kgenericfactory.h>
#include <kurl.h>
#include <kconfig.h>
#include <kconfiggroup.h>
#include <konq_popupmenuinformation.h>
#include <kmessagebox.h>
#include <kprocess.h>

//#include <iostream>

static QStringList* s_pHistory=0;

class KDiff3PluginFactory : public KGenericFactory < KDiff3Plugin, KonqPopupMenu >
{
   KConfig* m_pConfig;
   KConfigGroup* m_pConfigGroup;
public:
   KDiff3PluginFactory( const char* instanceName  = 0 )
   : KGenericFactory< KDiff3Plugin, KonqPopupMenu >( instanceName )
   {
      m_pConfig = 0;
      if (s_pHistory==0)
      {
         //std::cout << "New History: " << instanceName << std::endl;
         s_pHistory = new QStringList;
         m_pConfig = new KConfig( "kdiff3pluginrc", KConfig::SimpleConfig );
         m_pConfigGroup = new KConfigGroup( m_pConfig, "KDiff3Plugin" );
         *s_pHistory = m_pConfigGroup->readEntry("HistoryStack", QStringList() );
      }
   }

   ~KDiff3PluginFactory()
   {
      //std::cout << "Delete History" << std::endl;
      if ( s_pHistory && m_pConfigGroup )
         m_pConfigGroup->writeEntry("HistoryStack",*s_pHistory);
      delete s_pHistory;
      delete m_pConfigGroup;
      delete m_pConfig;
      s_pHistory = 0;
      m_pConfig = 0;
   }
};

K_EXPORT_COMPONENT_FACTORY (libkdiff3plugin, KDiff3PluginFactory ("kdiff3plugin"))

KDiff3Plugin::KDiff3Plugin( KonqPopupMenu* pPopupMenu, const QStringList & /* list */ )
:KonqPopupMenuPlugin(pPopupMenu)
{
   KGlobal::locale()->insertCatalog("kdiff3plugin");
   m_pPopupMenu = pPopupMenu;
   m_pParentWidget = pPopupMenu->parentWidget();
}

void KDiff3Plugin::setup( KActionCollection* actionCollection, const KonqPopupMenuInformation& popupMenuInfo, QMenu* pMenu )
{
   if (KStandardDirs::findExe("kdiff3").isNull ())
      return;

   // remember currently selected files (copy to a QStringList)
   KFileItemList itemList = popupMenuInfo.items();
   foreach ( const KFileItem& item, itemList )
   {
      //m_urlList.append( item.url() );
      m_list.append( item.url().url() );
   }


   /* Menu structure:
      KDiff3 -> (1 File selected):  Save 'selection' for later comparison (push onto history stack)
                                    Compare 'selection' with first file on history stack.
                                    Compare 'selection' with -> choice from history stack
                                    Merge 'selection' with first file on history stack.
                                    Merge 'selection' with last two files on history stack.
                (2 Files selected): Compare 's1' with 's2'
                                    Merge 's1' with 's2'
                (3 Files selected): Compare 's1', 's2' and 's3'
   */

   KActionMenu* pActionMenu = new KActionMenu (i18n ("KDiff3"), actionCollection );
   KAction* pAction = 0;
   QString s;

   if(m_list.count() == 1)
   {
      int historyCount = s_pHistory ? s_pHistory->count() : 0;
      s = i18n("Compare with %1", (historyCount>0 ? s_pHistory->front() : QString()) );
      pAction = new KAction ( s, actionCollection );
      connect( pAction, SIGNAL(triggered(bool)), this, SLOT(slotCompareWith()));
      pAction->setEnabled( m_list.count()>0 && historyCount>0 );
      pActionMenu->addAction(pAction);

      s = i18n("Merge with %1", historyCount>0 ? s_pHistory->front() : QString() );
      pAction = new KAction( s, actionCollection);
      connect( pAction, SIGNAL(triggered(bool)), this, SLOT(slotMergeWith()));
      pAction->setEnabled( m_list.count()>0 && historyCount>0 );
      pActionMenu->addAction (pAction);

      s = i18n("Save '%1' for later", ( m_list.front() ) );
      pAction = new KAction ( s, actionCollection);
      connect( pAction, SIGNAL(triggered(bool)), this, SLOT(slotSaveForLater()));
      pAction->setEnabled( m_list.count()>0 );
      pActionMenu->addAction(pAction);

      pAction = new KAction (i18n("3-way merge with base"), actionCollection);
      connect( pAction, SIGNAL(triggered(bool)), this, SLOT(slotMergeThreeWay()));
      pAction->setEnabled( m_list.count()>0 && historyCount>=2 );
      pActionMenu->addAction (pAction);

      if ( s_pHistory && !s_pHistory->empty() )
      {
         KActionMenu* pHistoryMenu = new KActionMenu( i18n("Compare with ..."), actionCollection );
         pHistoryMenu->setEnabled( m_list.count()>0 && historyCount>0 );
         pActionMenu->addAction(pHistoryMenu);
         for (QStringList::iterator i = s_pHistory->begin(); i!=s_pHistory->end(); ++i)
         {
            pAction = new KAction( *i, actionCollection);
            connect( pAction, SIGNAL(triggered(bool)), this, SLOT(slotCompareWithHistoryItem()));
            pHistoryMenu->addAction (pAction);
         }

         pAction = new KAction (i18n("Clear list"), actionCollection);
         connect( pAction, SIGNAL(triggered(bool)), this, SLOT(slotClearList()));
         pActionMenu->addAction (pAction);
         pAction->setEnabled( historyCount>0 );
      }
   }
   else if(m_list.count() == 2)
   {
      pAction = new KAction (i18n("Compare"), actionCollection);
      connect( pAction, SIGNAL(triggered(bool)), this, SLOT(slotCompareTwoFiles()));
      pActionMenu->addAction (pAction);
   }
   else if ( m_list.count() == 3 )
   {
      pAction = new KAction (i18n("3 way comparison"), actionCollection);
      connect( pAction, SIGNAL(triggered(bool)), this, SLOT(slotCompareThreeFiles()));
      pActionMenu->addAction (pAction);
   }
   pAction = new KAction (i18n("About KDiff3 menu plugin ..."), actionCollection);
   connect( pAction, SIGNAL(triggered(bool)), this, SLOT(slotAbout()));
   pActionMenu->addAction (pAction);

   pMenu->addSeparator();
   pMenu->addAction( pActionMenu );
   pMenu->addSeparator();
}

KDiff3Plugin::~KDiff3Plugin ()
{
}

void KDiff3Plugin::slotCompareWith()
{
   if ( m_list.count() > 0 && s_pHistory && ! s_pHistory->empty() )
   {
      QStringList args;
      args << s_pHistory->front();
      args << m_list.front();
      KProcess::startDetached("kdiff3", args);
   }
}

void KDiff3Plugin::slotCompareWithHistoryItem()
{
   const KAction* pAction = dynamic_cast<const KAction*>( sender() );
   if ( m_list.count() > 0 && pAction )
   {
      QStringList args;
      args << pAction->text();
      args << m_list.front();
      KProcess::startDetached ("kdiff3", args);
   }
}

void KDiff3Plugin::slotCompareTwoFiles()
{
   if ( m_list.count() == 2 )
   {
      QStringList args;
      args << m_list.front();
      args << m_list.back();
      KProcess::startDetached ("kdiff3", args);
   }
}

void KDiff3Plugin::slotCompareThreeFiles()
{
   if ( m_list.count() == 3 )
   {
      QStringList args;
      args << m_list[0];
      args << m_list[1];
      args << m_list[2];
      KProcess::startDetached ("kdiff3", args);
   }
}

void KDiff3Plugin::slotMergeWith()
{
   if ( m_list.count() > 0 && s_pHistory && ! s_pHistory->empty() )
   {
      QStringList args;
      args << s_pHistory->front();
      args << m_list.front();
      args << ( "-o" + m_list.front() );
      KProcess::startDetached ("kdiff3", args);
   }
}

void KDiff3Plugin::slotMergeThreeWay()
{
   if ( m_list.count() > 0 && s_pHistory &&  s_pHistory->count()>=2 )
   {
      QStringList args;
      args << (*s_pHistory)[1];
      args << (*s_pHistory)[0];
      args << m_list.front();
      args << ("-o" + m_list.front());
      KProcess::startDetached ("kdiff3", args);
   }
}

void KDiff3Plugin::slotSaveForLater()
{
   if ( !m_list.isEmpty() && s_pHistory )
   {
      while ( s_pHistory->count()>=10 )
         s_pHistory->pop_back();
      s_pHistory->push_front( m_list.front() );
   }
}

void KDiff3Plugin::slotClearList()
{
   if ( s_pHistory )
      s_pHistory->clear();
}

void KDiff3Plugin::slotAbout()
{
   QString s = i18n("KDiff3 Menu Plugin: Copyright (C) 2008 Joachim Eibl\n"
                    "KDiff3 homepage: http://kdiff3.sourceforge.net\n\n");
   s += i18n("Using the context menu extension:\n"
             "For simple comparison of two selected files choose \"Compare\".\n"
             "If the other file is somewhere else, \"Save\" the first file for later, "
             "and it will appear in the \"Compare With ...\" submenu. "
             "Then, use \"Compare With\" on the second file.\n"
             "For a 3-way merge first, \"Save\" the base file, then the branch to merge, and "
             "then \"3-way merge with base\" on the other branch which will be used as the destination.\n"
             "The same also applies to directory comparison and merge.");
   KMessageBox::information(m_pParentWidget, s, i18n("About KDiff3 Menu Plugin") );
}

