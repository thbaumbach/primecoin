#include "main_poolminer.h"

int64 GetTime()
{
	return 0;
}

int64 GetTimeMillis()
{
	return 0;
}

int64 GetTimeMicros()
{
	return 0;
}

template<CPUMODE cpumode>
void primecoin_init()
{
}

template<CPUMODE cpumode>
void primecoin_mine(CBlock* pblock, CBlockProvider* bp, unsigned int thread_id)
{
	printf("PrimecoinMiner started\n");

    unsigned int nPrimorialMultiplier = nPrimorialHashFactor;
    double dTimeExpected = 0;   // time expected to prime chain (micro-second)
    int64 nSieveGenTime = 0; // how many milliseconds sieve generation took
    bool fIncrementPrimorial = true; // increase or decrease primorial factor
	
	unsigned int old_nonce = 0;

    /*try {
	loop {
        while (block_provider == NULL && vNodes.empty())
            MilliSleep(1000);

        //
        // Create new block
        //
        CBlockIndex* pindexPrev = pindexBest;
		
		if (orgblock != block_provider->getOriginalBlock()) {
			orgblock = block_provider->getOriginalBlock();
			blockcnt = 0;
		}

        auto_ptr<CBlockTemplate> pblocktemplate;
        if (block_provider == NULL) {
          //rmvd
        } else if ((pblock = block_provider->getBlock(thread_id, pblock == NULL ? 0 : pblock->nTime, blockcnt)) == NULL) { //server not reachable?
          MilliSleep(20000);
          continue;
        } else if (old_hash == pblock->GetHeaderHash()) {
          if (old_nonce >= 0xffff0000) {
		    MilliSleep(100);
			//TODO: FORCE a new getblock!
			if (fDebug && GetBoolArg("-printmining"))
				printf("Nothing to do --- uh ih uh ah ah bing bang!!\n");
            continue;
		  } else
		    pblock->nNonce = old_nonce;
        } else {
            old_hash = pblock->GetHeaderHash();
			old_nonce = 0;
			if (orgblock == block_provider->getOriginalBlock())
				++blockcnt;
        }

        if (fDebug && GetBoolArg("-printmining"))
            printf("Running PrimecoinMiner with %"PRIszu" transactions in block (%u bytes)\n", pblock->vtx.size(),
               ::GetSerializeSize(*pblock, SER_NETWORK, PROTOCOL_VERSION));
		*/
        //
        // Search
        //
        int64 nStart = GetTime();
        bool fNewBlock = true;
        unsigned int nTriedMultiplier = 0;

        // Primecoin: try to find hash divisible by primorial
        unsigned int nHashFactor = PrimorialFast(nPrimorialHashFactor);

        // Based on mustyoshi's patch from https://bitcointalk.org/index.php?topic=251850.msg2689981#msg2689981
        uint256 phash;
        mpz_class mpzHash;
        for (;;)
		{
            // Fast loop
            if (pblock->nNonce >= 0xffff0000)
                break;

            // Check that the hash meets the minimum
            phash = pblock->GetHeaderHash();
            if (phash < hashBlockHeaderLimit) {
                pblock->nNonce++;
                continue;
            }

            // Check that the hash is divisible by the fixed primorial
            mpz_set_uint256(mpzHash.get_mpz_t(), phash);
            if (!mpz_divisible_ui_p(mpzHash.get_mpz_t(), nHashFactor)) {
                pblock->nNonce++;
                continue;
            }

            // Use the hash that passed the tests
            break;
        }
        if (pblock->nNonce >= 0xffff0000) {
			old_nonce = 0xffff0000;
            return; //actually: continue;
		}
        // Primecoin: primorial fixed multiplier
        mpz_class mpzPrimorial;
        unsigned int nRoundTests = 0;
        unsigned int nRoundPrimesHit = 0;
        int64 nPrimeTimerStart = GetTimeMicros();
        Primorial(nPrimorialMultiplier, mpzPrimorial);

		for (;;)
        {
            unsigned int nTests = 0;
            unsigned int nPrimesHit = 0;
            unsigned int nChainsHit = 0;

            // Primecoin: adjust round primorial so that the generated prime candidates meet the minimum
            mpz_class mpzMultiplierMin = mpzPrimeMin * nHashFactor / mpzHash + 1;
            while (mpzPrimorial < mpzMultiplierMin)
            {
                if (!PrimeTableGetNextPrime(nPrimorialMultiplier))
                    error("PrimecoinMiner() : primorial minimum overflow");
                Primorial(nPrimorialMultiplier, mpzPrimorial);
            }
            mpz_class mpzFixedMultiplier;
            if (mpzPrimorial > nHashFactor) {
                mpzFixedMultiplier = mpzPrimorial / nHashFactor;
            } else {
                mpzFixedMultiplier = 1;
            }

            // Primecoin: mine for prime chain
            unsigned int nProbableChainLength;
            if (MineProbablePrimeChain(*pblock, mpzFixedMultiplier, fNewBlock, nTriedMultiplier, nProbableChainLength, nTests, nPrimesHit, nChainsHit, mpzHash, nPrimorialMultiplier, nSieveGenTime, pindexPrev, block_provider != NULL))
            {
                SetThreadPriority(THREAD_PRIORITY_NORMAL);
				if (block_provider == NULL)
					CheckWork(pblock, *pwalletMain, reservekey);
				else
					block_provider->submitBlock(pblock);
                SetThreadPriority(THREAD_PRIORITY_LOWEST);
				old_nonce = pblock->nNonce + 1;
                break;
            }

            ///
            /// ENABLE the following code, if you need data for the perftool
            ///
            /*if (nProbableChainLength > 0 && nTests > 10)
            {
              static CCriticalSection cs;
              {
                LOCK(cs);

                std::ofstream output_file("miner_data");
                std::ofstream output_file_block("miner_data.blk", std::ofstream::out | std::ofstream::binary);

                ::Serialize(output_file_block, *pblock, 0, 0); //writeblock

                output_file << mpzFixedMultiplier.get_str(10) << std::endl;
                output_file << fNewBlock << std::endl;
                output_file << nTriedMultiplier << std::endl;
                output_file << nPrimorialMultiplier << std::endl;
                output_file << mpzHash.get_str(10) << std::endl;

                output_file.close();
                output_file_block.close();
              }
            }*/

            nRoundTests += nTests;
            nRoundPrimesHit += nPrimesHit;

            // Meter primes/sec
            static volatile int64 nPrimeCounter;
            static volatile int64 nTestCounter;
            static volatile int64 nChainCounter;
            static double dChainExpected;
            int64 nMillisNow = GetTimeMillis();
            if (nHPSTimerStart == 0)
            {
                nHPSTimerStart = nMillisNow;
                nPrimeCounter = 0;
                nTestCounter = 0;
                nChainCounter = 0;
                dChainExpected = 0;
            }
            else
            {
#ifdef __GNUC__
                // Use atomic increment
                __sync_add_and_fetch(&nPrimeCounter, nPrimesHit);
                __sync_add_and_fetch(&nTestCounter, nTests);
                __sync_add_and_fetch(&nChainCounter, nChainsHit);
#else
                nPrimeCounter += nPrimesHit;
                nTestCounter += nTests;
                nChainCounter += nChainsHit;
#endif
            }
            if (nMillisNow - nHPSTimerStart > 60000)
            {
                static CCriticalSection cs;
                {
                    LOCK(cs);
                    if (nMillisNow - nHPSTimerStart > 60000)
                    {
                        double dPrimesPerMinute = 60000.0 * nPrimeCounter / (nMillisNow - nHPSTimerStart);
                        dPrimesPerSec = dPrimesPerMinute / 60.0;
                        double dTestsPerSec = 1000.0 * nTestCounter / (nMillisNow - nHPSTimerStart);
                        dChainsPerMinute = 60000.0 * nChainCounter / (nMillisNow - nHPSTimerStart);
                        dChainsPerDay = 86400000.0 * dChainExpected / (GetTimeMillis() - nHPSTimerStart);
                        nHPSTimerStart = nMillisNow;
                        nPrimeCounter = 0;
                        nTestCounter = 0;
                        nChainCounter = 0;
                        dChainExpected = 0;
                        static int64 nLogTime = 0;
                        if (nMillisNow - nLogTime > 59000)
                        {
                            nLogTime = nMillisNow;
                            printf("[STATS] %s | %4.0f primes/s, %4.0f tests/s, %4.0f %d-chains/h, %3.3f chains/d\n", DateTimeStrFormat("%Y-%m-%d %H:%M:%S", nLogTime / 1000).c_str(), dPrimesPerSec, dTestsPerSec, dChainsPerMinute * 60.0, nStatsChainLength, dChainsPerDay);
                        }
                    }
                }
            }

			old_nonce = pblock->nNonce;

            // Check for stop or if block needs to be rebuilt
            boost::this_thread::interruption_point();
            if (block_provider == NULL && vNodes.empty())
                break;
            if (pblock->nNonce >= 0xffff0000)
                break;
            if (pindexPrev != pindexBest/* || (block_provider != NULL && GetTime() - nStart > 200)*/)
                break;
			if (thread_id == 0 && block_provider != NULL && (GetTime() - nStart) > 300) { //5 minutes no update? something's wrong -> reconnect!
				block_provider->forceReconnect();
				nStart = GetTime();
			}
            if (fNewBlock) //aka: sieve's done, we need a updated nonce
            {
                // Primecoin: a sieve+primality round completes
                // Primecoin: estimate time to block
                const double dTimeExpectedPrev = dTimeExpected;
                unsigned int nCalcRoundTests = max(1u, nRoundTests);
                // Make sure the estimated time is very high if only 0 primes were found
                if (nRoundPrimesHit == 0)
                    nCalcRoundTests *= 1000;
                int64 nRoundTime = (GetTimeMicros() - nPrimeTimerStart);
                dTimeExpected = (double) nRoundTime / nCalcRoundTests;
                double dRoundChainExpected = (double) nRoundTests;
                for (unsigned int n = 0, nTargetLength = TargetGetLength(pblock->nBits); n < nTargetLength; n++)
                {
                    double dPrimeProbability = EstimateCandidatePrimeProbability(nPrimorialMultiplier, n);
                    dTimeExpected = dTimeExpected / max(0.01, dPrimeProbability);
                    dRoundChainExpected *= dPrimeProbability;
                }
                dChainExpected += dRoundChainExpected;
                if (fDebug && GetBoolArg("-printmining"))
                {
                    double dPrimeProbabilityBegin = EstimateCandidatePrimeProbability(nPrimorialMultiplier, 0);
                    unsigned int nTargetLength = TargetGetLength(pblock->nBits);
                    double dPrimeProbabilityEnd = EstimateCandidatePrimeProbability(nPrimorialMultiplier, nTargetLength - 1);
                    printf("PrimecoinMiner() : Round primorial=%u tests=%u primes=%u time=%uus pprob=%1.6f pprob2=%1.6f tochain=%6.3fd expect=%3.9f\n", nPrimorialMultiplier, nRoundTests, nRoundPrimesHit, (unsigned int) nRoundTime, dPrimeProbabilityBegin, dPrimeProbabilityEnd, ((dTimeExpected/1000000.0))/86400.0, dRoundChainExpected);
                }

                // Primecoin: update time and nonce
                //pblock->nTime = max(pblock->nTime, (unsigned int) GetAdjustedTime());
                pblock->nTime = max(pblock->nTime, block_provider->GetAdjustedTimeWithOffset(thread_id));
                pblock->nNonce++;
                loop {
                    // Fast loop
                    if (pblock->nNonce >= 0xffff0000)
                        break;

                    // Check that the hash meets the minimum
                    phash = pblock->GetHeaderHash();
                    if (phash < hashBlockHeaderLimit) {
                        pblock->nNonce++;
                        continue;
                    }

                    // Check that the hash is divisible by the fixed primorial
                    mpz_set_uint256(mpzHash.get_mpz_t(), phash);
                    if (!mpz_divisible_ui_p(mpzHash.get_mpz_t(), nHashFactor)) {
                        pblock->nNonce++;
                        continue;
                    }

                    // Use the hash that passed the tests
                    break;
                }
                if (pblock->nNonce >= 0xffff0000)
                    break;

                // Primecoin: reset sieve+primality round timer
                nPrimeTimerStart = GetTimeMicros();
                if (dTimeExpected > dTimeExpectedPrev)
                    fIncrementPrimorial = !fIncrementPrimorial;

                // Primecoin: primorial always needs to be incremented if only 0 primes were found
                if (nRoundPrimesHit == 0)
                    fIncrementPrimorial = true;

                nRoundTests = 0;
                nRoundPrimesHit = 0;

                // Primecoin: dynamic adjustment of primorial multiplier
                if (fIncrementPrimorial)
                {
                    if (!PrimeTableGetNextPrime(nPrimorialMultiplier))
                        error("PrimecoinMiner() : primorial increment overflow");
                }
                else if (nPrimorialMultiplier > nPrimorialHashFactor)
                {
                    if (!PrimeTableGetPreviousPrime(nPrimorialMultiplier))
                        error("PrimecoinMiner() : primorial decrement overflow");
                }
                Primorial(nPrimorialMultiplier, mpzPrimorial);
            }
        }
    //}
}
