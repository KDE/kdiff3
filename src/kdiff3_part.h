/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
// clang-format on

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

    static void noInit() { bNeedInit = false; }
protected:
    /**
     * This must be implemented by each part
     */
    bool openFile() override;

    /**
     * This must be implemented by each read-write part
     */
    bool saveFile() override;

    static bool bNeedInit;
private:
    void getNameAndVersion(const QString& str, const QString& lineStart, QString& fileName, QString& version);
    KAboutData createAboutData();

    KDiff3App* m_widget;
};

#endif // _KDIFF3PART_H_
