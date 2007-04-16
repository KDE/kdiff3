/* This file is part of the KDiff3 project

   Copyright (C) 2006 Joachim Eibl <joachim dot eibl at gmx dot de>

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
#include <klocale.h>
#include <kgenericfactory.h>
#include <kurl.h>
#include <ksimpleconfig.h>
#include <kmessagebox.h>

//#include <iostream>

static QStringList* s_pHistory=0;

class KDiff3PluginFactory : public KGenericFactory < KDiff3Plugin, KonqPopupMenu >
{
   KSimpleConfig* m_pConfig;
public:
   KDiff3PluginFactory( const char* instanceName  = 0 )
   : KGenericFactory< KDiff3Plugin, KonqPopupMenu >( instanceName )
   {
      m_pConfig = 0;
      if (s_pHistory==0)
      {
         //std::cout << "New History: " << instanceName << std::endl;
         s_pHistory = new QStringList;
         m_pConfig = new KSimpleConfig( "kdiff3pluginrc", false );
         *s_pHistory = m_pConfig->readListEntry("HistoryStack");
      }
   }

   ~KDiff3PluginFactory()
   {
      //std::cout << "Delete History" << std::endl;
      if ( s_pHistory && m_pConfig )
         m_pConfig->writeEntry("HistoryStack",*s_pHistory);
      delete s_pHistory;
      delete m_pConfig;
      s_pHistory = 0;
      m_pConfig = 0;
   }
};

K_EXPORT_COMPONENT_FACTORY (libkdiff3plugin, KDiff3PluginFactory ("kdiff3plugin"))

KDiff3Plugin::KDiff3Plugin( KonqPopupMenu* pPopupmenu, const char *name, const QStringList & /* list */ )
:KonqPopupMenuPlugin (pPopupmenu, name)
{
   if (KStandardDirs::findExe ("kdiff3").isNull ())
      return;

   m_pParentWidget = pPopupmenu->parentWidget();

   KGlobal::locale()->insertCatalogue("kdiff3_plugin");

   // remember currently selected files (copy to a QStringList)
   KFileItemList itemList = pPopupmenu->fileItemList();
   for ( KFileItem *item = itemList.first(); item; item = itemList.next() )
   {
      //m_urlList.append( item->url() );
      m_list.append( item->url().url() );
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

   KActionMenu* pActionMenu = new KActionMenu (i18n ("KDiff3"), "kdiff3", actionCollection (), "kdiff3_menu" );
   KAction* pAction = 0;
   QString s;

   if(m_list.count() == 1)
   {
      int historyCount = s_pHistory ? s_pHistory->count() : 0;
      s = i18n("Compare with %1").arg( historyCount>0 ? s_pHistory->front() : QString() );
      pAction = new KAction ( s,0, this, SLOT(slotCompareWith()), actionCollection());
      pAction->setEnabled( m_list.count()>0 && historyCount>0 );
      pActionMenu->insert (pAction);

      s = i18n("Merge with %1").arg( historyCount>0 ? s_pHistory->front() : QString() );
      pAction = new KAction( s, 0, this, SLOT(slotMergeWith()), actionCollection());
      pAction->setEnabled( m_list.count()>0 && historyCount>0 );
      pActionMenu->insert (pAction);

      s = i18n("Save '%1' for later").arg( ( m_list.front() ) );
      pAction = new KAction ( s, 0, this, SLOT(slotSaveForLater()), actionCollection());
      pAction->setEnabled( m_list.count()>0 );
      pActionMenu->insert(pAction);

      pAction = new KAction (i18n("3-way merge with base"), 0, this, SLOT(slotMergeThreeWay()), actionCollection());
      pAction->setEnabled( m_list.count()>0 && historyCount>=2 );
      pActionMenu->insert (pAction);

      if ( s_pHistory && !s_pHistory->empty() )
      {
         KActionMenu* pHistoryMenu = new KActionMenu( i18n("Compare with ..."), "CompareWith", actionCollection (), "kdiff3_history_menu");
         pHistoryMenu->setEnabled( m_list.count()>0 && historyCount>0 );
         pActionMenu->insert(pHistoryMenu);
         for (QStringList::iterator i = s_pHistory->begin(); i!=s_pHistory->end(); ++i)
         {
            pAction = new KAction( *i, "History", 0, this, SLOT(slotCompareWithHistoryItem()), actionCollection());
            pHistoryMenu->insert (pAction);
         }

         pAction = new KAction (i18n("Clear list"), 0, this, SLOT(slotClearList()), actionCollection());
         pActionMenu->insert (pAction);
         pAction->setEnabled( historyCount>0 );
      }
   }
   else if(m_list.count() == 2)
   {
      pAction = new KAction (i18n("Compare"), 0, this, SLOT(slotCompareTwoFiles()), actionCollection());
      pActionMenu->insert (pAction);
   }
   else if ( m_list.count() == 3 )
   {
      pAction = new KAction (i18n("3 way comparison"), 0, this, SLOT(slotCompareThreeFiles()), actionCollection());
      pActionMenu->insert (pAction);
   }
   pAction = new KAction (i18n("About KDiff3 menu plugin ..."), 0, this, SLOT(slotAbout()), actionCollection());
   pActionMenu->insert (pAction);

   addSeparator();
   addAction( pActionMenu );
   addSeparator();
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
      kapp->kdeinitExec ("kdiff3", args);
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
      kapp->kdeinitExec ("kdiff3", args);
   }
}

void KDiff3Plugin::slotCompareTwoFiles()
{
   if ( m_list.count() == 2 )
   {
      QStringList args;
      args << m_list.front();
      args << m_list.back();
      kapp->kdeinitExec ("kdiff3", args);
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
      kapp->kdeinitExec ("kdiff3", args);
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
      kapp->kdeinitExec ("kdiff3", args);
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
      kapp->kdeinitExec ("kdiff3", args);
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
   QString s = i18n("KDiff3 Menu Plugin: Copyright (C) 2006 Joachim Eibl\n"
                    "KDiff3 homepage: http://kdiff3.sourceforge.net\n\n");
   s += i18n("Using the contextmenu extension:\n"
             "For simple comparison of two selected 2 files choose \"Compare\".\n"
             "If the other file is somewhere else \"Save\" the first file for later. "
             "It will appear in the \"Compare With ...\" submenu. "
             "Then use \"Compare With\" on second file.\n"
             "For a 3-way merge first \"Save\" the base file, then the branch to merge and "
             "choose \"3-way merge with base\" on the other branch which will be used as destination.\n"
             "Same also applies to directory comparison and merge.");
   KMessageBox::information(m_pParentWidget, s, tr("About KDiff3 Menu Plugin") );
}

#include "kdiff3plugin.moc"
