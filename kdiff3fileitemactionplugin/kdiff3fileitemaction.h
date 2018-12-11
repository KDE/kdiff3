/* This file is part of the KDiff3 project

   Copyright (C) 2008 Joachim Eibl <Joachim dot Eibl at gmx dot de>

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
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#ifndef KDIFF3FILEITEMACTIONPLUGIN_H
#define KDIFF3FILEITEMACTIONPLUGIN_H

#include <KAbstractFileItemActionPlugin>
#include <KFileItemListProperties>
#include <QStringList>

class QStringList;

class KDiff3FileItemAction : public KAbstractFileItemActionPlugin
{
   Q_OBJECT
public:
  KDiff3FileItemAction (QObject* pParent, const QVariantList & args);
  ~KDiff3FileItemAction() override;
  // implement pure virtual method from KonqPopupMenuPlugin
  QList<QAction*> actions( const KFileItemListProperties& fileItemInfos, QWidget* pParentWidget ) override;

private Q_SLOTS:
   void slotCompareWith();
   void slotCompareTwoFiles();
   void slotCompareThreeFiles();
   void slotMergeWith();
   void slotMergeThreeWay();
   void slotSaveForLater();
   void slotClearList();
   void slotCompareWithHistoryItem();
   void slotAbout();

private:
   QList<QUrl> m_list;
   QWidget* m_pParentWidget;
   //KFileItemListProperties m_fileItemInfos;
};
#endif
