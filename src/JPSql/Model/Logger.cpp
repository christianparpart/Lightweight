#include "Detail.hpp"
#include "Logger.hpp"

#include <chrono>
#include <string_view>
#include <vector>

namespace Model
{

class SqlStandardModelQueryLogger: public SqlModelQueryLogger
{
  private:
    std::chrono::steady_clock::time_point m_startedAt;
    std::string_view m_query;
    std::vector<SqlModelFieldBase*> m_output;

  public:
    void QueryStart(std::string_view query, std::vector<SqlModelFieldBase*> const& output) override
    {
        m_startedAt = std::chrono::steady_clock::now();
        m_query = query;
        m_output = output;
    }

    void QueryEnd() override
    {
        auto const stoppedAt = std::chrono::steady_clock::now();
        auto const duration = std::chrono::duration_cast<std::chrono::microseconds>(stoppedAt - m_startedAt);
        auto const seconds = std::chrono::duration_cast<std::chrono::seconds>(duration);
        auto const microseconds = std::chrono::duration_cast<std::chrono::microseconds>(duration - seconds);

        detail::StringBuilder output;

        for (SqlModelFieldBase const* field: m_output)
        {
            if (!output.empty())
                output << ", ";
            output << field->Name() << '=' << field->InspectValue();
        }

        auto const durationStr = std::format("{}.{:06}", seconds.count(), microseconds.count());

        std::println("[{}] {} WITH [{}]", durationStr, m_query, *output);
    }
};

static SqlModelQueryLogger sqlModelNullLogger;
static SqlStandardModelQueryLogger sqlModelStandardLogger;

SqlModelQueryLogger* SqlModelQueryLogger::NullLogger() noexcept
{
    return &sqlModelNullLogger;
}

SqlModelQueryLogger* SqlModelQueryLogger::StandardLogger() noexcept
{
    return &sqlModelStandardLogger;
}

SqlModelQueryLogger* SqlModelQueryLogger::m_instance = SqlModelQueryLogger::NullLogger();

} // namespace Model
