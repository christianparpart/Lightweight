// SPDX-License-Identifier: Apache-2.0

#include "SqlGuid.hpp"

#if __has_include(<Windows.h>)
    #include <Windows.h>
#elif __has_include(<uuid/uuid.h>)
    #include <cstring>

    #include <uuid/uuid.h>
#endif

#include <memory>
#include <random>

SqlGuid SqlGuid::Create() noexcept
{
    SqlGuid guid {};

#if (defined(_WIN32) || defined(_WIN64)) && defined(LIGHTWEIGHT_NATIVE_GUID)

    GUID winGuid;
    CreateGuid(&winGuid);
    guid.data[0] = winGuid.Data1 >> 24;
    guid.data[1] = winGuid.Data1 >> 16;
    guid.data[2] = winGuid.Data1 >> 8;
    guid.data[3] = winGuid.Data1;
    guid.data[4] = winGuid.Data2 >> 8;
    guid.data[5] = winGuid.Data2;
    guid.data[6] = winGuid.Data3 >> 8;
    guid.data[7] = winGuid.Data3;
    guid.data[8] = winGuid.Data4[0];
    guid.data[9] = winGuid.Data4[1];
    guid.data[10] = winGuid.Data4[2];
    guid.data[11] = winGuid.Data4[3];
    guid.data[12] = winGuid.Data4[4];
    guid.data[13] = winGuid.Data4[5];
    guid.data[14] = winGuid.Data4[6];
    guid.data[15] = winGuid.Data4[7];

#elif __has_include(<uuid/uuid.h>)

    uuid_t uuid {};
    uuid_generate_time_safe(uuid);
    std::memcpy(guid.data, uuid, sizeof(uuid));

#else

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<unsigned short> dis(0, 255);
    for (auto& byte: guid.data)
        byte = static_cast<uint8_t>(dis(gen));
    guid.data[6] = (guid.data[6] & 0x0F) | 0x40; // Set the version to 4
    guid.data[8] = (guid.data[8] & 0x3F) | 0x80; // Set the variant to 2

#endif
    return guid;
}

bool SqlGuid::Validate(std::string_view const& text) noexcept
{
    // TODO: verify the correctness of this function

    // Validate the length
    if (text.size() != 36)
        return false;

    // Validate the dashes
    if (text[8] != '-' || text[13] != '-' || text[18] != '-' || text[23] != '-')
        return false;

    // Validate the version and variant
    if (text[14] != '4' && text[14] != '5')
        return false;

    // Validate the variant
    if (text[19] != '8' && text[19] != '9' && text[19] != 'A' && text[19] != 'B')
        return false;

    // Validate the hex characters
    for (size_t i = 0; i < 36; ++i)
    {
        if (i == 8 || i == 13 || i == 18 || i == 23)
            continue;

        if (i == 14 || i == 19)
            continue;

        if (text[i] < '0' || text[i] > '9')
            return false;

        if (i == 19)
        {
            if (text[i] < '8' || text[i] > 'B')
                return false;
        }
        else if (text[i] < 'A' || text[i] > 'F')
            return false;
    }
    return true;
}

SqlGuid SqlGuid::Parse(std::string_view const& text) noexcept
{
    SqlGuid guid {};

    guid.data[0] = static_cast<uint8_t>(std::stoul(std::string(text.substr(0, 8)), nullptr, 16) >> 24);
    guid.data[1] = static_cast<uint8_t>(std::stoul(std::string(text.substr(0, 8)), nullptr, 16) >> 16);
    guid.data[2] = static_cast<uint8_t>(std::stoul(std::string(text.substr(0, 8)), nullptr, 16) >> 8);
    guid.data[3] = static_cast<uint8_t>(std::stoul(std::string(text.substr(0, 8)), nullptr, 16));
    guid.data[4] = static_cast<uint8_t>(std::stoul(std::string(text.substr(9, 4)), nullptr, 16) >> 8);
    guid.data[5] = static_cast<uint8_t>(std::stoul(std::string(text.substr(9, 4)), nullptr, 16));
    guid.data[6] = static_cast<uint8_t>(std::stoul(std::string(text.substr(14, 4)), nullptr, 16) >> 8);
    guid.data[7] = static_cast<uint8_t>(std::stoul(std::string(text.substr(14, 4)), nullptr, 16));
    guid.data[8] = static_cast<uint8_t>(std::stoul(std::string(text.substr(19, 4)), nullptr, 16));
    guid.data[9] = static_cast<uint8_t>(std::stoul(std::string(text.substr(19, 4)), nullptr, 16) >> 8);
    guid.data[10] = static_cast<uint8_t>(std::stoull(std::string(text.substr(24, 12)), nullptr, 16) >> 40);
    guid.data[11] = static_cast<uint8_t>(std::stoull(std::string(text.substr(24, 12)), nullptr, 16) >> 32);
    guid.data[12] = static_cast<uint8_t>(std::stoul(std::string(text.substr(24, 12)), nullptr, 16) >> 24);
    guid.data[13] = static_cast<uint8_t>(std::stoul(std::string(text.substr(24, 12)), nullptr, 16) >> 16);
    guid.data[14] = static_cast<uint8_t>(std::stoul(std::string(text.substr(24, 12)), nullptr, 16) >> 8);
    guid.data[15] = static_cast<uint8_t>(std::stoul(std::string(text.substr(24, 12)), nullptr, 16));

    return guid;
}

