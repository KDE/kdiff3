/**
 * Copyright (C) 2019 Michael Reeves <reeves.87@gmail.com>
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
#include "FileNameLineEdit.h"

#include <QDropEvent>
#include <QDragEnterEvent>
#include <QMimeData>
#include <QUrl>

void FileNameLineEdit::dropEvent(QDropEvent* event)
{
    Q_ASSERT(event->mimeData()->hasUrls());//Debugging aid in case FileNameLineEdit::dragEnterEvent is changed to accept other types.

    QList<QUrl> lst = event->mimeData()->urls();

    if(lst.count() > 0)
    {
        setText(lst[0].toString());
        setFocus();
        Q_EMIT returnPressed();
    }
}

void FileNameLineEdit::dragEnterEvent(QDragEnterEvent* e)
{
    e->setAccepted(e->mimeData()->hasUrls());
}
