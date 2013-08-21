
#include <iostream>
#include <fstream>
#include <cstdio>

#include "prime.h"
#include "serialize.h"
#include "bitcoinrpc.h"
#include "json/json_spirit_value.h"
#include <boost/thread.hpp>

//<START> be compatible to original code (not actually used!)
#include "txdb.h"
#include "walletdb.h"
#include "wallet.h"
#include "ui_interface.h"
CWallet* pwalletMain;
CClientUIInterface uiInterface;
void StartShutdown() {
  exit(0);
}
//<END>

/*********************************
* global variables, structs and extern functions
*********************************/

extern CBlockIndex* pindexBest; //*TODO* use this in master thread
extern void BitcoinMiner(CWallet *pwallet, CBlockProvider *CBlockProvider);
extern bool fPrintToConsole;
extern bool fDebug;
extern json_spirit::Object CallRPC(const std::string& strMethod, const json_spirit::Array& params, const std::string& server, const std::string& port);
extern int FormatHashBlocks(void* pbuffer, unsigned int len);

struct blockHeader_t { 
  //comments: BYTES <index> + <length>
	int  nVersion;   // 0+4
	uint256	hashPrevBlock; // 4+32
	uint256	hashMerkleRoot;    //36+32
	unsigned int  nTime; //68+4
	unsigned int  nBits;     //72+4
	unsigned int  nNonce;     //76+4
	unsigned char primemultiplier[48]; //80+48
}; //=128 bytes header (80 default + 48 primemultiplier)

/*********************************
* helping functions
*********************************/

unsigned int getHexDigitValue(unsigned char c) {
	if( c >= '0' && c <= '9' )
		return c-'0';
	else if( c >= 'a' && c <= 'f' )
		return c-'a'+10;
	else if( c >= 'A' && c <= 'F' )
		return c-'A'+10;
	return 0;
}

void parseHexString(const char* hexString, unsigned int length, unsigned char* output) {
	unsigned int lengthBytes = length / 2;
	for(unsigned int i=0; i<lengthBytes; ++i) {
		// high digit
		unsigned int d1 = getHexDigitValue(hexString[i*2+0]);
		// low digit
		unsigned int d2 = getHexDigitValue(hexString[i*2+1]);
		// build byte
		output[i] = (unsigned char)((d1<<4)|(d2));	
	}
}

/*********************************
* GET_WORK from server and convert to mineable block
*********************************/

bool getBlockFromServer(CBlock& pblock, const std::string& server, const std::string& port) {
  unsigned char localBlockData[128];
  { //JSON GETWORK
    std::string strMethod = "getwork";
    std::vector<std::string> strParams;
    json_spirit::Array params = RPCConvertValues(strMethod, strParams);
    json_spirit::Object reply_obj = CallRPC(strMethod, params, server, port); //request
    
    //parse reply
    const json_spirit::Value& result_val = find_value(reply_obj, "result");
    const json_spirit::Value& error_val  = find_value(reply_obj, "error");

    if (error_val.type() != json_spirit::null_type) {
      //error code recieved
      std::cerr << "[JSON_REQUEST] " << write_string(error_val, false) << std::endl;
      return false;
    } else {
      //result
      std::string strValue;
      if (result_val.type() == json_spirit::null_type) {
        std::cerr << "[JSON_REQUEST] reply empty" << std::endl;
        return false;
      } else if (result_val.type() == json_spirit::str_type)
        strValue = result_val.get_str();
      else
        strValue = write_string(result_val, true);
      //std::cout << "[JSON_REQUEST] result(" << strValue.length() << "): " << std::endl << strValue << std::endl;
      
      const json_spirit::Object& result_obj = result_val.get_obj();
      const json_spirit::Value& data_val = find_value(result_obj, "data");
      
      if (data_val.type() == json_spirit::null_type) {
        std::cerr << "[JSON_REQUEST] result empty" << std::endl;
        return false;
      } else if (data_val.type() == json_spirit::str_type)
        strValue = data_val.get_str();
      else
        strValue = write_string(data_val, true);
      //std::cout << "[JSON_REQUEST] data(" << strValue.length() << "): " << std::endl << strValue << std::endl;
      
      if (strValue.length() != 256) {
        std::cerr << "[JSON_REQUEST] data length != 256" << std::endl;
        return false;
      }
      
      parseHexString(strValue.c_str(), 256, localBlockData);
      
      for (unsigned int i = 0; i < 128/4; ++i)
        ((unsigned int*)localBlockData)[i] = ByteReverse(((unsigned int*)localBlockData)[i]);
    }
  }  
  
  {
    std::stringstream ss;
    for (int i=7; i>=0; --i)
	  ss << std::setw(8) << std::setfill('0') << std::hex << *((int*)(localBlockData+4)+i);
    ss.flush();
    pblock.hashPrevBlock.SetHex(ss.str().c_str());
  }  
  {
    std::stringstream ss;
    for (int i=7; i>=0; --i)
	  ss << std::setw(8) << std::setfill('0') << std::hex << *((int*)(localBlockData+36)+i);
    ss.flush();
    pblock.hashMerkleRoot.SetHex(ss.str().c_str());
  }
  
  pblock.nVersion = *((int*)(localBlockData));
  pblock.nTime  = *((unsigned int*)(localBlockData+68));
  pblock.nBits  = *((unsigned int*)(localBlockData+72));
  pblock.nNonce = *((unsigned int*)(localBlockData+76));
  pblock.bnPrimeChainMultiplier = 0;

  return true;
}