SQLRETURN SqlDataBinder<SqlGuid>::InputParameter(SQLHSTMT stmt,
                                                 SQLUSMALLINT column,
                                                 SqlGuid const& value,
                                                 SqlDataBinderCallback& cb) noexcept
{
    switch (cb.ServerType())
    {
        case SqlServerType::SQLITE: {
            // For SQlITE, we'll implement GUIDs as text
            auto text = std::make_shared<std::string>(to_string(value));
            auto rv = SqlDataBinder<std::string>::InputParameter(stmt, column, *text, cb);
            if (SQL_SUCCEEDED(rv))
                cb.PlanPostExecuteCallback([text = std::move(text)] {});
            return rv;
        }
        case SqlServerType::ORACLE: // TODO
        case SqlServerType::MYSQL:  // TODO
        case SqlServerType::POSTGRESQL:
        case SqlServerType::MICROSOFT_SQL:
        case SqlServerType::UNKNOWN:
            return SQLBindParameter(
                stmt, column, SQL_PARAM_INPUT, SQL_C_GUID, SQL_GUID, sizeof(value), 0, (SQLPOINTER) &value, 0, nullptr);
            break;
    }
    std::unreachable();
}

SQLRETURN SqlDataBinder<SqlGuid>::OutputColumn(
    SQLHSTMT stmt, SQLUSMALLINT column, SqlGuid* result, SQLLEN* indicator, SqlDataBinderCallback& cb) noexcept
{
    switch (cb.ServerType())
    {
        case SqlServerType::SQLITE: {
            // For SQlITE, we'll implement GUIDs as text
            auto text = std::make_shared<std::string>();
            auto rv = SqlDataBinder<std::string>::OutputColumn(stmt, column, text.get(), indicator, cb);
            if (SQL_SUCCEEDED(rv))
                cb.PlanPostProcessOutputColumn([text = std::move(text), result] { *result = SqlGuid::Parse(*text); });
            return rv;
        }
        case SqlServerType::ORACLE: // TODO
        case SqlServerType::MYSQL:  // TODO
        case SqlServerType::POSTGRESQL:
        case SqlServerType::MICROSOFT_SQL:
            return SQLBindCol(stmt, column, SQL_C_GUID, (SQLPOINTER) result->data, sizeof(result->data), indicator);
        case SqlServerType::UNKNOWN:
            break;
    }

    std::unreachable();
}

SQLRETURN SqlDataBinder<SqlGuid>::GetColumn(
    SQLHSTMT stmt, SQLUSMALLINT column, SqlGuid* result, SQLLEN* indicator, SqlDataBinderCallback const& cb) noexcept
{
    switch (cb.ServerType())
    {
        case SqlServerType::SQLITE: {
            // For SQlITE, we'll implement GUIDs as text
            std::string text;
            auto rv = SqlDataBinder<std::string>::GetColumn(stmt, column, &text, indicator, cb);
            if (SQL_SUCCEEDED(rv))
                *result = SqlGuid::Parse(text);
            return rv;
        }
        case SqlServerType::ORACLE: // TODO
        case SqlServerType::MYSQL:  // TODO
        case SqlServerType::MICROSOFT_SQL:
        case SqlServerType::POSTGRESQL:
        case SqlServerType::UNKNOWN:
            return SQLGetData(stmt, column, SQL_C_GUID, result->data, sizeof(result->data), indicator);
            return SQL_ERROR;
    }
    std::unreachable();
}
