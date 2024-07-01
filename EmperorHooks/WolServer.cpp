#include "WolServer.hpp"
#include <thread>
#include <vector>
#include "Error.hpp"
#include "Misc.hpp"
#include <optional>
#include "Log.hpp"
#include "WolPort.hpp"



// REFS:
// https://github.com/pvpgn/pvpgn-server/blob/develop/src/bnetd/handle_wserv.cpp
// https://medium.com/sean3z/xwis-wol-game-server-protocol-b2a3457bd06
// https://pvpgn.fandom.com/wiki/WOL_Gameres_Protocol_ServerCommands
// https://pvpgn.fandom.com/wiki/Westwood_Online_Protocols#WOL_IRC_Protocol
// https://www.rfc-editor.org/rfc/rfc1459#section-4.2.1



void WolServer::run()
{
  sockaddr_in serverAddr;
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_port = htons(wolPortH);
  serverAddr.sin_addr.S_un.S_addr = INADDR_ANY;

  SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, 0);
  release_assert(listenSocket != INVALID_SOCKET);

  release_assert(bind(listenSocket, (sockaddr*)&serverAddr, (int)sizeof(sockaddr_in)) != SOCKET_ERROR);
  release_assert(listen(listenSocket, SOMAXCONN) != SOCKET_ERROR);

  while (true)
  {
    sockaddr_in clientAddr = {};
    int clientAddrSize = sizeof(clientAddr);
    SOCKET clientSocket = accept(listenSocket, (sockaddr*)&clientAddr, &clientAddrSize);

    if (clientSocket != INVALID_SOCKET)
    {
      Connection* connection = nullptr;
      {
        std::scoped_lock lock(connectionsMutex);
        connection = connections.emplace_back(std::make_unique<Connection>(*this, clientSocket, clientAddr)).get();
      }

      std::thread([&]() { this->clientLoop(*connection); }).detach();
    }
  }
}

static std::vector<std::string> splitWhitespace(const std::string_view str)
{
  std::vector<std::string> result;

  std::string accumulator;
  for (char c : str)
  {
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
    {
      if (!accumulator.empty())
      {
        result.emplace_back(std::move(accumulator));
        accumulator.clear();
      }
    }
    else
    {
      accumulator.push_back(c);
    }
  }

  return result;
}

