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

using boost::asio::ip::tcp;

std::string time_t_to_string(std::time_t time) {
    std::tm* tm = std::localtime(&time);
    std::ostringstream ss;
    ss << std::put_time(tm, "%Y-%m-%dT%H:%M:%S");
    return ss.str();
}

std::time_t string_to_time_t(const std::string& time_string) {
    std::tm tm = {};
    std::istringstream ss(time_string);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    return std::mktime(&tm);
}

#pragma pack(push, 1)
struct LogRecord {
    std::string sensor_id; // supondo um ID de sensor de até 32 caracteres
    std::time_t timestamp; // timestamp UNIX
    double value; // valor da leitura
};
#pragma pack(pop)

class Sensor
{
public:
  Sensor(std::string sensor_id, const std::string &file_path)
      : sensor_id_(sensor_id),
        file_path_(file_path)
  {
  }

  std::string getSensor_id()
  {
    return sensor_id_;
  }

  const std::string &getFilePath() const
  {
    return file_path_;
  }

private:
  std::string sensor_id_;
  std::string file_path_;
};

std::vector<Sensor> sensores;

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

            std::string messageType = splitedMessage[0];
            std::string sensor_id = splitedMessage[1];
            std::string filepath = sensor_id + ".txt";
            std::string timeDate = splitedMessage[2];
            std::string data = splitedMessage[3];

            // write_message(message);
 
            if (messageType == "LOG") {
              std::fstream file(filepath, std::fstream::out | std::fstream::in | std::fstream::binary 
																	 | std::fstream::app); 
              if (file.is_open()) {

		            // Recupera o número de registros presentes no arquivo

		            // Escreve o registro no arquivo
		            std::cout << "Escrevendo o registro..." << std::endl;

			          LogRecord rec;
			          rec.sensor_id = sensor_id;
                rec.timestamp = string_to_time_t(timeDate);
                rec.value = std::stof(data);
			          file.write((char*)&rec, sizeof(LogRecord));
                std::cout << "Registro escrito" << std::endl; 

                // LogRecord recRead;
                // file.read((char*)&recRead, sizeof(LogRecord));
                // std::cout << "Registro lido: id) " << recRead.sensor_id << " valor) " << recRead.value << std::endl; 
                // std::cout << time_t_to_string(recRead.timestamp) << std::endl;
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