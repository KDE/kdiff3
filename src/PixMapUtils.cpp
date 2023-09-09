/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "MergeFileInfos.h"

#include <QApplication>
#include <QIcon>
#include <QPainter>
#include <QPixmap>
#include <QStyle>

namespace PixMapUtils {
namespace {
QPixmap* s_pm_dir = nullptr;
QPixmap* s_pm_file = nullptr;

QPixmap* pmNotThere;
QPixmap* pmNew = nullptr;
QPixmap* pmOld;
QPixmap* pmMiddle;

QPixmap* pmLink;

QPixmap* pmDirLink;
QPixmap* pmFileLink;

QPixmap* pmNewLink;
QPixmap* pmOldLink;
QPixmap* pmMiddleLink;

QPixmap* pmNewDir;
QPixmap* pmMiddleDir;
QPixmap* pmOldDir;

QPixmap* pmNewDirLink;
QPixmap* pmMiddleDirLink;
QPixmap* pmOldDirLink;
}// namespace

QPixmap colorToPixmap(const QColor& inColor)
{
    QPixmap pm(16, 16);
    QPainter p(&pm);
    p.setPen(Qt::black);
    p.setBrush(inColor);
    p.drawRect(0, 0, pm.width(), pm.height());
    return pm;
}

/*
    Copy pm2 onto pm1, but preserve the alpha value from pm1 where pm2 is transparent.
    Opactiy controls wheather or not pm1 will show through.
*/
QPixmap pixCombiner(const QPixmap* pm1, const QPixmap* pm2, const qreal inOpacity = 1)
{
    QImage img1 = pm1->toImage().convertToFormat(QImage::Format_ARGB32);
    QImage img2 = pm2->toImage().convertToFormat(QImage::Format_ARGB32);
    QPainter painter(&img1);

    painter.setOpacity(inOpacity);
    painter.drawImage(0, 0, img2);
    painter.end();

    return QPixmap::fromImage(img1);
}

void initPixmaps(const QColor& newest, const QColor& oldest, const QColor& middle, const QColor& notThere)
{
    if(s_pm_dir == nullptr || s_pm_file == nullptr)
    {
#include "xpm/file.xpm"
#include "xpm/folder.xpm"
        const qint32 smallIcon = qApp->style()->pixelMetric(QStyle::PM_SmallIconSize);
        s_pm_dir = new QPixmap(QIcon::fromTheme(QStringLiteral("folder")).pixmap(smallIcon));
        if(s_pm_dir->size() != QSize(16, 16))
        {
            delete s_pm_dir;
            s_pm_dir = new QPixmap(folder_pm);
        }
        s_pm_file = new QPixmap(file_pm);
    }

    if(pmNew == nullptr)
    {
#include "xpm/link_arrow.xpm"

        pmNotThere = new QPixmap;
        pmNew = new QPixmap;
        pmOld = new QPixmap;
        pmMiddle = new QPixmap;

        pmLink = new QPixmap(link_arrow);

        pmDirLink = new QPixmap;
        pmFileLink = new QPixmap;

        pmNewLink = new QPixmap;
        pmOldLink = new QPixmap;
        pmMiddleLink = new QPixmap;

        pmNewDir = new QPixmap;
        pmMiddleDir = new QPixmap;
        pmOldDir = new QPixmap;

        pmNewDirLink = new QPixmap;
        pmMiddleDirLink = new QPixmap;
        pmOldDirLink = new QPixmap;
    }

    *pmNotThere = colorToPixmap(notThere);
    *pmNew = colorToPixmap(newest);
    *pmOld = colorToPixmap(oldest);
    *pmMiddle = colorToPixmap(middle);

    *pmDirLink = pixCombiner(s_pm_dir, pmLink);
    *pmFileLink = pixCombiner(s_pm_file, pmLink);

    *pmNewLink = pixCombiner(pmNew, pmLink);
    *pmOldLink = pixCombiner(pmOld, pmLink);
    *pmMiddleLink = pixCombiner(pmMiddle, pmLink);

    *pmNewDir = pixCombiner(pmNew, s_pm_dir, 0.5);
    *pmMiddleDir = pixCombiner(pmMiddle, s_pm_dir, 0.5);
    *pmOldDir = pixCombiner(pmOld, s_pm_dir, 0.5);

    *pmNewDirLink = pixCombiner(pmNewDir, pmLink);
    *pmMiddleDirLink = pixCombiner(pmMiddleDir, pmLink);
    *pmOldDirLink = pixCombiner(pmOldDir, pmLink);
}

QPixmap getOnePixmap(e_Age eAge, bool bLink, bool bDir)
{
    QPixmap* ageToPm[] = {pmNew, pmMiddle, pmOld, pmNotThere, s_pm_file};
    QPixmap* ageToPmLink[] = {pmNewLink, pmMiddleLink, pmOldLink, pmNotThere, pmFileLink};
    QPixmap* ageToPmDir[] = {pmNewDir, pmMiddleDir, pmOldDir, pmNotThere, s_pm_dir};
    QPixmap* ageToPmDirLink[] = {pmNewDirLink, pmMiddleDirLink, pmOldDirLink, pmNotThere, pmDirLink};

    QPixmap** ppPm = bDir ? (bLink ? ageToPmDirLink : ageToPmDir) : (bLink ? ageToPmLink : ageToPm);

    return *ppPm[eAge];
}

} // namespace PixMapUtils