void WolServer::clientLoop(Connection& connection)
{
  std::vector<char> buffer;
  buffer.resize(1024 * 1024 * 10);

  std::string lineBuffer;

  printf("NEW CONNECTION\n");

  while (connection.connected)
  {
    int result = recv(connection.socket, buffer.data(), int(buffer.size()), 0);
    int a = WSAGetLastError();

    if (result <= 0)
      connection.connected = false;

    std::scoped_lock lock(this->connectionsMutex);

    for (int i = 0; i < result; i++)
    {
      lineBuffer.push_back(buffer[i]);

      if (buffer[i] == '\n')
      {
        std::vector<std::string> command = splitWhitespace(lineBuffer);

        if (!command.empty())
        {
          //printf("%s\n", command[0].c_str());

          if (command[0] == "verchk")
          {
            connection.handle_verchk(command);
          }
          else if (command[0] == "lobcount")
          {
            connection.handle_lobcount(command);
          }
          else if (command[0] == "QUIT")
          {
            connection.handle_QUIT(command);
          }
          else if (command[0] == "whereto")
          {
            connection.handle_whereto(command);
          }
          else if (command[0] == "CVERS")
          {
            connection.handle_CVERS(command);
          }
          else if (command[0] == "PASS")
          {
            connection.handle_PASS(command);
          }
          else if (command[0] == "NICK")
          {
            connection.handle_NICK(command);
          }
          else if (command[0] == "apgar")
          {
            connection.handle_apgar(command);
          }
          else if (command[0] == "SERIAL")
          {
            connection.handle_SERIAL(command);
          }
          else if (command[0] == "USER")
          {
            connection.handle_USER(command);
          }
          else if (command[0] == "SETOPT")
          {
            connection.handle_SETOPT(command);
          }
          else if (command[0] == "LIST")
          {
            connection.handle_LIST(command);
          }
          else if (command[0] == "SETCODEPAGE")
          {
            connection.handle_SETCODEPAGE(command);
          }
          else if (command[0] == "GETCODEPAGE")
          {
            connection.handle_GETCODEPAGE(command);
          }
          else if (command[0] == "SETLOCALE")
          {
            connection.handle_SETLOCALE(command);
          }
          else if (command[0] == "GETLOCALE")
          {
            connection.handle_GETLOCALE(command);
          }
          else if (command[0] == "SQUADINFO")
          {
            connection.handle_SQUADINFO(command);
          }
          else if (command[0] == "GETINSIDER")
          {
            connection.handle_GETINSIDER(command);
          }
          else if (command[0] == "TIME")
          {
            connection.handle_TIME(command);
          }
          else if (command[0] == "GETBUDDY")
          {
            connection.handle_GETBUDDY(command);
          }
          else if (command[0] == "TOPIC")
          {
            connection.handle_TOPIC(command, lineBuffer);
          }
          else if (command[0] == "JOINGAME")
          {
            connection.handle_JOINGAME(command);
          }
          else if (command[0] == "GAMEOPT")
          {
            connection.handle_GAMEOPT(command, lineBuffer);
          }
          else if (command[0] == "STARTG")
          {
            connection.handle_STARTG(command);
          }
          else if (command[0] == "PART")
          {
            connection.handle_PART(command);
          }
          else if (command[0] == "PRIVMSG")
          {
            connection.handle_PRIVMSG(command, lineBuffer);
          }
          else
          {
            release_assert(false);
          }
        }

        lineBuffer.clear();
      }
    }
  }

  std::scoped_lock lock(this->connectionsMutex);

  shutdown(connection.socket, 2);
  closesocket(connection.socket);


  for (int32_t i = 0; i < int32_t(connections.size()); i++)
  {
    // remove from any game channels this connection was in
    for (int32_t j = 0; j < int32_t(connections[i]->hostChannelMembers.size()); j++)
    {
      if (connections[i]->hostChannelMembers[j] == &connection)
        connections[i]->hostChannelMembers.erase(connections[i]->hostChannelMembers.begin() + j);;
    }

    // disconnect any clients that were connected to this channel TODO: notifiy them?
    if (connections[i]->connectedToChannel == &connection)
      connections[i]->connectedToChannel = nullptr;
  }

  // remove the connection
  for (int32_t i = 0; i < int32_t(connections.size()); i++)
  {
    if (connections[i].get() == &connection)
      connections.erase(connections.begin() + i);
  }
}

std::string WolServer::Connection::getIpIntString() const
{
  return std::to_string(ntohl(this->clientAddr.sin_addr.S_un.S_addr));
}

std::string WolServer::Connection::getIpString() const
{
  return inet_ntoa(this->clientAddr.sin_addr);
}

// all "example sessions" are based on wiresharking the game connecting to servserv.westwood.com,
// which at time of writing (2024) is a CNAME to xwis.net, a third party server that seems to have replaced
// the original westwood servers. The player "wheybags" is hosting, and "wheybags2" connects.
// I don't know what software the xwis server is running. It doesn't seem to be using PVPGN, as the
// responses are notably different from what I've seen in the PVPGN source code.

#define wolcommand_assert(X) release_assert(X)
//#define wolcommand_assert(X) if (!(X)) do { Log("wolcommand_assert failed: %s, %s:%d", #X, __FILE__, __LINE__); return; } while (0)


void WolServer::Connection::handle_verchk(const std::vector<std::string>& line)
{
  // example session (on servserv):
  // >>verchk 32512 65551
  // >>verchk 7936 1
  // **no response**

  // example session (on "irc"):
  // >>verchk 32512 720911
  // <<: 379 u :none none none 1 32512 NONREQ

  // I just ignore this for both servers. Pretty sure westwood isn't about to release a new version
}

void WolServer::Connection::handle_lobcount(const std::vector<std::string>& line)
{
  // example session:
  // >>lobcount 7936
  // <<: 610 u 1

  std::string_view response = ": 610 u 1\r\n";
  send(socket, response.data(), int(response.size()), 0);
}

