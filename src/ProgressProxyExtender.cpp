/**
 * Copyright (C) 2003-2007 by Joachim Eibl
 *   joachim.eibl at gmx.de
 * Copyright (C) 2018 Michael Reeves
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
 * 
 */

#include "ProgressProxyExtender.h"

#include <QString>

void ProgressProxyExtender::slotListDirInfoMessage(KJob*, const QString& msg)
{
    setInformation(msg, 0);
}

void ProgressProxyExtender::slotPercent(KJob*, qint64 percent)
{
    setCurrent(percent);
}


