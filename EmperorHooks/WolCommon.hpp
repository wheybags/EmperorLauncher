#pragma once
#include <windows.h>
#include <winsock.h>
#include <cstdint>

constexpr uint16_t wolPortH = 4005;

struct PortRange
{
  uint16_t startH;
  uint16_t endH;
};

enum class PacketType : uint8_t
{
  GamePacket,
  KeepAlive,
  ConnectionTest,
};

struct ForwardedPacketHeader
{
  uint32_t crc;
  sockaddr_in originalTo;
  uint16_t originalSourcePortH;
  in_addr originalSourceAddr; // only used for host->client packets
  uint32_t gameSpeed; // only used for host->client packets
  PacketType type;
  uint16_t portRangeStartH = 0;
  uint16_t portRangeEndH = 0;
};