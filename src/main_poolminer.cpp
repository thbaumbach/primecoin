//===
// by xolokram/TB
// 2013
//===

#include <iostream>
#include <fstream>
#include <cstdio>
#include <map>

#include "prime.h"
#include "serialize.h"
#include "bitcoinrpc.h"
#include "json/json_spirit_value.h"
#include <boost/thread.hpp>

#define VERSION_MAJOR 0
#define VERSION_MINOR 2

#include "primeminer_conx.hpp"

// <START> be compatible to original code (not actually used!)
#include "txdb.h"
#include "walletdb.h"
#include "wallet.h"
#include "ui_interface.h"
CWallet *pwalletMain;
CClientUIInterface uiInterface;
void StartShutdown() {
  exit(0);
}
// </END>

/*********************************
* global variables, structs and extern functions
*********************************/

extern CBlockIndex *pindexBest;
extern void BitcoinMiner(CWallet        *pwallet,
                         CBlockProvider *CBlockProvider,
                         unsigned int thread_id);
extern bool fPrintToConsole;
extern bool fDebug;
extern json_spirit::Object CallRPC(const std::string       & strMethod,
                                   const json_spirit::Array& params,
                                   const std::string       & server,
                                   const std::string       & port,
                                   std::map<std::string,
                                            std::string>   & mapHeadersRet);
extern int FormatHashBlocks(void        *pbuffer,
                            unsigned int len);

struct blockHeader_t {
  // comments: BYTES <index> + <length>
  int           nVersion;            // 0+4
  uint256       hashPrevBlock;       // 4+32
  uint256       hashMerkleRoot;      // 36+32
  unsigned int  nTime;               // 68+4
  unsigned int  nBits;               // 72+4
  unsigned int  nNonce;              // 76+4
  unsigned char primemultiplier[48]; // 80+48
};                                   // =128 bytes header (80 default + 48
                                     // primemultiplier)

extern unsigned int thread_num_max;

/*********************************
* helping functions
*********************************/
unsigned int getHexDigitValue(unsigned char c) {
  if ((c >= '0') && (c <= '9')) return c - '0';
  else if ((c >= 'a') && (c <= 'f')) return c - 'a' + 10;
  else if ((c >= 'A') && (c <= 'F')) return c - 'A' + 10;

  return 0;
}

void parseHexString(const char    *hexString,
                    unsigned int   length,
                    unsigned char *output) {
  unsigned int lengthBytes = length / 2;

  for (unsigned int i = 0; i < lengthBytes; ++i) {
    // high digit
    unsigned int d1 = getHexDigitValue(hexString[i * 2 + 0]);

    // low digit
    unsigned int d2 = getHexDigitValue(hexString[i * 2 + 1]);

    // build byte
    output[i] = (unsigned char)((d1 << 4) | (d2));
  }
}

/*********************************
* GET_WORK from server and convert to mineable block
*********************************/
bool getLongPollURL(std::string      & longpollurl,
                    const std::string& server,
                    const std::string& port) {
  // JSON GETWORK
  std::string strMethod = "getwork";

  std::vector<std::string> strParams;
  json_spirit::Array params = RPCConvertValues(strMethod, strParams);
  std::map<std::string, std::string> mapHeaders;
  json_spirit::Object reply_obj = CallRPC(strMethod,
                                          params,
                                          server,
                                          port,
                                          mapHeaders); // request

  // parse reply
  const json_spirit::Value& result_val = find_value(reply_obj, "result");
  const json_spirit::Value& error_val  = find_value(reply_obj, "error");

  if (error_val.type() != json_spirit::null_type) {
    // error code recieved
    std::cerr << "[REQUEST] " << write_string(error_val, false) << std::endl;
    return false;
  } else {
    // result
    std::string strValue;

    if (result_val.type() == json_spirit::null_type) {
      std::cerr << "[REQUEST] reply empty" << std::endl;
      return false;
    } else if (result_val.type() == json_spirit::str_type) strValue =
        result_val.get_str();
    else strValue = write_string(result_val, true);

    const json_spirit::Object& result_obj = result_val.get_obj();
    const json_spirit::Value & data_val   = find_value(result_obj, "data");

    if (data_val.type() == json_spirit::null_type) {
      std::cerr << "[REQUEST] result empty" << std::endl;
      return false;
    }

    std::map<std::string, std::string>::iterator it = mapHeaders.find(
      "x-long-polling");

    if (it == mapHeaders.end()) {
      std::cout << "long polling header -NOT- found" << std::endl;
      return false;
    } else {
      std::cout << "long polling header found" << std::endl;
      longpollurl = it->second;
    }
  }
  return true;
}

