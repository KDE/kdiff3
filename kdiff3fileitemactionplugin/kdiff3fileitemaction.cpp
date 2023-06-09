/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
// clang-format on

#include "kdiff3fileitemaction.h"

#include "../src/Utils.h"
#include "../src/TypeUtils.h"

#include <memory>

#include <QAction>
#include <QMenu>
#include <QProcess>
#include <QUrl>

#include <KConfig>
#include <KConfigGroup>
#include <KIOCore/KFileItem>
#include <KLocalizedString>
#include <KMessageBox>
#include <KPluginFactory>
#include <KPluginLoader>

std::unique_ptr<QStringList> s_pHistory;

class KDiff3PluginHistory
{
    std::unique_ptr<KConfig> m_pConfig;
    std::unique_ptr<KConfigGroup> m_pConfigGroup;

  public:
    KDiff3PluginHistory()
    {
        m_pConfig = nullptr;
        if(s_pHistory == nullptr)
        {
            //std::cout << "New History: " << instanceName << std::endl;
            s_pHistory = std::make_unique<QStringList>();
            m_pConfig = std::make_unique<KConfig>("kdiff3fileitemactionrc", KConfig::SimpleConfig);
            m_pConfigGroup = std::make_unique<KConfigGroup>(m_pConfig.get(), "KDiff3Plugin");
            *s_pHistory = m_pConfigGroup->readEntry("HistoryStack", QStringList());
        }
    }

    ~KDiff3PluginHistory()
    {
        //std::cout << "Delete History" << std::endl;
        if(s_pHistory && m_pConfigGroup)
            m_pConfigGroup->writeEntry("HistoryStack", *s_pHistory);

        s_pHistory = nullptr;
    }
};

KDiff3PluginHistory s_history;

K_PLUGIN_FACTORY_WITH_JSON(KDiff3FileItemActionFactory, "kdiff3fileitemaction.json", registerPlugin<KDiff3FileItemAction>();)
#include "kdiff3fileitemaction.moc"

KDiff3FileItemAction::KDiff3FileItemAction(QObject* pParent, const QVariantList& /*args*/):
    KAbstractFileItemActionPlugin(pParent)
{
}

QList<QAction*> KDiff3FileItemAction::actions(const KFileItemListProperties& fileItemInfos, QWidget* pParentWidget)
{
    QList<QAction*> actions;

    if(QStandardPaths::findExecutable("kdiff3").isEmpty())
        return actions;

    //m_fileItemInfos = fileItemInfos;
    m_pParentWidget = pParentWidget;

    QAction* pMenuAction = new QAction(QIcon::fromTheme(QStringLiteral("kdiff3")), i18nc("Contexualmenu title", "KDiff3..."), this);
    QMenu* pActionMenu = new QMenu();
    pMenuAction->setMenu(pActionMenu);

    // remember currently selected files
    m_list = fileItemInfos.urlList();

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

    QAction* pAction = nullptr;
    QString actionText;

    if(m_list.count() == 1)
    {
        QtSizeType historyCount = s_pHistory ? s_pHistory->count() : 0;

        actionText = i18nc("Contexualmenu option", "Compare with %1", (historyCount > 0 ? s_pHistory->first() : QString()));
        pAction = new QAction(actionText, this);
        connect(pAction, &QAction::triggered, this, &KDiff3FileItemAction::slotCompareWith);
        pAction->setEnabled(m_list.count() > 0 && historyCount > 0);
        pActionMenu->addAction(pAction);

        actionText = i18nc("Contexualmenu option", "Merge with %1", historyCount > 0 ? s_pHistory->first() : QString());
        pAction = new QAction(actionText, this);
        connect(pAction, &QAction::triggered, this, &KDiff3FileItemAction::slotMergeWith);
        pAction->setEnabled(m_list.count() > 0 && historyCount > 0);
        pActionMenu->addAction(pAction);

        actionText = i18nc("Contexualmenu option", "Save '%1' for later", m_list.first().fileName());
        pAction = new QAction(actionText, this);
        connect(pAction, &QAction::triggered, this, &KDiff3FileItemAction::slotSaveForLater);
        pAction->setEnabled(m_list.count() > 0);
        pActionMenu->addAction(pAction);

        pAction = new QAction(i18nc("Contexualmenu option", "3-way merge with base"), this);
        connect(pAction, &QAction::triggered, this, &KDiff3FileItemAction::slotMergeThreeWay);
        pAction->setEnabled(m_list.count() > 0 && historyCount >= 2);
        pActionMenu->addAction(pAction);

        if(s_pHistory && !s_pHistory->empty())
        {
            QAction* pHistoryMenuAction = new QAction(i18nc("Contexualmenu option", "Compare with..."), this);
            QMenu* pHistoryMenu = new QMenu();
            pHistoryMenuAction->setMenu(pHistoryMenu);
            pHistoryMenu->setEnabled(m_list.count() > 0 && historyCount > 0);
            pActionMenu->addAction(pHistoryMenuAction);
            for(const QString& file: qAsConst(*s_pHistory))
            {
                pAction = new QAction(file, this);
                pAction->setData(file);
                connect(pAction, &QAction::triggered, this, &KDiff3FileItemAction::slotCompareWithHistoryItem);
                pHistoryMenu->addAction(pAction);
            }

            pAction = new QAction(i18nc("Contexualmenu option to cleat comparison list", "Clear list"), this);
            connect(pAction, &QAction::triggered, this, &KDiff3FileItemAction::slotClearList);
            pActionMenu->addAction(pAction);
            pAction->setEnabled(historyCount > 0);
        }
    }
    else if(m_list.count() == 2)
    {
        pAction = new QAction(i18nc("Contexualmenu option", "Compare"), this);
        connect(pAction, &QAction::triggered, this, &KDiff3FileItemAction::slotCompareTwoFiles);
        pActionMenu->addAction(pAction);
    }
    else if(m_list.count() == 3)
    {
        pAction = new QAction(i18nc("Contexualmenu option", "3 way comparison"), this);
        connect(pAction, &QAction::triggered, this, &KDiff3FileItemAction::slotCompareThreeFiles);
        pActionMenu->addAction(pAction);
    }
    pAction = new QAction(i18nc("Contexualmenu option", "About KDiff3 menu plugin..."), this);
    connect(pAction, &QAction::triggered, this, &KDiff3FileItemAction::slotAbout);
    pActionMenu->addAction(pAction);

    //pMenu->addSeparator();
    //pMenu->addAction( pActionMenu );
    //pMenu->addSeparator();
    actions << pMenuAction;
    return actions;
}

