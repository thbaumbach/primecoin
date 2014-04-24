#if defined(__MINGW64__)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <boost/date_time/posix_time/posix_time_io.hpp>
#include <cstring>

#include "bignum.h"
#include "prime.h"

extern bool running;

struct blockHeader_t {
  // comments: BYTES <index> + <length>
  int           nVersion;            // 0+4
  uint256       hashPrevBlock;       // 4+32
  uint256       hashMerkleRoot;      // 36+32
  unsigned int  nTime;               // 68+4
  unsigned int  nBits;               // 72+4
  unsigned int  nNonce;              // 76+4
  unsigned char primemultiplier[48]; // 80+48
};

enum CPUMODE { SPHLIB = 0, SSE3, SSE4, AVX };

class CBlockProvider {
public:
	CBlockProvider() { }
	~CBlockProvider() { }
	virtual CBlock* getBlock(unsigned int thread_id, unsigned int last_time, unsigned int counter) = 0;
	virtual CBlock* getOriginalBlock() = 0;
	virtual void setBlockTo(CBlock* newblock) = 0;
	virtual void submitBlock(CBlock* block, unsigned int thread_id) = 0;
	virtual unsigned int GetAdjustedTimeWithOffset(unsigned int thread_id, unsigned int counter) = 0;
};

template<CPUMODE cpumode>
void primecoin_mine(CBlockProvider* bp, unsigned int thread_id);
