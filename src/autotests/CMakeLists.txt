find_package(
    Qt5 ${QT_MIN_VERSION}
    REQUIRED
    Test
    )

ecm_add_test(commentparser.cpp ../CommentParser.cpp
    TEST_NAME "commentparser"
    LINK_LIBRARIES Qt5::Test
)
