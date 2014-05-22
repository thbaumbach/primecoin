#ifndef BLOCK_H
#define BLOCK_H

#include <openssl/sha.h>

#include "bignum.h"

template<typename T1>
inline uint256 Hash(const T1 pbegin, const T1 pend)
{
    static unsigned char pblank[1];
    uint256 hash1;
    SHA256((pbegin == pend ? pblank : (unsigned char*)&pbegin[0]), (pend - pbegin) * sizeof(pbegin[0]), (unsigned char*)&hash1);
    uint256 hash2;
    SHA256((unsigned char*)&hash1, sizeof(hash1), (unsigned char*)&hash2);
    return hash2;
}

#define BEGIN_HASH(a)            ((char*)&(a))
#define END_HASH(a)              ((char*)&((&(a))[1]))

class CBlock
{
public:
    // header
    static const int CURRENT_VERSION=2;
    int nVersion;
    uint256 hashPrevBlock;
    uint256 hashMerkleRoot;
    unsigned int nTime;
    unsigned int nBits;  // Primecoin: prime chain target, see prime.cpp
    unsigned int nNonce;
	CBigNum bnPrimeChainMultiplier;

    CBlock() { //SetNull()
        nVersion = CBlock::CURRENT_VERSION;
        hashPrevBlock = 0;
        hashMerkleRoot = 0;
        nTime = 0;
        nBits = 0;
        nNonce = 0;
        bnPrimeChainMultiplier = 0;
    }
	
	CBlock(const CBlock &src) {
        nVersion = src.nVersion;
        hashPrevBlock = src.hashPrevBlock;
        hashMerkleRoot = src.hashMerkleRoot;
        nTime = src.nTime;
        nBits = src.nBits;
        nNonce = src.nNonce;
        bnPrimeChainMultiplier = src.bnPrimeChainMultiplier;
    }
	
    uint256 GetHeaderHash() const
    {
        return Hash(BEGIN_HASH(nVersion), END_HASH(nNonce));
    }
};

#endif //BLOCK_H
