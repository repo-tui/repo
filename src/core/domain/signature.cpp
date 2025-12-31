#include <repo/domain/signature.hpp>

#include <fmt/chrono.h>
#include <fmt/format.h>

#include <iomanip>
#include <sstream>

namespace repo::domain {

auto Signature::format() const -> std::string {
    auto time_since_epoch = when.time_since_epoch();
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(time_since_epoch).count();

    // Format timezone offset as +HHMM or -HHMM
    auto offset_mins = tz_offset.count();
    char tz_sign = offset_mins >= 0 ? '+' : '-';
    offset_mins = std::abs(offset_mins);
    int tz_hours = offset_mins / 60;
    int tz_mins = offset_mins % 60;

    return fmt::format("{} {} {}{:02d}{:02d}", format_name_email(), seconds, tz_sign, tz_hours,
                       tz_mins);
}

auto Signature::format_name_email() const -> std::string {
    return fmt::format("{} <{}>", name, email);
}

auto Signature::format_time() const -> std::string {
    auto time_t_val = std::chrono::system_clock::to_time_t(when);
    std::tm tm = *std::gmtime(&time_t_val);

    // Format timezone offset as +HHMM or -HHMM
    auto offset_mins = tz_offset.count();
    char tz_sign = offset_mins >= 0 ? '+' : '-';
    offset_mins = std::abs(offset_mins);
    int tz_hours = offset_mins / 60;
    int tz_mins = offset_mins % 60;

    return fmt::format("{:04d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d} {}{:02d}{:02d}",
                       tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min,
                       tm.tm_sec, tz_sign, tz_hours, tz_mins);
}

} // namespace repo::domain
