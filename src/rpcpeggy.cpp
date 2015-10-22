#ifdef PEGGY
//
// Created by BTCDDev on 8/17/15.
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "main.h"
#include "bitcoinrpc.h"
#include "kernel.h"
#include "init.h"
#include "wallet.h"
#include "walletdb.h"
#include "wallet.h"
#include "../libjl777/plugins/includes/cJSON.h"
#include <stdlib.h>
#include <stdio.h>
using namespace json_spirit;
using namespace std;

//libjl777 function declarations
extern "C" char *peggybase(uint32_t blocknum,uint32_t blocktimestamp);
extern "C" char *peggypayments(uint32_t blocknum,uint32_t blocktimestamp);
extern "C" char *peggy_tx(char *jsonstr);
extern "C" int32_t decode_hex(unsigned char *bytes,int32_t n,char *hex);

extern void TxToJSON(const CTransaction& tx, const uint256 hashBlock, Object& entry);


extern "C" int8_t isOpReturn(char *hexbits)
{
    if(hexbits == 0) return -1;
    if(strlen(hexbits) < (size_t)4) return -1;
    int8_t isOPRETURN = -1;
    char temp[4];
    sprintf(temp, "%c%c", hexbits[0], hexbits[1]);
    if(strcmp("6a", temp) == 0)
        isOPRETURN = 0;
    return isOPRETURN;
}

extern "C" char* GetPeggyByBlock(CBlock *pblock, CBlockIndex *pindex)
{
    cJSON *array, *arrayObj, *header, *peggybase, *peggypayments, *peggytx;
    header = cJSON_CreateObject();
    peggybase = cJSON_CreateObject();
    peggypayments = cJSON_CreateArray();
    peggytx = cJSON_CreateArray();
    array = cJSON_CreateArray();
    arrayObj = cJSON_CreateObject();
    //header
    jaddnum(header, "blocknum", pindex->nHeight);
    jaddnum(header, "blocktimestamp", pblock->nTime);
    jaddstr(header, "blockhash", (char*)pindex->GetBlockHash().ToString().c_str());

    jadd(arrayObj, "header", header);

    if(pindex->nHeight >= nMinPeggyHeight){
        if(!pblock->vtx[2].IsPeggyBase())
            throw runtime_error(
                "Block Does not contain a peggybase transaction!\n"
            );
    }
    else{
        return "Not a peggy block\n";
    }


    int index, vouts;

    //peggypayments
    char bits[4096];
    char hex[4096];

    bool fPeggy = false;
    uint64_t nPeggyPayments = 0;

    for(index=0; index<pblock->vtx.size(); index++){
        fPeggy = false;
        const CTransaction tempTx = pblock->vtx[index];
        if(tempTx.IsCoinBase() || tempTx.IsCoinStake())
            continue;

        if(tempTx.IsPeggyBase()){
            const CTxOut peggyOut = tempTx.vout[0];
            const CScript priceFeed = peggyOut.scriptPubKey;

            char peggybits[4096];
            strcpy(peggybits, (char*)HexStr(priceFeed.begin(), priceFeed.end(), false).c_str()+2);

            jaddstr(peggybase, "txid", (char*)tempTx.GetHash().ToString().c_str());
            jaddstr(peggybase, "peggybase", peggybits);
            jaddnum(peggybase, "time", tempTx.nTime);
            jaddnum(peggybase, "txind", 2);
            jaddnum(peggybase, "voutind", 0);
            jaddstr(peggybase, "address", "peggypayments");
            fPeggy = true;
        }

        for(vouts=0; vouts<tempTx.vout.size(); vouts++){

            const CTxOut tempVout = tempTx.vout[vouts];

            const CScript tempScriptPubKey = tempVout.scriptPubKey;
            CTxDestination destAddress;
            strcpy(hex, (char*)HexStr(tempScriptPubKey.begin(), tempScriptPubKey.end(), false).c_str());
            sprintf(hex, "%s", hex);

            if(fPeggy && (vouts != 0)){
                cJSON *peggyOut = cJSON_CreateObject();


                jaddstr(peggyOut, "txid", (char*)tempTx.GetHash().ToString().c_str());
                jaddnum(peggyOut, "time", tempTx.nTime);
                jaddnum(peggyOut, "txind", index);
                jaddnum(peggyOut, "amount", tempVout.nValue);
                jaddnum(peggyOut, "voutind", vouts);
                jaddstr(peggyOut, "scriptPubKey", hex);

                if(ExtractDestination(tempScriptPubKey, destAddress)){
                    jaddstr(peggyOut, "address", (char*)CBitcoinAddress(destAddress).ToString().c_str());
                }
                else{
                    jaddstr(peggyOut, "address", "null");
                }

                nPeggyPayments += (uint64_t)tempVout.nValue;

                jaddi(peggypayments, peggyOut);
            }

            else if(!fPeggy && (isOpReturn(hex) == 0)){ //peggy lock found
                cJSON *lockVout = cJSON_CreateObject();

                jaddstr(lockVout, "txid", (char*)tempTx.GetHash().ToString().c_str());
                jaddnum(lockVout, "time", tempTx.nTime);
                jaddnum(lockVout, "txind", index);
                jaddnum(lockVout, "voutind", vouts);
                jaddstr(lockVout, "scriptPubKey", hex);

                if(ExtractDestination(tempScriptPubKey, destAddress)){
                    jaddstr(lockVout, "address", (char*)CBitcoinAddress(destAddress).ToString().c_str());
                }
                else{
                    jaddstr(lockVout, "address", "null");
                }

                jaddnum(lockVout, "amount", (uint64_t)tempVout.nValue);

                jaddi(peggytx, lockVout);

                continue; //1 op_return per tx
            }

        }
    }
    jaddnum(peggybase, "amount", nPeggyPayments);

    jadd(arrayObj, "peggybase", peggybase);
    jadd(arrayObj, "peggypayments", peggypayments);
    jadd(arrayObj, "peggytx", peggytx);

    jaddi(array, arrayObj);
    return jprint(array,0);
}