void WolServer::Connection::handle_QUIT(const std::vector<std::string>& line)
{
  // example session:
  // >>QUIT
  // <<: 607
  std::string_view response = ": 607\r\n";
  send(socket, response.data(), int(response.size()), 0);

  this->connected = false;
}

void WolServer::Connection::handle_whereto(const std::vector<std::string>& line)
{
  // example session:
  // >>whereto tibsun tibpass99 7936 1
  // <<: 605 u :xwis.net 4000 '0:Emperor' -8 36.1083 -115.0582
  // <<: 608 u :xwis.net 4900 'Gameres server' -8 36.1083 -115.0582

  // reply with servserv.westwood.com, because we already have dns set up in the client to redirect it at this server.
  // Using the same port for servser and "chat" because it's more convenient to bundle servserv them both together.
  // xwis says it has a gameres server on port 4900, but attempts to connect to that port fail, so ¯\_(ツ)_/¯.
  // Client should be patched to never attempt connection anyway
  std::string response = ": 605 u :servserv.westwood.com " + std::to_string(wolPortH) + " '0:Emperor' - 8 36.1083 - 115.0582\r\n"
                         ": 608 u :servserv.westwood.com 4900 'Gameres server' -8 36.1083 -115.0582\r\n";


  send(socket, response.data(), int(response.size()), 0);
}

void WolServer::Connection::handle_CVERS(const std::vector<std::string>& line)
{
  // example session:
  // >>CVERS 11015 7936
  // **no response**

  // do nothing. This message tells the server what game the client is. Since we only support Emperor, we don't need to do anything.
}


// PASS, NICK, apgar, SERIAL and USER are all sent as one chunk as part of the login process.
// The server only responds after they're all sent.
// example session:
// >>CVERS 11015 7936
// >>PASS supersecret                                                // this is a hardcoded password
// >>NICK wheybags2
// >>apgar **omitted** 0                                             // **omitted** is where my real password hash was
// >>SERIAL
// >>USER UserName HostName irc.westwood.com :RealName
// <<: 375 u :- Welcome to XWIS!                                     // this is just a standard IRC MOTD
// <<: 372 u :-
// <<: 372 u :- 7 players (WC: 1, EBFD: 1, Nox: 5) are online
// <<: 376 u
//
// I don't care about user management or serials, so I'll just ignore it and accept whatever NICK the client requested.
// TODO: maybe I should complain about duplicate nicks?

void WolServer::Connection::handle_PASS(const std::vector<std::string>& line)
{
}

void WolServer::Connection::handle_NICK(const std::vector<std::string>& line)
{
  wolcommand_assert(line.size() == 2);
  this->nick = line[1];
}

void WolServer::Connection::handle_apgar(const std::vector<std::string>& line)
{
}

void WolServer::Connection::handle_SERIAL(const std::vector<std::string>& line)
{
}

void WolServer::Connection::handle_USER(const std::vector<std::string>& line)
{
  std::string_view response = ": 375 u :- blah blah motd\r\n"
                              ": 372 u :-\r\n"
                              ": 376 u\r\n";
  send(socket, response.data(), int(response.size()), 0);
}

void WolServer::Connection::handle_SETOPT(const std::vector<std::string>& line)
{
  // example session:
  // >>SETOPT 17,33
  // **no response**

  // This is used to set some options that I don't care about. I'll just ignore it.
}

