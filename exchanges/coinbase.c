/******************************************************************************
 * Copyright © 2014-2018 The SuperNET Developers.                             *
 *                                                                            *
 * See the AUTHORS, DEVELOPER-AGREEMENT and LICENSE files at                  *
 * the top-level directory of this distribution for the individual copyright  *
 * holder information and the developer policies on copyright and licensing.  *
 *                                                                            *
 * Unless otherwise agreed in a custom licensing agreement, no part of the    *
 * SuperNET software, including this file may be copied, modified, propagated *
 * or distributed except according to the terms contained in the LICENSE file *
 *                                                                            *
 * Removal or modification of this copyright notice is prohibited.            *
 *                                                                            *
 ******************************************************************************/

#define EXCHANGE_NAME "coinbase"
#define UPDATE prices777_ ## coinbase
#define SUPPORTS coinbase ## _supports
#define SIGNPOST coinbase ## _signpost
#define TRADE coinbase ## _trade
#define ORDERSTATUS coinbase ## _orderstatus
#define CANCELORDER coinbase ## _cancelorder
#define OPENORDERS coinbase ## _openorders
#define TRADEHISTORY coinbase ## _tradehistory
#define BALANCES coinbase ## _balances
#define PARSEBALANCE coinbase ## _parsebalance
#define WITHDRAW coinbase ## _withdraw
#define EXCHANGE_AUTHURL "https://api.exchange.coinbase.com"
#define CHECKBALANCE coinbase ## _checkbalance

double UPDATE(struct prices777 *prices,int32_t maxdepth)
{
    if ( prices->url[0] == 0 )
        sprintf(prices->url,"https://api.exchange.coinbase.com/products/%s-%s/book?level=2",prices->base,prices->rel);
    return(prices777_standard("coinbase",prices->url,prices,0,0,maxdepth,0));
}

int32_t SUPPORTS(char *base,char *rel)
{
    char *baserels[][2] = { {"btc","usd"} };
    return(baserel_polarity(baserels,(int32_t)(sizeof(baserels)/sizeof(*baserels)),base,rel));
}

cJSON *SIGNPOST(void **cHandlep,int32_t dotrade,char **retstrp,struct exchange_info *exchange,char *payload,uint64_t nonce,char *path,char *method)
{
    /*All REST requests must contain the following headers:
     
     CB-ACCESS-KEY The api key as a string.
     CB-ACCESS-SIGN The base64-encoded signature (see Signing a Message).
     CB-ACCESS-TIMESTAMP A timestamp for your request.
     CB-ACCESS-PASSPHRASE The passphrase you specified when creating the API key.
     All request bodies should have content type application/json and be valid JSON.
     
     Signing a Message
     The CB-ACCESS-SIGN header is generated by creating a sha256 HMAC using the base64-decoded
     secret key on the prehash string timestamp + method + requestPath + body (where + represents string concatenation)
     and base64-encode the output. The timestamp value is the same as the CB-ACCESS-TIMESTAMP header.
     
     The body is the request body string or omitted if there is no request body (typically for GET requests).
     
     The method should be UPPER CASE
     Remember to first base64-decode the alphanumeric secret string (resulting in 64 bytes) before using it as the key for HMAC. Also, base64-encode the digest output before sending in the header.
     */
   /* def __call__(self, request):
    timestamp = str(time.time())
    message = timestamp + request.method + request.path_url + (request.body or '')
    hmac_key = base64.b64decode(self.secret_key)
    signature = hmac.new(hmac_key, message, hashlib.sha256)
    signature_b64 = signature.digest().encode('base64').rstrip('\n')
    
    request.headers.update({
        'CB-ACCESS-SIGN': signature_b64,
        'CB-ACCESS-TIMESTAMP': timestamp,
        'CB-ACCESS-KEY': self.api_key,
        'CB-ACCESS-PASSPHRASE': self.passphrase,
        'Content-Type': 'application/json'
    })*/
    char url[1024],hdr1[512],hdr2[512],hdr3[512],hdr4[512],dest[1024]; cJSON *json; int32_t n;
    char prehash64[512],prehash[512],decodedsecret[512],sig64[512],*sig,*data = 0;
    hdr1[0] = hdr2[0] = hdr3[0] = hdr4[0] = 0;
    json = 0;
    n = nn_base64_decode((void *)exchange->apisecret,strlen(exchange->apisecret),(void *)decodedsecret,sizeof(decodedsecret));
    sprintf(prehash,"%llu%s/%s%s",(long long)nonce,method,path,payload);
    nn_base64_encode((void *)prehash,strlen(prehash),prehash64,sizeof(prehash64));
    if ( (sig= hmac_sha256_str(dest,decodedsecret,n,prehash64)) != 0 )
    {
        nn_base64_encode((void *)sig,strlen(sig),sig64,sizeof(sig64));
        //CB-ACCESS-KEY The api key as a string.
        //CB-ACCESS-SIGN The base64-encoded signature (see Signing a Message).
        //CB-ACCESS-TIMESTAMP A timestamp for your request.
        //CB-ACCESS-PASSPHRASE The passphrase you specified when creating the API key.
        sprintf(hdr1,"CB-ACCESS-KEY:%s",exchange->apikey);
        sprintf(hdr2,"CB-ACCESS-SIGN:%s",sig64);
        sprintf(hdr3,"CB-ACCESS-TIMESTAMP:%llu",(long long)nonce);
        //sprintf(hdr4,"CB-ACCESS-PASSPHRASE:%s; content-type:application/json; charset=utf-8",exchange->userid);
        sprintf(hdr4,"CB-ACCESS-PASSPHRASE:%s",exchange->userid);
        sprintf(url,"%s/%s",EXCHANGE_AUTHURL,path);
        if ( dotrade == 0 )
            data = exchange_would_submit(payload,hdr1,hdr2,hdr3,hdr4);
        else if ( (data= curl_post(cHandlep,url,0,payload,hdr1,hdr2,hdr3,hdr4)) != 0 )
            json = cJSON_Parse(data);
    }
    if ( retstrp != 0 )
        *retstrp = data;
    else if ( data != 0 )
        free(data);
    return(json);
}

