#include <algorithm>
#include <cassert>
#include <fstream>
#include <print>

#include "./JPSql/SqlConnectInfo.hpp"
#include "./JPSql/SqlConnection.hpp"
#include "./JPSql/SqlSchema.hpp"
#include "./JPSql/SqlStatement.hpp"
#include "./JPSql/SqlTraits.hpp"

// TODO: have an OdbcConnectionString API to help compose/decompose connection settings
// TODO: move SanitizePwd function into that API, like `string OdbcConnectionString::PrettyPrintSanitized()`

// TODO: get inspired by .NET's Dapper, and EF Core APIs

namespace
{

constexpr auto finally(auto&& cleanupRoutine) noexcept
{
    struct Finally
    {
        std::remove_cvref_t<decltype(cleanupRoutine)> cleanup;
        ~Finally() { cleanup(); }
    };
    return Finally { std::forward<decltype(cleanupRoutine)>(cleanupRoutine) };
}

std::string MakeType(SqlSchema::Column const& column)
{
    using ColumnType = SqlColumnType;
    switch (column.type)
    {
        case ColumnType::CHAR:
            if (column.size == 1)
                return "char";
            else
                return std::format("SqlTrimmedString<{}>", column.size);
        case ColumnType::STRING:
            if (column.size == 1)
                return "char";
            else
                return std::format("std::string", column.size);
        case ColumnType::TEXT:
            return std::format("SqlText");
        case ColumnType::BOOLEAN:
            return "bool";
        case ColumnType::INTEGER:
            return "int";
        case ColumnType::REAL:
            return "double";
        case ColumnType::BLOB:
            return "std::vector<std::byte>";
        case ColumnType::DATE:
            return "SqlDate";
        case ColumnType::TIME:
            return "SqlTime";
        case ColumnType::DATETIME:
            return "SqlDateTime";
    }
    return "void";
}

std::string MakeVariableName(SqlSchema::FullyQualifiedTableName const& table)
{
    auto name = std::format("{}", table.table);
    name.at(0) = std::tolower(name.at(0));
    return name;
}

constexpr bool isVowel(char c) noexcept
{
    switch (c)
    {
        case 'a':
        case 'e':
        case 'i':
        case 'o':
        case 'u':
            return true;
        default:
            return false;
    }
}

std::string MakePluralVariableName(SqlSchema::FullyQualifiedTableName const& table)
{
    auto const& sqlName = table.table;
    if (sqlName.back() == 'y' && sqlName.size() > 1 && !isVowel(sqlName.at(sqlName.size() - 2)))
    {
        auto name = std::format("{}ies", sqlName.substr(0, sqlName.size() - 1));
        name.at(0) = std::tolower(name.at(0));
        return name;
    }
    auto name = std::format("{}s", sqlName);
    name.at(0) = std::tolower(name.at(0));
    return name;
}

class CxxModelPrinter
{
  private:
    mutable std::vector<std::string> m_forwwardDeclarations;
    std::stringstream m_definitions;

  public:
    std::string str(std::string_view modelNamespace) const
    {
        std::ranges::sort(m_forwwardDeclarations);

        std::stringstream output;
        output << "#include \"src/JPSql/Model/All.hpp\"\n\n";
        if (!modelNamespace.empty())
            output << std::format("namespace {}\n{{\n\n", modelNamespace);
        for (auto const& name: m_forwwardDeclarations)
            output << std::format("struct {};\n", name);
        output << "\n";
        output << m_definitions.str();
        if (!modelNamespace.empty())
            output << std::format("}} // end namespace {}\n", modelNamespace);

        return output.str();
    }

