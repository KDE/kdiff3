/*
 * Copyright (C) 2003 Joachim Eibl <joachim.eibl@gmx.de>
 */

#ifndef _KDIFF3PART_H_
#define _KDIFF3PART_H_

#include <kparts/part.h>
#include <kparts/factory.h>

class QWidget;
class QPainter;
class KURL;
class KDiff3App;

/**
 * This is a "Part".  It that does all the real work in a KPart
 * application.
 *
 * @short Main Part
 * @author Joachim Eibl <joachim.eibl@gmx.de>
 */
class KDiff3Part : public KParts::ReadOnlyPart
{
    Q_OBJECT
public:
    /**
     * Default constructor
     */
    KDiff3Part(QWidget *parentWidget, const char *widgetName,
                    QObject *parent, const char *name);

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

class KInstance;
class KAboutData;

class KDiff3PartFactory : public KParts::Factory
{
    Q_OBJECT
public:
    KDiff3PartFactory();
    virtual ~KDiff3PartFactory();
    virtual KParts::Part* createPartObject( QWidget *parentWidget, const char *widgetName,
                                            QObject *parent, const char *name,
                                            const char *classname, const QStringList &args );
    static KInstance* instance();

private:
    static KInstance* s_instance;
    static KAboutData* s_about;
};

#endif // _KDIFF3PART_H_
