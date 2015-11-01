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

#define EXCHANGE_NAME "btce"
#define UPDATE prices777_ ## btce
#define SUPPORTS btce ## _supports
#define SIGNPOST btce ## _signpost
#define TRADE btce ## _trade
#define ORDERSTATUS btce ## _orderstatus
#define CANCELORDER btce ## _cancelorder
#define OPENORDERS btce ## _openorders
#define TRADEHISTORY btce ## _tradehistory
#define BALANCES btce ## _balances
#define PARSEBALANCE btce ## _parsebalance
#define WITHDRAW btce ## _withdraw
#define EXCHANGE_AUTHURL "https://btc-e.com/tapi"

double UPDATE(struct prices777 *prices,int32_t maxdepth)
{
    char field[64];
    sprintf(field,"%s_%s",prices->lbase,prices->lrel);
    if ( prices->url[0] == 0 )
        sprintf(prices->url,"https://btc-e.com/api/3/depth/%s",field);
    return(prices777_standard("btce",prices->url,prices,0,0,maxdepth,field));
}

int32_t SUPPORTS(char *base,char *rel)
{
    char *baserels[][2] = { {"btc","usd"}, {"btc","rur"}, {"btc","eur"}, {"ltc","btc"}, {"ltc","usd"}, {"ltc","rur"}, {"ltc","eur"}, {"nmc","btc"}, {"nmc","usd"}, {"nvc","btc"}, {"nvc","usd"}, {"eur","usd"}, {"eur","rur"}, {"ppc","btc"}, {"ppc","usd"} };
    return(baserel_polarity(baserels,(int32_t)(sizeof(baserels)/sizeof(*baserels)),base,rel));
}

cJSON *SIGNPOST(char **retstrp,struct exchange_info *exchange,char *url,char *payload)
{
    static CURL *cHandle;
    char dest[SHA512_DIGEST_SIZE*2+1],hdr1[512],hdr2[512],hdr3[512],hdr4[512],*data,*sig; cJSON *json;
    hdr1[0] = hdr2[0] = hdr3[0] = hdr4[0] = 0;
    json = 0;
    if ( (sig= hmac_sha512_str(dest,exchange->apisecret,(int32_t)strlen(exchange->apisecret),payload)) != 0 )
        sprintf(hdr1,"Sign:%s",sig);
    else hdr1[0] = 0;
    sprintf(hdr2,"Key:%s",exchange->apikey);
    if ( (data= curl_post(&cHandle,url,0,payload,hdr1,hdr2,hdr3,hdr4)) != 0 )
        json = cJSON_Parse(data);
    if ( retstrp != 0 )
        *retstrp = data;
    else if ( data != 0 )
        free(data);
    return(json);
}

uint64_t TRADE(char **retstrp,struct exchange_info *exchange,char *base,char *rel,int32_t dir,double price,double volume)
{
    char payload[1024],pairstr[512],*extra; cJSON *json,*resultobj; uint64_t txid = 0;
    if ( (extra= *retstrp) != 0 )
        *retstrp = 0;
    if ( (dir= flipstr_for_exchange(exchange,pairstr,"%s_%s",dir,&price,&volume,base,rel)) == 0 )
    {
        printf("cant find baserel (%s/%s)\n",base,rel);
        return(0);
    }
    sprintf(payload,"method=Trade&nonce=%llu&pair=%s&type=%s&rate=%.3f&amount=%.6f",(long long)exchange_nonce(exchange),pairstr,dir>0?"buy":"sell",price,volume);
    if ( (json= SIGNPOST(retstrp,exchange,EXCHANGE_AUTHURL,payload)) != 0 )
    {
        //{ "success":1, "return":{ "received":0.1, "remains":0, "order_id":0, "funds":{ "usd":325, "btc":2.498,  } } }
        if ( juint(json,"success") > 0 && (resultobj= jobj(json,"return")) != 0 )
        {
            if ( (txid= j64bits(resultobj,"order_id")) == 0 )
            {
                if ( j64bits(resultobj,"remains") == 0 )
                    txid = _crc32(0,payload,strlen(payload));
            }
        }
        free_json(json);
    }
    return(txid);
}

cJSON *BALANCES(struct exchange_info *exchange)
{
    char payload[1024];
    sprintf(payload,"method=getInfo&nonce=%llu",(long long)exchange_nonce(exchange));
    return(SIGNPOST(0,exchange,EXCHANGE_AUTHURL,payload));
}

char *PARSEBALANCE(struct exchange_info *exchange,double *balancep,char *coinstr)
{
    //btce.({"success":1,"return":{"funds":{"usd":73.02571846,"btc":0,"ltc":0,"nmc":0,"rur":0,"eur":0,"nvc":0.0000322,"trc":0,"ppc":0.00000002,"ftc":0,"xpm":2.28605349,"cnh":0,"gbp":0},"rights":{"info":1,"trade":1,"withdraw":0},"transaction_count":0,"open_orders":3,"server_time":1441918649}})
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
    if ( (json= SIGNPOST(&retstr,exchange,EXCHANGE_AUTHURL,payload)) != 0 )
    {
        free_json(json);
    }
    return(retstr); // return standardized orderstatus
}

char *CANCELORDER(struct exchange_info *exchange,cJSON *argjson,uint64_t quoteid)
{
    char payload[1024],*retstr = 0; cJSON *json;
    // generate payload
    if ( (json= SIGNPOST(&retstr,exchange,EXCHANGE_AUTHURL,payload)) != 0 )
    {
        free_json(json);
    }
    return(retstr); // return standardized cancelorder
}

char *OPENORDERS(struct exchange_info *exchange,cJSON *argjson)
{
    char payload[1024],*retstr = 0; cJSON *json;
    // generate payload
    if ( (json= SIGNPOST(&retstr,exchange,EXCHANGE_AUTHURL,payload)) != 0 )
    {
        free_json(json);
    }
    return(retstr); // return standardized open orders
}

char *TRADEHISTORY(struct exchange_info *exchange,cJSON *argjson)
{
    char payload[1024],*retstr = 0; cJSON *json;
    // generate payload
    if ( (json= SIGNPOST(&retstr,exchange,EXCHANGE_AUTHURL,payload)) != 0 )
    {
        free_json(json);
    }
    return(retstr); // return standardized tradehistory
}

char *WITHDRAW(struct exchange_info *exchange,cJSON *argjson)
{
    char payload[1024],*retstr = 0; cJSON *json;
    // generate payload
    if ( (json= SIGNPOST(&retstr,exchange,EXCHANGE_AUTHURL,payload)) != 0 )
    {
        free_json(json);
    }
    return(retstr); // return standardized withdraw
}

struct exchange_funcs btce_funcs = EXCHANGE_FUNCS(btce,EXCHANGE_NAME);

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