bool getBlockFromServer(CBlock           & pblock,
                        const std::string& server,
                        const std::string& port) {
  unsigned char localBlockData[128];

  { // JSON GETWORK
    std::string strMethod = "getwork";
    std::vector<std::string> strParams;
    json_spirit::Array params = RPCConvertValues(strMethod, strParams);
    std::map<std::string, std::string> mapHeaders;
    json_spirit::Object reply_obj = CallRPC(strMethod,
                                            params,
                                            server,
                                            port,
                                            mapHeaders); // request

    // parse reply
    const json_spirit::Value& result_val = find_value(reply_obj, "result");
    const json_spirit::Value& error_val  = find_value(reply_obj, "error");

    if (error_val.type() != json_spirit::null_type) {
      // error code recieved
      std::cerr << "[REQUEST] " << write_string(error_val, false) << std::endl;
      return false;
    } else {
      // result
      std::string strValue;

      if (result_val.type() == json_spirit::null_type) {
        std::cerr << "[REQUEST] reply empty / long-poll timeout (re-polling!)" <<
        std::endl;
        return false;
      } else if (result_val.type() == json_spirit::str_type) strValue =
          result_val.get_str();
      else strValue = write_string(result_val, true);

      const json_spirit::Object& result_obj = result_val.get_obj();
      const json_spirit::Value & data_val   = find_value(result_obj, "data");

      if (data_val.type() == json_spirit::null_type) {
        std::cerr << "[REQUEST] result empty" << std::endl;
        return false;
      } else if (data_val.type() == json_spirit::str_type) strValue =
          data_val.get_str();
      else strValue = write_string(data_val, true);

      if (strValue.length() != 256) {
        std::cerr << "[REQUEST] data length != 256" << std::endl;
        return false;
      }

      parseHexString(strValue.c_str(), 256, localBlockData);

      for (unsigned int i = 0; i < 128 / 4;
           ++i) ((unsigned int *)localBlockData)[i] =
          ByteReverse(((unsigned int *)localBlockData)[i]);
    }
  }

  {
    std::stringstream ss;

    for (int i = 7; i >= 0; --i) ss << std::setw(8) << std::setfill('0') <<
      std::hex << *((int *)(localBlockData + 4) + i);
    ss.flush();
    pblock.hashPrevBlock.SetHex(ss.str().c_str());
  }
  {
    std::stringstream ss;

    for (int i = 7; i >= 0; --i) ss << std::setw(8) << std::setfill('0') <<
      std::hex << *((int *)(localBlockData + 36) + i);
    ss.flush();
    pblock.hashMerkleRoot.SetHex(ss.str().c_str());
  }

  pblock.nVersion               = *((int *)(localBlockData));
  pblock.nTime                  = *((unsigned int *)(localBlockData + 68));
  pblock.nBits                  = *((unsigned int *)(localBlockData + 72));
  pblock.nNonce                 = *((unsigned int *)(localBlockData + 76));
  pblock.bnPrimeChainMultiplier = 0;

  return true;
}

/*********************************
* class CBlockProviderGW to (incl. SUBMIT_BLOCK)
*********************************/

