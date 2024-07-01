#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <boost/asio.hpp>
#include <boost/algorithm/string.hpp>
#include <fstream>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <vector>

using boost::asio::ip::tcp;

std::string time_t_to_string(std::time_t time)
{
  std::tm *tm = std::localtime(&time);
  std::ostringstream ss;
  ss << std::put_time(tm, "%Y-%m-%dT%H:%M:%S");
  return ss.str();
}

std::time_t string_to_time_t(const std::string &time_string)
{
  std::tm tm = {};
  std::istringstream ss(time_string);
  ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
  return std::mktime(&tm);
}

#pragma pack(push, 1)
struct LogRecord
{
  char sensor_id[32];    // supondo um ID de sensor de até 32 caracteres
  std::time_t timestamp; // timestamp UNIX
  double value;          // valor da leitura
};
#pragma pack(pop)

class session
    : public std::enable_shared_from_this<session>
{
public:
  session(tcp::socket socket)
      : socket_(std::move(socket)) {}

  void start()
  {
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
                                      boost::algorithm::split(splitedMessage, message, boost::is_any_of("|"));

                                      if (splitedMessage.size() < 2)
                                      {
                                        std::cerr << "Invalid message format\n";
                                        return;
                                      }

                                      std::string messageType = splitedMessage[0];
                                      std::string sensor_id = splitedMessage[1];
                                      std::string file_path = sensor_id + ".log";

                                      if (messageType == "LOG")
                                      {
                                        std::string timeDate = splitedMessage[2];
                                        std::string data = splitedMessage[3];
                                        std::ofstream file(file_path, std::ios::binary | std::ios::app);

                                        if (file.is_open())
                                        {
                                          LogRecord rec;
                                          std::strncpy(rec.sensor_id, sensor_id.c_str(), sizeof(rec.sensor_id));
                                          rec.timestamp = string_to_time_t(timeDate);
                                          rec.value = std::stof(data);

                                          file.write((char *)&rec, sizeof(LogRecord));
                                          file.close();
                                        }
                                      }
                                      else if (messageType == "GET")
                                      {
                                        std::string num_records_str = splitedMessage[2];
                                        int num_records = std::stoi(num_records_str);
                                        std::string response = get_records(file_path, num_records);
                                        write_message(response);
                                      }
                                      write_message(message);
                                    }
                                  });
  }

  void write_message(const std::string &message)
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

  std::string get_records(const std::string file_path, int num_records)
  {
    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open())
    {
      return "ERROR|INVALID_SENSOR_ID\r\n";
    }

    std::vector<LogRecord> records;
    LogRecord rec;
    while (file.read(reinterpret_cast<char *>(&rec), sizeof(LogRecord)))
    {
      records.push_back(rec);
    }
    file.close();

    int record_size = records.size();

    if (num_records > record_size)
    {
      num_records = record_size;
    }

    std::string response = std::to_string(num_records) + ";";
    for (int i = record_size - num_records; i < record_size; ++i)
    {
      response += time_t_to_string(records[i].timestamp) + "|" + std::to_string(records[i].value) + ";";
    }
    response.pop_back(); // Remove o último ponto e vírgula
    response += "\r\n";
    return response;
  }
  tcp::socket socket_;
  boost::asio::streambuf buffer_;
};

class server
{
public:
  server(boost::asio::io_context &io_context, short port)
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

int main(int argc, char *argv[])
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