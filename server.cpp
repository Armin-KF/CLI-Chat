#include <iostream>
#include <unordered_map>
#include <set>
#include <memory>
#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <thread>
#include <vector>
#include <mutex>

using namespace std;
using boost::asio::ip::tcp;

unordered_map<string, shared_ptr<tcp::socket>> user_sockets;
set<string> online_users;
mutex mtx;

void log_event(const string &event)
{
    cout << "[LOG] " << event << endl;
}

void broadcast_message(const string &message, shared_ptr<tcp::socket> exclude_socket = nullptr)
{
    lock_guard<mutex> lock(mtx);
    for (const auto &[username, client_socket] : user_sockets)
    {
        if (client_socket != exclude_socket)
        {
            boost::asio::async_write(*client_socket, boost::asio::buffer(message),
                                     [](boost::system::error_code, std::size_t) {});
        }
    }
}

void handle_client(shared_ptr<tcp::socket> socket)
{
    string username;
    try
    {
        log_event("Client connected");

        while (true)
        {
            boost::asio::streambuf buffer;
            boost::asio::read_until(*socket, buffer, "\n");
            istream input(&buffer);
            string message;
            getline(input, message);

            if (message.rfind("register:", 0) == 0)
            {
                username = message.substr(9);
                {
                    lock_guard<mutex> lock(mtx);
                    online_users.insert(username);
                    user_sockets[username] = socket;
                }

                log_event("User registered: " + username);

                string user_list = "users:";
                {
                    lock_guard<mutex> lock(mtx);
                    for (const auto &user : online_users)
                    {
                        user_list += user + ",";
                    }
                }
                if (!user_list.empty())
                {
                    user_list.pop_back();
                }
                user_list += "\n";

                boost::asio::write(*socket, boost::asio::buffer(user_list));
                broadcast_message(user_list, socket);
            }
            else if (message.rfind("@", 0) == 0)
            {
                size_t pos = message.find(':');
                if (pos != string::npos)
                {
                    string recipient = message.substr(1, pos - 1);
                    string direct_message = message.substr(pos + 1);

                    log_event(username + " sent direct message to " + recipient + ": " + direct_message);

                    lock_guard<mutex> lock(mtx);
                    if (user_sockets.find(recipient) != user_sockets.end())
                    {
                        auto recipient_socket = user_sockets[recipient];
                        boost::asio::async_write(*recipient_socket, boost::asio::buffer(username + ": " + direct_message + "\n"),
                                                 [](boost::system::error_code, std::size_t) {});
                    }
                }
            }
            else
            {
                log_event(username + " broadcasted message: " + message);
                broadcast_message(username + ": " + message + "\n", socket);
            }
        }
    }
    catch (const boost::system::system_error &e)
    {
        if (e.code() == boost::asio::error::eof || e.code() == boost::asio::error::connection_reset)
        {
            cerr << "Client disconnected: " << e.what() << "\n";
        }
        else
        {
            cerr << "Exception: " << e.what() << "\n";
        }
    }
    catch (const std::exception &e)
    {
        cerr << "Exception: " << e.what() << "\n";
    }

    // Remove user from online_users and user_sockets
    if (!username.empty())
    {
        lock_guard<mutex> lock(mtx);
        online_users.erase(username);
        user_sockets.erase(username);
        log_event("User disconnected: " + username);
    }

    // Notify all clients about the updated user list
    string user_list = "users:";
    {
        lock_guard<mutex> lock(mtx);
        for (const auto &user : online_users)
        {
            user_list += user + ",";
        }
    }
    if (!user_list.empty())
    {
        user_list.pop_back();
    }
    user_list += "\n";
    broadcast_message(user_list);
}

int main()
{
    try
    {
        boost::asio::io_context io_context;
        tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), 4000));
        cout << "Server is running on port 4000" << endl;
        log_event("Server started");

        while (true)
        {
            auto socket = make_shared<tcp::socket>(io_context);
            acceptor.accept(*socket);
            thread(handle_client, socket).detach();
        }
    }
    catch (std::exception &e)
    {
        cerr << "Exception: " << e.what() << "\n";
        log_event("Server exception: " + string(e.what()));
    }

    return 0;
}
