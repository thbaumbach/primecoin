
#include <iostream>
#include <fstream>
#include <cstdio>

#include "prime.h"
#include "serialize.h"
#include "json/json_spirit_value.h"
#include "bitcoinrpc.h"
//#include "json/json_spirit_reader_template.h"
//#include "json/json_spirit_writer_template.h"
//#include "json/json_spirit_utils.h"

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

extern CBlockIndex* pindexBest; //*TODO*
extern void BitcoinMiner(CWallet *pwallet, CBlock *pblock_input);
extern bool fPrintToConsole;
extern bool fDebug;
extern json_spirit::Object CallRPC(const std::string& strMethod, const json_spirit::Array& params, const std::string& server, const std::string& port);

typedef struct { 
  //comments: BYTES <index> + <length>
	int  version;   // 0+4
	char	prevBlockHash[32]; // 4+32
	char	merkleRoot[32];    //36+32
	unsigned int  timestamp; //68+4
	unsigned int  nBits;     //72+4
	unsigned int  nonce;     //76+4
} blockHeader_t; //=80 bytes header, carefully 'bout (un)packed struct

unsigned int getHexDigitValue(unsigned char c)
{
	if( c >= '0' && c <= '9' )
		return c-'0';
	else if( c >= 'a' && c <= 'f' )
		return c-'a'+10;
	else if( c >= 'A' && c <= 'F' )
		return c-'A'+10;
	return 0;
}

void parseHexString(const char* hexString, unsigned int length, unsigned char* output)
{
	unsigned int lengthBytes = length / 2;
	for(unsigned int i=0; i<lengthBytes; ++i)
	{
		// high digit
		unsigned int d1 = getHexDigitValue(hexString[i*2+0]);
		// low digit
		unsigned int d2 = getHexDigitValue(hexString[i*2+1]);
		// build byte
		output[i] = (unsigned char)((d1<<4)|(d2));	
	}
}

bool getBlock(CBlock& pblock, const std::string& server, const std::string& port) {
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
      std::cout << "[JSON_REQUEST] result(" << strValue.length() << "): " << std::endl << strValue << std::endl;
      
      const json_spirit::Object& result_obj = result_val.get_obj();
      const json_spirit::Value& data_val = find_value(result_obj, "data");
      
      if (data_val.type() == json_spirit::null_type) {
        std::cerr << "[JSON_REQUEST] result empty" << std::endl;
        return false;
      } else if (result_val.type() == json_spirit::str_type)
        strValue = data_val.get_str();
      else
        strValue = write_string(data_val, true);
      std::cout << "[JSON_REQUEST] data(" << strValue.length() << "): " << std::endl << strValue << std::endl;
      
      if (strValue.length() != 258) {
        std::cerr << "[JSON_REQUEST] data length != 258" << std::endl;
        return false;
      }
      
      parseHexString(strValue.c_str()+1, 256, localBlockData);
      
      for (unsigned int i = 0; i < 128/4; ++i)
        ((unsigned int*)localBlockData)[i] = ByteReverse(((unsigned int*)localBlockData)[i]);
    }
  }  
  
  {
    std::stringstream ss;
    for (int i=0; i<8; ++i)
      ss << std::hex << *((int*)(localBlockData+4)+i);
    ss.flush();
    std::cout << ss.str() << std::endl;
    pblock.hashPrevBlock.SetHex(ss.str().c_str());
  }  
  {
    std::stringstream ss;
    for (int i=0; i<8; ++i)
      ss << std::hex << *((int*)(localBlockData+36)+i);
    ss.flush();
    std::cout << ss.str() << std::endl;
    pblock.hashMerkleRoot.SetHex(ss.str().c_str());
  }
  
  pblock.nVersion = *((int*)(localBlockData));
  pblock.nTime  = *((unsigned int*)(localBlockData+68));
  pblock.nBits  = *((unsigned int*)(localBlockData+72));
  pblock.nNonce = *((unsigned int*)(localBlockData+76));

  blockHeader_t* header = (blockHeader_t*)localBlockData;
  std::cout << header->version << std::endl <<
               pblock.hashPrevBlock.ToString() << std::endl <<
               pblock.hashMerkleRoot.ToString() << std::endl <<
               header->timestamp << std::endl <<
               header->nBits << std::endl <<
               header->nonce << std::endl;            
  
  return true;
}

int main (int argc, char** argv)
{
  if (argc < 2)
  {
    std::cerr << "usage: " << argv[0] << " -poolip=<ip> -poolport=<port> -pooluser=<user> -poolpassword=<password>" << std::endl;
    return EXIT_FAILURE;
  }
  
  ParseParameters(argc, argv);
  
  fPrintToConsole = true; //always on
  fDebug = GetBoolArg("-debug");
  std::string server = GetArg("-poolip", "127.0.0.1");
  std::string port = GetArg("-poolport", "9912");
  
  GeneratePrimeTable();
  
  //init variables
  CBlock pblock;
  if (!getBlock(pblock, server, port))
    return EXIT_FAILURE;
  //see CreateNewBlock & Genesis Block
  BitcoinMiner(NULL, &pblock);

  return EXIT_SUCCESS;
}
