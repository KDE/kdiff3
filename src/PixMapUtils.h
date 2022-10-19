/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
// clang-format on
#ifndef PIXMAPUTILSH
#define PIXMAPUTILSH

#include "MergeFileInfos.h"

class QPixmap;
class QColor;

namespace PixMapUtils
{
QPixmap colorToPixmap(const QColor &inColor);
/*
    Copy pm2 onto pm1, but preserve the alpha value from pm1 where pm2 is transparent.
    Opactiy controls wheather or not pm1 will show through.
*/
QPixmap pixCombiner(const QPixmap* pm1, const QPixmap* pm2, const qreal inOpacity = 1);

void initPixmaps(const QColor& newest, const QColor& oldest, const QColor& middle, const QColor& notThere);

QPixmap getOnePixmap(e_Age eAge, bool bLink, bool bDir);

} // namespace PixMapUtils

#endif