void WolServer::Connection::handle_JOINGAME(const std::vector<std::string>& line)
{
  Connection* hostConnection = nullptr;

  if (line.size() == 9)  // hosting
  {
    // example session:
    // >>JOINGAME #wheybags 1 2 31 3 1 0 0
    // <<:wheybags!u@h JOINGAME 1 99 31 1 0 1458982652 0 :#wheybags
    // <<: 332 u #wheybags :                              // 332 = irc standard RPL_TOPIC
    // <<: 353 u = #wheybags :@wheybags,0,1458982652      // 353 = irc standard RPL_NAMREPLY, with some extra stuff (0 = ?, 1458982652 = ip address as int)
    // <<: 366 u #wheybags :                              // 366 = irc standard RPL_ENDOFNAMES


    // JOINGAME #wheybags 1 2 31 3 1 0 0
    //
    // 1.             2.            3.            4.        5.  6.  7.            8.
    // #wheybags      1             2             31        3   1   0             0
    // [channel name] [min players] [max players] [game ID] [?] [?] [Tournament?] [Extension? this is a string apparently]
    //
    // There can be a 9th parameter, which is a password, if present.


    wolcommand_assert(!this->hosting);

    // Not entirely sure how we're really supposed to handle these, so I'm just
    // asserting that the parameters are what I expect, then hardcoding the responses
    wolcommand_assert(line[1] == "#" + this->nick);
    wolcommand_assert(line[2] == "1");
    wolcommand_assert(line[3] == "2");
    wolcommand_assert(line[4] == "31");
    wolcommand_assert(line[5] == "3");
    wolcommand_assert(line[6] == "1");
    wolcommand_assert(line[7] == "0");
    wolcommand_assert(line[8] == "0");

    this->hosting = true;
    this->hostChannelName = line[1].substr(1);
    hostConnection = this;
  }
  else if (line.size() == 3) // connecting
  {
    // example session:
    // >>JOINGAME #wheybags 1
    // <<:wheybags2!u@h JOINGAME 1 99 31 1 0 1458982652 0 :#wheybags
    // <<: 332 u #wheybags :g1\x932\x03\x01wheybags
    // <<: 353 u = #wheybags :wheybags2,0,1458982652 @wheybags,0,1458982652
    // <<: 366 u #wheybags :
    // <<:xwis!u@h PAGE u :wheybags: #0 0 / 0 0p                              // apparently wol uses PAGE instead of PRIVMSG sometimes

    // This is also sent to the host (all clients as well?) when a client joins:
    // <<:wheybags2!u@h JOINGAME 1 99 31 1 0 1458982652 0 :#wheybags
    // <<:xwis!u@h PAGE #wheybags :wheybags2: #0 0 / 0 0p

    for (const auto& connection : server.connections)
    {
      if (connection->hosting && line[1] == "#" + connection->hostChannelName)
      {
        hostConnection = connection.get();
        break;
      }
    }
  }
  else
  {
    wolcommand_assert(false);
  }

  wolcommand_assert(hostConnection);

  hostConnection->hostChannelMembers.emplace_back(this);
  this->connectedToChannel = hostConnection;

  // send the joingame message to all clients in the channel
  for (int32_t i = 0; i < int32_t(hostConnection->hostChannelMembers.size()); i++)
  {
    std::string joingameMessage = ssprintf(":%s!u@h JOINGAME 1 99 31 1 0 %s 0 :#%s\r\n", this->nick.c_str(), this->getIpIntString().c_str(), hostConnection->hostChannelName.c_str());
    send(hostConnection->hostChannelMembers[i]->socket, joingameMessage.data(), int(joingameMessage.size()), 0);
  }

  // send topic + names to the new client
  {
    std::string joinMessages;
    joinMessages += ssprintf(": 332 u #%s :%s\r\n", hostConnection->hostChannelName.c_str(), hostConnection->hostChannelTopic.c_str()); // RPL_TOPIC
    joinMessages += ssprintf(": 353 u = #%s :", hostConnection->hostChannelName.c_str()); // RPL_NAMREPLY
    for (int32_t i = int32_t(hostConnection->hostChannelMembers.size()) - 1; i >= 0; i--)
    {
      std::string nameString = hostConnection->hostChannelMembers[i]->nick;
      if (hostConnection->hostChannelMembers[i] == hostConnection)
        nameString = "@" + nameString;

      joinMessages += ssprintf("%s,0,%s", nameString.c_str(), hostConnection->hostChannelMembers[i]->getIpIntString().c_str());
      if (i != 0)
        joinMessages += " ";
    }
    joinMessages += "\r\n";
    joinMessages += ssprintf(": 366 u #%s :\r\n", hostConnection->hostChannelName.c_str()); // RPL_ENDOFNAMES
    send(socket, joinMessages.data(), int(joinMessages.size()), 0);
  }

  // send PAGE messages to self re: others, and others re: self
  for (int32_t i = 0; i < int32_t(hostConnection->hostChannelMembers.size()); i++)
  {
    if (hostConnection->hostChannelMembers[i] != this)
    {
      std::string pageToOthers = ssprintf(":xwis!u@h PAGE #%s :%s: #0 0 / 0 0p\r\n", hostConnection->hostChannelName.c_str(), this->nick.c_str());
      send(hostConnection->hostChannelMembers[i]->socket, pageToOthers.data(), int(pageToOthers.size()), 0);

      std::string pageToMe = ssprintf(":xwis!u@h PAGE u :%s: #0 0 / 0 0p\r\n", hostConnection->hostChannelMembers[i]->nick.c_str());
      send(socket, pageToMe.data(), int(pageToMe.size()), 0);
    }
  }
}

