#include <iostream>
#include <vector>
#include <boost/asio.hpp>

using namespace std;
using boost::asio::ip::tcp;

const string RESET = "\033[0m";
const string RED = "\033[31m";
const string GREEN = "\033[32m";
const string YELLOW = "\033[33m";
const string BLUE = "\033[34m";
const string GREEN_CIRCLE = "\033[32m\u25CF\033[0m";

void terminal(boost::asio::io_context &io_context, const string &server, const string &port)
{
    try
    {
        tcp::resolver resolver(io_context);
        tcp::resolver::results_type endpoints = resolver.resolve(server, port);
        tcp::socket socket(io_context);
        boost::asio::connect(socket, endpoints);

        cout << GREEN << "Successfully connected to the server at " << server << ":" << port << RESET << endl;

        string username;
        vector<string> users; // Online Users List

        cout << GREEN << "Welcome To CPP CLI Chat App" << RESET << endl;
        cout << YELLOW << "=============================" << RESET << endl;
        cout << BLUE << "Please Enter Your Username : " << RESET << endl;
        cin >> username;

        // Send username to server
        boost::asio::write(socket, boost::asio::buffer("register:" + username + "\n"));

        // Read the list of online users from the server
        boost::asio::streambuf buffer;
        boost::asio::read_until(socket, buffer, "\n");
        istream input(&buffer);
        string user_list;
        getline(input, user_list);

        if (user_list.rfind("users:", 0) == 0)
        {
            user_list = user_list.substr(6);
            size_t pos = 0;
            while ((pos = user_list.find(',')) != string::npos)
            {
                users.push_back(user_list.substr(0, pos));
                user_list.erase(0, pos + 1);
            }
            users.push_back(user_list);
        }

        cout << YELLOW << "=============================" << RESET << endl;
        cout << GREEN << "Welcome " << username << RESET << endl;
        cout << YELLOW << "=============================" << RESET << endl;

        while (true)
        {
            cout << GREEN << "What Would You Like To DO ? " << endl;
            cout << RED << "1. See The Online Users List" << RESET << endl;
            cout << RED << "2. Chat" << RESET << endl;
            cout << RED << "3. Exit" << RESET << endl;
            int choice;
            cin >> choice;

            if (choice == 1)
            {
                cout << GREEN << "Online Users:" << RESET << endl;
                for (const auto &user : users)
                {
                    cout << BLUE << user << " " << GREEN_CIRCLE << RESET << endl;
                }
            }

            if (choice == 2)
            {
                // Chat functionality will be here
            }

            if (choice == 3)
            {
                cout << RED << "Goodbye" << RESET << endl;
                users.clear();
                return;
            }
        }
    }
    catch (const std::exception &e)
    {
        cerr << RED << "Failed to connect to the server: " << e.what() << RESET << endl;
    }
}

int main()
{
    try
    {
        boost::asio::io_context io_context;
        terminal(io_context, "127.0.0.1", "4000");
    }
    catch (std::exception &e)
    {
        cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}