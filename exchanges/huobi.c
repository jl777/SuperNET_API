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

#define EXCHANGE_NAME "huobi"
#define UPDATE prices777_ ## huobi
#define SUPPORTS huobi ## _supports
#define SIGNPOST huobi ## _signpost
#define TRADE huobi ## _trade
#define ORDERSTATUS huobi ## _orderstatus
#define CANCELORDER huobi ## _cancelorder
#define OPENORDERS huobi ## _openorders
#define TRADEHISTORY huobi ## _tradehistory
#define BALANCES huobi ## _balances
#define PARSEBALANCE huobi ## _parsebalance
#define WITHDRAW huobi ## _withdraw

double UPDATE(struct prices777 *prices,int32_t maxdepth)
{
    if ( prices->url[0] == 0 )
        sprintf(prices->url,"http://api.huobi.com/staticmarket/depth_%s_json.js ",prices->lbase);
    return(prices777_standard("huobi",prices->url,prices,0,0,maxdepth,0));
}

int32_t SUPPORTS(char *base,char *rel)
{
    char *baserels[][2] = { {"btc","cny"}, {"ltc","cny"} };
    return(baserel_polarity(baserels,(int32_t)(sizeof(baserels)/sizeof(*baserels)),base,rel));
}

cJSON *SIGNPOST(char **retstrp,struct exchange_info *exchange,char *payload)
{
    static CURL *cHandle;
    char *data; cJSON *json;
    json = 0;
    if ( (data= curl_post(&cHandle,"https://api.huobi.com/apiv3",0,payload,"Content-Type:application/x-www-form-urlencoded",0,0,0)) != 0 )
        json = cJSON_Parse(data);
    if ( retstrp != 0 )
        *retstrp = data;
    else if ( data != 0 )
        free(data);
    return(json);
}

uint64_t TRADE(char **retstrp,struct exchange_info *exchange,char *base,char *rel,int32_t dir,double price,double volume)
{
    char payload[1024],pairstr[64],buf[1024],digest[33],pricestr[64],*extra,*method;
    cJSON *json; int32_t type; uint64_t nonce,txid = 0;
    if ( (extra= *retstrp) != 0 )
        *retstrp = 0;
    if ( (dir= flipstr_for_exchange(exchange,pairstr,"%s%s",dir,&price,&volume,base,rel)) == 0 )
    {
        printf("cant find baserel (%s/%s)\n",base,rel);
        return(0);
    }
    if ( extra != 0 && strcmp(extra,"market") == 0 )
        method = (dir > 0) ? "buy_market" : "sell_market";
    else method = (dir > 0) ? "buy" : "sell", sprintf(pricestr,"&price=%.2f",price);
    if ( strcmp(pairstr,"btccny") == 0 )
        type = 1;
    else if ( strcmp(pairstr,"ltccny") == 0 )
        type = 2;
    else
    {
        printf("cant find baserel (%s/%s)\n",base,rel);
        return(0);
    }
    nonce = exchange_nonce(exchange);
    sprintf(buf,"access_key=%s&amount=%.4f&coin_type=%d&created=%llu&method=%s%s&secret_key=%s",exchange->apikey,volume,type,(long long)nonce,method,pricestr,exchange->apisecret);
    calc_md5(digest,buf,(int32_t)strlen(buf));
    sprintf(payload,"access_key=%s&amount=%.4f&coin_type=%d&created=%llu&method=%s%s&sign=%s",exchange->apikey,volume,type,(long long)nonce,method,pricestr,digest);
    if ( (json= SIGNPOST(retstrp,exchange,payload)) != 0 )
    {
        txid = j64bits(json,"order_id");
        free_json(json);
    }
    return(txid);
}

cJSON *BALANCES(struct exchange_info *exchange)
{
    char buf[1024],digest[33],payload[1024],*method; uint64_t nonce;
    nonce = exchange_nonce(exchange);
    method = "get_account_info";
    sprintf(buf,"access_key=%s&created=%llu&method=%s&secret_key=%s",exchange->apikey,(long long)nonce,method,exchange->apisecret);
    calc_md5(digest,buf,(int32_t)strlen(buf));
    sprintf(payload,"access_key=%s&created=%llu&method=%s&sign=%s",exchange->apikey,(long long)nonce,method,digest);
    return(SIGNPOST(0,exchange,payload));
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
    if ( (json= SIGNPOST(&retstr,exchange,payload)) != 0 )
    {
        free_json(json);
    }
    return(retstr); // return standardized orderstatus
}

char *CANCELORDER(struct exchange_info *exchange,cJSON *argjson,uint64_t quoteid)
{
    char payload[1024],*retstr = 0; cJSON *json;
    // generate payload
    if ( (json= SIGNPOST(&retstr,exchange,payload)) != 0 )
    {
        free_json(json);
    }
    return(retstr); // return standardized cancelorder
}

char *OPENORDERS(struct exchange_info *exchange,cJSON *argjson)
{
    char payload[1024],*retstr = 0; cJSON *json;
    // generate payload
    if ( (json= SIGNPOST(&retstr,exchange,payload)) != 0 )
    {
        free_json(json);
    }
    return(retstr); // return standardized open orders
}

char *TRADEHISTORY(struct exchange_info *exchange,cJSON *argjson)
{
    char payload[1024],*retstr = 0; cJSON *json;
    // generate payload
    if ( (json= SIGNPOST(&retstr,exchange,payload)) != 0 )
    {
        free_json(json);
    }
    return(retstr); // return standardized tradehistory
}

char *WITHDRAW(struct exchange_info *exchange,cJSON *argjson)
{
    char payload[1024],*retstr = 0; cJSON *json;
    // generate payload
    if ( (json= SIGNPOST(&retstr,exchange,payload)) != 0 )
    {
        free_json(json);
    }
    return(retstr); // return standardized withdraw
}

struct exchange_funcs huobi_funcs = EXCHANGE_FUNCS(huobi,EXCHANGE_NAME);

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

