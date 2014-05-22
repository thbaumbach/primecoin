#if defined(__MINGW64__)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <boost/date_time/posix_time/posix_time_io.hpp>
#include <cstring>

#include "bignum.h"
#include "block.h"
#include "blockprovider.h"

extern bool running;
extern bool fDebug;
extern unsigned int pool_share_minimum;

std::string GetArg(const std::string& strArg, const std::string& strDefault);
int64 GetArg(const std::string& strArg, int64 nDefault);
bool GetBoolArg(const std::string& strArg, bool fDefault=false);
void ParseParameters(int argc, const char* const argv[]);
void ParseConfigFile(const char* file_name);

typedef int CBlockIndex;
extern CBlockIndex* pindexBest;

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

enum CPUMODE { UNKNOWN = 0, SPHLIB, SSE3, SSE4, AVX };

template<CPUMODE cpumode>
void primecoin_init(unsigned int thread_id);

template<CPUMODE cpumode>
void primecoin_mine(CBlockProvider* bp, unsigned int thread_id);

template<CPUMODE cpumode>
void primecoin_cleanup(unsigned int thread_id);
