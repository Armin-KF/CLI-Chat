#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <hiredis/hiredis.h>
#include <thread>
#include <atomic>
#include <mutex>

namespace chat {

class RedisClient {
public:
    RedisClient(const std::string& host = "127.0.0.1", int port = 6379, const std::string& password = "");
    ~RedisClient();

    bool connect();
    void disconnect();
    bool isConnected() const { return connected_; }

    // Basic Redis operations
    bool set(const std::string& key, const std::string& value, int ttl = 0);
    std::string get(const std::string& key);
    bool del(const std::string& key);
    bool exists(const std::string& key);

    // List operations
    bool lpush(const std::string& key, const std::string& value);
    bool rpush(const std::string& key, const std::string& value);
    std::string lpop(const std::string& key);
    std::string rpop(const std::string& key);
    std::vector<std::string> lrange(const std::string& key, int start, int stop);

    // Set operations
    bool sadd(const std::string& key, const std::string& member);
    bool srem(const std::string& key, const std::string& member);
    std::vector<std::string> smembers(const std::string& key);
    bool sismember(const std::string& key, const std::string& member);

    // Hash operations
    bool hset(const std::string& key, const std::string& field, const std::string& value);
    std::string hget(const std::string& key, const std::string& field);
    bool hdel(const std::string& key, const std::string& field);
    std::vector<std::pair<std::string, std::string>> hgetall(const std::string& key);

    // Pub/Sub operations
    void subscribe(const std::string& channel, std::function<void(const std::string&, const std::string&)> callback);
    void unsubscribe(const std::string& channel);
    bool publish(const std::string& channel, const std::string& message);

    // Server instance management
    bool registerServer(const std::string& server_id, const std::string& host, int port);
    bool unregisterServer(const std::string& server_id);
    std::vector<std::pair<std::string, std::string>> getServerList();

    // User session distribution
    bool setUserServer(const std::string& username, const std::string& server_id);
    std::string getUserServer(const std::string& username);
    bool removeUserServer(const std::string& username);

    // Message broadcasting
    bool broadcastMessage(const std::string& message, const std::string& sender_server = "");
    bool sendPrivateMessage(const std::string& recipient, const std::string& message, const std::string& sender);

private:
    void subscriptionWorker();
    bool executeCommand(const std::string& command, std::vector<std::string>& result);
    redisReply* executeRedisCommand(const char* format, ...);

    std::string host_;
    int port_;
    std::string password_;
    redisContext* context_;
    redisContext* sub_context_;
    std::atomic<bool> connected_;
    std::atomic<bool> subscribing_;

    std::thread subscription_thread_;
    std::mutex context_mutex_;
    std::mutex sub_context_mutex_;

    std::function<void(const std::string&, const std::string&)> message_callback_;
};

} // namespace chat