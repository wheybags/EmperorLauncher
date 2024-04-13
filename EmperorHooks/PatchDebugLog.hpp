#pragma once

enum class NetworkFromLogType
{
  NotSeenYet,
  Client,
  Server,
};

extern NetworkFromLogType networkTypeDerivedFromLogOutput;

void patchDebugLog();
