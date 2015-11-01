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

#define EXCHANGE_NAME "okcoin"
#define UPDATE prices777_ ## okcoin
#define SUPPORTS okcoin ## _supports
#define SIGNPOST okcoin ## _signpost
#define TRADE okcoin ## _trade
#define ORDERSTATUS okcoin ## _orderstatus
#define CANCELORDER okcoin ## _cancelorder
#define OPENORDERS okcoin ## _openorders
#define TRADEHISTORY okcoin ## _tradehistory
#define BALANCES okcoin ## _balances
#define PARSEBALANCE okcoin ## _parsebalance
#define WITHDRAW okcoin ## _withdraw
#define EXCHANGE_AUTHURL "https://www.okcoin.com/api/v1"

double UPDATE(struct prices777 *prices,int32_t maxdepth)
{
    if ( prices->url[0] == 0 )
        sprintf(prices->url,"https://www.okcoin.com/api/v1/depth.do?symbol=%s_%s",prices->lbase,prices->lrel);
    if ( strcmp(prices->rel,"USD") != 0 && strcmp(prices->rel,"BTC") != 0 )
    {
        fprintf(stderr,">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> FATAL ERROR OKCOIN.(%s) only supports USD\n",prices->url);
        printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> FATAL ERROR OKCOIN.(%s) only supports USD\n",prices->url);
        exit(-1);
        return(0);
    }
    return(prices777_standard("okcoin",prices->url,prices,0,0,maxdepth,0));
}

int32_t SUPPORTS(char *base,char *rel)
{
    char *baserels[][2] = { {"btc","usd"}, {"ltc","usd"} };
    return(baserel_polarity(baserels,(int32_t)(sizeof(baserels)/sizeof(*baserels)),base,rel));
}

cJSON *SIGNPOST(char **retstrp,struct exchange_info *exchange,char *url,char *payload)
{
    static CURL *cHandle;
    char hdr1[512],hdr2[512],hdr3[512],hdr4[512],*data; cJSON *json;
    hdr1[0] = hdr2[0] = hdr3[0] = hdr4[0] = 0;
    json = 0;
    if ( (data= curl_post(&cHandle,url,0,payload,hdr1,hdr2,hdr3,hdr4)) != 0 )
        json = cJSON_Parse(data);
    if ( retstrp != 0 )
        *retstrp = data;
    else if ( data != 0 )
        free(data);
    return(json);
}
/*
static CURL *cHandle;
char *data,*path,*typestr,*extra,pricestr[64],base[64],rel[64],pairstr[64],url[1024],cmdbuf[8192],buf[512],digest[33]; cJSON *json; uint64_t nonce,txid = 0;
nonce = exchange_nonce(exchange);
if ( (extra= *retstrp) != 0 )
*retstrp = 0;
if ( dir == 0 )
{
    path = "userinfo.do";
    sprintf(buf,"api_key=%s&secret_key=%s",exchange->apikey,exchange->apisecret);
    calc_md5(digest,buf,(int32_t)strlen(buf));
    touppercase(digest);
    sprintf(cmdbuf,"api_key=%s&sign=%s",exchange->apikey,digest);
}
else
{
    path = "trade.do";
    if ( (dir= flipstr_for_exchange(exchange,pairstr,"%s_%s",dir,&price,&volume,base,rel)) == 0 )
    {
        printf("cant find baserel (%s/%s)\n",base,rel);
        return(0);
    }
    if ( extra != 0 && strcmp(extra,"market") == 0 )
        typestr = (dir > 0) ? "buy_market" : "sell_market", sprintf(pricestr,"&price=%.2f",price); // docs say market orders put volume in price
        else typestr = (dir > 0) ? "buy" : "sell", sprintf(pricestr,"&price=%.2f",price);
            sprintf(buf,"amount=%.4f&api_key=%s%ssymbol=%s&type=%s&secret_key=%s",volume,exchange->apikey,pricestr,pairstr,typestr,exchange->apisecret);
            calc_md5(digest,buf,(int32_t)strlen(buf));
            touppercase(digest);
            sprintf(cmdbuf,"amount=%.4f&api_key=%s%s&symbol=%s&type=%s&sign=%s",volume,exchange->apikey,pricestr,pairstr,typestr,digest);
            }
//printf("MD5.(%s)\n",buf);
sprintf(url,"https://www.okcoin.com/api/v1/%s",path);
if ( (data= curl_post(&cHandle,url,0,cmdbuf,0,0,0,0)) != 0 ) // "{\"Content-type\":\"application/x-www-form-urlencoded\"}","{\"User-Agent\":\"OKCoin Javascript API Client\"}"
{
    //printf("submit cmd.(%s) [%s]\n",cmdbuf,data);
    if ( (json= cJSON_Parse(data)) != 0 )
    {
        txid = j64bits(json,"order_id");
        free_json(json);
    }
} else fprintf(stderr,"submit err cmd.(%s)\n",cmdbuf);
*/

