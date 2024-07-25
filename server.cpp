#include <iostream>
#include <set>
#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>

using namespace std;
using boost::asio::ip::tcp;

set<string> online_users;

void handle_client(tcp::socket socket)
{
    try
    {
        boost::asio::streambuf buffer;
        boost::asio::read_until(socket, buffer, "\n");
        istream input(&buffer);
        string message;
        getline(input, message);

        if (message.rfind("register:", 0) == 0)
        {
            string username = message.substr(9);
            online_users.insert(username);

            string user_list = "users:";
            for (const auto &user : online_users)
            {
                user_list += user + ",";
            }
            user_list.pop_back();
            user_list += "\n";

            boost::asio::write(socket, boost::asio::buffer(user_list));
        }
    }
    catch (const std::exception &e)
    {
        cerr << "Exception: " << e.what() << "\n";
    }
}

int main()
{
    try
    {
        boost::asio::io_context io_context;
        tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), 4000));
        cout << "Server is running on port 4000" << endl;

        while (true)
        {
            tcp::socket socket(io_context);
            acceptor.accept(socket);
            handle_client(move(socket));
        }
    }
    catch (std::exception &e)
    {
        cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}