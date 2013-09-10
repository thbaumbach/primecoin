//SYSTEM
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <map>
#include <cstring>
#include <boost/shared_ptr.hpp>

#include "primeminer_conx_msg_type.hpp"

typedef unsigned char uchar;
typedef unsigned short ushort;

const ushort t_message_max_length = 200;
const ushort t_message_header_length = sizeof(uchar) + sizeof(uchar) + sizeof(uchar) + sizeof(ushort);

class message
{
public:
   message(message_type type_in, uchar source_id_in, uchar snapshot_in, ushort length_in) : data_(NULL)
   {
      body_reset(type_in, source_id_in, snapshot_in, length_in);
   }

   message(message_type type_in, uchar source_id_in, uchar snapshot_in, ushort length_in, const char* data_in) : data_(NULL)
   {
      body_reset(type_in, source_id_in, snapshot_in, length_in);
      if (length_in > 0)
         memcpy(body(), data_in, length_in);
   }

   message(message& other) : data_(NULL)
   {
      body_reset(other.type(), other.source_id(), other.snapshot(), other.length_body());
      if (length_body() > 0)
         memcpy(body(), other.body(), length_body());
   }

   ~message() { delete[] data_; }

   message_type type()
   {
      return (message_type)(*type_ptr());
   }

   uchar source_id()
   {
      return *source_id_ptr();
   }

   void change_id(uchar id)
   {
      *source_id_ptr() = id;
   }

   uchar snapshot()
   {
      return *snapshot_ptr();
   }

   ushort length_header()
   {
      return t_message_header_length;
   }

   ushort length_body()
   {
      return *length_body_ptr();
   }

   ushort length_data()
   {
      return length_body() + length_header();
   }

   char* data()
   {
      return data_;
   }

   char* body()
   {
      return data_+length_header();
   }

   void body_reset(unsigned short length_new)
   {
      message_type t = type();
      uchar id_backup = source_id();
      uchar snapshot_backup = snapshot();
      if (data_ != NULL) delete[] data_;
      data_ = (length_new > 0) ? new char[length_header()+length_new] : new char[length_header()];
      *type_ptr() = (uchar)t;
      *source_id_ptr() = id_backup;
      *snapshot_ptr() = snapshot_backup;
      *length_body_ptr() = length_new;
   }

   uchar& state_id()
   {
      return *((uchar*)body());
   }

   friend std::ostream& operator<<( std::ostream &s, message &m );

protected:
   void body_reset(message_type t_in, uchar id_in, uchar snapshot_in, ushort length_new)
   {
      if (data_ != NULL) delete[] data_;
      data_ = (length_new > 0) ? new char[length_header()+length_new] : new char[length_header()];
      *type_ptr() = (uchar)t_in;
      *source_id_ptr() = id_in;
      *snapshot_ptr() = snapshot_in;
      *length_body_ptr() = length_new;
   }

   uchar* type_ptr()
   {
      return (uchar*)(data_);
   }

   uchar* source_id_ptr()
   {
      return (uchar*)(data_+sizeof(uchar));
   }

   uchar* snapshot_ptr()
   {
      return (uchar*)(data_+(2*sizeof(uchar)));
   }

   ushort* length_body_ptr()
   {
      return (ushort*)(data_+(3*sizeof(uchar)));
   }

private:
   char* data_;
};

typedef boost::shared_ptr<message> message_ptr;
typedef std::deque<message_ptr> message_queue;

std::ostream& operator<<( std::ostream &s, message &m );