/*********************************
* class CBlockProviderGW to
*********************************/

class CBlockProviderGW : public CBlockProvider {
public:
	CBlockProviderGW(const std::string& server, const std::string& port, int thread_id)
	 : CBlockProvider(), _pblock(NULL), _server(server), _port(port), _thread_id(thread_id) { }
	virtual ~CBlockProviderGW() { }
	virtual CBlock* getBlock() {
		if (_pblock != NULL)
			delete _pblock;
		_pblock = new CBlock();
		if (!getBlockFromServer(*_pblock, _server, _port))
			return NULL;
		_pblock->nTime += _thread_id; //use timestamp for multithreading
		std::cout << "[WORKER" << _thread_id << "] got_work block=" << _pblock->GetHash().ToString().c_str() << std::endl;
		return _pblock;
	}
	virtual void submitBlock(CBlock* pblock) {
		std::string strMethod = "getwork";
		std::vector<std::string> strParams;
		//build block data
		blockHeader_t block;
		block.nVersion       = pblock->nVersion;
		block.hashPrevBlock  = pblock->hashPrevBlock;
		block.hashMerkleRoot = pblock->hashMerkleRoot;
		block.nTime          = pblock->nTime;
		block.nBits          = pblock->nBits;
		block.nNonce         = pblock->nNonce;
		//block.bnPrimeChainMultiplier = pblock->bnPrimeChainMultiplier;
		std::vector<unsigned char> primemultiplier = pblock->bnPrimeChainMultiplier.getvch();
		if (primemultiplier.size() > 47)
			std::cerr << "[WORKER" << _thread_id << "] share submission warning: not enough space for primemultiplier" << std::endl;
		block.primemultiplier[0] = primemultiplier.size();
		for (size_t i = 0; i < primemultiplier.size(); ++i)
			block.primemultiplier[1+i] = primemultiplier[i];
		//FormatHashBlocks(&block, sizeof(block)); //not used, unnecessary
		for (unsigned int i = 0; i < 128/4; ++i)
		  ((unsigned int*)&block)[i] = ByteReverse(((unsigned int*)&block)[i]);
		char pdata[128];
		memcpy(pdata, &block, 128);
		std::string data_hex = HexStr(BEGIN(pdata), END(pdata));
		//std::cout << "[JSON_REQUEST] SUBMIT data(" << data_hex.length() << "): " << std::endl << data_hex << std::endl;
		strParams.push_back(data_hex);
		json_spirit::Array params = RPCConvertValues(strMethod, strParams);
		json_spirit::Object reply_obj = CallRPC(strMethod, params, _server, _port); //submit
		const json_spirit::Value& result_val = find_value(reply_obj, "result");
		int retval = 0;
		if (result_val.type() == json_spirit::int_type)
			retval = result_val.get_int();
		std::cout << "[WORKER" << _thread_id << "] share submitted -> " << (retval == 0 ? "REJECTED" : retval < 0 ? "STALE" : retval == 1 ? "BLOCK" : "SHARE") << std::endl;
	}
private:
	CBlock* _pblock;
	std::string _server;
	std::string _port;
	int _thread_id;
};

