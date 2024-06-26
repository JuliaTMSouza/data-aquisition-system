#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <boost/asio.hpp>
#include <vector>
#include <boost/algorithm/string.hpp>

using boost::asio::ip::tcp;

std::vector<Sensor> sensores;
int counter = 0;

class Sensor {
public:
    Sensor(tcp::socket& socket, const std::string& file_path)
        : socket_(std::move(socket)),
          file_path_(file_path)
    {
    }

    tcp::socket& getSocket() {
        return socket_;
    }

    const std::string& getFilePath() const {
        return file_path_;
    }

private:
    tcp::socket socket_;
    std::string file_path_;
};

class session
  : public std::enable_shared_from_this<session>
{
public:
  session(tcp::socket socket)
    : socket_(std::move(socket))
  {
  }

  void start()
  {
    std::string  path_name = counter + ".txt";
    std::string& file_path = path_name;
    Sensor newSensor = Sensor(this->socket_, file_path);
    counter++;
    read_message();
  }

private:
  void read_message()
  {
    auto self(shared_from_this());
    boost::asio::async_read_until(socket_, buffer_, "\r\n",
        [this, self](boost::system::error_code ec, std::size_t length)
        {
          if (!ec)
          {
            std::istream is(&buffer_);
            std::string message(std::istreambuf_iterator<char>(is), {});
            std::cout << "Received: " << message << std::endl;
            std::vector<std::string> splitedMessage;
            boost::algorithm::split(splitedMessage, message, boost::is_any_of(","));

            if (splitedMessage[0] == "LOG") {
                 
            } else if (splitedMessage[0] == "GET") {
                
            }
            write_message(message);
            for (int i = 0; i < sensores.size(); i++) {
                if (sensores[i].getSocket() == std::move(socket_)) {
                    std::cout << "true" << " | " << sensores[i].getSocket() << std::endl;
                    
                }
            }
          }
        });
  }

  void write_message(const std::string& message)
  {
    auto self(shared_from_this());
    boost::asio::async_write(socket_, boost::asio::buffer(message),
        [this, self, message](boost::system::error_code ec, std::size_t /*length*/)
        {
          if (!ec)
          {
            read_message();
          }
        });
  }

  tcp::socket socket_;
  boost::asio::streambuf buffer_;
};

class server
{
public:
  server(boost::asio::io_context& io_context, short port)
    : acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
  {
    accept();
  }

private:
  void accept()
  {
    acceptor_.async_accept(
        [this](boost::system::error_code ec, tcp::socket socket)
        {
          if (!ec)
          {
            std::make_shared<session>(std::move(socket))->start();
          }

          accept();
        });
  }

  tcp::acceptor acceptor_;
};

int main(int argc, char* argv[])
{
  if (argc != 2)
  {
    std::cerr << "Usage: chat_server <port>\n";
    return 1;
  }

  boost::asio::io_context io_context;

  server s(io_context, std::atoi(argv[1]));

  io_context.run();

  return 0;
}
