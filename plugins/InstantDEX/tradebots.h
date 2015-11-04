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


#ifndef xcode_tradebots_h
#define xcode_tradebots_h

#define TRADEBOT_DEFAULT_DURATION (60)
struct tradebot_info
{
    char name[64],*prevobookstr; struct prices777 *prices;
    uint32_t starttime,expiration,finishtime,startedtrades; int32_t invert,numtrades,havetrade,numlinks;
    struct prices777_order trades[256]; void *cHandles[256]; int32_t curlings[256];
    struct tradebot_info *linkedbots[8];
    struct InstantDEX_quote iQ;
};

// ./SNapi  "{\"allfields\":1,\"agent\":\"InstantDEX\",\"method\":\"orderbook\",\"exchange\":\"active\",\"base\":\"NXT\",\"rel\":\"BTC\"}"

// need balance verifier
// need tradeleg verifier
// add multiple simplebot layer
// pass through quotes
// user lockin addrs

int32_t tradebot_havealltrades(struct tradebot_info *bot)
{
    int32_t i;
    if ( bot->havetrade != 0 )
    {
        if ( bot->numlinks > 0 )
        {
            for (i=0; i<bot->numlinks; i++)
                if ( bot->linkedbots[i] == 0 || bot->linkedbots[i]->havetrade == 0 )
                    return(0);
        }
        return(1);
    }
    return(0);
}

struct tradebot_info *tradebot_compile(struct prices777 *prices,cJSON *argjson,struct InstantDEX_quote *iQ,int32_t invert)
{
    char *name; int32_t duration; struct tradebot_info *bot = calloc(1,sizeof(*bot));
    bot->prices = prices;
    bot->iQ = *iQ;
    bot->invert = invert;
    if ( (duration= juint(argjson,"duration")) == 0 )
        duration = TRADEBOT_DEFAULT_DURATION;
    bot->expiration = (uint32_t)time(NULL) + duration;
    if ( (name= jstr(argjson,"name")) != 0 )
        safecopy(bot->name,name,sizeof(bot->name));
    else sprintf(bot->name,"bot.%u",(uint32_t)time(NULL));
    //bot->arbmargin = jdouble(argjson,"arbmargin");
    return(bot);
}

int32_t tradebot_acceptable(struct tradebot_info *bot,cJSON *item)
{
    double price,volume; int32_t dir;
    if ( bot->iQ.s.isask != 0 )
        dir = -1;
    else dir = 1;
    price = jdouble(item,"price");
    volume = jdouble(item,"volume");
    if ( bot->invert != 0 )
    {
        volume = (price * volume);
        price = 1. / price;
        dir = -dir;
    }
    if ( (dir > 0 && price < bot->iQ.s.price) || (dir < 0 && price >= bot->iQ.s.price) )
        return(1);
    return(0);
}

int32_t tradebot_isvalidtrade(struct tradebot_info *bot,struct prices777_order *order,cJSON *retjson)
{
    cJSON *array,*item; char *resultval; double balance,required; int32_t i,n,valid = 0;
    if ( (array= jarray(&n,retjson,"traderesults")) != 0 )
    {
        for (i=0; i<n; i++)
        {
            item = jitem(array,i);
            if ( jstr(item,"error") == 0 && (resultval= jstr(item,"success")) != 0 )
            {
                balance = jdouble(item,"balance");
                required = jdouble(item,"required");
                printf("[%s %f R%f] ",resultval,balance,required);
                valid++;
            }
        }
        printf("valid.%d of %d\n",valid,n);
        if ( valid == n )
            return(0);
    }
    return(-1);
}

int32_t tradebot_tradedone(struct tradebot_info *bot,struct prices777_order *order)
{
    struct pending_trade *pend;
    if ( (pend= order->pend) != 0 && pend->finishtime != 0 )
        return(1);
    else return(0);
}

int32_t tradebot_haspending(struct tradebot_info *bot)
{
    int32_t i,finished;
    for (i=finished=0; i<bot->numtrades; i++)
    {
        if ( tradebot_tradedone(bot,&bot->trades[i]) > 0 )
            finished++;
    }
    return(finished < bot->numtrades);
}

void tradebot_free(struct tradebot_info *bot)
{
    int32_t i; struct pending_trade *pend;
    for (i=0; i<bot->numtrades; i++)
    {
        if ( (pend= bot->trades[i].pend) != 0 )
            free_pending(pend);
        if ( bot->trades[i].retitem != 0 )
            free_json(bot->trades[i].retitem);
        if ( bot->cHandles[i] != 0 )
        {
            while ( bot->curlings[i] != 0 )
            {
                fprintf(stderr,"%s: wait for curlrequest[%d] to finish\n",bot->name,i);
                sleep(3);
            }
            curlhandle_free(bot->cHandles[i]);
        }
    }
    if ( bot->prevobookstr != 0 )
        free(bot->prevobookstr);
    free(bot);
}