KDiff3FileItemAction::~KDiff3FileItemAction() = default;

void KDiff3FileItemAction::slotCompareWith()
{
    if(m_list.count() > 0 && s_pHistory && !s_pHistory->empty())
    {
        QStringList args = {
            s_pHistory->first(),
            Utils::urlToString(m_list.first())
        };
        QProcess::startDetached("kdiff3", args);
    }
}

void KDiff3FileItemAction::slotCompareWithHistoryItem()
{
    const QAction* pAction = dynamic_cast<const QAction*>(sender());
    if(!m_list.isEmpty() && pAction)
    {
        QStringList args = {
            pAction->data().toString(),
            Utils::urlToString(m_list.first())
        };
        QProcess::startDetached("kdiff3", args);
    }
}

void KDiff3FileItemAction::slotCompareTwoFiles()
{
    if(m_list.count() == 2)
    {
        QStringList args = {
            Utils::urlToString(m_list.first()),
            Utils::urlToString(m_list.last())
        };
        QProcess::startDetached("kdiff3", args);
    }
}

void KDiff3FileItemAction::slotCompareThreeFiles()
{
    if(m_list.count() == 3)
    {
        QStringList args = {
            Utils::urlToString(m_list.at(0)),
            Utils::urlToString(m_list.at(1)),
            Utils::urlToString(m_list.at(2))
        };
        QProcess::startDetached("kdiff3", args);
    }
}

void KDiff3FileItemAction::slotMergeWith()
{
    if(m_list.count() > 0 && s_pHistory && !s_pHistory->empty())
    {
        QStringList args = {
            s_pHistory->first(),
            Utils::urlToString(m_list.first()),
            ("-o" + Utils::urlToString(m_list.first()))
        };
        QProcess::startDetached("kdiff3", args);
    }
}

void KDiff3FileItemAction::slotMergeThreeWay()
{
    if(m_list.count() > 0 && s_pHistory && s_pHistory->count() >= 2)
    {
        QStringList args = {
            s_pHistory->at(1),
            s_pHistory->at(0),
            Utils::urlToString(m_list.first()),
            ("-o" + Utils::urlToString(m_list.first()))
        };
        QProcess::startDetached("kdiff3", args);
    }
}

void KDiff3FileItemAction::slotSaveForLater()
{
    if(!m_list.isEmpty() && s_pHistory)
    {
        while(s_pHistory->count() >= 10)
        {
            s_pHistory->removeLast();
        }
        const QString file = Utils::urlToString(m_list.first());
        s_pHistory->removeAll(file);
        s_pHistory->prepend(file);
    }
}

void KDiff3FileItemAction::slotClearList()
{
    if(s_pHistory)
    {
        s_pHistory->clear();
    }
}

void KDiff3FileItemAction::slotAbout()
{
    QString s = i18n("KDiff3 File Item Action Plugin: Copyright (C) 2011 Joachim Eibl\n");
    s += i18n("Using the context menu extension:\n"
              "For simple comparison of two selected files choose \"Compare\".\n"
              "If the other file is somewhere else \"Save\" the first file for later. "
              "It will appear in the \"Compare with...\" submenu. "
              "Then use \"Compare With\" on the second file.\n"
              "For a 3-way merge first \"Save\" the base file, then the branch to merge and "
              "choose \"3-way merge with base\" on the other branch which will be used as destination.\n"
              "Same also applies to folder comparison and merge.");
    KMessageBox::information(m_pParentWidget, s, i18n("About KDiff3 File Item Action Plugin"));
}
