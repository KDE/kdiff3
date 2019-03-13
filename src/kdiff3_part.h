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

#ifndef KDIFF3PART_H
#define KDIFF3PART_H

#include <kparts/part.h>
#include <KPluginFactory>
#include <KParts/ReadWritePart>

class QWidget;
class KDiff3App;

/**
 * This is a "Part".  It that does all the real work in a KPart
 * application.
 *
 * @short Main Part
 * @author Joachim Eibl <joachim.eibl at gmx.de>
 */
class KDiff3Part : public KParts::ReadWritePart
{
    Q_OBJECT
public:
    /**
     * Default constructor
     */
    KDiff3Part(QWidget *parentWidget, QObject *parent, const QVariantList &args );

    /**
     * Destructor
     */
    ~KDiff3Part() override;

    /**
     * This is a virtual function inherited from KParts::ReadWritePart.
     * A shell will use this to inform this Part if it should act
     * read-only
     */
    void setReadWrite(bool rw) override;

    /**
     * Reimplemented to disable and enable Save action
     */
    void setModified(bool modified) override;

protected:
    /**
     * This must be implemented by each part
     */
    bool openFile() override;

    /**
     * This must be implemented by each read-write part
     */
    bool saveFile() override;

private:
    void getNameAndVersion(const QString& str, const QString& lineStart, QString& fileName, QString& version);
    KAboutData createAboutData();

    KDiff3App* m_widget;
    bool m_bIsShell;
};

#endif // _KDIFF3PART_H_
