//===
// by xolokram/TB
// 2014 *yey*
//===

#include <iostream>
#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <map>
#include <boost/uuid/sha1.hpp>

#include "main_poolminer.h"

#if defined(__GNUG__) && !defined(__MINGW32__) && !defined(__MINGW64__)
#include <sys/syscall.h>
#include <sys/time.h> //depr?
#include <sys/resource.h>
#elif defined(__MINGW32__) || defined(__MINGW64__)
#include <windows.h>
#endif

#define VERSION_MAJOR 0
#define VERSION_MINOR 9
#define VERSION_EXT "RC1 <experimental>"

/*********************************
* global variables, structs and extern functions
*********************************/

bool running;
size_t thread_num_max;
static size_t fee_to_pay;
static size_t miner_id;
static boost::asio::ip::tcp::socket* socket_to_server; //connection socket
static boost::posix_time::ptime t_start; //for stats
uint64 totalShareCount; //^
static std::map<int,unsigned long> statistics; //^
static volatile int submitting_share;
std::string pool_username;
std::string pool_password;

/*********************************
* helping functions
*********************************/

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

	CBlockProviderGW() : CBlockProvider(), nTime_offset(0), _block(NULL) {}

	virtual ~CBlockProviderGW() { /* TODO */ }

	virtual unsigned int GetAdjustedTimeWithOffset(unsigned int thread_id, unsigned int counter) {
		return nTime_offset + ((((unsigned int)time(NULL) + thread_num_max) / thread_num_max) * thread_num_max) + thread_id + counter * thread_num_max;
	}
	
	virtual CBlock* getBlock(unsigned int thread_id, unsigned int last_time, unsigned int counter) {
		CBlock* block = NULL;
		{
			boost::shared_lock<boost::shared_mutex> lock(_mutex_getwork);
			if (_block == NULL) return NULL;
			block = new CBlock(*_block);
			//memcpy(block, _block, 80+32+8);
		}
		block->nTime = GetAdjustedTimeWithOffset(thread_id, counter);
		//std::cout << "[WORKER" << thread_id << "] block created @ " << new_time << std::endl;
		return block;
	}
	
	virtual CBlock* getOriginalBlock() {
		//boost::shared_lock<boost::shared_mutex> lock(_mutex_getwork);
		return _block;
	}
	
	virtual void setBlockTo(CBlock* newblock) {
		CBlock* old_block = NULL;
		{
			boost::unique_lock<boost::shared_mutex> lock(_mutex_getwork);
			old_block = _block;
			_block = newblock;
		}
		if (old_block != NULL) delete old_block;
	}

	void setBlocksFromData(unsigned char* data) {
		CBlock* block = new CBlock();
		//
		convertDataToBlock(data,*block);
		//
		unsigned int nTime_local = time(NULL);
		unsigned int nTime_server = block->nTime;
		nTime_offset = nTime_local > nTime_server ? 0 : (nTime_server-nTime_local);
		//
		setBlockTo(block);
	}

	void submitBlock(CBlock *block, unsigned int thread_id) {
		if (socket_to_server != NULL) {
			blockHeader_t blockraw;
			blockraw.nVersion       = block->nVersion;
			blockraw.hashPrevBlock  = block->hashPrevBlock;
			blockraw.hashMerkleRoot = block->hashMerkleRoot;
			blockraw.nTime          = block->nTime;
			blockraw.nBits          = block->nBits;
			blockraw.nNonce         = block->nNonce;

			//std::cout << "submitting: " << block->hashMerkleRoot.ToString().c_str() << " from " << thread_id << std::endl;

			std::vector<unsigned char> primemultiplier = block->bnPrimeChainMultiplier.getvch();
			if (primemultiplier.size() > 47) {
				std::cerr << "[WORKER] share submission warning: not enough space for primemultiplier" << std::endl;
				return;
			}

			blockraw.primemultiplier[0] = primemultiplier.size();
			for (size_t i = 0; i < primemultiplier.size(); ++i)
				blockraw.primemultiplier[1 + i] = primemultiplier[i];		

			if (socket_to_server == NULL)
				return;

			std::cout << "[WORKER] share found @ " << blockraw.nTime << " by thr" << thread_id << std::endl;
			boost::system::error_code submit_error = boost::asio::error::host_not_found;
			if (socket_to_server != NULL) boost::asio::write(*socket_to_server, boost::asio::buffer((unsigned char*)&blockraw, 128), boost::asio::transfer_all(), submit_error); //FaF
			//if (submit_error)
			//	std::cout << submit_error << " @ submit" << std::endl;
			if (!submit_error)
				++totalShareCount;
		}
	}

