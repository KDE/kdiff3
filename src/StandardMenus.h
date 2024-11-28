/**
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2024 Michael Reeves <reeves.87@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 */

#ifndef STANDARDMENUS_H
#define STANDARDMENUS_H

#include "combiners.h"

#include <boost/signals2.hpp>

#include <QAction>
#include <QPointer>

class KDiff3App;
class KActionCollection;

class StandardMenus
{
  public:
    inline static boost::signals2::signal<bool(), or_> allowSave;
    inline static boost::signals2::signal<bool(), or_> allowSaveAs;
    inline static boost::signals2::signal<bool(), or_> allowCopy;
    inline static boost::signals2::signal<bool(), or_> allowCut;

    void setup(KDiff3App* app, KActionCollection* ac);
    void updateAvailabilities();

    void slotClipboardChanged();

  private:
    // QAction pointers to enable/disable standard actions
    QPointer<QAction> fileOpen;
    QPointer<QAction> fileSave;
    QPointer<QAction> fileSaveAs;
    QPointer<QAction> filePrint;
    QPointer<QAction> fileQuit;
    QPointer<QAction> fileReload;
    QPointer<QAction> editUndo;
    QPointer<QAction> editCut;
    QPointer<QAction> editCopy;
    QPointer<QAction> editPaste;
    QPointer<QAction> editSelectAll;
};

#endif /* STANDARDMENUS_H */
