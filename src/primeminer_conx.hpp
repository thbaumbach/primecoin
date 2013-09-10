//SYSTEM
#include <ctime>
#include <iostream>
#include <string>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/asio.hpp>

extern size_t thread_num_max;
#define MSG_GETWORK_LENGTH 128 //#bytes

class message
{
public:
	message(size_t length_in, const char* data_in = NULL) : _data(NULL) {
		body_reset(length_in);
		if (length_in > 0 && data_in != NULL)
			memcpy(data(), data_in, length_in);
	}

	message(message& other) : _data(NULL) {
		body_reset(other.length_body());
		data()[0] = other.data()[0]; //copy header
		if (length_body() > 0)
			memcpy(data(), other.data(), length_body());
	}

   ~message() { delete[] _data; }
   
   size_t length() { return _length; }
   size_t length_header() { return 1; } //1 byte header   
   size_t length_body() {
	   switch (((unsigned char*)_data)[0]) {
		   case 0: return (thread_num_max * MSG_GETWORK_LENGTH);
		   case 1: return 4;
	   }
	   return 0;
   }
   
   char* body() { return _data+1; }
   char* data() { return _data; }   

	void body_reset(size_t length_new) {
		if (_data != NULL) delete[] _data;
		_data = new char[length_header()+length_new];
		_length = length_header()+length_new;
	}

private:
	size_t _length;
	char* _data;
};

typedef boost::shared_ptr<message> message_ptr;
typedef std::deque<message_ptr> message_queue;

enum tcp_connection_state {
   CS_TCP_UNDEFINED = 100,
   CS_TCP_OPEN,
   CS_TCP_CLOSING
};

using boost::asio::ip::tcp;

class CNotifyStub
{
public:
  CNotifyStub();
  CNotifyStub(int num_threads_in) : num_threads_(num_threads_in) { }
  virtual int num_threads() { return num_threads_; }
  virtual void process_message(message_ptr& msg) = 0;
protected:
  int num_threads_;
};

class CClientConnection
{
public:
   CClientConnection(CNotifyStub* notifier, boost::asio::io_service& io_service, tcp::resolver::iterator& endpoint)
	  : _notifier(notifier), io_service_(io_service), socket_tcp_(io_service), read_msg_tcp_(1), state_tcp_(CS_TCP_UNDEFINED)
   {
	   socket_tcp_.async_connect(*endpoint,
         boost::bind(&CClientConnection::tcp_handle_connect, this,
         boost::asio::placeholders::error));
   }

   ~CClientConnection() {
   }
   
   void write_tcp(message_ptr msg) {
      io_service_.post(boost::bind(&CClientConnection::tcp_write, this, msg));
   }

   void close() {
      io_service_.post(boost::bind(&CClientConnection::do_close, this));
   }

   bool is_open() {
      return socket_tcp_.is_open() && state_tcp_ == CS_TCP_OPEN;
   }

private:
   void tcp_handle_connect(const boost::system::error_code& error)
   {
      if (!error) {
         std::cout << "establishing TCP connection to " << socket_tcp_.remote_endpoint().address().to_string() << ":" << socket_tcp_.remote_endpoint().port() << std::endl;

         state_tcp_ = CS_TCP_OPEN; //before or after async_read(...) ?
         
         boost::asio::async_read(socket_tcp_,
            boost::asio::buffer(read_msg_tcp_.data(), read_msg_tcp_.length_header()),
            boost::asio::transfer_exactly(read_msg_tcp_.length_header()),
            boost::bind(&CClientConnection::tcp_handle_read_header, this,
               boost::asio::placeholders::error));

         //message_ptr msg(new message(MSG_HELLO, 0, 0, config.getOptionString("name").size(), config.getOptionString("name").data()));
         //write_tcp(msg); //TODO
      } else
        std::cout << "error @ tcp_handle_connect" << std::endl;
   }