    void PrintTable(SqlSchema::Table const& table)
    {
        m_forwwardDeclarations.push_back(table.name);

        std::string cxxPrimaryKeys;
        for (auto const& key: table.primaryKeys)
        {
            if (!cxxPrimaryKeys.empty())
                cxxPrimaryKeys += ", ";
            cxxPrimaryKeys += '"' + key + '"';
        }

        m_definitions << std::format("struct {0} final: Model::Record<{0}>\n", table.name);
        m_definitions << std::format("{{\n");

        int columnPosition = 0;
        for (auto const& column: table.columns)
        {
            ++columnPosition;
            std::string type = MakeType(column);
            if (column.isPrimaryKey)
                continue;
            if (column.isForeignKey)
                continue;
            m_definitions << std::format("    Model::Field<{}, {}, \"{}\"{}> {};\n",
                                         type,
                                         columnPosition,
                                         column.name,
                                         column.isNullable ? ", Nullable" : "",
                                         column.name);
        }

        columnPosition = 0;
        for (auto const& foreignKey: table.foreignKeys)
        {
            ++columnPosition;
            m_definitions << std::format("    Model::BelongsTo<{}, {}, \"{}\"> {};\n",
                                         foreignKey.primaryKey.table,
                                         columnPosition,
                                         foreignKey.foreignKey.column,
                                         MakeVariableName(foreignKey.primaryKey.table));
        }

        for (SqlSchema::ForeignKeyConstraint const& foreignKey: table.externalForeignKeys)
        {
            m_definitions << std::format("    Model::HasMany<{}, \"{}\"> {};\n",
                                         foreignKey.foreignKey.table,
                                         foreignKey.foreignKey.column,
                                         MakePluralVariableName(foreignKey.foreignKey.table));
        }

        std::vector<std::string> fieldNames;
        for (auto const& column: table.columns)
            if (!column.isPrimaryKey && !column.isForeignKey)
                fieldNames.push_back(column.name);

        // Create default ctor
        auto const cxxModelTypeName = table.name;
        m_definitions << '\n';
        m_definitions << std::format("    {}():\n", cxxModelTypeName);
        m_definitions << std::format("        Record {{ \"{}\", {} }}", table.name, cxxPrimaryKeys);
        for (auto const& fieldName: fieldNames)
            m_definitions << std::format(",\n        {} {{ *this }}", fieldName, fieldName);
        for (auto const& constraint: table.foreignKeys)
            m_definitions << std::format(",\n        {} {{ *this }}", MakeVariableName(constraint.primaryKey.table));
        for (auto const& constraint: table.externalForeignKeys)
            m_definitions << std::format(",\n        {} {{ *this }}",
                                         MakePluralVariableName(constraint.foreignKey.table));
        m_definitions << "\n";
        m_definitions << "    {\n";
        m_definitions << "    }\n";

        m_definitions << "\n";

        // Create move ctor
        m_definitions << std::format("    {0}({0}&& other) noexcept:\n", cxxModelTypeName);
        m_definitions << std::format("        Record {{ std::move(other) }}");
        for (auto const& fieldName: fieldNames)
            m_definitions << std::format(",\n        {0} {{ *this, std::move(other.{0}) }}", fieldName);
        for (auto const& constraint: table.foreignKeys)
            m_definitions << std::format(",\n        {0} {{ *this, std::move(other.{0}) }}",
                                         MakeVariableName(constraint.primaryKey.table));
        for (auto const& constraint: table.externalForeignKeys)
            m_definitions << std::format(",\n        {0} {{ *this, std::move(other.{0}) }}",
                                         MakePluralVariableName(constraint.foreignKey.table));
        m_definitions << "\n";
        m_definitions << "    {\n";
        m_definitions << "    }\n";

        m_definitions << "};\n\n";
    }
};

void CreateTestTables()
{
    auto constexpr createStatement = R"(
        CREATE TABLE User (
            id              {0},
            fullname        VARCHAR(128) NOT NULL,
            email           VARCHAR(60) NOT NULL
        );
        CREATE TABLE TaskList (
            id              {0},
            user_id         INT NOT NULL,
            CONSTRAINT      fk1 FOREIGN KEY (user_id) REFERENCES user(id)
        );
        CREATE TABLE TaskListEntry (
            id              {0},
            tasklist_id     INT NOT NULL,
            completed       DATETIME NULL,
            task            VARCHAR(255) NOT NULL,
            CONSTRAINT      fk1 FOREIGN KEY (tasklist_id) REFERENCES TaskList(id)
        );
    )";
    auto stmt = SqlStatement();
    stmt.ExecuteDirect(std::format(createStatement, stmt.Connection().Traits().PrimaryKeyAutoIncrement));
}

