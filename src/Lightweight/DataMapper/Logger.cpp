// SPDX-License-Identifier: Apache-2.0
#include "Detail.hpp"
#include "Field.hpp"
#include "Logger.hpp"

#include <chrono>
#include <string_view>

class StandardQueryLogger: public QueryLogger
{
  private:
    std::chrono::steady_clock::time_point m_startedAt;
    std::string m_query;
    // FieldList m_output;
    size_t m_rowCount {};

  public:
    LIGHTWEIGHT_API void QueryStart(std::string_view query, FieldList const& /*output*/) override
    {
        m_startedAt = std::chrono::steady_clock::now();
        m_query = query;
        // m_output = output;
        m_rowCount = 0;
    }

    LIGHTWEIGHT_API void QueryNextRow(AbstractRecord const& /*record*/) override
    {
        ++m_rowCount;
    }

    LIGHTWEIGHT_API void QueryEnd() override
    {
        auto const stoppedAt = std::chrono::steady_clock::now();
        auto const duration = std::chrono::duration_cast<std::chrono::microseconds>(stoppedAt - m_startedAt);
        auto const seconds = std::chrono::duration_cast<std::chrono::seconds>(duration);
        auto const microseconds = std::chrono::duration_cast<std::chrono::microseconds>(duration - seconds);
        auto const durationStr = std::format("{}.{:06}", seconds.count(), microseconds.count());

        auto const rowCountStr = m_rowCount == 0   ? ""
                                 : m_rowCount == 1 ? " [1 row]"
                                                   : std::format(" [{} rows]", m_rowCount);

#if 0
        if (m_output.empty())
        {
            std::println("[{}]{} {}", durationStr, rowCountStr, m_query);
            return;
        }

        detail::StringBuilder output;

        for (AbstractField const* field: m_output)
        {
            if (!output.empty())
                output << ", ";
            output << field->Name().name << '=' << field->InspectValue();
        }

        std::println("[{}]{} {} WITH [{}]", durationStr, rowCountStr, m_query, *output);
#else
        std::println("[{}]{} {}", durationStr, rowCountStr, m_query);
#endif
    }
};

static QueryLogger theNullLogger;

QueryLogger* QueryLogger::NullLogger() noexcept
{
    return &theNullLogger;
}

static StandardQueryLogger theStandardLogger;

QueryLogger* QueryLogger::StandardLogger() noexcept
{
    return &theStandardLogger;
}

QueryLogger* QueryLogger::m_instance = QueryLogger::NullLogger();
