/***************************************************************************
 * Copyright (C) 2003-2007 Joachim Eibl <joachim.eibl at gmx.de>           *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Steet, Fifth Floor, Boston, MA 02110-1301, USA.           *
 ***************************************************************************/

#ifndef _KDIFF3SHELL_H_
#define _KDIFF3SHELL_H_

#include <config-kdiff3.h>

#include <kapplication.h>
#include <kparts/mainwindow.h>

class KToggleAction;

/**
 * This is the application "Shell".  It has a menubar, toolbar, and
 * statusbar but relies on the "Part" to do all the real work.
 *
 * @short Application Shell
 * @author Joachim Eibl <joachim.eibl at gmx.de>
 */
class KDiff3Shell : public KParts::MainWindow
{
    Q_OBJECT
public:
    /**
     * Default Constructor
     */
    KDiff3Shell(bool bCompleteInit=true);

    /**
     * Default Destructor
     */
    virtual ~KDiff3Shell();

    bool queryClose();
    bool queryExit();
    virtual void closeEvent(QCloseEvent*e);

private slots:
    void optionsShowToolbar();
    void optionsShowStatusbar();
    void optionsConfigureKeys();
    void optionsConfigureToolbars();

    void applyNewToolbarConfig();
    void slotNewInstance( const QString& fn1, const QString& fn2, const QString& fn3 );

private:
    KParts::ReadWritePart *m_part;

    KToggleAction *m_toolbarAction;
    KToggleAction *m_statusbarAction;
    bool m_bUnderConstruction;
};

#endif // _KDIFF3_H_
