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

#ifndef _KDIFF3PART_H_
#define _KDIFF3PART_H_

#include <kparts/part.h>
#include <kparts/factory.h>

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
    KDiff3Part(QWidget *parentWidget, const char *widgetName,
                    QObject *parent );

    /**
     * Destructor
     */
    virtual ~KDiff3Part();

    /**
     * This is a virtual function inherited from KParts::ReadWritePart.
     * A shell will use this to inform this Part if it should act
     * read-only
     */
    virtual void setReadWrite(bool rw);

    /**
     * Reimplemented to disable and enable Save action
     */
    virtual void setModified(bool modified);

protected:
    /**
     * This must be implemented by each part
     */
    virtual bool openFile();

    /**
     * This must be implemented by each read-write part
     */
    virtual bool saveFile();

private:
    KDiff3App* m_widget;
    bool m_bIsShell;
};

class KComponentData;
class KAboutData;

class KDiff3PartFactory : public KParts::Factory
{
    Q_OBJECT
public:
    KDiff3PartFactory();
    virtual ~KDiff3PartFactory();
    virtual KParts::Part* createPartObject( QWidget *parentWidget,
                                            QObject *parent,
                                            const char *classname, 
                                            const QStringList &args );
    static KComponentData* instance();

private:
    static KComponentData* s_instance;
    static KAboutData* s_about;
};

#endif // _KDIFF3PART_H_
