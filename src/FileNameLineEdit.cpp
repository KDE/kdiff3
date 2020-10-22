/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2019-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "FileNameLineEdit.h"
#include "fileaccess.h"

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
        /*
            Do not use QUrl::toString() here. Sadly the Qt5 version does not permit Qt4 style
            fullydecoded conversions. It also treats empty schemes as non-local.
        */
        setText(FileAccess::prettyAbsPath(lst[0]));
        setFocus();
        Q_EMIT returnPressed();
    }
}

void FileNameLineEdit::dragEnterEvent(QDragEnterEvent* e)
{
    e->setAccepted(e->mimeData()->hasUrls());
}