void WolServer::Connection::handle_LIST(const std::vector<std::string>& line)
{
  // example session:
  // >>LIST -1 31                                                             // -1 indicates "include quick match rooms", we don't implement those. 31 is the game ID for Emperor
  // >>: 321 u:
  // >>: 327 u #Lob_31_0 0 0 388                                              // these three are default chat rooms
  // >>: 327 u #Lob_31_1 0 0 388
  // >>: 327 u #Lob_31_2 0 0 388
  // >>: 326 u #wheybags 1 0 31 0 0 1458982652 128::g1\x932\x03\x01wheybags   // this is a real game
  // >>: 323 u:

  // Breakdown of the example LIST response:
  // #wheybags 1 0 31 0 0 1458982652 128::g1\x932\x03\x01wheybags
  //
  // 1.             2.                              3.    4.          5.              6.
  // #wheybags      1                               0     31          0               0
  // [channel name] [number of players in channel]  [?]   [game ID]   [Tournament?]   [extension? this is a string]
  //
  // 7.                                8.
  // 1458982652                        128::g1\x932\x03\x01wheybags
  // [host ip address as 32 bit int]   [384: if game has password, 128: otherwise]:[Channel topic]


  std::string response = ": 321 u:\r\n"
                         ": 327 u #Lob_31_0 0 0 388\r\n"
                         ": 327 u #Lob_31_1 0 0 388\r\n"
                         ": 327 u #Lob_31_2 0 0 388\r\n";

  for (const auto& connection : server.connections)
  {
    if (connection->hosting)
    {
      response += ": 326 u #" + connection->hostChannelName + " " + std::to_string(connection->hostChannelMembers.size()) + " 0 31 0 0 " + connection->getIpIntString() + " 128::" + connection->hostChannelTopic + "\r\n";
    }
  }


  response += ": 323 u:\r\n";
  send(socket, response.data(), int(response.size()), 0);
}

void WolServer::Connection::handle_SETCODEPAGE(const std::vector<std::string>& line)
{
  // example session:
  // >>SETCODEPAGE 1252
  // <<: 329 u 1252

  wolcommand_assert(line.size() == 2);

  this->codepage = std::stoi(line[1]);

  std::string response = ": 329 u " + line[1] + "\r\n";
  send(socket, response.data(), int(response.size()), 0);
}

void WolServer::Connection::handle_GETCODEPAGE(const std::vector<std::string>& line)
{
  // example session:
  // >>GETCODEPAGE wheybags
  // <<: 328 u wheybags`1252

  wolcommand_assert(line.size() >= 2);

  std::string response = ": 328 u ";

  for (int32_t i = 1; i < int32_t(line.size()); i++)
  {
    const std::string& nick = line[i];
    Connection* connection = nullptr;

    for (int32_t i = 0; i < int32_t(server.connections.size()); i++)
    {
      if (server.connections[i]->nick == nick)
      {
        connection = server.connections[i].get();
        break;
      }
    }

    wolcommand_assert(connection);

    response += line[i] + "`" + std::to_string(connection->codepage);
    if (i != int32_t(line.size()) - 1)
      response += "`";
  }

  response += "\r\n";

  send(socket, response.data(), int(response.size()), 0);
}

void WolServer::Connection::handle_SETLOCALE(const std::vector<std::string>& line)
{
  // example session:
  // >>SETLOCALE 0
  // <<: 310 u 0

  wolcommand_assert(line.size() == 2);

  this->locale = std::stoi(line[1]);

  std::string response = ": 310 u " + line[1] + "\r\n";
  send(socket, response.data(), int(response.size()), 0);
}

