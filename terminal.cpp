#include <iostream>
#include <vector>
#include <thread>
#include <boost/asio.hpp>
#include <memory>

using namespace std;
using boost::asio::ip::tcp;

const string RESET = "\033[0m";
const string RED = "\033[31m";
const string GREEN = "\033[32m";
const string YELLOW = "\033[33m";
const string BLUE = "\033[34m";
const string PURPLE = "\033[35m";
const string GREEN_CIRCLE = "\033[32m\u25CF\033[0m";

void print_centered(const string &text, int console_width, const string &color = "")
{
    int padding = (console_width - text.length()) / 2;
    cout << color << string(padding, ' ') << text << RESET << endl;
}

void display_logo()
{
    const int console_width = 120;

    vector<string> logo_lines = {
        "  ____ _     ___     ____ _           _   ",
        " / ___| |   |_ _|   / ___| |__   __ _| |_ ",
        "| |   | |    | |   | |   | '_ \\ / _` | __|",
        "| |___| |___ | |   | |___| | | | (_| | |_ ",
        " \\____|_____|___|   \\____|_| |_|\\__,_|\\__|"};

    for (const auto &line : logo_lines)
    {
        print_centered(line, console_width, GREEN);
    }
    print_centered("=========================================", console_width, RED);
    print_centered("=========================================", console_width, RED);
    print_centered(" Press Enter To Continue... ", console_width, RED);
    try
    {
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
    }
    catch (const ios_base::failure &e)
    {
        cerr << "Error: " << e.what() << endl;
    }
}

void read_messages(shared_ptr<tcp::socket> socket, vector<string> &users)
{
    try
    {
        while (true)
        {
            boost::asio::streambuf buffer;
            boost::asio::read_until(*socket, buffer, "\n");
            istream input(&buffer);
            string message;
            getline(input, message);

            if (message.rfind("users:", 0) == 0)
            {
                users.clear();
                message = message.substr(6);
                size_t pos = 0;
                while ((pos = message.find(',')) != string::npos)
                {
                    users.push_back(message.substr(0, pos));
                    message.erase(0, pos + 1);
                }
                users.push_back(message);
            }
            else
            {
                cout << YELLOW << "Message : " << RESET << message << endl;
            }
        }
    }
    catch (const std::exception &e)
    {
        cerr << RED << "Failed To Read From Server: " << e.what() << RESET << endl;
    }
}

void show_online_users(const vector<string> &users, const string &username)
{
    cout << GREEN << "Online Users:" << RESET << endl;
    bool is_there_user_online = false;
    for (const auto &user : users)
    {
        if (user != username) // Exclude the current user
        {
            cout << BLUE << user << " " << GREEN_CIRCLE << RESET << endl;
            is_there_user_online = true;
        }
    }
    if (!is_there_user_online)
    {
        cout << RED << "No Online Users" << RESET << endl;
        cout << YELLOW << "=============================" << RESET << endl;
    }
}

void terminal(boost::asio::io_context &io_context, const string &server, const string &port)
{
    try
    {
        tcp::resolver resolver(io_context);
        tcp::resolver::results_type endpoints = resolver.resolve(server, port);
        auto socket = std::make_shared<tcp::socket>(io_context);
        boost::asio::connect(*socket, endpoints);

        cout << GREEN << "Successfully connected to the server at " << server << ":" << port << RESET << endl;

        string username;
        vector<string> users;

        cout << GREEN << "Welcome To CLI Chat App" << RESET << endl;
        cout << YELLOW << "=============================" << RESET << endl;
        cout << BLUE << "Please Enter Your Username : " << RESET << endl;
        cin >> username;

        // Send username to server
        boost::asio::write(*socket, boost::asio::buffer("register:" + username + "\n"));

        // Read the initial list of online users from the server
        boost::asio::streambuf buffer;
        boost::asio::read_until(*socket, buffer, "\n");
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

        thread read_thread(read_messages, socket, ref(users));

        while (true)
        {

            cout << GREEN << "What Would You Like To DO ? " << endl;
            cout << PURPLE << "1. See The Online Users List" << RESET << endl;
            cout << PURPLE << "2. Chat" << RESET << endl;
            cout << PURPLE << "3. Exit" << RESET << endl;
            int choice;
            cin >> choice;
            if (cin.fail() || choice < 1 || choice > 3)
            {
                cin.clear();
                cin.ignore(numeric_limits<streamsize>::max(), '\n');
                cout << RED << "Invalid Input. Please Enter A Number Between 1 and 3." << RESET << endl;
                continue;
            }
            cout << YELLOW << "=============================" << RESET << endl;

            if (choice == 1)
            {
                show_online_users(users, username);
            }
            else if (choice == 2)
            {
                show_online_users(users, username);

                // Check if there are online users before entering chat mode
                bool has_online_users = false;
                for (const auto &user : users)
                {
                    if (user != username)
                    {
                        has_online_users = true;
                        break;
                    }
                }

                if (!has_online_users)
                {
                    cout << RED << "No Online Users To Chat With. Returning To Main Menu." << RESET << endl;
                    cout << YELLOW << " =============================" << RESET << endl;
                    continue;
                }

                string recipient;
                cout << GREEN << "Who Would You Like To Message? (Type 'Back' To Return To Main Menu) " << RESET << endl;
                cin >> recipient;
                if (recipient == "back" || recipient == "Back")
                {
                    continue;
                }

                // Check if the recipient is an online user
                bool is_valid_user = false;
                for (const auto &user : users)
                {
                    if (user == recipient)
                    {
                        is_valid_user = true;
                        break;
                    }
                }

                if (!is_valid_user)
                {
                    cout << RED << "Error: Username Not Found. Returning To Main Menu." << RESET << endl;
                    continue;
                }

                cin.ignore();
                while (true)
                {
                    string message;
                    cout << BLUE << "Enter Your Message (or 'Back' to return to main menu): " << RESET;
                    getline(cin, message);

                    if (message == "back" || message == "Back")
                    {
                        break;
                    }

                    boost::asio::write(*socket, boost::asio::buffer("@" + recipient + ":" + message + "\n"));
                    cout << GREEN << "Message Sent" << RESET << endl;
                }
            }
            else if (choice == 3)
            {
                cout << RED << " Press Ctrl+C To Exit... " << RESET << endl;
                boost::asio::write(*socket, boost::asio::buffer("Disconnected : " + username + "\n"));
                users.clear();
                break;
            }
            else
            {
                cout << RED << "Invalid choice." << RESET << endl;
            }
        }

        read_thread.join();
    }
    catch (const std::exception &e)
    {
        cerr << RED << "Failed to connect to the server: " << e.what() << RESET << endl;
    }
}

int main()
{
    display_logo();

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
