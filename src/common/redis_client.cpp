#include "redis_client.hpp"
#include <iostream>
#include <cstdarg>
#include <chrono>
#include <thread>

namespace chat {

RedisClient::RedisClient(const std::string& host, int port, const std::string& password)
    : host_(host), port_(port), password_(password), context_(nullptr), sub_context_(nullptr),
      connected_(false), subscribing_(false) {
}

RedisClient::~RedisClient() {
    disconnect();
}

bool RedisClient::connect() {
    std::lock_guard<std::mutex> lock(context_mutex_);

    // Main context for commands
    context_ = redisConnect(host_.c_str(), port_);
    if (context_ == nullptr || context_->err) {
        if (context_) {
            std::cerr << "[ERROR] Redis connection error: " << context_->errstr << std::endl;
            redisFree(context_);
            context_ = nullptr;
        } else {
            std::cerr << "[ERROR] Redis connection error: Can't allocate redis context" << std::endl;
        }
        return false;
    }

    // Authenticate if password provided
    if (!password_.empty()) {
        redisReply* reply = (redisReply*)redisCommand(context_, "AUTH %s", password_.c_str());
        if (reply == nullptr || reply->type == REDIS_REPLY_ERROR) {
            std::cerr << "[ERROR] Redis authentication failed" << std::endl;
            if (reply) freeReplyObject(reply);
            redisFree(context_);
            context_ = nullptr;
            return false;
        }
        freeReplyObject(reply);
    }

    // Subscription context
    sub_context_ = redisConnect(host_.c_str(), port_);
    if (sub_context_ == nullptr || sub_context_->err) {
        std::cerr << "[ERROR] Redis subscription context error" << std::endl;
        redisFree(context_);
        context_ = nullptr;
        if (sub_context_) {
            redisFree(sub_context_);
            sub_context_ = nullptr;
        }
        return false;
    }

    // Authenticate subscription context
    if (!password_.empty()) {
        redisReply* reply = (redisReply*)redisCommand(sub_context_, "AUTH %s", password_.c_str());
        if (reply == nullptr || reply->type == REDIS_REPLY_ERROR) {
            std::cerr << "[ERROR] Redis subscription authentication failed" << std::endl;
            if (reply) freeReplyObject(reply);
            redisFree(context_);
            redisFree(sub_context_);
            context_ = nullptr;
            sub_context_ = nullptr;
            return false;
        }
        freeReplyObject(reply);
    }

    connected_ = true;
    std::cout << "[INFO] Connected to Redis at " << host_ << ":" << port_ << std::endl;
    return true;
}

void RedisClient::disconnect() {
    connected_ = false;
    subscribing_ = false;

    if (subscription_thread_.joinable()) {
        subscription_thread_.join();
    }

    std::lock_guard<std::mutex> lock(context_mutex_);
    if (context_) {
        redisFree(context_);
        context_ = nullptr;
    }

    if (sub_context_) {
        redisFree(sub_context_);
        sub_context_ = nullptr;
    }
}

bool RedisClient::set(const std::string& key, const std::string& value, int ttl) {
    if (!connected_) return false;

    redisReply* reply;
    if (ttl > 0) {
        reply = executeRedisCommand("SETEX %s %d %s", key.c_str(), ttl, value.c_str());
    } else {
        reply = executeRedisCommand("SET %s %s", key.c_str(), value.c_str());
    }

    if (reply == nullptr || reply->type == REDIS_REPLY_ERROR) {
        if (reply) freeReplyObject(reply);
        return false;
    }

    freeReplyObject(reply);
    return true;
}

std::string RedisClient::get(const std::string& key) {
    if (!connected_) return "";

    redisReply* reply = executeRedisCommand("GET %s", key.c_str());
    if (reply == nullptr || reply->type != REDIS_REPLY_STRING) {
        if (reply) freeReplyObject(reply);
        return "";
    }

    std::string result(reply->str, reply->len);
    freeReplyObject(reply);
    return result;
}

bool RedisClient::del(const std::string& key) {
    if (!connected_) return false;

    redisReply* reply = executeRedisCommand("DEL %s", key.c_str());
    if (reply == nullptr || reply->type == REDIS_REPLY_ERROR) {
        if (reply) freeReplyObject(reply);
        return false;
    }

    bool result = reply->integer > 0;
    freeReplyObject(reply);
    return result;
}

bool RedisClient::sadd(const std::string& key, const std::string& member) {
    if (!connected_) return false;

    redisReply* reply = executeRedisCommand("SADD %s %s", key.c_str(), member.c_str());
    if (reply == nullptr || reply->type == REDIS_REPLY_ERROR) {
        if (reply) freeReplyObject(reply);
        return false;
    }

    bool result = reply->integer > 0;
    freeReplyObject(reply);
    return result;
}

bool RedisClient::srem(const std::string& key, const std::string& member) {
    if (!connected_) return false;

    redisReply* reply = executeRedisCommand("SREM %s %s", key.c_str(), member.c_str());
    if (reply == nullptr || reply->type == REDIS_REPLY_ERROR) {
        if (reply) freeReplyObject(reply);
        return false;
    }

    bool result = reply->integer > 0;
    freeReplyObject(reply);
    return result;
}

std::vector<std::string> RedisClient::smembers(const std::string& key) {
    std::vector<std::string> members;
    if (!connected_) return members;

    redisReply* reply = executeRedisCommand("SMEMBERS %s", key.c_str());
    if (reply == nullptr || reply->type != REDIS_REPLY_ARRAY) {
        if (reply) freeReplyObject(reply);
        return members;
    }

    for (size_t i = 0; i < reply->elements; i++) {
        if (reply->element[i]->type == REDIS_REPLY_STRING) {
            members.emplace_back(reply->element[i]->str, reply->element[i]->len);
        }
    }

    freeReplyObject(reply);
    return members;
}

bool RedisClient::publish(const std::string& channel, const std::string& message) {
    if (!connected_) return false;

    redisReply* reply = executeRedisCommand("PUBLISH %s %s", channel.c_str(), message.c_str());
    if (reply == nullptr || reply->type == REDIS_REPLY_ERROR) {
        if (reply) freeReplyObject(reply);
        return false;
    }

    freeReplyObject(reply);
    return true;
}

void RedisClient::subscribe(const std::string& channel, std::function<void(const std::string&, const std::string&)> callback) {
    if (!connected_) return;

    message_callback_ = callback;
    subscribing_ = true;

    subscription_thread_ = std::thread(&RedisClient::subscriptionWorker, this);

    // Subscribe to channel
    std::lock_guard<std::mutex> lock(sub_context_mutex_);
    redisCommand(sub_context_, "SUBSCRIBE %s", channel.c_str());
}

void RedisClient::subscriptionWorker() {
    while (subscribing_ && connected_) {
        std::lock_guard<std::mutex> lock(sub_context_mutex_);
        redisReply* reply = nullptr;

        if (redisGetReply(sub_context_, (void**)&reply) == REDIS_OK && reply) {
            if (reply->type == REDIS_REPLY_ARRAY && reply->elements == 3) {
                if (reply->element[0]->type == REDIS_REPLY_STRING &&
                    std::string(reply->element[0]->str) == "message") {

                    std::string channel(reply->element[1]->str, reply->element[1]->len);
                    std::string message(reply->element[2]->str, reply->element[2]->len);

                    if (message_callback_) {
                        message_callback_(channel, message);
                    }
                }
            }
            freeReplyObject(reply);
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

bool RedisClient::registerServer(const std::string& server_id, const std::string& host, int port) {
    std::string server_info = host + ":" + std::to_string(port);
    return sadd("chat:servers", server_id) &&
           set("chat:server:" + server_id, server_info, 30); // 30 second TTL
}

bool RedisClient::unregisterServer(const std::string& server_id) {
    return srem("chat:servers", server_id) &&
           del("chat:server:" + server_id);
}

bool RedisClient::setUserServer(const std::string& username, const std::string& server_id) {
    return set("chat:user_server:" + username, server_id, 3600); // 1 hour TTL
}

std::string RedisClient::getUserServer(const std::string& username) {
    return get("chat:user_server:" + username);
}

bool RedisClient::removeUserServer(const std::string& username) {
    return del("chat:user_server:" + username);
}

bool RedisClient::broadcastMessage(const std::string& message, const std::string& sender_server) {
    return publish("chat:broadcast", sender_server + "|" + message);
}

bool RedisClient::sendPrivateMessage(const std::string& recipient, const std::string& message, const std::string& sender) {
    std::string pm_data = sender + "|" + recipient + "|" + message;
    return publish("chat:private", pm_data);
}

redisReply* RedisClient::executeRedisCommand(const char* format, ...) {
    if (!connected_ || !context_) return nullptr;

    std::lock_guard<std::mutex> lock(context_mutex_);

    va_list args;
    va_start(args, format);
    redisReply* reply = (redisReply*)redisvCommand(context_, format, args);
    va_end(args);

    return reply;
}

} // namespace chat