void WolServer::Connection::handle_GETLOCALE(const std::vector<std::string>& line)
{
  // >>GETLOCALE wheybags
  // <<: 309 u wheybags`0

  wolcommand_assert(line.size() == 2);

  std::string response = ": 309 u " + line[1] + "`" + std::to_string(this->locale) + "\r\n";
  send(socket, response.data(), int(response.size()), 0);
}

void WolServer::Connection::handle_SQUADINFO(const std::vector<std::string>& line)
{
  // example session:
  // >>SQUADINFO 0        // get clan info, 0 means "my clan"
  // <<: 439

  std::string response = ": 439\r\n";
  send(socket, response.data(), int(response.size()), 0);
}

void WolServer::Connection::handle_GETINSIDER(const std::vector<std::string>& line)
{
  // example session:
  // >>GETINSIDER wheybags
  // <<: 399 u wheybags`0

  wolcommand_assert(line.size() == 2);

  std::string response = ": 399 u " + line[1] + "`0\r\n";
  send(socket, response.data(), int(response.size()), 0);
}

void WolServer::Connection::handle_TIME(const std::vector<std::string>& line)
{
  // example session:
  // >>TIME
  // <<: 391 u irc.westwood.com :1716318379

  wolcommand_assert(line.size() == 1);

  std::string response = ": 391 u irc.westwood.com :" + std::to_string(time(nullptr)) + "\r\n";
  send(socket, response.data(), int(response.size()), 0);
}

void WolServer::Connection::handle_GETBUDDY(const std::vector<std::string>& line)
{
  // example session:
  // >>GETBUDDY
  // <<: 333 u // trailing space

  wolcommand_assert(line.size() == 1);

  std::string response = ": 333 u \r\n";
  send(socket, response.data(), int(response.size()), 0);
}

void WolServer::Connection::handle_TOPIC(const std::vector<std::string>& line, const std::string& lineStr)
{
  // example session:
  // >>TOPIC #wheybags :g1\x932\x03\x01wheybags       // one trailing space at the end of this line
  // << **no response**

  wolcommand_assert(line.size() >= 3);
  wolcommand_assert(this->hosting);
  wolcommand_assert(line[1] == "#" + this->hostChannelName);

  this->hostChannelTopic = lineStr.substr(line[0].length() + 1 + line[1].length() + 1);
  if (this->hostChannelTopic.ends_with("\r\n"))
    this->hostChannelTopic.resize(this->hostChannelTopic.size() - 2);
  else if (this->hostChannelTopic.ends_with("\n"))
    this->hostChannelTopic.resize(this->hostChannelTopic.size() - 1);

  wolcommand_assert(!this->hostChannelTopic.empty() && this->hostChannelTopic[0] == ':');
  this->hostChannelTopic.erase(0, 1);
}

void WolServer::Connection::handle_GAMEOPT(const std::vector<std::string>& line, const std::string& lineStr)
{
  // example session:
  // >>GAMEOPT #wheybags **a pile of binary**
  // << **no response**

  // All other clients receive:
  // <<:wheybags!u@h GAMEOPT wheybags2 :**a pile of binary**

  wolcommand_assert(line.size() >= 3);
  wolcommand_assert(this->connectedToChannel);

  std::string data = lineStr.substr(line[0].length() + 1 + line[1].length() + 2);
  std::string forwardMessage = ":" + this->nick + "!u@h GAMEOPT " + line[1] + " :" + data;


  if (line[1].starts_with("#"))
  {
    for (int32_t i = 0; i < int32_t(this->connectedToChannel->hostChannelMembers.size()); i++)
    {
      if (this->connectedToChannel->hostChannelMembers[i] != this)
        send(this->connectedToChannel->hostChannelMembers[i]->socket, forwardMessage.data(), int(forwardMessage.size()), 0);
    }
  }
  else
  {
    for (int32_t i = 0; i < int32_t(this->connectedToChannel->hostChannelMembers.size()); i++)
    {
      if (line[1] == this->connectedToChannel->hostChannelMembers[i]->nick)
      {
        send(this->connectedToChannel->hostChannelMembers[i]->socket, forwardMessage.data(), int(forwardMessage.size()), 0);
        break;
      }
    }
  }
}