extern "C" char* GetPeggyByHeight(uint32_t blocknum) //(0-based)
{
    CBlockIndex *pindex = FindBlockByHeight(blocknum);
    CBlock block;
    if(!block.ReadFromDisk(pindex, true))
        throw runtime_error(
            "Could not find block\n"
        );
    return GetPeggyByBlock(&block, pindex);
}


/*
*
*   Begin RPC functions
*
*/

/*
peggytx '{"BitcoinDark": "A revolution in cryptocurrency"}' '{"RWoDDki8gfqYMHDEzsyFdsCtdSkB79DbVc":1}' false
*/
Value peggytx(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 2)
        throw runtime_error(
            "peggytx\n"
            "Creates a peggy transaction: \n"
            "'<json string>' '{\"<btcd addr>\" : <amount>}' [send?] \n"
            "!WARNING!: adding true as an option will attempt to automatically send coins from your wallet."
            "You will not be able to get them back until you redeem an equivalent number of BTCD."
        );
    std::string retVal("");
    const std::string peggyJson = params[0].get_str();
    const Object& sendTo = params[1].get_obj();
    bool signAndSend = false;
    if (params.size() > 2)
        signAndSend = params[2].get_bool();
    const Pair& out = sendTo[0];
    CBitcoinAddress returnAddr = CBitcoinAddress(out.name_);
    if (!returnAddr.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, string("Invalid BitcoinDark address: ")+out.name_);

    int64_t amountLocked = AmountFromValue(out.value_);
    std::string hex = HexStr(peggyJson.begin(), peggyJson.end(), false);

    //Construct a peggy locking hexstr from the json and the redeem address/lock amount.
    char *peggytx = peggy_tx((char*)peggyJson.c_str());

    int i;
    CWallet wallet;
    CWalletTx wtx;
    CScript scriptPubKey = CScript();

    unsigned char buf[4096];
    char test[100];

    strcpy(test, (char*)hex.c_str());
    if(strlen(test) > 0)
        decode_hex(buf,(int)strlen(test),test);
    fprintf(stderr, "peggytx=%s\n", peggytx);
    scriptPubKey << OP_RETURN;
    scriptPubKey << ParseHex(peggytx);
    //for(i=0;i<strlen((const char*)buf);i++)
       // scriptPubKey << test[i];
    //scriptPubKey << ParseHex(hex);
    //scriptPubKey.SetDestination(returnAddr.Get());
    CReserveKey reservekey(pwalletMain);
    int64_t nFeeRequired;
    if(!pwalletMain->CreateTransaction(scriptPubKey, amountLocked, wtx, reservekey, nFeeRequired))
        return std::string("Failed to Create the Transaction. Is your wallet unlocked?\n");


    CDataStream ssTx(SER_NETWORK, PROTOCOL_VERSION);
    ssTx << wtx;
    string strHex = HexStr(ssTx.begin(), ssTx.end());


    cJSON *obj = cJSON_CreateObject();

    jaddstr(obj, "txid", (char*)wtx.GetHash().ToString().c_str());
    jaddstr(obj, "rawtx", (char*)strHex.c_str());
    jaddstr(obj, "opreturnstr", (char*)HexStr(scriptPubKey.begin(), scriptPubKey.end(), false).c_str());

    free(peggytx);

    if(signAndSend){
        if(!pwalletMain->CommitTransaction(wtx, reservekey))
            return std::string("The transaction was Rejected\n");
        else{
           return jprint(obj, 1);
        }
    }
    else{
       return jprint(obj, 1);
    }
