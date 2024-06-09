#include "Misc.hpp"
#include <cstdarg>

std::string ssprintf(const char* format, ...)
{
  std::string result;
  result.resize(1024);

  while (true)
  {
    va_list args = {};
    va_start(args, format);
    errno = 0;
    int ret = _vsnprintf_s(&result[0], result.size(), result.size(), format, args);
    va_end(args);

    if (ret >= 0)
    {
      result.resize(ret);
      return result;
    }

    if (errno == ERANGE)
      result.resize(result.size() * 2);
    else
      return "FORMAT_ERROR";
  }
}