/*********************************
* multi-threading
*********************************/

class CMasterThreadStub {
public:
  virtual void wait_for_master() = 0;
  virtual boost::shared_mutex& get_working_lock() = 0;
};

class CWorkerThread { //worker=miner
public:
   CWorkerThread(CMasterThreadStub* master, int id) : _working_lock(NULL), _id(id), _master(master), _thread(&CWorkerThread::run, this) { }
   void run() {
      std::cout << "[WORKER" << _id << "] Hello World!" << std::endl;
      _master->wait_for_master();
      std::cout << "[WORKER" << _id << "] GoGoGo!" << std::endl;
	  CBlockProviderGW* bprovider;
      BitcoinMiner(NULL, bprovider = new CBlockProviderGW(GetArg("-poolip", "127.0.0.1"), GetArg("-poolport", "9912"), _id));
      delete bprovider;
      std::cout << "[WORKER" << _id << "] Bye Bye!" << std::endl;
   }
   void work() { //called from within master thread
	  _working_lock = new boost::shared_lock<boost::shared_mutex>(_master->get_working_lock());
   }
private:
   boost::shared_lock<boost::shared_mutex>* _working_lock;
   int _id;
   CMasterThreadStub* _master;
   boost::thread _thread;
};

class CMasterThread : public CMasterThreadStub {
public:
	CMasterThread() : CMasterThreadStub() { }
	void run() {
	  int num_threads_to_use = GetArg("-genproclimit", 1);
	  {
        boost::unique_lock<boost::shared_mutex> lock(_mutex_master);
        std::cout << "spawning " << num_threads_to_use << " worker thread(s)" << std::endl;
        for (int i = 0; i < num_threads_to_use; ++i)
        {
          CWorkerThread* worker = new CWorkerThread(this, i);
          worker->work(); //set working lock
        }
      }
      // WORKER WILL START HERE implicitly by destroying the lock on mutex_master
      // this part is very tricky, btw there's a minimum chance everything crashs here ;-) good luck
	  wait_for_workers(); //TODO: replace this by a loop to check for a new block and update pindexBest
	}
	~CMasterThread() { }
	void wait_for_master() { boost::shared_lock<boost::shared_mutex> lock(_mutex_master); }
	boost::shared_mutex& get_working_lock() { return _mutex_working; }
private:
	void wait_for_workers() { boost::unique_lock<boost::shared_mutex> lock(_mutex_working); }
	boost::shared_mutex _mutex_master;
	boost::shared_mutex _mutex_working;
};

/*********************************
* main - this is where it begins
*********************************/

int main (int argc, char** argv)
{
  if (argc < 2)
  {
    std::cerr << "usage: " << argv[0] << " -poolip=<ip> -poolport=<port> -pooluser=<user> -poolpassword=<password>" << std::endl;
    return EXIT_FAILURE;
  }
  
  //init everything:  
  ParseParameters(argc, argv);
  
  fPrintToConsole = true; //always on
  fDebug = GetBoolArg("-debug");
  
  pindexBest = new CBlockIndex();
  
  GeneratePrimeTable();
  
  //ok, start mining:
  CMasterThread* mt = new CMasterThread(); //<- the magic is in there
  mt->run();

  //end:
  return EXIT_SUCCESS;
}

/*********************************
* and this is where it ends
*********************************/
