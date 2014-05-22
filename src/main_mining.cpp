#include "main_poolminer.h"
#include "xpmclient.h"

XPMClient* client; //TODO: map<thread_id,xpmclient>

template<CPUMODE cpumode>
void primecoin_init(unsigned int thread_id)
{
	client = new XPMClient();
	client->Initialize();
	client->getWorkers()[0].first->Init();
}

template<CPUMODE cpumode>
void primecoin_mine(CBlockProvider* bp, unsigned int thread_id)
{
	while (bp->getOriginalBlock() == NULL)
		boost::this_thread::sleep(boost::posix_time::milliseconds(100));
	client->getWorkers()[0].first->Mining(bp);
}

template<CPUMODE cpumode>
void primecoin_cleanup(unsigned int thread_id)
{
	client->getWorkers()[0].first->CleanUp();
}

//"UNKNOWN"
template void primecoin_init<UNKNOWN>(unsigned int thread_id);
template void primecoin_mine<UNKNOWN>(CBlockProvider* bp, unsigned int thread_id);
template void primecoin_cleanup<UNKNOWN>(unsigned int thread_id);
