#ifndef BLOCKPROVIDER_H
#define BLOCKPROVIDER_H

#include "block.h"

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

#endif //BLOCKPROVIDER_H