   void tcp_handle_read_header(const boost::system::error_code& error)
   {
      if (state_tcp_ != CS_TCP_OPEN)
         return;
      if (!error) { //TODO: and check if message is oversized
         int msg_length = read_msg_tcp_.length_body();
         std::cout << "awaiting " << msg_length << " bytes!" << std::endl;
         read_msg_tcp_.body_reset(msg_length);
         boost::asio::async_read(socket_tcp_,
            boost::asio::buffer(read_msg_tcp_.body(), msg_length),
            boost::asio::transfer_exactly(msg_length),
            boost::bind(&CClientConnection::tcp_handle_read_body, this,
               boost::asio::placeholders::error));
      }
      else if (error != boost::asio::error::operation_aborted) {
		 std::cout << "operation_aborted @ tcp_handle_read_header" << std::endl;
         do_close();
      } else
        std::cout << "error @ tcp_handle_read_header" << std::endl;
   }

   void tcp_handle_read_body(const boost::system::error_code& error)
   {
      if (state_tcp_ != CS_TCP_OPEN)
         return;
      if (!error) {
		 std::cout << "woohoo!" << std::endl;
         message_ptr copy(new message(read_msg_tcp_));
         _notifier->process_message(copy);

         boost::asio::async_read(socket_tcp_,
            boost::asio::buffer(read_msg_tcp_.data(), read_msg_tcp_.length_header()),
            boost::asio::transfer_exactly(read_msg_tcp_.length_header()),
            boost::bind(&CClientConnection::tcp_handle_read_header, this,
               boost::asio::placeholders::error));
      }
      else if (error != boost::asio::error::operation_aborted) {
		 std::cout << "operation_aborted @ tcp_handle_read_body" << std::endl;
         do_close();
	  } else
	   std::cout << "error @ tcp_handle_read_body" << std::endl;
   }

   void tcp_write(message_ptr msg)
   {
      if (state_tcp_ != CS_TCP_OPEN) {
		 std::cout << "!CS_TCP_OPEN @ tcp_write" << std::endl;
         return;
	  }
      bool write_in_progress = !write_msgs_tcp_.empty();
      write_msgs_tcp_.push_back(msg); //TODO: threadsafe?
      if (!write_in_progress)
         boost::asio::async_write(socket_tcp_,
            boost::asio::buffer(write_msgs_tcp_.front()->data(),
               write_msgs_tcp_.front()->length()),
            boost::bind(&CClientConnection::tcp_handle_write, this,
               boost::asio::placeholders::error));
   }

   void tcp_handle_write(const boost::system::error_code& error)
   {
      if (state_tcp_ != CS_TCP_OPEN) {
		 std::cout << "!CS_TCP_OPEN @ tcp_handle_write" << std::endl;
         return;
	  }
      if (!error) {
         //std::cout << "sent via TCP " << *(write_msgs_tcp_.front()) << std::endl;
         write_msgs_tcp_.pop_front();
         if (!write_msgs_tcp_.empty())
            boost::asio::async_write(socket_tcp_,
               boost::asio::buffer(write_msgs_tcp_.front()->data(),
                  write_msgs_tcp_.front()->length()),
               boost::bind(&CClientConnection::tcp_handle_write, this,
                  boost::asio::placeholders::error));
      } else {
		 std::cout << "error @ tcp_handle_write" << std::endl;
         do_close();
      }
   }

   void do_close() {
      tcp_close();
   }

   void tcp_close()
   {
      if (state_tcp_ == CS_TCP_CLOSING)
         return;
      state_tcp_ = CS_TCP_CLOSING;
      std::cout << "closing TCP connection: " << std::endl;
      try {
         if (socket_tcp_.is_open()) {
            boost::system::error_code error;
            socket_tcp_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, error);
            socket_tcp_.close();
         }
      }
      catch(boost::system::system_error& e) {
         std::cout << "tcp_close(): " << e.what() << std::endl;
      }
      std::cout << "done." << std::endl;
   }

private:
   //notifier & service
   CNotifyStub* _notifier;
   boost::asio::io_service& io_service_;
   //sockets
   tcp::socket socket_tcp_;
   //messages
   message read_msg_tcp_;
   //queues
   message_queue write_msgs_tcp_;
   //states
   tcp_connection_state state_tcp_;
};
