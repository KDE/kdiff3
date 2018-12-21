/**
 * Copyright (C) 2003-2007 by Joachim Eibl <joachim.eibl at gmx.de>
 * Copyright (C) 2018 Michael Reeves <reeves.87@gmail.com>
 *
 * This file is part of KDiff3.
 *
 * KDiff3 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * KDiff3 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with KDiff3.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef PIXMAPUTILSH
#define PIXMAPUTILSH

#include "MergeFileInfos.h"

class QPixmap;
class QColor;

namespace PixMapUtils
{
QPixmap colorToPixmap(const QColor &inColor);
// Copy pm2 onto pm1, but preserve the alpha value from pm1 where pm2 is transparent.
QPixmap pixCombiner(const QPixmap* pm1, const QPixmap* pm2);

// like pixCombiner but let the pm1 color shine through
QPixmap pixCombiner2(const QPixmap* pm1, const QPixmap* pm2);
void initPixmaps(QColor newest, QColor oldest, QColor middle, QColor notThere);

QPixmap getOnePixmap(e_Age eAge, bool bLink, bool bDir);

} // namespace PixMapUtils

#endif