class CBlockProviderGW : public CBlockProvider {
public:

  CBlockProviderGW()
    : CBlockProvider(), _longpoll(true), _pblock(NULL) {}

  virtual ~CBlockProviderGW() { /* TODO, who cares */ }

  virtual CBlock* getBlock(unsigned int thread_id) {
    if (_longpoll) {
      boost::unique_lock<boost::shared_mutex> lock(_mutex_getwork);
	  CBlock* block = NULL;
      block = new CBlock((_pblock+thread_id)->GetBlockHeader());
      block->nTime = GetAdjustedTime();
      std::cout << "[WORKER" << thread_id << "] got_work block=" <<
        block->GetHash().ToString().c_str() << std::endl;
      return block;
    } else {
      if (_pblock != NULL) delete _pblock;
      _pblock = new CBlock();
      if (!getBlockFromServer(*_pblock, server, port)) return NULL;
      std::cout << "[WORKER" << thread_id << "] got_work block=" <<
        _pblock->GetHash().ToString().c_str() << std::endl;
      return _pblock;
    }
    return NULL;
  }

  virtual bool getBlockLongPoll() {
    CBlock *blocks = new CBlock[thread_num_max];
    if (getBlockFromServer(*blocks, server, port)) {
	  //
	  //EITHER
	  // get a block for every thread (like here right now!)
	  // this needs more bandwidth (for the pool)
	  //OR
	  // use nTime and nNonce to differ for all threads
	  // needs some testing: nTime = second-resolution, nNonce enough?
	  //
      for (unsigned int i = 1; i < thread_num_max; ++i)
        getBlockFromServer(*(blocks+i), GetArg("-poolip", "127.0.0.1"),
                                        GetArg("-poolport", "9912"));
      boost::unique_lock<boost::shared_mutex> lock(_mutex_getwork);
      delete[] _pblock;
      _pblock    = blocks;
      std::cout << "[MASTER] got_work" << std::endl;
    } else {
      delete[] blocks;
      return false;
    }
    return true;
  }

private:

  boost::shared_mutex _mutex_getwork;
  bool _longpoll;
  CBlock* _pblock;

public:

  static std::string server;
  static std::string port;
};

std::string CBlockProviderGW::server = ""; //TODO: seperate direct from
std::string CBlockProviderGW::port   = ""; //longpoll URLs

void CBlockProvider::submitBlock(CBlock *pblock) {
  std::string strMethod = "getwork";

  std::vector<std::string> strParams;

  // build block data
  blockHeader_t block;
  block.nVersion       = pblock->nVersion;
  block.hashPrevBlock  = pblock->hashPrevBlock;
  block.hashMerkleRoot = pblock->hashMerkleRoot;
  block.nTime          = pblock->nTime;
  block.nBits          = pblock->nBits;
  block.nNonce         = pblock->nNonce;

  // block.bnPrimeChainMultiplier = pblock->bnPrimeChainMultiplier;
  std::vector<unsigned char> primemultiplier =
    pblock->bnPrimeChainMultiplier.getvch();

  if (primemultiplier.size() > 47) {
    std::cerr <<
    "[WORKER] share submission warning: not enough space for primemultiplier" <<
    std::endl;
    return;
  }
  block.primemultiplier[0] = primemultiplier.size();

  for (size_t i = 0; i < primemultiplier.size();
       ++i) block.primemultiplier[1 + i] = primemultiplier[i];

  // FormatHashBlocks(&block, sizeof(block)); //not used, unnecessary
  for (unsigned int i = 0; i < 128 / 4; ++i) ((unsigned int *)&block)[i] =
          ByteReverse(((unsigned int *)&block)[i]);
  char pdata[128];
  memcpy(pdata, &block, 128);
  std::string data_hex = HexStr(BEGIN(pdata), END(pdata));
  strParams.push_back(data_hex);
  std::map<std::string, std::string> mapHeaders;
  json_spirit::Array  params    = RPCConvertValues(strMethod, strParams);
  json_spirit::Object reply_obj =
    CallRPC(strMethod, params,
            GetArg("-poolip", "127.0.0.1"), GetArg("-poolport",
                                                   "9912"), mapHeaders); // submit

  if (reply_obj.empty()) {
    std::cout << "[WORKER] share submission failed" << std::endl;
  } else {
    const json_spirit::Value& result_val = find_value(reply_obj, "result");
    int retval                           = 0;

    if (result_val.type() == json_spirit::int_type) retval = result_val.get_int();
    std::cout << "[WORKER] share submitted -> " <<
    (retval == 0 ? "REJECTED" : retval < 0 ? "STALE" : retval ==
     1 ? "BLOCK" : "SHARE") << std::endl;
  }
}

