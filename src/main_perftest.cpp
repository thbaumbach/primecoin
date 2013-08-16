
#include <iostream>
#include <fstream>
#include <cstdio>

#include "prime.h"
#include "serialize.h"

//<START> be compatible to original code
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

extern bool fPrintToConsole;

int main (int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "usage: " << argv[0] << " <file>" << std::endl;
    return EXIT_FAILURE;
  }
  
  ParseParameters(argc, argv);
  
  fPrintToConsole = true; //always on
  
  std::ifstream input_file(argv[1]);
  std::ifstream input_file_block((std::string(argv[1])+std::string(".blk")).c_str(), std::ifstream::in | std::ifstream::binary);
	if (!input_file.good() || !input_file_block.good()) {
		std::cout << "***ERROR*** error opening files." << std::endl;
		return EXIT_FAILURE;
	}
  
  //init variables
  CBlock pblock;
  mpz_class mpzFixedMultiplier;
  bool fNewBlock = true; //TODO: what if false?
  unsigned int nTriedMultiplier = 0;
  unsigned int nProbableChainLength = 0;
  unsigned int nTests = 0;
  unsigned int nPrimesHit = 0;
  unsigned int nChainsHit = 0;
  int64 nGenSieveTime = 0;
  mpz_class mpzHash;
  unsigned int nPrimorialMultiplier = 0;
  
  std::string line; //for getline-function
  
  ::Unserialize(input_file_block, pblock, 0, 0); //readblock
  
  if (getline(input_file, line)) mpzFixedMultiplier.set_str(line.c_str(), 10);
  if (getline(input_file, line)) fNewBlock = atoi(line.c_str());
  if (getline(input_file, line)) nTriedMultiplier = atoi(line.c_str());
  if (getline(input_file, line)) nPrimorialMultiplier = atoi(line.c_str());
  if (getline(input_file, line)) mpzHash.set_str(line.c_str(), 10);
  
  input_file.close();
  input_file_block.close();
  
  std::cout << "---BLOCK:" << std::endl;
  pblock.print();
  std::cout << "---IN:" << std::endl
            << "   mpzFixedMultiplier: " << mpzFixedMultiplier.get_str(10) << std::endl
            << "   fNewBlock: " << fNewBlock << std::endl
            << "   nTriedMultiplier: " << nTriedMultiplier << std::endl
            << "   nPrimorialMultiplier: " << nPrimorialMultiplier << std::endl
            << "   mpzHash: " << mpzHash.get_str(10) << std::endl;
  
  GeneratePrimeTable();
  
  std::cout << "Initialize: "; // BUILD SIEVE & PRIME TABLE
  MineProbablePrimeChain(pblock,
                         mpzFixedMultiplier,
                         fNewBlock,
                         nTriedMultiplier,
                         nProbableChainLength,
                         nTests,
                         nPrimesHit,
                         nChainsHit,
                         mpzHash,
                         nPrimorialMultiplier,
                         nGenSieveTime,
                         NULL);
  std::cout << "Done." << std::endl;
  
  std::cout << "Mining: "; // MINING
  MineProbablePrimeChain(pblock,
                         mpzFixedMultiplier,
                         fNewBlock,
                         nTriedMultiplier,
                         nProbableChainLength,
                         nTests,
                         nPrimesHit,
                         nChainsHit,
                         mpzHash,
                         nPrimorialMultiplier,
                         nGenSieveTime,
                         NULL);
  std::cout << "Done." << std::endl;

  std::cout << "---OUT:" << std::endl
            << "   nProbableChainLength: " << nProbableChainLength << std::endl
            << "   nTests: " << nTests << std::endl
            << "   nPrimesHit: " << nPrimesHit << std::endl
            << "   nChainsHit: " << nChainsHit << std::endl;

  return EXIT_SUCCESS;
}
