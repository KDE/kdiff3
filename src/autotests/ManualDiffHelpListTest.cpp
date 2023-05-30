#include "../diff.h"
#include "../Logging.h"
#include "../options.h"

#include <QObject>
#include <QTest>

std::unique_ptr<Options> gOptions = std::make_unique<Options>();

class ManualDiffHelpListTest: public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void testOverlappingRangesRemoved()
    {
        ManualDiffHelpList expected = {ManualDiffHelpEntry(e_SrcSelector::A, 20, 50)};
        ManualDiffHelpList list = {ManualDiffHelpEntry(e_SrcSelector::A, 1, 40)};

        list.insertEntry(e_SrcSelector::A, 20, 50);

        QVERIFY(list.size() == 1);
        QVERIFY(list == expected);
    }

    void testListKeeptSorted()
    {
        ManualDiffHelpList expected = {
            ManualDiffHelpEntry(e_SrcSelector::A, 1, 19),
            ManualDiffHelpEntry(e_SrcSelector::A, 20, 30),
            ManualDiffHelpEntry(e_SrcSelector::A, 50, 60),
        };
        ManualDiffHelpList list;

        list.insertEntry(e_SrcSelector::A, 1, 19);
        list.insertEntry(e_SrcSelector::A, 50, 60);
        list.insertEntry(e_SrcSelector::A, 20, 30);

        QVERIFY(list.size() == 3);
        QVERIFY(list == expected);
    }

    void testDeleteAndSort()
    {
        ManualDiffHelpList expected = {
            ManualDiffHelpEntry(e_SrcSelector::A, 1, 19),
            ManualDiffHelpEntry(e_SrcSelector::A, 25, 35),
            ManualDiffHelpEntry(e_SrcSelector::A, 50, 60),
        };
        ManualDiffHelpList list;

        list.insertEntry(e_SrcSelector::A, 1, 19);
        list.insertEntry(e_SrcSelector::A, 50, 60);
        list.insertEntry(e_SrcSelector::A, 20, 30);
        list.insertEntry(e_SrcSelector::A, 30, 40);
        list.insertEntry(e_SrcSelector::A, 25, 35);

        QVERIFY(list.size() == 3);
        QVERIFY(list == expected);
    }
};

QTEST_MAIN(ManualDiffHelpListTest);

#include "ManualDiffHelpListTest.moc"