/*********************************
* multi-threading
*********************************/

class CMasterThreadStub {
public:

  virtual void                 wait_for_master()  = 0;
  virtual boost::shared_mutex& get_working_lock() = 0;
};

class CWorkerThread { // worker=miner
public:

  CWorkerThread(CMasterThreadStub *master,
                unsigned int           id,
                CBlockProviderGW  *bprovider) : _working_lock(NULL), _id(id),
                                                _master(master),
                                                _bprovider(bprovider),
                                                _thread(&CWorkerThread::run,
                                                        this) {}

  void run() {
    std::cout << "[WORKER" << _id << "] Hello, World!" << std::endl;
    _master->wait_for_master();
    std::cout << "[WORKER" << _id << "] GoGoGo!" << std::endl;

    if (_bprovider == NULL) _bprovider = new CBlockProviderGW();  // TODO: delete
    BitcoinMiner(NULL, _bprovider, _id);
    std::cout << "[WORKER" << _id << "] Bye Bye!" << std::endl;
  }

  void work() {                     // called from within master thread
    _working_lock = new boost::shared_lock<boost::shared_mutex>(
    _master->get_working_lock()); // TODO: delete?
  }

private:

  boost::shared_lock<boost::shared_mutex> *_working_lock;
  unsigned int _id;
  CMasterThreadStub *_master;
  CBlockProviderGW  *_bprovider;
  boost::thread _thread;
};

class CMasterThread : public CMasterThreadStub {
public:

  CMasterThread(CClientConnection& con) : CMasterThreadStub(), _con(con) {}

  void run() {
	  
	boost::this_thread::sleep(boost::posix_time::milliseconds(1000));
    
    message_ptr msg(new message(30));
    *(msg->data()) = 28;
    memcpy(msg->body(), "abcdefghijklmnopqrstuvwxyz12", 28);
    *(msg->data()+29) = thread_num_max;
	_con.write_tcp(msg);
	
	boost::this_thread::sleep(boost::posix_time::milliseconds(5000));
	
	return;
	
    CBlockProviderGW *bprovider = NULL;
    bool longpoll               = false;

    {
      std::string longpollurl;
      bprovider->server = GetArg("-poolip", "127.0.0.1");
      bprovider->port   = GetArg("-poolport", "9912");

      if (getLongPollURL(longpollurl,
                         GetArg("-poolip",
                                "127.0.0.1"), GetArg("-poolport", "9912"))) {
        bprovider = new CBlockProviderGW();
        bprovider->getBlockLongPoll();                 // get the first block by
                                                       // direct polling (ip +
                                                       // port)
        size_t c = longpollurl.find_last_of(':');
        bprovider->server = longpollurl.substr(0, c);  // setup longpoll server
                                                       // ip
        bprovider->port   = longpollurl.substr(c + 1); // setup longpoll server
                                                       // port
        //
        // TODO: url without ip, port and/or with directory @ longpollurl???
        //
        std::cout << "LONGPOLL-URL: " << bprovider->server << ":" <<
        bprovider->port << std::endl;
        longpoll = true;
      } else {
        bprovider->server = GetArg("-poolip", "127.0.0.1");
        bprovider->port   = GetArg("-poolport", "9912");
      }
      boost::unique_lock<boost::shared_mutex> lock(_mutex_master);
      std::cout << "spawning " << thread_num_max << " worker thread(s)" <<
      std::endl;

      for (unsigned int i = 0; i < thread_num_max; ++i) {
        CWorkerThread *worker = new CWorkerThread(this, i, bprovider); // delete
                                                                       // on
                                                                       // exit?
        worker->work();                                                // set
                                                                       // working
                                                                       // lock
      }
    }

    // WORKER WILL START HERE implicitly by destroying the lock on mutex_master
    // this part is "tricky", btw there's a minimal chance everything crashs
    // here ;-) good luck
    if (longpoll) {
      for (;;) {                          // check longpoll info and update
                                          // pindexBest
        if (bprovider->getBlockLongPoll()) {
          CBlockIndex *pindexOld = pindexBest;
          pindexBest = new CBlockIndex(); // this could need a efficient
                                          // solution
          delete pindexOld;
        }
      }
    } else {
      wait_for_workers();
    }
  }

