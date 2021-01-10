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

#include "fileaccess.h"
#include "Logging.h"
#include "Utils.h"

#include <QDropEvent>
#include <QDragEnterEvent>
#include <QMimeData>
#include <QUrl>

void FileNameLineEdit::dropEvent(QDropEvent* event)
{
    Q_ASSERT(event->mimeData()->hasUrls());//Debugging aid in case FileNameLineEdit::dragEnterEvent is changed to accept other types.

    qCDebug(kdeMain) << "Enter FileNameLineEdit::dropEvent";
    QList<QUrl> lst = event->mimeData()->urls();

    if(lst.count() > 0)
    {
        /*
            Do not use QUrl::toString() here. Sadly the Qt5 version does not permit Qt4 style
            fullydecoded conversions. It also treats emtpy schemes as non-local.
        */
        qCDebug(kdeMain) << "Recieved Drop Event";
        qCDebug(kdeMain) << "Url List Size: " << lst.count();
        qCDebug(kdeMain) << "lst[0] = " << lst[0];

        if(FileAccess::isLocal(lst[0]))
        {
            qCInfo(kdeMain) << "Interpurting lst[0] as a local file.";
            if(lst[0].scheme().isEmpty())
                lst[0].setScheme("file");
            //QUrl::toLocalFile returns empty for empty schemes or anything else Qt thinks is not local
            setText(Utils::urlToString(lst[0]));
        }
        else
        {
            qCInfo(kdeMain) << "Interpurting lst[0] as a remote url.";
            setText(lst[0].toDisplayString());
        }
        qCDebug(kdeMain) << "Set line edit text to: " << text() ;
        setFocus();
        emit returnPressed();
    }
    qCDebug(kdeMain) << "Leave FileNameLineEdit::dropEvent";
}

void FileNameLineEdit::dragEnterEvent(QDragEnterEvent* e)
{
    e->setAccepted(e->mimeData()->hasUrls());
}
