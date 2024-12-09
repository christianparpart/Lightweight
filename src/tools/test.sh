
run_test() {
    echo "Running test $1"
    sqlite3 test.db < ./src/tools/tests/$1.sql
    ./build/src/tools/ddl2cpp --connection-string "DRIVER=SQLite3;Database=test.db" --output ./src/tools/tests/$1.result 1> /dev/null
    diff ./src/tools/tests/$1.result ./src/tools/tests/$1.expected --ignore-all-space --ignore-blank-lines
    rm test.db
    rm ./src/tools/tests/$1.result
}

for test in $(ls ./src/tools/tests/*.sql); do
    run_test $(basename $test .sql)
done
