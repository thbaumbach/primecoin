#include "main_poolminer.h"

static double dPrimesPerSec;
static double dChainsPerDay;
static double dBlocksPerDay;
static int64 nHPSTimerStart;

int64 GetTime()
{
	return (boost::posix_time::second_clock::universal_time() - boost::posix_time::from_time_t(0)).total_seconds();
}

int64 GetTimeMillis()
{
	return (boost::posix_time::microsec_clock::universal_time() - boost::posix_time::from_time_t(0)).total_milliseconds();
}

/*int64 GetTimeMicros() //found in prime.h
{
	return 0;
}*/

int64 GetAdjustedTime()
{
	return GetTime();
}

template<CPUMODE cpumode>
void primecoin_mine(CBlockProvider* bp, unsigned int thread_id)
{
	InitPrimeMiner();
	
    unsigned int blockcnt = 0;
	CBlock* orgblock = NULL;
	
	dPrimesPerSec = 0.0;
    dChainsPerDay = 0.0;
    dBlocksPerDay = 0.0;
    nHPSTimerStart = 0;
	
	static bool fTimerStarted = false;
    bool fPrintStatsAtEnd = false;
    printf("PrimecoinMiner started\n");

    // Each thread has its own key and counter
    //CReserveKey reservekey(pwallet);
    //unsigned int nExtraNonce = 0;

    unsigned int nPrimorialMultiplier = nPrimorialHashFactor;
    int nAdjustPrimorial = 1; // increase or decrease primorial factor
    const unsigned int nRoundSamples = 40; // how many rounds to sample before adjusting primorial
    double dSumBlockExpected = 0.0; // sum of expected blocks
    int64 nSumRoundTime = 0; // sum of round times
    unsigned int nRoundNum = 0; // number of rounds
    double dAverageBlockExpectedPrev = 0.0; // previous average expected blocks per second
    unsigned int nPrimorialMultiplierPrev = nPrimorialMultiplier; // previous primorial factor

    // Primecoin HP: Increase initial primorial
    //if (fTestNet)
    //    nPrimorialMultiplier = nInitialPrimorialMultiplierTestnet;
    //else
        nPrimorialMultiplier = nInitialPrimorialMultiplier;

    // Primecoin: Check if a fixed primorial was requested
    unsigned int nFixedPrimorial = (unsigned int)GetArg("-primorial", 0);
    if (nFixedPrimorial > 0)
    {
        nFixedPrimorial = std::max(nFixedPrimorial, nPrimorialHashFactor);
        nPrimorialMultiplier = nFixedPrimorial;
    }

    // Primecoin: Allow choosing the mining protocol version
    unsigned int nMiningProtocol = (unsigned int)GetArg("-miningprotocol", 1);

    // Primecoin: Allocate data structures for mining
    CSieveOfEratosthenes sieve;
    CPrimalityTestParams testParams;

    /*if (!fTimerStarted)
    {
        LOCK(cs);
        if (!fTimerStarted)
        {
            fTimerStarted = true;
            minerTimer.start();

            // First thread will print the stats
            fPrintStatsAtEnd = true;
        }
    }*/

    // Many machines may be using the same key if they are sharing the same wallet
    // Make extra nonce unique by setting it to a modulo of the high resolution clock's value
    //const unsigned int nExtraNonceModulo = 10000000;
    //boost::chrono::high_resolution_clock::time_point time_now = boost::chrono::high_resolution_clock::now();
    //boost::chrono::nanoseconds ns_now = boost::chrono::duration_cast<boost::chrono::nanoseconds>(time_now.time_since_epoch());
    //nExtraNonce = ns_now.count() % nExtraNonceModulo;

    // Print the chosen extra nonce for debugging
    //printf("BitcoinMiner() : Setting initial extra nonce to %u\n", nExtraNonce);

    //try { loop {
	while (running) {
        //while (vNodes.empty())
        //    MilliSleep(1000);

        //
        // Create new block
        //
        //unsigned int nTransactionsUpdatedLast = nTransactionsUpdated;
        CBlockIndex* pindexPrev = pindexBest;

        // pindexBest may be NULL (e.g. when doing a -reindex)
        //if (!pindexPrev) {
        //    MilliSleep(1000);
        //    continue;
        //}

        //auto_ptr<CBlockTemplate> pblocktemplate(CreateNewBlock(reservekey));
        //if (!pblocktemplate.get())
        //    return;
        CBlock *pblock = NULL; //&pblocktemplate->block;
        //IncrementExtraNonce(pblock, pindexPrev, nExtraNonce, true);*/

        //if (fDebug && GetBoolArg("-printmining"))
        //    printf("Running PrimecoinMiner with %"PRIszu" transactions in block (%u bytes)\n", pblock->vtx.size(),
        //       ::GetSerializeSize(*pblock, SER_NETWORK, PROTOCOL_VERSION));

		if (orgblock != bp->getOriginalBlock()) {
			orgblock = bp->getOriginalBlock();
			blockcnt = 0;
		}
		pblock = bp->getBlock(thread_id, pblock == NULL ? 0 : pblock->nTime, blockcnt);
		if (orgblock == bp->getOriginalBlock()) {
			++blockcnt;
		}
		if (pblock == NULL) {
			boost::this_thread::sleep(boost::posix_time::seconds(1)); //we've lost the connection or something else very bad happened
			continue;
		}
        //
        // Search
        //
        int64 nStart = GetTime();
        bool fNewBlock = true;

        // Primecoin: try to find hash divisible by primorial
        unsigned int nHashFactor = PrimorialFast(nPrimorialHashFactor);

        mpz_class mpzHash;
        for(;;)
        {
            pblock->nNonce++;
            if (pblock->nNonce >= 0xffff0000)
                break;

            // Check that the hash meets the minimum
            uint256 phash = pblock->GetHeaderHash();
            if (phash < hashBlockHeaderLimit)
                continue;

            mpz_set_uint256(mpzHash.get_mpz_t(), phash);
            if (nMiningProtocol >= 2) {
                // Primecoin: Mining protocol v0.2
                // Try to find hash that is probable prime
                if (!ProbablePrimalityTestWithTrialDivision(mpzHash, 1000, testParams))
                    continue;
            } else {
                // Primecoin: Check that the hash is divisible by the fixed primorial
                if (!mpz_divisible_ui_p(mpzHash.get_mpz_t(), nHashFactor))
                    continue;
            }

            // Use the hash that passed the tests
            break;
        }
        if (pblock->nNonce >= 0xffff0000)
            continue; //TODO: continue...
        // Primecoin: primorial fixed multiplier
        mpz_class mpzPrimorial;
        mpz_class mpzFixedMultiplier;
        unsigned int nRoundTests = 0;
        unsigned int nRoundPrimesHit = 0;
        int64 nPrimeTimerStart = GetTimeMicros();
        Primorial(nPrimorialMultiplier, mpzPrimorial);

        for(;;)
        {
            unsigned int nTests = 0;
            unsigned int nPrimesHit = 0;
            unsigned int vChainsFound[nMaxChainLength];
            for (unsigned int i = 0; i < nMaxChainLength; i++)
                vChainsFound[i] = 0;

            // Meter primes/sec
            static volatile int64 nPrimeCounter = 0;
            static volatile int64 nTestCounter = 0;
            static volatile double dChainExpected = 0.0;
            static volatile double dBlockExpected = 0.0;
            static volatile unsigned int vFoundChainCounter[nMaxChainLength];
            int64 nMillisNow = GetTimeMillis();
            if (nHPSTimerStart == 0)
            {
                nHPSTimerStart = nMillisNow;
                nPrimeCounter = 0;
                nTestCounter = 0;
                dChainExpected = 0.0;
                dBlockExpected = 0.0;
                for (unsigned int i = 0; i < nMaxChainLength; i++)
                    vFoundChainCounter[i] = 0;
            }

            // Primecoin: Mining protocol v0.2
            if (nMiningProtocol >= 2)
                mpzFixedMultiplier = mpzPrimorial;
            else
            {
                if (mpzPrimorial > nHashFactor)
                    mpzFixedMultiplier = mpzPrimorial / nHashFactor;
                else
                    mpzFixedMultiplier = 1;
            }

            // Primecoin: mine for prime chain
            if (MineProbablePrimeChain(*pblock, mpzFixedMultiplier, fNewBlock, nTests, nPrimesHit, mpzHash, pindexPrev, vChainsFound, sieve, testParams))
            {
                //SetThreadPriority(THREAD_PRIORITY_NORMAL);
                //nTotalBlocksFound++;
                //CheckWork(pblock, *pwalletMain, reservekey);
                //SetThreadPriority(THREAD_PRIORITY_LOWEST);
                bp->submitBlock(pblock);
            }
            nRoundTests += nTests;
            nRoundPrimesHit += nPrimesHit;

#ifdef __GNUC__
            // Use atomic increment
            __sync_add_and_fetch(&nPrimeCounter, nPrimesHit);
            __sync_add_and_fetch(&nTestCounter, nTests);
            __sync_add_and_fetch(&nTotalTests, nTests);
            for (unsigned int i = 0; i < nMaxChainLength; i++)
            {
                __sync_add_and_fetch(&vTotalChainsFound[i], vChainsFound[i]);
                __sync_add_and_fetch(&vFoundChainCounter[i], vChainsFound[i]);
            }
#else
            nPrimeCounter += nPrimesHit;
            nTestCounter += nTests;
            nTotalTests += nTests;
            for (unsigned int i = 0; i < nMaxChainLength; i++)
            {
                vTotalChainsFound[i] += vChainsFound[i];
                vFoundChainCounter[i] += vChainsFound[i];
            }
#endif

            nMillisNow = GetTimeMillis();
            if (nMillisNow - nHPSTimerStart > 60000)
            {
                //TODO: LOCK(cs);
                nMillisNow = GetTimeMillis();
                if (nMillisNow - nHPSTimerStart > 60000)
                {
                    int64 nTimeDiffMillis = nMillisNow - nHPSTimerStart;
                    nHPSTimerStart = nMillisNow;
                    double dPrimesPerMinute = 60000.0 * nPrimeCounter / nTimeDiffMillis;
                    dPrimesPerSec = dPrimesPerMinute / 60.0;
                    double dTestsPerMinute = 60000.0 * nTestCounter / nTimeDiffMillis;
                    dChainsPerDay = 86400000.0 * dChainExpected / nTimeDiffMillis;
                    dBlocksPerDay = 86400000.0 * dBlockExpected / nTimeDiffMillis;
                    nPrimeCounter = 0;
                    nTestCounter = 0;
                    dChainExpected = 0;
                    dBlockExpected = 0;
                    static int64 nLogTime = 0;
                    if (nMillisNow - nLogTime > 59000)
                    {
                        nLogTime = nMillisNow;
                        //if (fLogTimestamps)
                            printf("primemeter %9.0f prime/h %9.0f test/h %3.8f chain/d %3.8f block/d\n", dPrimesPerMinute * 60.0, dTestsPerMinute * 60.0, dChainsPerDay, dBlocksPerDay);
                        //else
                        //    printf("%s primemeter %9.0f prime/h %9.0f test/h %3.8f chain/d %3.8f block/d\n", DateTimeStrFormat("%Y-%m-%d %H:%M:%S", nLogTime / 1000).c_str(), dPrimesPerMinute * 60.0, dTestsPerMinute * 60.0, dChainsPerDay, dBlocksPerDay);
                        PrintCompactStatistics(vFoundChainCounter);
                    }
                }
            }

            // Check for stop or if block needs to be rebuilt
            boost::this_thread::interruption_point();
            //if (vNodes.empty())
            //    break;
            if (pblock->nNonce >= 0xffff0000) //TODO:
                break;
            //if (nTransactionsUpdated != nTransactionsUpdatedLast && GetTime() - nStart > 10)
            //    break;
            if (pindexPrev != pindexBest)
                break;
            if (fNewBlock)
            {
                // Primecoin: a sieve+primality round completes
                // Primecoin: estimate time to block
                unsigned int nCalcRoundTests = std::max(1u, nRoundTests);
                // Make sure the estimated time is very high if only 0 primes were found
                if (nRoundPrimesHit == 0)
                    nCalcRoundTests *= 1000;
                int64 nRoundTime = (GetTimeMicros() - nPrimeTimerStart);
                double dTimeExpected = (double) nRoundTime / nCalcRoundTests;
                double dRoundChainExpected = (double) nRoundTests;
                unsigned int nTargetLength = TargetGetLength(pblock->nBits);
                unsigned int nRequestedLength = nTargetLength;
                // Override target length if requested
                if (nSieveTargetLength > 0)
                    nRequestedLength = nSieveTargetLength;
                // Calculate expected number of chains for requested length
                for (unsigned int n = 0; n < nRequestedLength; n++)
                {
                    double dPrimeProbability = EstimateCandidatePrimeProbability(nPrimorialMultiplier, n, nMiningProtocol);
                    dTimeExpected /= dPrimeProbability;
                    dRoundChainExpected *= dPrimeProbability;
                }
                dChainExpected += dRoundChainExpected;
                // Calculate expected number of blocks
                double dRoundBlockExpected = dRoundChainExpected;
                for (unsigned int n = nRequestedLength; n < nTargetLength; n++)
                {
                    double dPrimeProbability = EstimateNormalPrimeProbability(nPrimorialMultiplier, n, nMiningProtocol);
                    dTimeExpected /= dPrimeProbability;
                    dRoundBlockExpected *= dPrimeProbability;
                }
                // Calculate the effect of fractional difficulty
                double dFractionalDiff = GetPrimeDifficulty(pblock->nBits) - nTargetLength;
                double dExtraPrimeProbability = EstimateNormalPrimeProbability(nPrimorialMultiplier, nTargetLength, nMiningProtocol);
                double dDifficultyFactor = ((1.0 - dFractionalDiff) * (1.0 - dExtraPrimeProbability) + dExtraPrimeProbability);
                dRoundBlockExpected *= dDifficultyFactor;
                dTimeExpected /= dDifficultyFactor;
                dBlockExpected += dRoundBlockExpected;
                // Calculate the sum of expected blocks and time
                dSumBlockExpected += dRoundBlockExpected;
                nSumRoundTime += nRoundTime;
                nRoundNum++;
                if (nRoundNum >= nRoundSamples)
                {
                    // Calculate average expected blocks per time
                    double dAverageBlockExpected = dSumBlockExpected / ((double) nSumRoundTime / 1000000.0);
                    // Compare to previous value
                    if (dAverageBlockExpected > dAverageBlockExpectedPrev)
                        nAdjustPrimorial = (nPrimorialMultiplier >= nPrimorialMultiplierPrev) ? 1 : -1;
                    else
                        nAdjustPrimorial = (nPrimorialMultiplier >= nPrimorialMultiplierPrev) ? -1 : 1;
                    if (fDebug && GetBoolArg("-printprimorial"))
                        printf("PrimecoinMiner() : Rounds total: num=%u primorial=%u block/s=%3.12f\n", nRoundNum, nPrimorialMultiplier, dAverageBlockExpected);
                    // Store the new value and reset
                    dAverageBlockExpectedPrev = dAverageBlockExpected;
                    nPrimorialMultiplierPrev = nPrimorialMultiplier;
                    dSumBlockExpected = 0.0;
                    nSumRoundTime = 0;
                    nRoundNum = 0;
                }
                if (fDebug && GetBoolArg("-printmining"))
                {
                    double dPrimeProbabilityBegin = EstimateCandidatePrimeProbability(nPrimorialMultiplier, 0, nMiningProtocol);
                    double dPrimeProbabilityEnd = EstimateCandidatePrimeProbability(nPrimorialMultiplier, nTargetLength - 1, nMiningProtocol);
                    printf("PrimecoinMiner() : Round primorial=%u tests=%u primes=%u time=%uus pprob=%1.6f pprob2=%1.6f pprobextra=%1.6f tochain=%6.3fd expect=%3.12f expectblock=%3.12f\n", nPrimorialMultiplier, nRoundTests, nRoundPrimesHit, (unsigned int) nRoundTime, dPrimeProbabilityBegin, dPrimeProbabilityEnd, dExtraPrimeProbability, ((dTimeExpected/1000000.0))/86400.0, dRoundChainExpected, dRoundBlockExpected);
                }

                // Primecoin: primorial always needs to be incremented if only 0 primes were found
                if (nRoundPrimesHit == 0)
                    nAdjustPrimorial = 1;

                // Primecoin: reset sieve+primality round timer
                nPrimeTimerStart = GetTimeMicros();
                nRoundTests = 0;
                nRoundPrimesHit = 0;

                // Primecoin: update time and nonce
                pblock->nTime = std::max(pblock->nTime, (unsigned int) GetAdjustedTime()); //TODO:
                for(;;)
                {
                    pblock->nNonce++;
                    if (pblock->nNonce >= 0xffff0000)
                        break;

                    // Check that the hash meets the minimum
                    uint256 phash = pblock->GetHeaderHash();
                    if (phash < hashBlockHeaderLimit)
                        continue;

                    mpz_set_uint256(mpzHash.get_mpz_t(), phash);
                    if (nMiningProtocol >= 2) {
                        // Primecoin: Mining protocol v0.2
                        // Try to find hash that is probable prime
                        if (!ProbablePrimalityTestWithTrialDivision(mpzHash, 1000, testParams))
                            continue;
                    } else {
                        // Primecoin: Check that the hash is divisible by the fixed primorial
                        if (!mpz_divisible_ui_p(mpzHash.get_mpz_t(), nHashFactor))
                            continue;
                    }

                    // Use the hash that passed the tests
                    break;
                }
                if (pblock->nNonce >= 0xffff0000)
                    break;

                // Primecoin: dynamic adjustment of primorial multiplier
                if (nFixedPrimorial == 0 && nAdjustPrimorial != 0) {
                    if (nAdjustPrimorial > 0)
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
                    nAdjustPrimorial = 0;
                }
            }
        }
    }
	std::cout << "lol" << std::endl;
}

//"SPHLIB"
template void primecoin_mine<SPHLIB>(CBlockProvider* bp, unsigned int thread_id);
