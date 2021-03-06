#-----------------------------------------------------------------------------
#
#  CMake Config
#
#  Tests
#
#-----------------------------------------------------------------------------

include_directories(include)
include_directories(../src)
include_directories(../include)

set(ALL_UNIT_TESTS
    t/test-config.cpp
    t/test-osmobj.cpp
    t/test-util.cpp
)

add_executable(unit-tests unit-tests.cpp ${ALL_UNIT_TESTS}
               ../src/config.cpp ../src/io.cpp ../src/osmobj.cpp ../src/util.cpp)
target_link_libraries(unit-tests ${PQXX_LIB} ${PQ_LIB} ${YAML_LIB})
add_test(NAME unit-tests COMMAND unit-tests WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")

add_test(NAME db-init COMMAND ${PROJECT_SOURCE_DIR}/test/db/init.sh)
set_tests_properties(db-init PROPERTIES FIXTURES_SETUP Database)

add_test(NAME db-test COMMAND osmdbt-testdb -c test-config.yaml)
set_tests_properties(db-test PROPERTIES FIXTURES_REQUIRED Database)

add_test(NAME db-enable COMMAND osmdbt-enable-replication -c test-config.yaml)
set_tests_properties(db-enable PROPERTIES FIXTURES_REQUIRED Database)
set_tests_properties(db-enable PROPERTIES FIXTURES_SETUP Replication)

add_test(NAME db-get-log-1 COMMAND osmdbt-get-log -c test-config.yaml)
set_tests_properties(db-get-log-1 PROPERTIES WILL_FAIL 1)
set_tests_properties(db-get-log-1 PROPERTIES FIXTURES_SETUP Replication)

add_test(NAME db-data COMMAND ${PROJECT_SOURCE_DIR}/test/db/create-objects.sh)
set_tests_properties(db-data PROPERTIES DEPENDS db-get-log-1)

add_test(NAME db-fake-log COMMAND osmdbt-fake-log -c test-config.yaml -t 2020-01-01T00:00:00Z)
set_tests_properties(db-fake-log PROPERTIES DEPENDS db-data)

add_test(NAME db-get-log-2 COMMAND osmdbt-get-log -c test-config.yaml --catchup)
set_tests_properties(db-get-log-2 PROPERTIES DEPENDS db-data)

add_test(NAME db-check-log COMMAND ${PROJECT_SOURCE_DIR}/test/db/check-log.sh)
set_tests_properties(db-check-log PROPERTIES DEPENDS db-get-log-2)

add_test(NAME db-create-diff COMMAND osmdbt-create-diff -c test-config.yaml -f osm-repl.log)
set_tests_properties(db-create-diff PROPERTIES DEPENDS db-check-log)

add_test(NAME db-check-diff COMMAND ${PROJECT_SOURCE_DIR}/test/db/check-diff.sh)
set_tests_properties(db-check-diff PROPERTIES DEPENDS db-create-diff)

add_test(NAME db-disable COMMAND osmdbt-disable-replication -c test-config.yaml)
set_tests_properties(db-disable PROPERTIES FIXTURES_CLEANUP Replication)

