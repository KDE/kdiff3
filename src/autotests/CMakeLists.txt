# SPDX-FileCopyrightText: 2018-2021 Michael Reeves reeves.87@gmail.com
# SPDX-License-Identifier: GPL-2.0-or-later

if(NOT BUILD_WITH_QT6)
    find_package(
        Qt5 ${QT_MIN_VERSION}
        REQUIRED
        COMPONENTS
        Test
    )
else()
    find_package(
        Qt6 ${QT_MIN_VERSION}
        REQUIRED
        COMPONENTS
        Test
    )

    set(COMPATLIB Qt6::Core5Compat)
endif()
add_definitions(-DAUTOTEST)

ecm_add_test(connectiontest.cpp
    TEST_NAME "connectiontest"
    LINK_LIBRARIES Qt::Test
)

ecm_add_test(commentparser.cpp ../CommentParser.cpp
    TEST_NAME "commentparser"
    LINK_LIBRARIES Qt::Test
)

ecm_add_test(CvsIgnoreListTest.cpp ../CvsIgnoreList.cpp ../fileaccess.cpp ../Utils.cpp ../ProgressProxy.cpp ../CompositeIgnoreList.cpp ../Logging.cpp
    TEST_NAME "cvsignorelisttest"
    LINK_LIBRARIES Qt::Test Qt::Gui Qt::Widgets
)

ecm_add_test(FileAccessTest.cpp ../fileaccess.cpp ../Utils.cpp ../ProgressProxy.cpp ../CvsIgnoreList.cpp ../CompositeIgnoreList.cpp ../Logging.cpp
    TEST_NAME "fileaccesstest"
    LINK_LIBRARIES Qt::Test Qt::Gui Qt::Widgets
)

ecm_add_test(combinertest.cpp
    TEST_NAME "combinertest"
    LINK_LIBRARIES Qt::Test
)

ecm_add_test(CompositeIgnoreListTest.cpp ../CompositeIgnoreList.cpp
    TEST_NAME "CompositeIgnoreListTest"
    LINK_LIBRARIES Qt::Test
)

ecm_add_test(GitIgnoreListTest.cpp ../GitIgnoreList.cpp ../fileaccess.cpp ../Utils.cpp ../ProgressProxy.cpp ../Logging.cpp
    TEST_NAME "GitIgnoreListTest"
    LINK_LIBRARIES Qt::Test Qt::Gui Qt::Widgets
)

ecm_add_test(datareadtest.cpp ../fileaccess.cpp ../SourceData.cpp ../CommentParser.cpp ../Utils.cpp ../ProgressProxy.cpp ../Logging.cpp
    TEST_NAME "datareadtest"
    LINK_LIBRARIES Qt::Test Qt::Gui Qt::Widgets ${COMPATLIB} KF${KF_MAJOR_VERSION}::ConfigCore
)

ecm_add_test(DiffTest.cpp ../diff.cpp ../Logging.cpp ../Utils.cpp ../ProgressProxy.cpp ../gnudiff_io.cpp ../gnudiff_analyze.cpp ../gnudiff_xmalloc.cpp ../fileaccess.cpp ../SourceData.cpp ../CommentParser.cpp
    TEST_NAME "difftest"
    LINK_LIBRARIES Qt::Test Qt::Gui Qt::Widgets ${COMPATLIB} KF${KF_MAJOR_VERSION}::ConfigCore
)

ecm_add_test(Diff3LineTest.cpp ../diff.cpp ../gnudiff_io.cpp ../gnudiff_analyze.cpp ../gnudiff_xmalloc.cpp ../Logging.cpp ../Utils.cpp ../ProgressProxy.cpp
    TEST_NAME "diff3linetest"
    LINK_LIBRARIES Qt::Test Qt::Gui Qt::Widgets KF${KF_MAJOR_VERSION}::ConfigCore
)

ecm_add_test(ManualDiffHelpListTest.cpp ../diff.cpp ../gnudiff_io.cpp ../gnudiff_analyze.cpp ../gnudiff_xmalloc.cpp ../Logging.cpp ../Utils.cpp ../ProgressProxy.cpp
    TEST_NAME "manualdiffhelplisttest"
    LINK_LIBRARIES Qt::Test Qt::Gui Qt::Widgets KF${KF_MAJOR_VERSION}::ConfigCore
)
