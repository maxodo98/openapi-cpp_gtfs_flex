#include "openapi/date_time.h"

#include <ostream>

#include "utl/verify.h"

#include "date/date.h"
#include "date/tz.h"

namespace openapi {

std::optional<date_time_t> now_test = std::nullopt;

std::ostream& operator<<(std::ostream& out, date_time_t const& t) {
  utl::verify(t.offset_ == std::chrono::minutes{0}, "offset not supported yet");
  out << date::format("%FT%TZ", t.time_);
  return out;
}

date_time_t now() {
  if (now_test.has_value()) {
    return now_test.value();
  }
  return std::chrono::time_point_cast<std::chrono::seconds>(
      std::chrono::system_clock::now());
}

void parse(std::string_view s, date_time_t& v) {
  auto in = std::istringstream{std::string{s}};

  auto tp = date::sys_time<std::chrono::milliseconds>{};
  in >> date::parse("%FT%TZ", tp);

  if (!in.fail()) {
    v = tp;
    return;
  }

  auto offset = std::chrono::minutes{};
  in.clear();
  in.str(std::string{s});
  in >> date::parse("%FT%T%Ez", tp, offset);
  if (!in.fail()) {
    v = {tp, offset};
    return;
  }

  in.clear();
  in.str(std::string{s});
  in >> date::parse("%FT%H:%MZ", tp);
  if (!in.fail()) {
    v = tp;
    return;
  }

  in.clear();
  in.str(std::string{s});
  in >> date::parse("%FT%H:%M%Ez", tp, offset);
  if (!in.fail()) {
    v = {tp, offset};
    return;
  }

  throw utl::fail("failed to parse timestamp \"{}\"", s);
}

}  // namespace openapi