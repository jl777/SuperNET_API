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

#define EXCHANGE_NAME "quadriga"
#define UPDATE prices777_ ## quadriga
#define SUPPORTS quadriga ## _supports
#define SIGNPOST quadriga ## _signpost
#define TRADE quadriga ## _trade
#define ORDERSTATUS quadriga ## _orderstatus
#define CANCELORDER quadriga ## _cancelorder
#define OPENORDERS quadriga ## _openorders
#define TRADEHISTORY quadriga ## _tradehistory
#define BALANCES quadriga ## _balances
#define PARSEBALANCE quadriga ## _parsebalance
#define WITHDRAW quadriga ## _withdraw

double UPDATE(struct prices777 *prices,int32_t maxdepth)
{
    if ( prices->url[0] == 0 )
        sprintf(prices->url,"https://api.quadrigacx.com/v2/order_book?book=%s_%s",prices->lbase,prices->lrel);
    return(prices777_standard("quadriga",prices->url,prices,0,0,maxdepth,0));
}

int32_t SUPPORTS(char *base,char *rel)
{
    char *baserels[][2] = { {"btc","usd"}, {"btc","cad"} };
    return(baserel_polarity(baserels,(int32_t)(sizeof(baserels)/sizeof(*baserels)),base,rel));
}

cJSON *SIGNPOST(char **retstrp,struct exchange_info *exchange,char *payload,char *path)
{
    static CURL *cHandle;
    char url[1024],req[1024],md5secret[128],tmp[1024],dest[1025],hdr1[512],hdr2[512],hdr3[512],hdr4[512],*sig,*data = 0;
    cJSON *json; uint64_t nonce;
    hdr1[0] = hdr2[0] = hdr3[0] = hdr4[0] = 0;
    json = 0;
    nonce = exchange_nonce(exchange) * 1000 + ((uint64_t)milliseconds() % 1000);
    sprintf(tmp,"%llu%s%s",(long long)nonce,exchange->userid,exchange->apikey);
    calc_md5(md5secret,exchange->apisecret,(int32_t)strlen(exchange->apisecret));
    if ( (sig= hmac_sha256_str(dest,md5secret,(int32_t)strlen(md5secret),tmp)) != 0 )
    {
        sprintf(req,"{\"key\":\"%s\",%s\"nonce\":%llu,\"signature\":\"%s\"}",exchange->apikey,payload,(long long)nonce,sig);
        sprintf(hdr1,"Content-Type:application/json"), sprintf(hdr2,"charset=utf-8"), sprintf(hdr3,"Content-Length:%ld",(long)strlen(req));
        sprintf(url,"https://api.quadrigacx.com/v2/%s",path);
        if ( (data= curl_post(&cHandle,url,0,req,hdr1,hdr2,hdr3,hdr4)) != 0 )
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
    char payload[1024],pairstr[64],*extra,*path; cJSON *json; uint64_t txid = 0;
    if ( (extra= *retstrp) != 0 )
        *retstrp = 0;
    if ( (dir= flipstr_for_exchange(exchange,pairstr,"%s_%s",dir,&price,&volume,base,rel)) == 0 )
    {
        printf("cant find baserel (%s/%s)\n",base,rel);
        return(0);
    }
    path = (dir > 0) ? "buy" : "sell";
    //key - API key
    //signature - signature
    //nonce - nonce
    //amount - amount of major currency
    //price - price to buy at
    //book - optional, if not specified, will default to btc_cad
    sprintf(payload,"\"amount\":%.6f,\"price\":%.3f,\"book\":\"%s_%s\",",volume,price,base,rel);
    if ( (json= SIGNPOST(retstrp,exchange,payload,path)) != 0 )
    {
        // parse json and set txid
        free_json(json);
    }
    return(txid);
}

cJSON *BALANCES(struct exchange_info *exchange)
{
    return(SIGNPOST(0,exchange,"","balance"));
}

char *PARSEBALANCE(struct exchange_info *exchange,double *balancep,char *coinstr)
{
    //[{"btc_available":"0.00000000","btc_reserved":"0.00000000","btc_balance":"0.00000000","cad_available":"0.00","cad_reserved":"0.00","cad_balance":"0.00","usd_available":"0.00","usd_reserved":"0.00","usd_balance":"0.00","xau_available":"0.000000","xau_reserved":"0.000000","xau_balance":"0.000000","fee":"0.5000"}]
    char field[128],*str,*itemstr = 0; cJSON *obj; double reserv,total;
    *balancep = 0.;
    strcpy(field,coinstr);
    tolowercase(field);
    strcat(field,"_available");
    if ( exchange->balancejson != 0 && (str= jstr(exchange->balancejson,field)) != 0 )
    {
        *balancep = jdouble(exchange->balancejson,field);
        strcpy(field,coinstr), tolowercase(field), strcat(field,"_reserved");
        reserv = jdouble(exchange->balancejson,field);
        strcpy(field,coinstr), tolowercase(field), strcat(field,"_balance");
        total = jdouble(exchange->balancejson,field);
        obj = cJSON_CreateObject();
        jaddnum(obj,"balance",*balancep);
        jaddnum(obj,"locked_balance",reserv);
        jaddnum(obj,"total",total);
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

struct exchange_funcs quadriga_funcs = EXCHANGE_FUNCS(quadriga,EXCHANGE_NAME);

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