void Tradebot_loop(void *ptr)
{
    int32_t i,n,dotrade; char *obookstr,*retstr; cJSON *json,*array,*item,*retjson;
    struct tradebot_info *bot = ptr;
    printf("START Tradebot.(%s)\n",bot->name);
    while ( bot->finishtime == 0 && time(NULL) < bot->expiration )
    {
        if ( bot->startedtrades == 0 && (obookstr= orderbook_clonestr(bot->prices,bot->invert,1)) != 0 )
        {
            if ( bot->prevobookstr == 0 || strcmp(obookstr,bot->prevobookstr) != 0 )
            {
                if ( bot->prevobookstr != 0 )
                    free(bot->prevobookstr);
                bot->prevobookstr = obookstr;
                //printf("UPDATE.(%s)\n",obookstr);
                if ( (json= cJSON_Parse(obookstr)) != 0 )
                {
                    array = (bot->iQ.s.isask != 0) ? jarray(&n,json,"bids") : jarray(&n,json,"asks");
                    if ( array != 0 && n > 0 )
                    {
                        dotrade = 0;
                        for (i=0; i<n; i++)
                        {
                            item = jitem(array,i);
                            if ( tradebot_acceptable(bot,item) > 0 )
                            {
                                retstr = InstantDEX_tradesequence(bot->curlings,bot,bot->cHandles,&bot->numtrades,bot->trades,(int32_t)( sizeof(bot->trades)/sizeof(*bot->trades)),dotrade,SUPERNET.NXTADDR,SUPERNET.NXTACCTSECRET,item);
                                if ( retstr != 0 )
                                {
                                    if ( (retjson= cJSON_Parse(retstr)) != 0 )
                                    {
                                        if ( tradebot_isvalidtrade(bot,&bot->trades[i],retjson) > 0 )
                                            bot->havetrade = 1;
                                        free_json(retjson);
                                    }
                                    free(retstr);
                                }
                                break;
                            }
                        }
                        if ( tradebot_havealltrades(bot) != 0 )
                        {
                            dotrade = 1;
                            bot->startedtrades = (uint32_t)time(NULL);
                            retstr = InstantDEX_tradesequence(bot->curlings,bot,bot->cHandles,&bot->numtrades,bot->trades,(int32_t)(sizeof(bot->trades)/sizeof(*bot->trades)),dotrade,SUPERNET.NXTADDR,SUPERNET.NXTACCTSECRET,item);
                            printf("TRADE RESULT.(%s)\n",retstr);
                            break;
                        }
                    }
                    free_json(json);
                }
            }
        }
        else if ( bot->startedtrades != 0 )
        {
             if ( tradebot_haspending(bot) > 0 && bot->finishtime == 0 )
                 bot->finishtime = (uint32_t)time(NULL);
        }
        msleep(100);
    }
    while ( tradebot_haspending(bot) != 0 )
        sleep(60);
    printf("FINISHED Tradebot.(%s) at %u finishtime.%u expiration.%u\n",bot->name,(uint32_t)time(NULL),bot->finishtime,bot->expiration);
    tradebot_free(bot);
}

char *InstantDEX_tradebot(struct prices777 *prices,cJSON *argjson,struct InstantDEX_quote *iQ,int32_t invert)
{
    char *submethod,*exchange; struct tradebot_info *bot;
    printf("InstantDEX_tradebot.(%s) prices.%p\n",jprint(argjson,0),prices);
    if ( prices != 0 && (submethod= jstr(argjson,"submethod")) != 0 && (exchange= jstr(argjson,"exchange")) != 0 && strcmp(exchange,"active") == 0 && iQ != 0 )
    {
        if ( strcmp(submethod,"simplebot") == 0 )
        {
             if ( (bot= tradebot_compile(prices,argjson,iQ,invert)) == 0 )
                return(clonestr("{\"error\":\"tradebot compiler error\"}"));
            portable_thread_create((void *)Tradebot_loop,bot);
            return(clonestr("{\"result\":\"tradebot started\"}"));
        } else return(clonestr("{\"error\":\"unrecognized tradebot command\"}"));
        return(clonestr("{\"result\":\"tradebot command processed\"}"));
    } else return(clonestr("{\"error\":\"no prices777 or no tradebot submethod or not active exchange\"}"));
}

int offer_checkitem(struct pending_trade *pend,cJSON *item)
{
    uint64_t quoteid; struct InstantDEX_quote *iQ;
    if ( (quoteid= j64bits(item,"quoteid")) != 0 && (iQ= find_iQ(quoteid)) != 0 && iQ->s.closed != 0 )
        return(0);
    return(-1);
}

void trades_update()
{
    int32_t iter; struct pending_trade *pend;
    for (iter=0; iter<2; iter++)
    {
        while ( (pend= queue_dequeue(&Pending_offersQ.pingpong[iter],0)) != 0 )
        {
            if ( time(NULL) > pend->expiration )
            {
                printf("now.%ld vs timestamp.%u vs expiration %u | ",(long)time(NULL),pend->timestamp,pend->expiration);
                printf("offer_statemachine %llu/%llu %d %f %f\n",(long long)pend->orderid,(long long)pend->quoteid,pend->dir,pend->price,pend->volume);
                //InstantDEX_history(1,pend,retstr);
                if ( pend->bot == 0 )
                    free_pending(pend);
                else pend->finishtime = (uint32_t)time(NULL);
            }
            else
            {
                printf("InstantDEX_update requeue %llu/%llu %d %f %f\n",(long long)pend->orderid,(long long)pend->quoteid,pend->dir,pend->price,pend->volume);
                queue_enqueue("requeue",&Pending_offersQ.pingpong[iter ^ 1],&pend->DL);
            }
        }
    }
}

#endif
