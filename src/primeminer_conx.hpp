//SYSTEM
#include <ctime>
#include <iostream>
#include <string>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/asio.hpp>

#include "primeminer_conx_msg.hpp"

enum tcp_connection_state {
   CS_TCP_UNDEFINED = 100,
   CS_TCP_OPEN,
   CS_TCP_CLOSING
};

using boost::asio::ip::tcp;

class CWorldStub
{
public:
  virtual void process_message(message_ptr& msg) = 0;
};

class CClientConnection
{
public:
   CClientConnection(CWorldStub* world, boost::asio::io_service& io_service, tcp::resolver::iterator& endpoint)
	  : world_(world), io_service_(io_service), socket_tcp_(io_service), read_msg_tcp_(MSG_UNDEFINED, 0, 0, 0), state_tcp_(CS_TCP_UNDEFINED)
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
      return socket_tcp_.is_open();
   }

private:
   void tcp_handle_connect(const boost::system::error_code& error)
   {
      if (!error) {
         std::cout << "establishing TCP connection to " << socket_tcp_.remote_endpoint().address().to_string() << ":" << socket_tcp_.remote_endpoint().port() << std::endl;

         state_tcp_ = CS_TCP_OPEN;

         boost::asio::async_read(socket_tcp_,
            boost::asio::buffer(read_msg_tcp_.data(), read_msg_tcp_.length_header()),
            //boost::asio::transfer_at_least(read_msg_tcp_.length_header()),
            boost::bind(&CClientConnection::tcp_handle_read_header, this,
               boost::asio::placeholders::error));

         //message_ptr msg(new message(MSG_HELLO, 0, 0, config.getOptionString("name").size(), config.getOptionString("name").data()));
         //write_tcp(msg); //TODO
      }
   }

   void tcp_handle_read_header(const boost::system::error_code& error)
   {
      if (state_tcp_ != CS_TCP_OPEN)
         return;
      if (!error) { //TODO: and check if message is oversized
         int msg_length = read_msg_tcp_.length_body();
         read_msg_tcp_.body_reset(msg_length);
         if (msg_length == 0) {
            tcp_handle_read_body(error);
         }
         else
         {
            boost::asio::async_read(socket_tcp_,
               boost::asio::buffer(read_msg_tcp_.body(), msg_length),
               //boost::asio::transfer_all(),
               boost::bind(&CClientConnection::tcp_handle_read_body, this,
                  boost::asio::placeholders::error));
         }
      }
      else if (error != boost::asio::error::operation_aborted)
         do_close();
   }

   void tcp_handle_read_body(const boost::system::error_code& error)
   {
      if (state_tcp_ != CS_TCP_OPEN)
         return;
      if (!error) {
         message_ptr copy(new message(read_msg_tcp_));
         world_->process_message(copy);

         boost::asio::async_read(socket_tcp_,
            boost::asio::buffer(read_msg_tcp_.data(), read_msg_tcp_.length_header()),
            //boost::asio::transfer_at_least(read_msg_tcp_.length_header()),
            boost::bind(&CClientConnection::tcp_handle_read_header, this,
               boost::asio::placeholders::error));
      }
      else if (error != boost::asio::error::operation_aborted)
         do_close();
   }

   void tcp_write(message_ptr msg)
   {
      if (state_tcp_ != CS_TCP_OPEN)
         return;
      bool write_in_progress = !write_msgs_tcp_.empty();
      write_msgs_tcp_.push_back(msg); //TODO: threadsafe?
      if (!write_in_progress)
         boost::asio::async_write(socket_tcp_,
            boost::asio::buffer(write_msgs_tcp_.front()->data(),
               write_msgs_tcp_.front()->length_data()),
            boost::bind(&CClientConnection::tcp_handle_write, this,
               boost::asio::placeholders::error));
   }

   void tcp_handle_write(const boost::system::error_code& error)
   {
      if (state_tcp_ != CS_TCP_OPEN)
         return;
      if (!error) {
         std::cout << "sent via TCP " << *(write_msgs_tcp_.front()) << std::endl;
         write_msgs_tcp_.pop_front();
         if (!write_msgs_tcp_.empty())
            boost::asio::async_write(socket_tcp_,
               boost::asio::buffer(write_msgs_tcp_.front()->data(),
                  write_msgs_tcp_.front()->length_data()),
               boost::bind(&CClientConnection::tcp_handle_write, this,
                  boost::asio::placeholders::error));
      } else
         do_close();
   }

   void do_close() {
      tcp_close();
   }

   void tcp_close()
   {
      if (state_tcp_ == CS_TCP_CLOSING)
         return;
      state_tcp_ = CS_TCP_CLOSING;
      std::cout << "closing TCP connection: "; std::cout.flush();
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
   //world & service
   CWorldStub* world_;
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
