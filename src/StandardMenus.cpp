#include "StandardMenus.h"

#include "compat.h"
#include "guiutils.h"
#include "kdiff3.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QMimeData>

#include <KActionCollection>
#include <KStandardAction>

void StandardMenus::setup(KDiff3App* app, KActionCollection* ac)
{
    fileOpen = KStandardAction::open(app, &KDiff3App::slotFileOpen, ac);
    fileOpen->setStatusTip(i18n("Opens documents for comparison..."));

    fileReload = GuiUtils::createAction<QAction>(i18n("Reload"), QKeySequence(QKeySequence::Refresh), app, &KDiff3App::slotReload, ac, u8"file_reload");

    fileSave = KStandardAction::save(app, &KDiff3App::slotFileSave, ac);
    fileSave->setStatusTip(i18n("Saves the merge result. All conflicts must be solved!"));
    fileSaveAs = KStandardAction::saveAs(app, &KDiff3App::slotFileSaveAs, ac);
    fileSaveAs->setStatusTip(i18n("Saves the current document as..."));
#ifndef QT_NO_PRINTER
    filePrint = KStandardAction::print(app, &KDiff3App::slotFilePrint, ac);
    filePrint->setStatusTip(i18n("Print the differences"));
#endif
    fileQuit = KStandardAction::quit(app, &KDiff3App::slotFileQuit, ac);
    fileQuit->setStatusTip(i18n("Quits the application"));

    editUndo = KStandardAction::undo(app, &KDiff3App::slotEditUndo, ac);
    editUndo->setShortcuts(QKeySequence::Undo);
    editUndo->setStatusTip(i18n("Undo last action."));

    editCut = KStandardAction::cut(app, &KDiff3App::slotEditCut, ac);
    editCut->setShortcuts(QKeySequence::Cut);
    editCut->setStatusTip(i18n("Cuts the selected section and puts it to the clipboard"));
    editCopy = KStandardAction::copy(app, &KDiff3App::slotEditCopy, ac);
    editCopy->setShortcut(QKeySequence::Copy);
    editCopy->setStatusTip(i18n("Copies the selected section to the clipboard"));
    editPaste = KStandardAction::paste(app, &KDiff3App::slotEditPaste, ac);
    editPaste->setStatusTip(i18n("Pastes the clipboard contents to current position"));
    editPaste->setShortcut(QKeySequence::Paste);
    editSelectAll = KStandardAction::selectAll(app, &KDiff3App::slotEditSelectAll, ac);
    editSelectAll->setStatusTip(i18n("Select everything in current window"));
}

void StandardMenus::slotClipboardChanged()
{
    const QClipboard* clipboard = QApplication::clipboard();
    const QMimeData* mimeData = clipboard->mimeData();
    if(mimeData && mimeData->hasText())
    {
        QString s = clipboard->text();
        editPaste->setEnabled(!s.isEmpty());
    }
    else
    {
        editPaste->setEnabled(false);
    }
}

void StandardMenus::updateAvailabilities()
{
    fileSave->setEnabled(KDiff3App::allowSave());
    fileSaveAs->setEnabled(KDiff3App::allowSaveAs());

    editUndo->setEnabled(false); //Not yet implemented but planned.
    editCut->setEnabled(KDiff3App::allowCut());
    editCopy->setEnabled(KDiff3App::allowCopy());
}
