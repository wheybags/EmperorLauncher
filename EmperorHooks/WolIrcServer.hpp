#pragma once
#include <windows.h>
#include <winsock.h>
#include <string>
#include <vector>
#include <memory>
#include <mutex>

class WolIrcServer
{
public:
  void run();

private:

  class Connection
  {
  public:
    Connection(WolIrcServer& server, SOCKET socket, sockaddr_in clientAddr)
      : server(server), socket(socket), clientAddr(clientAddr), gameId(server.nextGameId++)
    {}

    std::string getIpIntString() const;
    std::string getIpString() const;

    // servserv
    void handle_verchk(const std::vector<std::string>& line); // also on "chat"
    void handle_lobcount(const std::vector<std::string>& line);
    void handle_QUIT(const std::vector<std::string>& line);
    void handle_whereto(const std::vector<std::string>& line);

    // "chat"
    void handle_CVERS(const std::vector<std::string>& line);
    void handle_PASS(const std::vector<std::string>& line);
    void handle_NICK(const std::vector<std::string>& line);
    void handle_apgar(const std::vector<std::string>& line);
    void handle_SERIAL(const std::vector<std::string>& line);
    void handle_USER(const std::vector<std::string>& line);
    void handle_SETOPT(const std::vector<std::string>& line);
    void handle_JOINGAME(const std::vector<std::string>& line);
    void handle_LIST(const std::vector<std::string>& line);
    void handle_SETCODEPAGE(const std::vector<std::string>& line);
    void handle_GETCODEPAGE(const std::vector<std::string>& line);
    void handle_SETLOCALE(const std::vector<std::string>& line);
    void handle_GETLOCALE(const std::vector<std::string>& line);
    void handle_SQUADINFO(const std::vector<std::string>& line);
    void handle_GETINSIDER(const std::vector<std::string>& line);
    void handle_TIME(const std::vector<std::string>& line);
    void handle_GETBUDDY(const std::vector<std::string>& line);
    void handle_TOPIC(const std::vector<std::string>& line, const std::string& lineStr);
    void handle_GAMEOPT(const std::vector<std::string>& line, const std::string& lineStr);
    void handle_STARTG(const std::vector<std::string>& line);
    void handle_PART(const std::vector<std::string>& line);
    void handle_PRIVMSG(const std::vector<std::string>& line, const std::string& lineStr);

  public:
    std::string nick;
    int codepage = 0;
    int locale = 0;

    bool hosting = false;
    std::string hostChannelName;
    std::string hostChannelTopic;
    std::vector<Connection*> hostChannelMembers;

    Connection* connectedToChannel = nullptr;

    WolIrcServer& server;
    SOCKET socket = INVALID_SOCKET;
    bool connected = true;
    sockaddr_in clientAddr = {};

    int32_t gameId = 0;
  };

  friend class Connection;


private:
  void clientLoop(Connection& connection);

private:
  std::vector<std::unique_ptr<Connection>> connections;
  std::mutex connectionsMutex;
  int32_t nextGameId = 1;
};
