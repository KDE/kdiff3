/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
// clang-format on

#ifndef KDIFF3FILEITEMACTIONPLUGIN_H
#define KDIFF3FILEITEMACTIONPLUGIN_H

#include <KAbstractFileItemActionPlugin>
#include <KFileItemListProperties>

class KDiff3FileItemAction: public KAbstractFileItemActionPlugin
{
    Q_OBJECT
  public:
    KDiff3FileItemAction(QObject* pParent, const QVariantList& args);
    ~KDiff3FileItemAction() override;
    // implement pure virtual method from KonqPopupMenuPlugin
    QList<QAction*> actions(const KFileItemListProperties& fileItemInfos, QWidget* pParentWidget) override;

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
    QWidget* m_pParentWidget = nullptr;
    //KFileItemListProperties m_fileItemInfos;
};
#endif
