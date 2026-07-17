/**

Redis init: minimal, shell-free entrypoint for the redis container.

Writes /run/redis/redis.conf from env, then execv's redis-server. Same
three-stage / scratch / execvp() pattern as the sibling mwaeckerlin
service containers. The runtime image contains no shell, no perl, no
busybox — only redis-server, its shared libraries, and this init.

Behaviour:

  1. Read the handful of env knobs (bind, port, password, maxmemory,
     appendonly).
  2. Compose /run/redis/redis.conf. Persistence defaults to appendonly
     (writes an AOF file to /data), so that a container restart does
     not lose Bayes / greylist / ratelimit state.
  3. execv redis-server -c /run/redis/redis.conf --dir /data.

Supports --healthcheck: TCP-probes the configured port on 127.0.0.1.
No redis-cli or auth needed — a successful TCP connect proves the
server accepted our socket.

*/

#include <arpa/inet.h>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <fstream>
#include <iostream>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
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

int
tcp_probe(int port) {
  int s = socket(AF_INET, SOCK_STREAM, 0);
  if (s < 0) return 1;
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
  int rc = connect(s, reinterpret_cast<sockaddr *>(&addr), sizeof addr);
  close(s);
  return rc == 0 ? 0 : 1;
}

} // namespace

int main(int argc, char *argv[]) try {
  const int port = std::atoi(env_or("REDIS_PORT", "6379").c_str());

  if (argc > 1 && std::string(argv[1]) == "--healthcheck")
    return tcp_probe(port);

  const std::string bind        = env_or("REDIS_BIND",         "0.0.0.0");
  const std::string password    = env_or("REDIS_PASSWORD",     "");
  const std::string maxmemory   = env_or("REDIS_MAXMEMORY",    "");
  const std::string appendonly  = env_or("REDIS_APPENDONLY",   "yes");
  const std::string save_policy = env_or("REDIS_SAVE",         "3600 1 300 100 60 10000");

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
  conf += "maxmemory-policy " + env_or("REDIS_MAXMEMORY_POLICY", "allkeys-lfu") + "\n";

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