/*
                  CTransaction peggy;
                    char *paymentScript= "{\"RWoDDki8gfqYMHDEzsyFdsCtdSkB79DbVc\":10000000}"; // temp.

                    char *priceFeedHash = "5f43ac64";
                    if(wallet.CreatePeggyBase(peggy, paymentScript, priceFeedHash))
                    {
                        peggy.nTime = 0;

                    }

                    Object o;
                            TxToJSON(peggy, 0, o);
                           return o;*/
}

Value getpeggyblock(const Array& params, bool fHelp)
{
    if(fHelp || params.size() != 1)
        throw runtime_error(
            "getpeggyblock <blockheight> "
            "returns all peggy information about a block\n"
        );
    int64_t nHeight = params[0].get_int64();

    if(nHeight < nMinPeggyHeight)
        throw runtime_error(
            "getpeggyblock <blockheight> "
            "the block height you entered is not a peggy block\n"
        );

    char *peggy = GetPeggyByHeight(nHeight);
    std::string retVal(peggy);
    free(peggy);
    return retVal;

}
Value peggypayments(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1)
        throw runtime_error(
            "peggypayments <block height>\n"
            "Shows all redeems for a certain block: \n"
            "(-1 for latest block)\n"
            );


        int64_t nHeight, nBlockTime;

        nHeight = params[0].get_int64();

        if(nHeight != -1){

            if(nHeight < nMinPeggyHeight || nHeight > pindexBest->nHeight)
                throw runtime_error(
                    "peggypayments <block height> <block time>\n"
                    "the block height you entered is not a peggy block, or the height is out of range\n"
                );


            CBlockIndex *pindex = FindBlockByHeight(nHeight);
            nHeight = pindex->nHeight;
            nBlockTime = pindex->GetBlockTime();
        }
        else{
            nHeight = pindexBest->nHeight;
            nBlockTime = pindexBest->GetBlockTime();
        }
        CWallet wallet;
        char *paymentScript = peggypayments(nHeight, nBlockTime);
        //char *priceFeedHash = peggybase(nHeight, nBlockTime);

        std::string retVal = std::string(paymentScript);
        free(paymentScript);
        return std::string(paymentScript);
}
#endif