void PostConnectedHook(SqlConnection& connection)
{
    switch (connection.ServerType())
    {
        case SqlServerType::SQLITE: {
            auto stmt = SqlStatement { connection };
            // Enable foreign key constraints for SQLite
            (void) stmt.ExecuteDirect("PRAGMA foreign_keys = ON");
            break;
        }
        case SqlServerType::MICROSOFT_SQL:
        case SqlServerType::POSTGRESQL:
        case SqlServerType::ORACLE:
        case SqlServerType::MYSQL:
        case SqlServerType::UNKNOWN:
            break;
    }
}

void PrintInfo()
{
    auto c = SqlConnection();
    assert(c.IsAlive());
    std::println("Connected to   : {}", c.DatabaseName());
    std::println("Server name    : {}", c.ServerName());
    std::println("Server version : {}", c.ServerVersion());
    std::println("User name      : {}", c.UserName());
    std::println("");
}

} // end namespace

struct Configuration
{
    std::string_view connectionString {};
    std::string_view database {};
    std::string_view schema {};
    std::string_view modelNamespace {};
    std::string_view outputFileName {};
    bool createTestTables = false;
};

std::expected<Configuration, int> ParseArguments(int argc, char const* argv[])
{
    using namespace std::string_view_literals;
    auto config = Configuration {};

    int i = 1;

    for (; i < argc; ++i)
    {
        if (argv[i] == "--trace-sql"sv)
            SqlLogger::SetLogger(SqlLogger::TraceLogger());
        else if (argv[i] == "--connection-string"sv)
        {
            if (++i >= argc)
                return std::unexpected { EXIT_FAILURE };
            config.connectionString = argv[i];
        }
        else if (argv[i] == "--database"sv)
        {
            if (++i >= argc)
                return std::unexpected { EXIT_FAILURE };
            config.database = argv[i];
        }
        else if (argv[i] == "--schema"sv)
        {
            if (++i >= argc)
                return std::unexpected { EXIT_FAILURE };
            config.schema = argv[i];
        }
        else if (argv[i] == "--create-test-tables"sv)
            config.createTestTables = true;
        else if (argv[i] == "--model-namespace"sv)
        {
            if (++i >= argc)
                return std::unexpected { EXIT_FAILURE };
            config.modelNamespace = argv[i];
        }
        else if (argv[i] == "--output"sv)
        {
            if (++i >= argc)
                return std::unexpected { EXIT_FAILURE };
            config.outputFileName = argv[i];
        }
        else if (argv[i] == "--help"sv || argv[i] == "-h"sv)
        {
            std::println("Usage: {} [options] [database] [schema]", argv[0]);
            std::println("Options:");
            std::println("  --trace-sql             Enable SQL tracing");
            std::println("  --connection-string STR ODBC connection string");
            std::println("  --database STR          Database name");
            std::println("  --schema STR            Schema name");
            std::println("  --create-test-tables    Create test tables");
            std::println("  --output STR            Output file name");
            std::println("  --help, -h              Display this information");
            std::println("");
            return std::unexpected { EXIT_SUCCESS };
        }
        else if (argv[i] == "--"sv)
        {
            ++i;
            break;
        }
        else
        {
            std::println("Unknown option: {}", argv[i]);
            return std::unexpected { EXIT_FAILURE };
        }
    }

    if (i < argc)
        argv[i - 1] = argv[0];

    return { std::move(config) };
}

int main(int argc, char const* argv[])
{
    auto const configOpt = ParseArguments(argc, argv);
    if (!configOpt)
        return configOpt.error();
    auto const config = configOpt.value();

    SqlConnection::SetDefaultConnectInfo(SqlConnectionString { std::string(config.connectionString) });
    SqlConnection::SetPostConnectedHook(&PostConnectedHook);

    auto const _ = finally([] { SqlConnection::KillAllIdle(); });

    if (config.createTestTables)
        CreateTestTables();

    PrintInfo();

    std::vector<SqlSchema::Table> tables = SqlSchema::ReadAllTables(config.database, config.schema);
    CxxModelPrinter printer;
    for (auto const& table: tables)
        printer.PrintTable(table);

    if (config.outputFileName.empty() || config.outputFileName == "-")
        std::println("{}", printer.str(config.modelNamespace));
    else
    {
        auto file = std::ofstream(config.outputFileName.data());
        file << printer.str(config.modelNamespace);
    }

    return EXIT_SUCCESS;
}
