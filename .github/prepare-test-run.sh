#! /usr/bin/env bash

# This script is used to prepare the test run environment on Github Actions.

DBMS="$1" # One of: "SQLite3", "MS SQL Server 2019", "MS SQL Server 2022" "PostgreSQL", "Oracle", "MySQL"

setup_sqlite3() {
    echo "Setting up SQLite3..."
    sudo apt install -y \
                 libsqlite3-dev \
                 libsqliteodbc \
                 sqlite3 \
                 unixodbc-dev

    if [ -n "$GITHUB_OUTPUT" ]; then
        echo "Exporting ODBC_CONNECTION_INFO..."
        # expose the ODBC connection string to connect to the database
        echo "ODBC_CONNECTION_STRING=DRIVER=SQLite3;DATABASE=file::memory:" >> "${GITHUB_OUTPUT}"
    fi
}

setup_sqlserver() {
    # References:
    # https://learn.microsoft.com/en-us/sql/linux/sample-unattended-install-ubuntu?view=sql-server-ver16
    # https://learn.microsoft.com/en-us/sql/tools/sqlcmd/sqlcmd-utility
    # https://learn.microsoft.com/en-us/sql/linux/quickstart-install-connect-docker

    set -x
    local MSSQL_PID='evaluation'
    local MSSQL_SA_PASSWORD="${MSSQL_SA_PASSWORD:-R3a11yStrohng_P@ssw0rd}"
    local SS_VERSION="$1"
    local UBUNTU_RELEASE="20.04" # we fixiate the version, because the latest isn't always available by MS -- "$(lsb_release -r | awk '{print $2}')

    echo "Installing sqlcmd..."
    curl https://packages.microsoft.com/keys/microsoft.asc | sudo tee /etc/apt/trusted.gpg.d/microsoft.asc
    sudo add-apt-repository "$(wget -qO- https://packages.microsoft.com/config/ubuntu/${UBUNTU_RELEASE}/prod.list)"
    sudo apt update
    sudo apt install sqlcmd

    echo "Installing ODBC..."
    sudo ACCEPT_EULA=y DEBIAN_FRONTEND=noninteractive apt install -y unixodbc-dev unixodbc odbcinst mssql-tools18
    dpkg -L mssql-tools18

    echo "ODBC drivers installed:"
    sudo odbcinst -q -d

    echo "Querying ODBC driver for MS SQL Server..."
    sudo odbcinst -q -d -n "ODBC Driver 18 for SQL Server"

    echo "Pulling SQL Server ${SS_VERSION} image..."
    docker pull mcr.microsoft.com/mssql/server:${SS_VERSION}-latest

    echo "Starting SQL Server ${SS_VERSION}..."
    docker run \
            -e "ACCEPT_EULA=Y" \
            -e "MSSQL_SA_PASSWORD=${MSSQL_SA_PASSWORD}" \
            -p 1433:1433 --name sql1 --hostname sql1 \
            -d \
            "mcr.microsoft.com/mssql/server:${SS_VERSION}-latest"

    docker ps -a
    set +x

    echo "Wait for the SQL Server to start..."
    counter=1
    errstatus=1
    while [ $counter -le 15 ] && [ $errstatus = 1 ]
    do
        echo "$counter..."
        sleep 1s
        sqlcmd \
            -S localhost \
            -U SA \
            -P "$MSSQL_SA_PASSWORD" \
            -Q "SELECT @@VERSION" 2>/dev/null
        errstatus=$?
        ((counter++))
    done

    # Connect to server and print the version
    sqlcmd -S localhost -U SA -P "${MSSQL_SA_PASSWORD}" -Q 'SELECT * FROM sys. databases;'

    if [ -n "$GITHUB_OUTPUT" ]; then
        echo "Exporting ODBC_CONNECTION_INFO..."
        # expose the ODBC connection string to connect to the database server
        echo "ODBC_CONNECTION_STRING=DRIVER={ODBC Driver 18 for SQL Server};SERVER=localhost;PORT=1433;DATABASE=master;UID=SA;PWD=${MSSQL_SA_PASSWORD};TrustServerCertificate=yes;DATABASE=JPSqlTest" >> "${GITHUB_OUTPUT}"
    fi
}

setup_postgres() {
    echo "Setting up PostgreSQL..."
    sudo apt install -y \
                postgresql \
                postgresql-contrib
}

setup_oracle() {
    echo "Setting up Oracle..." # TODO
}

setup_mysql() {
    # install mysql server and its odbc driver
    sudo apt install -y mysql-server # TODO: odbc driver
}

case "$DBMS" in
    "SQLite3")
        setup_sqlite3
        ;;
    "MS SQL Server 2019")
        setup_sqlserver 2019
        ;;
    "MS SQL Server 2022")
        setup_sqlserver 2022
        ;;
    "PostgreSQL")
        setup_postgres
        ;;
    "Oracle")
        setup_oracle
        ;;
    "MySQL")
        setup_mysql
        ;;
    *)
        echo "Unknown DBMS: $DBMS"
        exit 1
        ;;
esac