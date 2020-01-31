/*
 * KDiff3 - Text Diff And Merge Tool
 * 
 * SPDX-FileCopyrightText: 2002-2007  Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QPixmap>
#include <KIconLoader>
#include <QPainter>

#include "MergeFileInfos.h"

namespace PixMapUtils
{
namespace{
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
}
QPixmap colorToPixmap(const QColor &inColor)
{
    QPixmap pm(16, 16);
    QPainter p(&pm);
    p.setPen(Qt::black);
    p.setBrush(inColor);
    p.drawRect(0, 0, pm.width(), pm.height());
    return pm;
}
// Copy pm2 onto pm1, but preserve the alpha value from pm1 where pm2 is transparent.
QPixmap pixCombiner(const QPixmap* pm1, const QPixmap* pm2)
{
    QImage img1 = pm1->toImage().convertToFormat(QImage::Format_ARGB32);
    QImage img2 = pm2->toImage().convertToFormat(QImage::Format_ARGB32);

    for(int y = 0; y < img1.height(); y++)
    {
        quint32* line1 = reinterpret_cast<quint32*>(img1.scanLine(y));
        quint32* line2 = reinterpret_cast<quint32*>(img2.scanLine(y));
        for(int x = 0; x < img1.width(); x++)
        {
            if(qAlpha(line2[x]) > 0)
                line1[x] = (line2[x] | 0xff000000);
        }
    }
    return QPixmap::fromImage(img1);
}

// like pixCombiner but let the pm1 color shine through
QPixmap pixCombiner2(const QPixmap* pm1, const QPixmap* pm2)
{
    QPixmap pix = *pm1;
    QPainter p(&pix);
    p.setOpacity(0.5);
    p.drawPixmap(0, 0, *pm2);
    p.end();

    return pix;
}

void initPixmaps(const QColor& newest, const QColor& oldest, const QColor& middle, const QColor& notThere)
{
    if(s_pm_dir == nullptr || s_pm_file == nullptr)
    {
#include "xpm/file.xpm"
#include "xpm/folder.xpm"
        s_pm_dir = new QPixmap(KIconLoader::global()->loadIcon("folder", KIconLoader::Small));
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

    *pmNewDir = pixCombiner2(pmNew, s_pm_dir);
    *pmMiddleDir = pixCombiner2(pmMiddle, s_pm_dir);
    *pmOldDir = pixCombiner2(pmOld, s_pm_dir);

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
