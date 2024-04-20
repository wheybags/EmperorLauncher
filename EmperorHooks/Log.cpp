#include "pch.h"
#include "Log.hpp"
#include <cstdio>

void Log(const char* format, ...)
{
  va_list args;
  va_start(args, format);
  vaLog(format, args);
  va_end(args);
}

void vaLog(const char* format, va_list args)
{
  vprintf(format, args);

  static FILE* f = fopen("emperor.txt", "wb");
  vfprintf(f, format, args);
  fflush(f);
}