protected:
	unsigned int nTime_offset;
	boost::shared_mutex _mutex_getwork;
	CBlock* _block;
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
		: _working_lock(NULL), _id(id), _master(master), _bprovider(bprovider), _thread(&CWorkerThread::run, this) {
	}

	void run() {
		std::cout << "[WORKER" << _id << "] Hello, World!" << std::endl;
		{
			//<set_low_priority>
#if defined(__GNUG__) && !defined(__MINGW32__) && !defined(__MINGW64__)
			pid_t tid = (pid_t) syscall (SYS_gettid);
			setpriority(PRIO_PROCESS, tid, 1);
#elif defined(__MINGW32__) || defined(__MINGW64__)
			HANDLE th = _thread.native_handle();
			if (!SetThreadPriority(th, THREAD_PRIORITY_LOWEST))
				std::cerr << "failed to set thread priority to low" << std::endl;
#endif
			//</set_low_priority>
		}
		_master->wait_for_master();
		std::cout << "[WORKER" << _id << "] GoGoGo!" << std::endl;
		boost::this_thread::sleep(boost::posix_time::seconds(1));
		primecoin_mine<SPHLIB>(_bprovider,_id); //TODO: optimize the code using SPH,SSE,AVX,etc.pp. #1/2
		std::cout << "[WORKER" << _id << "] Bye Bye!" << std::endl;
	}

	void work() { // called from within master thread
		_working_lock = new boost::shared_lock<boost::shared_mutex>(_master->get_working_lock());
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
			boost::unique_lock<boost::shared_mutex> lock(_mutex_master); //only in this scope
			std::cout << "spawning " << thread_num_max << " worker thread(s)" << std::endl;

			for (unsigned int i = 0; i < thread_num_max; ++i) {
				CWorkerThread *worker = new CWorkerThread(this, i, _bprovider);
				worker->work(); //spawn thread(s)
			}
		}

		boost::asio::io_service io_service;
		boost::asio::ip::tcp::resolver resolver(io_service); //resolve dns
		boost::asio::ip::tcp::resolver::iterator endpoint;
		boost::asio::ip::tcp::resolver::iterator end;
		boost::asio::ip::tcp::no_delay nd_option(true);
		boost::asio::socket_base::keep_alive ka_option(true);

		unsigned char poolnum = 0;
		while (running) {
			boost::asio::ip::tcp::resolver::query query(
				(poolnum == 0) ? GetArg("-poolip", "127.0.0.1") :
				(poolnum == 1 && GetArg("-poolip2", "").length() > 0) ? GetArg("-poolip2", "") :
				(poolnum == 2 && GetArg("-poolip3", "").length() > 0) ? GetArg("-poolip3", "") :
				GetArg("-poolip", "127.0.0.1")
				,
				(poolnum == 0) ? GetArg("-poolport", "1337") :
				(poolnum == 1 && GetArg("-poolport2", "").length() > 0) ? GetArg("-poolport2", "") :
				(poolnum == 2 && GetArg("-poolport3", "").length() > 0) ? GetArg("-poolport3", "") :
				GetArg("-poolport", "1337")
			);
			poolnum = (poolnum + 1) % 3;
			endpoint = resolver.resolve(query);
			boost::scoped_ptr<boost::asio::ip::tcp::socket> socket;
			boost::system::error_code error_socket = boost::asio::error::host_not_found;
			while (error_socket && endpoint != end)
			{
				//socket->close();
				socket.reset(new boost::asio::ip::tcp::socket(io_service));
				boost::asio::ip::tcp::endpoint tcp_ep = *endpoint++;
				std::cout << "connecting to " << tcp_ep << std::endl;
				socket->connect(tcp_ep, error_socket);			
			}
			socket->set_option(nd_option);
			socket->set_option(ka_option);

			if (error_socket) {
				std::cout << error_socket << std::endl;
				boost::this_thread::sleep(boost::posix_time::seconds(10));
				if (GetArg("-exitondisc", 0) == 1)
					running = false;
				continue;
			} else {
				t_start = boost::posix_time::second_clock::local_time();
				totalShareCount = 0;
			}

			{ //send hello message
				char* hello = new char[pool_username.length()+/*v0.2/0.3=*/2+/*v0.4=*/20+/*v0.7=*/1+pool_password.length()];
				memcpy(hello+1, pool_username.c_str(), pool_username.length());
				*((unsigned char*)hello) = pool_username.length();
				*((unsigned char*)(hello+pool_username.length()+1)) = 0; //hi, i'm v0.4+
				*((unsigned char*)(hello+pool_username.length()+2)) = VERSION_MAJOR;
				*((unsigned char*)(hello+pool_username.length()+3)) = VERSION_MINOR;
				*((unsigned char*)(hello+pool_username.length()+4)) = thread_num_max;
				*((unsigned char*)(hello+pool_username.length()+5)) = fee_to_pay;
				*((unsigned short*)(hello+pool_username.length()+6)) = miner_id;
				*((unsigned int*)(hello+pool_username.length()+8)) = 0; //TODO: nSieveExtensions;
				*((unsigned int*)(hello+pool_username.length()+12)) = 0; //TODO: nSievePercentage;
				*((unsigned int*)(hello+pool_username.length()+16)) = 0; //TODO: nSieveSize;
				*((unsigned char*)(hello+pool_username.length()+20)) = pool_password.length();
				memcpy(hello+pool_username.length()+21, pool_password.c_str(), pool_password.length());
				*((unsigned short*)(hello+pool_username.length()+21+pool_password.length())) = 0; //EXTENSIONS
				boost::system::error_code error;
				socket->write_some(boost::asio::buffer(hello, pool_username.length()+2+20+1+pool_password.length()), error);
				//if (error)
				//	std::cout << error << " @ write_some_hello" << std::endl;
				delete[] hello;
			}

			socket_to_server = socket.get(); //TODO: lock/mutex

			int reject_counter = 0;
			bool done = false;
			while (!done) {
				int type = -1;
				{ //get the data header
					unsigned char buf = 0; //get header
					boost::system::error_code error;
					size_t len = boost::asio::read(*socket_to_server, boost::asio::buffer(&buf, 1), boost::asio::transfer_all(), error);
					if (error == boost::asio::error::eof)
						break; // Connection closed cleanly by peer.
					else if (error) {
						//std::cout << error << " @ read_some1" << std::endl;
						break;
					}
					type = buf;
					if (len != 1)
						std::cout << "error on read1: " << len << " should be " << 1 << std::endl;
				}
				switch (type) {
					case 0: {
						size_t buf_size = 128;
						unsigned char* buf = new unsigned char[buf_size]; //get header
						boost::system::error_code error;
						size_t len = boost::asio::read(*socket_to_server, boost::asio::buffer(buf, buf_size), boost::asio::transfer_all(), error);
						if (error == boost::asio::error::eof) {
							done = true;
							break; // Connection closed cleanly by peer.
						} else if (error) {
							//std::cout << error << " @ read2a" << std::endl;
							done = true;
							break;
						}
						if (len == buf_size) {
							_bprovider->setBlocksFromData(buf);
							std::cout << "[MASTER] work received" << std::endl;
							//TODO:
							//if (_bprovider->getOriginalBlock() != NULL) print256("sharetarget", (uint32_t*)(_bprovider->getOriginalBlock()->targetShare));
							//else std::cout << "<NULL>" << std::endl;
						} else
							std::cout << "error on read2a: " << len << " should be " << buf_size << std::endl;
						delete[] buf;
						//TODO: check prime.cpp
						CBlockIndex *pindexOld = pindexBest;
						pindexBest = new CBlockIndex(); //=notify worker (this could need a efficient alternative)
						delete pindexOld;
					} break;
					case 1: {
						size_t buf_size = 4;
						int buf; //get header
						boost::system::error_code error;
						size_t len = boost::asio::read(*socket_to_server, boost::asio::buffer(&buf, buf_size), boost::asio::transfer_all(), error);
						if (error == boost::asio::error::eof) {
							done = true;
							break; // Connection closed cleanly by peer.
						} else if (error) {
							//std::cout << error << " @ read2b" << std::endl;
							done = true;
							break;
						}
						if (len == buf_size) {
							int retval = buf > 100000 ? 1 : buf;
							std::cout << "[MASTER] submitted share -> " << (retval == 0 ? "REJECTED" : retval < 0 ? "STALE" : retval == 1 ? "BLOCK" : "SHARE") << std::endl;
							if (retval > 0)
								reject_counter = 0;
							else
								reject_counter++;
							if (reject_counter >= 3) {
								std::cout << "too many rejects (3) in a row, forcing reconnect." << std::endl;
								socket->close();
								done = true;
							}
							{
								std::map<int,unsigned long>::iterator it = statistics.find(retval);
								if (it == statistics.end())
									statistics.insert(std::pair<int,unsigned long>(retval,1));
								else
									statistics[retval]++;
								stats_running();
							}
						} else
							std::cout << "error on read2b: " << len << " should be " << buf_size << std::endl;
					} break;
					case 2: {
						//PING-PONG EVENT, nothing to do
					} break;
					default: {
						//std::cout << "unknown header type = " << type << std::endl;
					}
				}
			}

			_bprovider->setBlockTo(NULL);
			socket_to_server = NULL; //TODO: lock/mutex		
			if (GetArg("-exitondisc", 0) == 1) {
				running = false;
			} else {
				std::cout << "no connection to the server, reconnecting in 10 seconds" << std::endl;
				boost::this_thread::sleep(boost::posix_time::seconds(10));
			}
		}
	}

	~CMasterThread() {} //TODO: <-- this

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
	
	// Provides real time stats
	void stats_running() {
		if (!running) return;
		std::cout << std::fixed;
		std::cout << std::setprecision(1);
		boost::posix_time::ptime t_end = boost::posix_time::second_clock::local_time();
		unsigned long rejects = 0;
		unsigned long stale = 0;
		unsigned long valid = 0;
		unsigned long blocks = 0;
		for (std::map<int,unsigned long>::iterator it = statistics.begin(); it != statistics.end(); ++it) {
			if (it->first < 0) stale += it->second;
			if (it->first == 0) rejects = it->second;
			if (it->first == 1) blocks = it->second;
			if (it->first > 1) valid += it->second;
		}
		std::cout << "[STATS] " << boost::posix_time::second_clock::local_time() << " | ";
		for (std::map<int,unsigned long>::iterator it = statistics.begin(); it != statistics.end(); ++it)
			if (it->first > 1)
				std::cout << it->first << "-CH: " << it->second << " (" <<
				  ((valid+blocks > 0) ? (static_cast<double>(it->second) / static_cast<double>(valid+blocks)) * 100.0 : 0.0) << "% | " <<
				  ((valid+blocks > 0) ? (static_cast<double>(it->second) / (static_cast<double>((t_end - t_start).total_seconds()) / 3600.0)) : 0.0) << "/h), ";
		if (valid+blocks+rejects+stale > 0) {
			std::cout << "VL: " << valid+blocks << " (" << (static_cast<double>(valid+blocks) / static_cast<double>(valid+blocks+rejects+stale)) * 100.0 << "%), ";
			std::cout << "RJ: " << rejects << " (" << (static_cast<double>(rejects) / static_cast<double>(valid+blocks+rejects+stale)) * 100.0 << "%), ";
			std::cout << "ST: " << stale << " (" << (static_cast<double>(stale) / static_cast<double>(valid+blocks+rejects+stale)) * 100.0 << "%)" << std::endl;
		} else {
			std::cout <<  "VL: " << 0 << " (" << 0.0 << "%), ";
			std::cout <<  "RJ: " << 0 << " (" << 0.0 << "%), ";
			std::cout <<  "ST: " << 0 << " (" << 0.0 << "%)" << std::endl;
		}
	}
};

