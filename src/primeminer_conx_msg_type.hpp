//SYSTEM
#include <cstdio>
#include <cstdlib>

enum message_type {
   MSG_UNDEFINED = 10,
   MSG_HELLO, //11: TO SERVER
   MSG_WELCOME, //12: TO CLIENT
   MSG_JOIN, //13: TO CLIENTs
   MSG_QUIT, //14: TO CLIENTs
   MSG_SNAPSHOT, //15: TO SERVER/CLIENTs
   MSG_DATA, //16: TO SERVER/CLIENT
   MSG_PING, //18: TO SERVER/CLIENT
   MSG_PONG //19: TO SERVER/CLIENT
};

std::ostream& operator<<( std::ostream &s, message_type &t );