  ~CMasterThread() {}

  void wait_for_master() {
    boost::shared_lock<boost::shared_mutex> lock(_mutex_master);
  }

  boost::shared_mutex& get_working_lock() {
    return _mutex_working;
  }

private:

  void wait_for_workers() {
    boost::unique_lock<boost::shared_mutex> lock(_mutex_working);
  }
  
  CClientConnection& _con;

  boost::shared_mutex _mutex_master;
  boost::shared_mutex _mutex_working;
};

class CNotify : public CNotifyStub
{
public:
  CNotify(int thr) : CNotifyStub(thr) { }
  void process_message(message_ptr& msg) {
    //std::cout << "received via TCP " << msg << std::endl;
  }
};


/*********************************
* main - this is where it begins
*********************************/
int main(int argc, char **argv)
{
  std::cout << "********************************************" << std::endl;
  std::cout << "*** Primeminer - Primecoin Pool Miner v" << VERSION_MAJOR << "." << VERSION_MINOR << std::endl;
  std::cout << "*** by xolokram/TB - www.beeeeer.org" << std::endl;
  std::cout << "***" << std::endl;
  std::cout << "*** thx to Sunny King & mikaelh" << std::endl;
  std::cout << "********************************************" << std::endl;

  if (argc < 2)
  {
    std::cerr << "usage: " << argv[0] <<
    " -poolip=<ip> -poolport=<port> -pooluser=<user> -poolpassword=<password>" <<
    std::endl;
    return EXIT_FAILURE;
  }
  
  // init everything:
  ParseParameters(argc, argv);
  
  thread_num_max = GetArg("-genproclimit", 1); // what about boost's 
                                               // hardware_concurrency() ?
  fPrintToConsole = true; // always on
  fDebug          = GetBoolArg("-debug");

  pindexBest = new CBlockIndex();

  GeneratePrimeTable();
  
  CNotify* notifier = new CNotify(thread_num_max);  
  boost::asio::io_service io_service;
  boost::asio::ip::tcp::resolver resolver(io_service); //resolve dns
  boost::asio::ip::tcp::resolver::query query(GetArg("-poolip", "127.0.0.1"), GetArg("-poolport", "1337"));
  boost::asio::ip::tcp::resolver::iterator endpoint = resolver.resolve(query);
  
  CClientConnection c(notifier, io_service, endpoint);

  boost::thread t(boost::bind(&boost::asio::io_service::run, &io_service));
  
  // ok, start mining:
  CMasterThread *mt = new CMasterThread(c);
  mt->run();
  
  c.close();
  t.join();  
  
  // end:
  return EXIT_SUCCESS;
}

/*********************************
* and this is where it ends
*********************************/
