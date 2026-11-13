add_executable(ReadingStatsLogsTest ReadingStatsLogsTest.cpp ${REPO_ROOT}/src/stats/DailyReadingLog.cpp
               ${REPO_ROOT}/src/stats/FinishedBooksLog.cpp ${REPO_ROOT}/src/stats/ReadingStatsUtils.cpp)
target_link_libraries(ReadingStatsLogsTest PRIVATE crosspoint_test_common GTest::gtest_main)
target_include_directories(ReadingStatsLogsTest PRIVATE stubs ../library_builder/stubs ${REPO_ROOT}/src
                           ${REPO_ROOT}/src/stats)
gtest_discover_tests(ReadingStatsLogsTest TEST_PREFIX "ReadingStatsLogsTest.")