uint64_t TRADE(char **retstrp,struct exchange_info *exchange,char *base,char *rel,int32_t dir,double price,double volume)
{
    char payload[1024],buf[1024],url[1024],digest[512],pairstr[512],pricestr[64],*extra,*typestr;
    cJSON *json; uint64_t txid = 0;
    if ( (extra= *retstrp) != 0 )
        *retstrp = 0;
    if ( (dir= flipstr_for_exchange(exchange,pairstr,"%s_%s",dir,&price,&volume,base,rel)) == 0 )
    {
        printf("cant find baserel (%s/%s)\n",base,rel);
        return(0);
    }
    if ( extra != 0 && strcmp(extra,"market") == 0 )
        typestr = (dir > 0) ? "buy_market" : "sell_market", sprintf(pricestr,"&price=%.2f",price); // docs say market orders put volume in price
    else typestr = (dir > 0) ? "buy" : "sell";
    sprintf(pricestr,"&price=%.2f",price);
    sprintf(buf,"amount=%.4f&api_key=%s%ssymbol=%s&type=%s&secret_key=%s",volume,exchange->apikey,pricestr,pairstr,typestr,exchange->apisecret);
    calc_md5(digest,buf,(int32_t)strlen(buf));
    touppercase(digest);
    sprintf(payload,"amount=%.4f&api_key=%s%s&symbol=%s&type=%s&sign=%s",volume,exchange->apikey,pricestr,pairstr,typestr,digest);
    sprintf(url,"%s/%s",EXCHANGE_AUTHURL,"trade.do");
    if ( (json= SIGNPOST(retstrp,exchange,url,payload)) != 0 )
    {
        txid = j64bits(json,"order_id");
        free_json(json);
    }
    return(txid);
}

cJSON *BALANCES(struct exchange_info *exchange)
{
    char payload[1024],buf[512],digest[512],url[512];
    sprintf(buf,"api_key=%s&secret_key=%s",exchange->apikey,exchange->apisecret);
    calc_md5(digest,buf,(int32_t)strlen(buf));
    touppercase(digest);
    sprintf(payload,"api_key=%s&sign=%s",exchange->apikey,digest);
    sprintf(url,"%s/%s",EXCHANGE_AUTHURL,"userinfo.do");
    return(SIGNPOST(0,exchange,url,payload));
}

char *PARSEBALANCE(struct exchange_info *exchange,double *balancep,char *coinstr)
{
    //okcoin.({"info":{"funds":{"asset":{"net":"0","total":"0"},"free":{"btc":"0","ltc":"0","usd":"0"},"freezed":{"btc":"0","ltc":"0","usd":"0"}}},"result":true})
    char field[128],*itemstr = 0; cJSON *obj,*item,*avail,*locked; double lockval = 0;
    *balancep = 0.;
    strcpy(field,coinstr);
    tolowercase(field);
    if ( exchange->balancejson != 0 && (obj= jobj(exchange->balancejson,"info")) != 0 && (item= jobj(obj,"funds")) != 0 )
    {
        if ( (avail= jobj(item,"free")) != 0 )
            *balancep = jdouble(avail,field);
        if ( (locked= jobj(item,"freezed")) != 0 )
            lockval = jdouble(locked,field);
        obj = cJSON_CreateObject();
        touppercase(field);
        jaddstr(obj,"base",field);
        jaddnum(obj,"balance",*balancep);
        jaddnum(obj,"locked",lockval);
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

struct exchange_funcs okcoin_funcs = EXCHANGE_FUNCS(okcoin,EXCHANGE_NAME);

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

