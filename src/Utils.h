/**
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

#ifndef UTILS_H
#define UTILS_H

#include <QStringList>
#include <QString>

class Utils{
    public:
       static QString getArguments(QString cmd, QString& program, QStringList& args);
};

#endif