cJSON *BALANCES(void **cHandlep,struct exchange_info *exchange)
{
    return(SIGNPOST(cHandlep,1,0,exchange,"",exchange_nonce(exchange),"accounts","GET"));
}

char *PARSEBALANCE(struct exchange_info *exchange,double *balancep,char *coinstr)
{
    char field[128],*itemstr = 0; cJSON *obj,*item;
    *balancep = 0.;
    strcpy(field,coinstr);
    tolowercase(field);
    if ( exchange->balancejson != 0 && (obj= jobj(exchange->balancejson,"return")) != 0 && (item= jobj(obj,"funds")) != 0 )
    {
        *balancep = jdouble(item,field);
        obj = cJSON_CreateObject();
        touppercase(field);
        jaddstr(obj,"base",field);
        jaddnum(obj,"balance",*balancep);
        itemstr = jprint(obj,1);
    }
    if ( itemstr == 0 )
        return(clonestr("{\"error\":\"cant find coin balance\"}"));
    return(itemstr);
}

#include "checkbalance.c"

uint64_t TRADE(void **cHandlep,int32_t dotrade,char **retstrp,struct exchange_info *exchange,char *base,char *rel,int32_t dir,double price,double volume)
{
    char payload[1024],pairstr[512],method[32],*path,*extra;
    cJSON *json; uint64_t nonce,txid = 0;
    if ( (extra= *retstrp) != 0 )
        *retstrp = 0;
    path = "trade", strcpy(method,"POST");
    if ( (dir= flip_for_exchange(pairstr,"%s_%s","BTC",dir,&price,&volume,base,rel)) == 0 )
    {
        printf("cant find baserel (%s/%s)\n",base,rel);
        return(0);
    }
    nonce = exchange_nonce(exchange);
    sprintf(payload,"method=Trade&nonce=%llu&pair=%s&type=%s&rate=%.6f&amount=%.6f",(long long)nonce,pairstr,dir>0?"buy":"sell",price,volume);
    if ( CHECKBALANCE(retstrp,dotrade,exchange,dir,base,rel,price,volume) == 0 && (json= SIGNPOST(cHandlep,dotrade,retstrp,exchange,payload,nonce,path,method)) != 0 )
    {
        // parse json and set txid
        free_json(json);
    }
    return(txid);
}

char *ORDERSTATUS(void **cHandlep,struct exchange_info *exchange,cJSON *argjson,uint64_t quoteid)
{
    char payload[1024],*retstr = 0; cJSON *json;
    // generate payload
    if ( (json= SIGNPOST(cHandlep,1,0,exchange,payload,exchange_nonce(exchange),"accounts","GET")) != 0 )
    {
        free_json(json);
    }
    return(retstr); // return standardized orderstatus
}

char *CANCELORDER(void **cHandlep,struct exchange_info *exchange,cJSON *argjson,uint64_t quoteid)
{
    char payload[1024],*retstr = 0; cJSON *json;
    // generate payload
    if ( (json= SIGNPOST(cHandlep,1,0,exchange,payload,exchange_nonce(exchange),"accounts","GET")) != 0 )
    {
        free_json(json);
    }
    return(retstr); // return standardized cancelorder
}

char *OPENORDERS(void **cHandlep,struct exchange_info *exchange,cJSON *argjson)
{
    char payload[1024],*retstr = 0; cJSON *json;
    // generate payload
    if ( (json= SIGNPOST(cHandlep,1,0,exchange,payload,exchange_nonce(exchange),"accounts","GET")) != 0 )
    {
        free_json(json);
    }
    return(retstr); // return standardized open orders
}

char *TRADEHISTORY(void **cHandlep,struct exchange_info *exchange,cJSON *argjson)
{
    char payload[1024],*retstr = 0; cJSON *json;
    // generate payload
    if ( (json= SIGNPOST(cHandlep,1,0,exchange,payload,exchange_nonce(exchange),"accounts","GET")) != 0 )
    {
        free_json(json);
    }
    return(retstr); // return standardized tradehistory
}

char *WITHDRAW(void **cHandlep,struct exchange_info *exchange,cJSON *argjson)
{
    char payload[1024],*retstr = 0; cJSON *json;
    // generate payload
    if ( (json= SIGNPOST(cHandlep,1,0,exchange,payload,exchange_nonce(exchange),"accounts","GET")) != 0 )
    {
        free_json(json);
    }
    return(retstr); // return standardized withdraw
}

struct exchange_funcs coinbase_funcs = EXCHANGE_FUNCS(coinbase,EXCHANGE_NAME);

#undef UPDATE
#undef SUPPORTS
#undef SIGNPOST
#undef TRADE
#undef ORDERSTATUS
#undef CANCELORDER
#undef OPENORDERS
#undef TRADEHISTORY
#undef BALANCES
#undef PARSEBALANCE
#undef WITHDRAW
#undef EXCHANGE_NAME
#undef EXCHANGE_AUTHURL
#undef CHECKBALANCE
