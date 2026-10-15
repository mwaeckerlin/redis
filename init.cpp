/**

Redis init: minimal, shell-free entrypoint for the redis container.

Writes /run/redis/redis.conf from env, then execv's redis-server. Same
three-stage / scratch / execvp() pattern as the sibling mwaeckerlin
service containers. The runtime image contains no shell, no perl, no
busybox — only redis-server, its shared libraries, and this init.

Behaviour:

  1. Read the handful of env knobs (bind, port, password, maxmemory,
     appendonly, save, eviction policy) and refuse any value its
     directive does not accept, with `invalid <VAR>`.
  2. Compose /run/redis/redis.conf. Persistence defaults to appendonly
     (writes an AOF file to /data), so that a container restart does
     not lose Bayes / greylist / ratelimit state.
  3. execv redis-server -c /run/redis/redis.conf --dir /data.

Supports --healthcheck: sends AUTH (when REDIS_PASSWORD is set) and
PING to the configured port on 127.0.0.1 and exits 0 only on +PONG.
The image's HEALTHCHECK runs it, so no redis-cli is needed.

*/

#include <arpa/inet.h>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <fstream>
#include <iostream>
#include <netinet/in.h>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <vector>

namespace {

constexpr const char *REDIS_SERVER = "/usr/bin/redis-server";
constexpr const char *CONF_PATH    = "/run/redis/redis.conf";

std::string
env_or(const char *name, const std::string &fallback = {}) {
  const char *v = std::getenv(name);
  return (v && *v) ? std::string(v) : fallback;
}

// Every value is written into redis.conf, one directive per line. A newline
// or a quote would let a value add directives of its own, so each value is
// checked against what its directive accepts before anything is written.
std::vector<std::string>
words(const std::string &value) {
  std::vector<std::string> result;
  std::string word;
  for (char c : value) {
    if (c == ' ') {
      if (word.empty()) return {};  // leading or double space
      result.push_back(word);
      word.clear();
    } else {
      word += c;
    }
  }
  if (word.empty()) return {};      // trailing space or empty value
  result.push_back(word);
  return result;
}

bool
all_of(const std::string &value, const char *allowed) {
  return !value.empty() && value.find_first_not_of(allowed) == std::string::npos;
}

constexpr const char *DIGITS = "0123456789";

bool
valid_password(const std::string &v) {
  if (v.empty()) return true;
  for (unsigned char c : v)
    if (c <= ' ' || c >= 0x7f || c == '"' || c == '\'' || c == '\\') return false;
  return true;
}

bool
valid_bind(const std::string &v) {
  const auto list = words(v);
  if (list.empty()) return false;
  for (const auto &address : list)
    if (!all_of(address, "0123456789abcdefABCDEF:.-*")) return false;
  return true;
}

bool
valid_port(const std::string &v) {
  if (!all_of(v, DIGITS) || v.size() > 5) return false;
  const int port = std::stoi(v);
  return port >= 1 && port <= 65535;
}

bool
valid_save(const std::string &v) {
  if (v == "\"\"") return true;     // save "" switches snapshots off
  const auto list = words(v);
  if (list.empty() || list.size() % 2) return false;
  for (const auto &number : list)
    if (!all_of(number, DIGITS)) return false;
  return true;
}

bool
valid_memory(const std::string &v) {
  if (v.empty()) return true;
  const auto unit = v.find_first_not_of(DIGITS);
  if (unit == 0) return false;
  if (unit == std::string::npos) return true;
  std::string suffix = v.substr(unit);
  for (auto &c : suffix) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  for (const char *allowed : {"k", "kb", "m", "mb", "g", "gb"})
    if (suffix == allowed) return true;
  return false;
}

bool
valid_policy(const std::string &v) {
  for (const char *allowed : {"volatile-lru", "allkeys-lru", "volatile-lfu", "allkeys-lfu",
                              "volatile-random", "allkeys-random", "volatile-ttl", "noeviction"})
    if (v == allowed) return true;
  return false;
}

void
check(const char *name, const std::string &value, bool (*valid)(const std::string &)) {
  if (!valid(value))
    throw std::runtime_error(std::string("invalid ") + name + ": the value cannot be used in redis.conf");
}

std::string
resp(const std::vector<std::string> &command) {
  std::string out = "*" + std::to_string(command.size()) + "\r\n";
  for (const auto &arg : command) out += "$" + std::to_string(arg.size()) + "\r\n" + arg + "\r\n";
  return out;
}

// A TCP connect only proves that the socket accepts: a server still loading
// its AOF, or a password that no longer matches, accept it as well. The check
// therefore sends AUTH (when a password is set) and PING, and passes only on
// the reply +PONG.
int
ping_probe(int port, const std::string &password) {
  int s = socket(AF_INET, SOCK_STREAM, 0);
  if (s < 0) return 1;
  timeval timeout{3, 0};
  setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);
  setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof timeout);
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
  if (connect(s, reinterpret_cast<sockaddr *>(&addr), sizeof addr) != 0) {
    close(s);
    return 1;
  }
  std::string request = password.empty() ? std::string() : resp({"AUTH", password});
  request += resp({"PING"});
  const std::size_t replies = password.empty() ? 1 : 2;
  if (send(s, request.data(), request.size(), MSG_NOSIGNAL) != static_cast<ssize_t>(request.size())) {
    close(s);
    return 1;
  }
  std::string answer;
  char buffer[256];
  std::size_t lines = 0;
  while (lines < replies) {
    const ssize_t n = recv(s, buffer, sizeof buffer, 0);
    if (n <= 0) break;
    answer.append(buffer, static_cast<std::size_t>(n));
    lines = 0;
    for (std::size_t at = answer.find("\r\n"); at != std::string::npos; at = answer.find("\r\n", at + 2)) ++lines;
  }
  close(s);
  const std::string pong = "+PONG\r\n";
  if (answer.size() < pong.size() || answer.compare(answer.size() - pong.size(), pong.size(), pong) != 0) {
    std::cerr << "health check: no PONG from redis on port " << port << std::endl;
    return 1;
  }
  return 0;
}

} // namespace

