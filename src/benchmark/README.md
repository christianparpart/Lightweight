# Get database 
`sh ./src/benchmark/getdb.sh`

# Create Lightweight header for the database

`./build/src/tools/ddl2cpp --connection-string 'DRIVER=SQLite3;Database=mubi_db.sqlite' --output ./src/benchmark/tables.hpp`

# Current output

command to measure time from sqlite3 directly 
`time sqlite3 ./benchmark/mubi_db.sqlite < benchmark/select.sql`
`time sqlite3 ./benchmark/mubi_db.sqlite < benchmark/select.sql`

count      took   327 ms from sqlite:    15 ms
longQuery  took  3999 ms from sqlite:  4018 ms

