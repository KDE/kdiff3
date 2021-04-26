# SPDX-FileCopyrightText: 2018-2021 Michael Reeves reeves.87@gmail.com
# SPDX-License-Identifier: GPL-2.0-or-later

find_package(
    Qt5 ${QT_MIN_VERSION}
    REQUIRED
    Test
    )
add_definitions(-DAUTOTEST)

ecm_add_test(connectiontest.cpp
    TEST_NAME "connectiontest"
    LINK_LIBRARIES Qt5::Test
)

ecm_add_test(commentparser.cpp ../CommentParser.cpp
    TEST_NAME "commentparser"
    LINK_LIBRARIES Qt5::Test
)

ecm_add_test(CvsIgnoreListTest.cpp ../CvsIgnoreList.cpp ../fileaccess.cpp ../Utils.cpp ../progress.cpp ../CompositeIgnoreList.cpp
    TEST_NAME "cvsignorelisttest"
    LINK_LIBRARIES Qt5::Test Qt5::Gui Qt5::Widgets KF5::KIOCore KF5::I18n
)

ecm_add_test(FileAccessTest.cpp ../fileaccess.cpp ../Utils.cpp ../progress.cpp ../CvsIgnoreList.cpp ../CompositeIgnoreList.cpp
    TEST_NAME "fileaccesstest"
    LINK_LIBRARIES Qt5::Test Qt5::Gui Qt5::Widgets KF5::KIOCore KF5::I18n
)

ecm_add_test(combinertest.cpp
    TEST_NAME "combinertest"
    LINK_LIBRARIES Qt5::Test
)

ecm_add_test(CompositeIgnoreListTest.cpp ../CompositeIgnoreList.cpp
    TEST_NAME "CompositeIgnoreListTest"
    LINK_LIBRARIES Qt5::Test
)

ecm_add_test(GitIgnoreListTest.cpp ../GitIgnoreList.cpp ../fileaccess.cpp ../Utils.cpp ../progress.cpp ../Logging.cpp
    TEST_NAME "GitIgnoreListTest"
    LINK_LIBRARIES Qt5::Test Qt5::Gui Qt5::Widgets KF5::KIOCore KF5::I18n
)

ecm_add_test(DiffTest.cpp ../diff.cpp ../Logging.cpp ../Utils.cpp ../progress.cpp ../gnudiff_io.cpp ../gnudiff_analyze.cpp ../gnudiff_xmalloc.cpp ../fileaccess.cpp ../SourceData.cpp ../CommentParser.cpp
    TEST_NAME "difftest"
    LINK_LIBRARIES Qt5::Test Qt5::Widgets Qt5::Gui KF5::ConfigCore KF5::I18n KF5::KIOCore KF5::Parts
)

ecm_add_test(Diff3LineTest.cpp ../diff.cpp ../gnudiff_io.cpp ../gnudiff_analyze.cpp ../gnudiff_xmalloc.cpp ../Logging.cpp ../Utils.cpp ../progress.cpp
    TEST_NAME "diff3linetest"
    LINK_LIBRARIES Qt5::Test Qt5::Widgets Qt5::Gui KF5::ConfigCore KF5::KIOCore KF5::I18n
)
