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
#include <boost/asio.hpp>

#define VERSION_MAJOR 0
#define VERSION_MINOR 2

using boost::asio::ip::tcp;
//#include "primeminer_conx.hpp"

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
};                                   // =128 bytes header (80 default + 48 primemultiplier)

size_t thread_num_max;
boost::asio::ip::tcp::socket* socket_to_server;

/*********************************
* helping functions
*********************************/

void exit_handler() {
  std::cout << "cleanup!" << std::endl;
  if (socket_to_server != NULL) socket_to_server->close();
}

void convertDataToBlock(unsigned char* blockData, CBlock& block) {
  {
    std::stringstream ss;
    for (int i = 7; i >= 0; --i)
		ss << std::setw(8) << std::setfill('0') << std::hex << *((int *)(blockData + 4) + i);
    ss.flush();
    block.hashPrevBlock.SetHex(ss.str().c_str());
  }
  {
    std::stringstream ss;
    for (int i = 7; i >= 0; --i)
		ss << std::setw(8) << std::setfill('0') << std::hex << *((int *)(blockData + 36) + i);
    ss.flush();
    block.hashMerkleRoot.SetHex(ss.str().c_str());
  }
  block.nVersion               = *((int *)(blockData));
  block.nTime                  = *((unsigned int *)(blockData + 68));
  block.nBits                  = *((unsigned int *)(blockData + 72));
  block.nNonce                 = *((unsigned int *)(blockData + 76));
  block.bnPrimeChainMultiplier = 0;
}

/*********************************
* class CBlockProviderGW to (incl. SUBMIT_BLOCK)
*********************************/
  
class CBlockProviderGW : public CBlockProvider {
public:

	CBlockProviderGW() : CBlockProvider(), _blocks(NULL) {}

	virtual ~CBlockProviderGW() { /* TODO */ }

	virtual CBlock* getBlock(unsigned int thread_id) {
		boost::unique_lock<boost::shared_mutex> lock(_mutex_getwork);
		if (_blocks == NULL) return NULL;
		CBlock* block = NULL;
		block = new CBlock((_blocks+thread_id)->GetBlockHeader());
		block->nTime = GetAdjustedTime();
		//std::cout << "[WORKER" << thread_id << "] got_work block=" << block->GetHash().ToString().c_str() << std::endl;
		return block;
	}
	
	void setBlocksFromData(unsigned char* data) {
		CBlock* blocks = new CBlock[thread_num_max];
		for (size_t i = 0; i < thread_num_max; ++i)
			convertDataToBlock(data+i*128,blocks[i]);
		CBlock* old_blocks = NULL;
		{
			boost::unique_lock<boost::shared_mutex> lock(_mutex_getwork);
			old_blocks = _blocks;
			_blocks = blocks;
		}
		if (old_blocks != NULL) delete[] old_blocks;
	}
	
	void submitBlock(CBlock *block) {
		blockHeader_t blockraw;
		blockraw.nVersion       = block->nVersion;
		blockraw.hashPrevBlock  = block->hashPrevBlock;
		blockraw.hashMerkleRoot = block->hashMerkleRoot;
		blockraw.nTime          = block->nTime;
		blockraw.nBits          = block->nBits;
		blockraw.nNonce         = block->nNonce;
		
		//std::cout << "submit: " << block->hashMerkleRoot.ToString().c_str() << std::endl;
		
		std::vector<unsigned char> primemultiplier = block->bnPrimeChainMultiplier.getvch();
		if (primemultiplier.size() > 47) {
			std::cerr << "[WORKER] share submission warning: not enough space for primemultiplier" << std::endl;
			return;
		}
		blockraw.primemultiplier[0] = primemultiplier.size();
		for (size_t i = 0; i < primemultiplier.size(); ++i)
			blockraw.primemultiplier[1 + i] = primemultiplier[i];

		boost::system::error_code error;
		while (socket_to_server == NULL)
			boost::this_thread::sleep(boost::posix_time::seconds(1));
		socket_to_server->write_some(boost::asio::buffer((unsigned char*)&blockraw, 128));
		if (error)
			std::cout << error << " @ write_some_submit" << std::endl;
	}

protected:
	boost::shared_mutex _mutex_getwork;
	CBlock* _blocks;
};

/*********************************
* multi-threading
*********************************/

class CMasterThreadStub {
public:
  virtual void wait_for_master() = 0;
  virtual boost::shared_mutex& get_working_lock() = 0;
};

class CWorkerThread { // worker=miner
public:

	CWorkerThread(CMasterThreadStub *master, unsigned int id, CBlockProviderGW *bprovider)
		: _working_lock(NULL), _id(id), _master(master), _bprovider(bprovider), _thread(&CWorkerThread::run, this) { }

	void run() {
		std::cout << "[WORKER" << _id << "] Hello, World!" << std::endl;
		_master->wait_for_master();
		std::cout << "[WORKER" << _id << "] GoGoGo!" << std::endl;
		boost::this_thread::sleep(boost::posix_time::seconds(2));
		BitcoinMiner(NULL, _bprovider, _id);
		std::cout << "[WORKER" << _id << "] Bye Bye!" << std::endl;
	}

	void work() { // called from within master thread
		_working_lock = new boost::shared_lock<boost::shared_mutex>(
		_master->get_working_lock());
	}

protected:
  boost::shared_lock<boost::shared_mutex> *_working_lock;
  unsigned int _id;
  CMasterThreadStub *_master;
  CBlockProviderGW  *_bprovider;
  boost::thread _thread;
};

class CMasterThread : public CMasterThreadStub {
public:

  CMasterThread(CBlockProviderGW *bprovider) : CMasterThreadStub(), _bprovider(bprovider) {}