/*********************************
* exit / end / shutdown
*********************************/

void exit_handler() {
	//cleanup for not-retarded OS
	if (socket_to_server != NULL) {
		socket_to_server->close();
		socket_to_server = NULL;
	}
	running = false;
}

#if defined(__MINGW32__) || defined(__MINGW64__)

//#define WIN32_LEAN_AND_MEAN
//#include <windows.h>

BOOL WINAPI ctrl_handler(DWORD dwCtrlType) {
	//'special' cleanup for windows
	switch(dwCtrlType) {
		case CTRL_C_EVENT:
		case CTRL_BREAK_EVENT: {
			if (socket_to_server != NULL) {
				socket_to_server->close();
				socket_to_server = NULL;
			}
			running = false;
		} break;
		default: break;
	}
	return FALSE;
}

#elif defined(__GNUG__) && !defined(__APPLE__)

static sighandler_t set_signal_handler (int signum, sighandler_t signalhandler) {
   struct sigaction new_sig, old_sig;
   new_sig.sa_handler = signalhandler;
   sigemptyset (&new_sig.sa_mask);
   new_sig.sa_flags = SA_RESTART;
   if (sigaction (signum, &new_sig, &old_sig) < 0)
      return SIG_ERR;
   return old_sig.sa_handler;
}

