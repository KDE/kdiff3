# SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
# SPDX-License-Identifier: GPL-2.0-or-later

find_package(
    Qt5 ${QT_MIN_VERSION}
    REQUIRED
    Test
    )
add_definitions(-DAUTOTEST)

ecm_add_test(commentparser.cpp ../CommentParser.cpp
    TEST_NAME "commentparser"
    LINK_LIBRARIES Qt5::Test
)

ecm_add_test(CvsIgnoreListTest.cpp ../ignorelist.cpp ../fileaccess.cpp ../Utils.cpp ../progress.cpp
    TEST_NAME "cvsignorelisttest"
    LINK_LIBRARIES Qt5::Test Qt5::Gui Qt5::Widgets KF5::KIOCore KF5::I18n
)

ecm_add_test(FileAccessTest.cpp ../fileaccess.cpp ../Utils.cpp ../progress.cpp ../ignorelist.cpp
    TEST_NAME "fileaccesstest"
    LINK_LIBRARIES Qt5::Test Qt5::Gui Qt5::Widgets KF5::KIOCore KF5::I18n
)

ecm_add_test(combinertest.cpp
    TEST_NAME "combinertest"
    LINK_LIBRARIES Qt5::Test
)


ecm_add_test(DiffTest.cpp ../diff.cpp ../Logging.cpp ../Utils.cpp ../progress.cpp ../gnudiff_io.cpp ../gnudiff_analyze.cpp ../gnudiff_xmalloc.cpp ../fileaccess.cpp ../SourceData.cpp ../CommentParser.cpp
    TEST_NAME "difftest"
    LINK_LIBRARIES Qt5::Test Qt5::Widgets Qt5::Gui KF5::ConfigCore KF5::I18n KF5::WidgetsAddons KF5::Parts KF5::XmlGui KF5::KIOCore
)