int main(int argc, char *argv[]) try {
  const std::string port_value  = env_or("REDIS_PORT",         "6379");
  const std::string bind        = env_or("REDIS_BIND",         "0.0.0.0");
  const std::string password    = env_or("REDIS_PASSWORD",     "");
  const std::string maxmemory   = env_or("REDIS_MAXMEMORY",    "");
  const std::string appendonly  = env_or("REDIS_APPENDONLY",   "yes");
  const std::string save_policy = env_or("REDIS_SAVE",         "3600 1 300 100 60 10000");
  const std::string policy      = env_or("REDIS_MAXMEMORY_POLICY", "allkeys-lfu");

  // validated before the health check as well, so a malformed value shows
  // up in the health state and can be tested without starting the server
  check("REDIS_PORT", port_value, valid_port);
  check("REDIS_BIND", bind, valid_bind);
  check("REDIS_PASSWORD", password, valid_password);
  check("REDIS_MAXMEMORY", maxmemory, valid_memory);
  check("REDIS_APPENDONLY", appendonly, [](const std::string &v) { return v == "yes" || v == "no"; });
  check("REDIS_SAVE", save_policy, valid_save);
  check("REDIS_MAXMEMORY_POLICY", policy, valid_policy);
  const int port = std::stoi(port_value);

  if (argc > 1 && std::string(argv[1]) == "--healthcheck")
    return ping_probe(port, password);

  std::string conf;
  conf += "bind " + bind + "\n";
  conf += "port " + std::to_string(port) + "\n";
  conf += "protected-mode no\n";                       // reachable from other containers
  conf += "dir /data\n";
  conf += "appendonly " + appendonly + "\n";
  conf += "appendfsync everysec\n";
  conf += "save " + save_policy + "\n";
  if (!password.empty()) conf += "requirepass " + password + "\n";
  if (!maxmemory.empty()) conf += "maxmemory " + maxmemory + "\n";
  // Always configured (the image ENV promises this default), even
  // without a maxmemory limit — the policy only takes effect once a
  // limit exists, which an operator may add at runtime via CONFIG SET.
  // LFU evicts least-frequently-used keys under memory pressure, a
  // safe choice for Bayes / greylist / ratelimit state.
  conf += "maxmemory-policy " + policy + "\n";

  std::ofstream out(CONF_PATH, std::ios::binary | std::ios::trunc);
  if (!out) throw std::runtime_error(std::string("cannot write ") + CONF_PATH);
  out << conf;
  out.close();

  std::cerr << "**** Starting redis-server on " << bind << ":" << port
            << (appendonly == "yes" ? " (appendonly on)" : " (RDB only)")
            << std::endl;

  const char *exec_argv[] = {"redis-server", CONF_PATH, nullptr};
  execv(REDIS_SERVER, const_cast<char *const *>(exec_argv));
  std::perror(REDIS_SERVER);
  return 1;
} catch (const std::exception &e) {
  std::cerr << "EXCEPTION: " << e.what() << std::endl;
  return 1;
} catch (...) {
  std::cerr << "UNKNOWN ERROR" << std::endl;
  return 1;
}
