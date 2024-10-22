// SPDX-License-Identifier: MIT

#pragma once

#if defined(_WIN32) || defined(_WIN64)
    #include <Windows.h>
#endif

#include "SqlError.hpp"

#include <sql.h>
#include <sqlext.h>
#include <sqlspi.h>
#include <sqltypes.h>

class SqlConnection;

// Represents the mode of a SQL transaction to be applied, if not done so explicitly.
enum class SqlTransactionMode
{
    NONE,
    COMMIT,
    ROLLBACK,
};

// Represents a transaction to a SQL database.
//
// This class is used to control the transaction manually. It disables the auto-commit mode when constructed,
// and automatically commits the transaction when destructed if not done so.
//
// This class is designed with RAII in mind, so that the transaction is automatically committed or rolled back
// when the object goes out of scope.
class SqlTransaction
{
  public:
    // Construct a new SqlTransaction object, and disable the auto-commit mode, so that the transaction can be
    // controlled manually.
    explicit SqlTransaction(SqlConnection& connection,
                            SqlTransactionMode defaultMode = SqlTransactionMode::COMMIT) noexcept;

    // Automatically commit the transaction if not done so
    ~SqlTransaction();

    // Rollback the transaction
    void Rollback();

    // Commit the transaction
    void Commit();

  private:
    SQLHDBC m_hDbc;
    SqlTransactionMode m_defaultMode;
};