void WolServer::Connection::handle_STARTG(const std::vector<std::string>& line)
{
  // example session:
  // >>STARTG #wheybags wheybags, wheybags2
  // <<:wheybags!u@h STARTG u :wheybags 86.246.78.252 wheybags2 86.246.78.252 :1644425801 1716237521    // sent to all clients

  wolcommand_assert(this->hosting);
  wolcommand_assert(line[1] == "#" + this->hostChannelName);

  std::string response = ":" + this->nick + "!u@h STARTG u :";
  for (int32_t i = 0; i < int32_t(this->hostChannelMembers.size()); i++)
    response += this->hostChannelMembers[i]->nick + " " + this->hostChannelMembers[i]->getIpString() + " ";

  response += ":" + std::to_string(this->gameId) + " " + std::to_string(time(nullptr)) + "\r\n";

  for (int32_t i = 0; i < int32_t(this->hostChannelMembers.size()); i++)
    send(this->hostChannelMembers[i]->socket, response.data(), int(response.size()), 0);
}

void WolServer::Connection::handle_PART(const std::vector<std::string>& line)
{
  // example session:
  // >>PART #wheybags
  // <<:wheybags!u@h PART #wheybags
  // other clients receive the same message. if the host leaves, all clients get PART messages for themselves as well

  wolcommand_assert(line.size() == 2);

  if (!this->connectedToChannel)
    return;

  wolcommand_assert(line[1] == "#" + this->connectedToChannel->hostChannelName);

  std::string iLeaveMessage = ":" + this->nick + "!u@h PART #" + this->connectedToChannel->hostChannelName + "\r\n";

  for (int32_t i = 0; i < int32_t(this->hostChannelMembers.size()); i++)
  {
    send(this->hostChannelMembers[i]->socket, iLeaveMessage.data(), int(iLeaveMessage.size()), 0);

    if (this->hostChannelMembers[i] != this && this->hosting)
    {
      std::string message = ":" + this->hostChannelMembers[i]->nick + "!u@h PART #" + this->hostChannelMembers[i]->connectedToChannel->hostChannelName + "\r\n";
      send(this->hostChannelMembers[i]->socket, message.data(), int(message.size()), 0);
    }
  }

  if (this->hosting)
  {
    for (int32_t i = 0; i < int32_t(this->hostChannelMembers.size()); i++)
      this->hostChannelMembers[i]->connectedToChannel = nullptr;

    this->hosting = false;
    this->hostChannelName.clear();
    this->hostChannelTopic.clear();
    this->hostChannelMembers.clear();
  }
  else
  {
    this->connectedToChannel = nullptr;
  }
}

void WolServer::Connection::handle_PRIVMSG(const std::vector<std::string>& line, const std::string& lineStr)
{
  // example session:
  // >>PRIVMSG #wheybags :asasdasd
  // other clients receive:
  // <<:wheybags!u@h PRIVMSG #wheybags :asasdasd

  // can also PRIVMSG a user directly instead of a channel

  wolcommand_assert(line.size() >= 3);
  wolcommand_assert(line[2].starts_with(":"));

  std::string data = lineStr.substr(line[0].length() + 1 + line[1].length() + 2);

  if (line[1].starts_with("#"))
  {
    wolcommand_assert(this->connectedToChannel);
    wolcommand_assert(line[1] == "#" + this->connectedToChannel->hostChannelName);

    std::string message = ":" + this->nick + "!u@h PRIVMSG #" + this->connectedToChannel->hostChannelName + " :" + data + "\r\n";

    for (int32_t i = 0; i < int32_t(this->connectedToChannel->hostChannelMembers.size()); i++)
    {
      if (this->connectedToChannel->hostChannelMembers[i] != this)
        send(this->connectedToChannel->hostChannelMembers[i]->socket, message.data(), int(message.size()), 0);
    }
  }
  else
  {
    Connection* target = nullptr;

    for (int32_t i = 0; i < int32_t(server.connections.size()); i++)
    {
      if (server.connections[i]->nick == line[1])
      {
        target = server.connections[i].get();
        break;
      }
    }

    wolcommand_assert(target);

    std::string message = ":" + this->nick + "!u@h PRIVMSG " + target->nick + " :" + data + "\r\n";
    send(target->socket, message.data(), int(message.size()), 0);
  }
}