  void run() {
  
	{
		boost::unique_lock<boost::shared_mutex> lock(_mutex_master);
		std::cout << "spawning " << thread_num_max << " worker thread(s)" << std::endl;

		for (unsigned int i = 0; i < thread_num_max; ++i) {
			CWorkerThread *worker = new CWorkerThread(this, i, _bprovider);
			worker->work();
		}
	}
	
    boost::asio::io_service io_service;
    boost::asio::ip::tcp::resolver resolver(io_service); //resolve dns
    boost::asio::ip::tcp::resolver::query query(GetArg("-poolip", "127.0.0.1"), GetArg("-poolport", "1337"));
    boost::asio::ip::tcp::resolver::iterator endpoint;
	boost::asio::ip::tcp::resolver::iterator end;
	
	for (;;) {
		endpoint = resolver.resolve(query);
		boost::scoped_ptr<boost::asio::ip::tcp::socket> socket;
		boost::system::error_code error_socket = boost::asio::error::host_not_found;
		while (error_socket && endpoint != end)
		{
		  //socket.close();
		  socket.reset(new boost::asio::ip::tcp::socket(io_service));
		  socket->connect(*endpoint++, error_socket);
		}
		
		if (error_socket) {
			std::cout << error_socket << std::endl;
			boost::this_thread::sleep(boost::posix_time::seconds(10));
			continue;
		}
		
		{ //send hello message
			std::string username = GetArg("-pooluser", "");
			char* hello = new char[username.length()+2];
			memcpy(hello+1, username.c_str(), username.length());
			*((unsigned char*)hello) = username.length();
			*((unsigned char*)hello+username.length()+1) = thread_num_max;
			boost::system::error_code error;
			socket->write_some(boost::asio::buffer(hello, username.length()+2), error);
			if (error)
				std::cout << error << " @ write_some_hello" << std::endl;
			delete[] hello;
		}
		
		socket_to_server = socket.get(); //TODO: lock/mutex
			
		bool done = false;
		while (!done) {
			int type = -1;
			{ //get the data header
				unsigned char buf = 0; //get header
				boost::system::error_code error;
				size_t len = socket->read_some(boost::asio::buffer(&buf, 1), error);				
				if (error == boost::asio::error::eof)
					break; // Connection closed cleanly by peer.
				else if (error) {
					std::cout << error << " @ read_some1" << std::endl;
					break;
				}					
				type = buf;
				if (len != 1)
					std::cout << "error on read_some1: " << len << " should be " << 1 << std::endl;					
			}
			
			switch (type) {
				case 0: {
					size_t buf_size = 128*thread_num_max;
					unsigned char* buf = new unsigned char[buf_size]; //get header
					boost::system::error_code error;
					size_t len = socket->read_some(boost::asio::buffer(buf, buf_size), error);
					while (len < buf_size)
						len += socket->read_some(boost::asio::buffer(buf+len, buf_size-len), error);
					if (error == boost::asio::error::eof) {
						done = true;
						break; // Connection closed cleanly by peer.
					} else if (error) {
						std::cout << error << " @ read_some2a" << std::endl;
						done = true;
						break;
					}
					if (len == buf_size) {
						_bprovider->setBlocksFromData(buf);
						std::cout << "[MASTER] work received" << std::endl;
					} else
						std::cout << "error on read_some2a: " << len << " should be " << buf_size << std::endl;					
					delete[] buf;					
					CBlockIndex *pindexOld = pindexBest;
					pindexBest = new CBlockIndex(); //=notify worker (this could need a efficient alternative)
					delete pindexOld;
					
				} break;
				case 1: {
					size_t buf_size = 4;
					int buf; //get header
					boost::system::error_code error;
					size_t len = socket->read_some(boost::asio::buffer(&buf, buf_size), error);				
					while (len < buf_size)
						len += socket->read_some(boost::asio::buffer(&buf+len, buf_size-len), error);
					if (error == boost::asio::error::eof) {
						done = true;
						break; // Connection closed cleanly by peer.
					} else if (error) {
						std::cout << error << " @ read_some2b" << std::endl;
						done = true;
						break;
					}
					if (len == buf_size) {
						int retval = buf;
						std::cout << "[MASTER] submitted share -> " <<
							(retval == 0 ? "REJECTED" : retval < 0 ? "STALE" : retval ==
							1 ? "BLOCK" : "SHARE") << std::endl;
					} else
						std::cout << "error on read_some2b: " << len << " should be " << buf_size << std::endl;					
				} break;
				case 2: {
					//PING-PONG EVENT, nothing to do
				} break;
				default: {
					std::cout << "unknown header type = " << type << std::endl;
				}
			}
		}
		
		socket_to_server = NULL; //TODO: lock/mutex
		boost::this_thread::sleep(boost::posix_time::seconds(10));
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
  
  CBlockProviderGW  *_bprovider;

  boost::shared_mutex _mutex_master;
  boost::shared_mutex _mutex_working;
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
  
  const int atexit_res = std::atexit(exit_handler);
  if (atexit_res != 0)
    std::cerr << "atexit registration failed, shutdown will be dirty!" << std::endl;
  
  // init everything:
  ParseParameters(argc, argv);
  
  socket_to_server = NULL;
  thread_num_max = GetArg("-genproclimit", 1); // what about boost's 
                                               // hardware_concurrency() ?
  fPrintToConsole = true; // always on
  fDebug          = GetBoolArg("-debug");

  pindexBest = new CBlockIndex();

  GeneratePrimeTable();
  
  // ok, start mining:
  CBlockProviderGW* bprovider = new CBlockProviderGW();
  CMasterThread *mt = new CMasterThread(bprovider);
  mt->run();
  
  // end:
  return EXIT_SUCCESS;
}

/*********************************
* and this is where it ends
*********************************/
