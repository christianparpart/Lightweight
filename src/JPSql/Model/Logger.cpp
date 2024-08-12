#include "Detail.hpp"
#include "Field.hpp"
#include "Logger.hpp"

#include <chrono>
#include <ranges>
#include <string_view>
#include <vector>

namespace Model
{

class StandardQueryLogger: public QueryLogger
{
  private:
    std::chrono::steady_clock::time_point m_startedAt;
    std::string_view m_query;
    std::vector<AbstractField*> m_output;
    size_t m_rowCount {};

  public:
    void QueryStart(std::string_view query, std::vector<AbstractField*> const& output) override
    {
        m_startedAt = std::chrono::steady_clock::now();
        m_query = query;
        m_output = output;
        m_rowCount = 0;
    }

    void QueryNextRow(AbstractRecord const& /*record*/)
    {
        ++m_rowCount;
    }

    void QueryEnd() override
    {
        auto const stoppedAt = std::chrono::steady_clock::now();
        auto const duration = std::chrono::duration_cast<std::chrono::microseconds>(stoppedAt - m_startedAt);
        auto const seconds = std::chrono::duration_cast<std::chrono::seconds>(duration);
        auto const microseconds = std::chrono::duration_cast<std::chrono::microseconds>(duration - seconds);
        auto const durationStr = std::format("{}.{:06}", seconds.count(), microseconds.count());

        auto const rowCountStr = m_rowCount == 0   ? ""
                                 : m_rowCount == 1 ? " [1 row]"
                                                   : std::format(" [{} rows]", m_rowCount);

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
            output << field->Name() << '=' << field->InspectValue();
        }

        std::println("[{}]{} {} WITH [{}]", durationStr, rowCountStr, m_query, *output);
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

} // namespace Model
