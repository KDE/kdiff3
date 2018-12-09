/***************************************************************************
 *   Copyright (C) 2003-2007 by Joachim Eibl <joachim.eibl at gmx.de>      *
 *   Copyright (C) 2018 Michael Reeves reeves.87@gmail.com                 *
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
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.          *
 ***************************************************************************/

#ifndef KDIFF3SHELL_H
#define KDIFF3SHELL_H

#include <KParts/MainWindow>
#include <QCommandLineParser>

class KToggleAction;

namespace KParts {
    class ReadWritePart;
}

class KDiff3App;

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
    explicit KDiff3Shell(bool bCompleteInit=true);

    /**
     * Default Destructor
     */
    ~KDiff3Shell() override;

    bool queryClose() override;
    bool queryExit();
    void closeEvent(QCloseEvent*e) override;
    
    static inline QCommandLineParser* getParser(){
      static QCommandLineParser *parser = new QCommandLineParser();
      return parser;
    };
private Q_SLOTS:
    void optionsShowToolbar();
    void optionsShowStatusbar();
    void optionsConfigureKeys();
    void optionsConfigureToolbars();

    void applyNewToolbarConfig();
    void slotNewInstance( const QString& fn1, const QString& fn2, const QString& fn3 );

private:
    KParts::ReadWritePart *m_part;
    KDiff3App *m_widget;

    KToggleAction *m_toolbarAction;
    KToggleAction *m_statusbarAction;
    bool m_bUnderConstruction;
};

#endif // _KDIFF3_H_