void ctrl_handler(int signum) {
	exit(1);
}

#endif

void print_help(const char* _exec) {
	std::cerr << "usage: " << _exec << " *TODO*" << std::endl;
}

/*********************************
* main - this is where it begins
*********************************/
int main(int argc, char **argv)
{
	std::cout << "********************************************" << std::endl;
	std::cout << "*** Xolominer - Primecoin Pool Miner v" << VERSION_MAJOR << "." << VERSION_MINOR << " " << VERSION_EXT << std::endl;
	std::cout << "*** by xolokram/TB - www.beeeeer.org - glhf" << std::endl;
	std::cout << "***" << std::endl;
	std::cout << "*** thx to Sunny King & mikaelh" << std::endl;
	std::cout << "*** press CTRL+C to exit" << std::endl;
	std::cout << "********************************************" << std::endl;

	//TODO: optimize the code using SPH,SSE,AVX,etc.pp. #2/2

	t_start = boost::posix_time::second_clock::local_time();
	totalShareCount = 0;
	running = true;

#if defined(__MINGW32__) || defined(__MINGW64__)
	SetConsoleCtrlHandler(ctrl_handler, TRUE);
#elif defined(__GNUG__) && !defined(__APPLE__)
	set_signal_handler(SIGINT, ctrl_handler);
#endif

	const int atexit_res = std::atexit(exit_handler);
	if (atexit_res != 0)
		std::cerr << "atexit registration failed, shutdown will be (more) dirty!!" << std::endl;
		
	if (argc < 2) {
		std::cerr << "usage: " << argv[0] << " -poolfee=<fee-in-%> -poolip=<ip> -poolport=<port> -pooluser=<user> -poolpassword=<password>" << std::endl;
		return EXIT_FAILURE;
	}

	// init everything:
	ParseParameters(argc, argv);

	pool_share_minimum = (unsigned int)GetArg("-poolshare", 7);
	//
	socket_to_server = NULL;
	pindexBest = NULL;
	thread_num_max = GetArg("-genproclimit", 1); //TODO: what about boost's hardware_concurrency() ?
	fee_to_pay = GetArg("-poolfee", 3);
	miner_id = GetArg("-minerid", 0);
	pool_username = GetArg("-pooluser", "");
	pool_password = GetArg("-poolpassword", "");

	if (thread_num_max < 1) {
		std::cerr << "error: unsupported number of threads" << std::endl;
		return EXIT_FAILURE;
	}
	
	if (fee_to_pay == 0 || fee_to_pay > 100) {
		std::cerr << "usage: " << "please use a pool fee between [1 , 100]" << std::endl;
		return EXIT_FAILURE;
	}

	if (miner_id > 65535) {
		std::cerr << "usage: " << "please use a miner id between [0 , 65535]" << std::endl;
		return EXIT_FAILURE;
	}
	
	{ //password to sha1
		boost::uuids::detail::sha1 sha;
		sha.process_bytes(pool_password.c_str(), pool_password.size());
		unsigned int digest[5];
		sha.get_digest(digest);
		std::stringstream ss;
		ss << std::setw(5) << std::setfill('0') << std::hex << (digest[0] ^ digest[1] ^ digest[4]) << (digest[2] ^ digest[3] ^ digest[4]);
		pool_password = ss.str();
	}
	std::cout << pool_username << std::endl;

	//TODO: fPrintToConsole = true; // always on
	//TODO: fDebug          = GetBoolArg("-debug");

	pindexBest = new CBlockIndex();

	GeneratePrimeTable();

	//start mining:
	CBlockProviderGW* bprovider = new CBlockProviderGW();
	CMasterThread *mt = new CMasterThread(bprovider);
	mt->run();

	//ok, done.
	return EXIT_SUCCESS;
}

/*********************************
* and this is where it ends
*********************************/
