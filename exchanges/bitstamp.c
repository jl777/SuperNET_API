/******************************************************************************
 * Copyright © 2014-2015 The SuperNET Developers.                             *
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

#define EXCHANGE_NAME "bitstamp"
#define UPDATE prices777_ ## bitstamp
#define SUPPORTS bitstamp ## _supports
#define SIGNPOST bitstamp ## _signpost
#define TRADE bitstamp ## _trade
#define ORDERSTATUS bitstamp ## _orderstatus
#define CANCELORDER bitstamp ## _cancelorder
#define OPENORDERS bitstamp ## _openorders
#define TRADEHISTORY bitstamp ## _tradehistory
#define BALANCES bitstamp ## _balances
#define PARSEBALANCE bitstamp ## _parsebalance
#define WITHDRAW bitstamp ## _withdraw
#define EXCHANGE_AUTHURL "https://www.bitstamp.net/api"

double UPDATE(struct prices777 *prices,int32_t maxdepth)
{
    if ( prices->url[0] == 0 )
        sprintf(prices->url,"https://www.bitstamp.net/api/order_book/");
    return(prices777_standard("bitstamp",prices->url,prices,0,0,maxdepth,0));
}

int32_t SUPPORTS(char *base,char *rel)
{
    char *baserels[][2] = { {"btc","usd"} };
    return(baserel_polarity(baserels,(int32_t)(sizeof(baserels)/sizeof(*baserels)),base,rel));
}

cJSON *SIGNPOST(char **retstrp,struct exchange_info *exchange,char *url,char *payload)
{
    /*signature is a HMAC-SHA256 encoded message containing: nonce, customer ID (can be found here) and API key. The HMAC-SHA256 code must be generated using a secret key that was generated with your API key. This code must be converted to it's hexadecimal representation (64 uppercase characters).Example (Python):
     message = nonce + customer_id + api_key
     signature = hmac.new(API_SECRET, msg=message, digestmod=hashlib.sha256).hexdigest().upper()
     
     key - API key
     signature - signature
     nonce - nonce
     */
    static CURL *cHandle;
    char dest[1025],req[1024],hdr1[512],hdr2[512],hdr3[512],hdr4[512],*sig,*data = 0;
    cJSON *json; uint64_t nonce;
    hdr1[0] = hdr2[0] = hdr3[0] = hdr4[0] = 0;
    nonce = exchange_nonce(exchange);
    sprintf(req,"%llu%s%s",(long long)nonce,exchange->userid,exchange->apikey);
    json = 0;
    if ( (sig= hmac_sha256_str(dest,exchange->apisecret,(int32_t)strlen(exchange->apisecret),req)) != 0 )
    {
        touppercase(sig);
        sprintf(req,"{\"key\":\"%s\",\"signature\":\"%s\",\"nonce\":%llu}",exchange->apikey,sig,(long long)nonce);
        if ( (data= curl_post(&cHandle,url,0,payload,req,hdr2,hdr3,hdr4)) != 0 )
            json = cJSON_Parse(data);
    }
    if ( retstrp != 0 )
        *retstrp = data;
    else if ( data != 0 )
        free(data);
    return(json);
}

uint64_t TRADE(char **retstrp,struct exchange_info *exchange,char *base,char *rel,int32_t dir,double price,double volume)
{
    char payload[1024],url[512],pairstr[512],*extra; cJSON *json; uint64_t txid = 0;
    if ( (extra= *retstrp) != 0 )
        *retstrp = 0;
    if ( (dir= flipstr_for_exchange(exchange,pairstr,"%s%s",dir,&price,&volume,base,rel)) == 0 )
    {
        printf("cant find baserel (%s/%s)\n",base,rel);
        return(0);
    }
    sprintf(url,"%s/%s/",EXCHANGE_AUTHURL,dir>0 ? "buy" : "sell");
    if ( (json= SIGNPOST(retstrp,exchange,url,payload)) != 0 )
    {
        // parse json and set txid
        free_json(json);
    }
    return(txid);
}
    
cJSON *BALANCES(struct exchange_info *exchange)
{
    char payload[1024],url[512];
    sprintf(url,"%s/%s/",EXCHANGE_AUTHURL,"balance");
    return(SIGNPOST(0,exchange,url,payload));
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

char *ORDERSTATUS(struct exchange_info *exchange,cJSON *argjson,uint64_t quoteid)
{
    char payload[1024],*retstr = 0; cJSON *json;
    // generate payload
    if ( (json= SIGNPOST(&retstr,exchange,"https://",payload)) != 0 )
    {
        free_json(json);
    }
    return(retstr); // return standardized orderstatus
}

char *CANCELORDER(struct exchange_info *exchange,cJSON *argjson,uint64_t quoteid)
{
    char payload[1024],*retstr = 0; cJSON *json;
    // generate payload
    if ( (json= SIGNPOST(&retstr,exchange,"https://",payload)) != 0 )
    {
        free_json(json);
    }
    return(retstr); // return standardized cancelorder
}

char *OPENORDERS(struct exchange_info *exchange,cJSON *argjson)
{
    char payload[1024],*retstr = 0; cJSON *json;
    // generate payload
    if ( (json= SIGNPOST(&retstr,exchange,"https://",payload)) != 0 )
    {
        free_json(json);
    }
    return(retstr); // return standardized open orders
}

char *TRADEHISTORY(struct exchange_info *exchange,cJSON *argjson)
{
    char payload[1024],*retstr = 0; cJSON *json;
    // generate payload
    if ( (json= SIGNPOST(&retstr,exchange,"https://",payload)) != 0 )
    {
        free_json(json);
    }
    return(retstr); // return standardized tradehistory
}

char *WITHDRAW(struct exchange_info *exchange,cJSON *argjson)
{
    char payload[1024],*retstr = 0; cJSON *json;
    // generate payload
    if ( (json= SIGNPOST(&retstr,exchange,"https://",payload)) != 0 )
    {
        free_json(json);
    }
    return(retstr); // return standardized withdraw
}

struct exchange_funcs bitstamp_funcs = EXCHANGE_FUNCS(bitstamp,EXCHANGE_NAME);

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
