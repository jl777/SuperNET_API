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

#define BUNDLED
#include <stdio.h>
//#include <curl/curl.h>
#define DEFINES_ONLY
#include "plugins/includes/portable777.h"
#include "plugins/utils/utils777.c"
#include "plugins/utils/files777.c"
#include "plugins/KV/kv777.c"
#include "plugins/common/system777.c"
#include "plugins/agents/plugins.h"
#undef DEFINES_ONLY

#define DEFAULT_SUPERNET_CONF "{\"peggy\":0,\"secret\":\"randvals\",\"pangeatest\":\"9\",\"notabot\":0}"
int32_t numxmit,Totalxmit;

#ifdef INSIDE_MGW
struct db777 *DB_revNXTtrades,*DB_NXTtrades;
#else
struct kv777 *DB_revNXTtrades,*DB_NXTtrades;
#endif

struct pending_cgi { struct queueitem DL; char apitag[24],*jsonstr; cJSON *json; int32_t sock,retind; };

char *SuperNET_install(char *plugin,char *jsonstr,cJSON *json)
{
    struct destbuf ipaddr,path; char *str,*retstr;
    int32_t i,ind,async;
    uint16_t port,websocket;
    if ( find_daemoninfo(&ind,plugin,0,0) != 0 )
        return(clonestr("{\"error\":\"plugin already installed\"}"));
    copy_cJSON(&path,cJSON_GetObjectItem(json,"path"));
    copy_cJSON(&ipaddr,cJSON_GetObjectItem(json,"ipaddr"));
    port = get_API_int(cJSON_GetObjectItem(json,"port"),0);
    async = get_API_int(cJSON_GetObjectItem(json,"daemonize"),0);
    websocket = get_API_int(cJSON_GetObjectItem(json,"websocket"),0);
    str = stringifyM(jsonstr);
    retstr = language_func(plugin,ipaddr.buf,port,websocket,async,path.buf,str,call_system);
    for (i=0; i<3; i++)
    {
        if ( find_daemoninfo(&ind,plugin,0,0) != 0 )
            break;
        poll_daemons();
        msleep(1000);
    }
    free(str);
    return(retstr);
}

int32_t got_newpeer(const char *ip_port) { if ( Debuglevel > 2 ) printf("got_newpeer.(%s)\n",ip_port); return(0); }

void *issue_cgicall(void *_ptr)
{
    struct destbuf apitag,plugin,method; char *str = 0,*broadcaststr,*destNXT; struct pending_cgi *ptr =_ptr;
    uint32_t nonce; int32_t localaccess,checklen,retlen,timeout;
    copy_cJSON(&apitag,cJSON_GetObjectItem(ptr->json,"apitag"));
    safecopy(ptr->apitag,apitag.buf,sizeof(ptr->apitag));
    copy_cJSON(&plugin,cJSON_GetObjectItem(ptr->json,"agent"));
    if ( plugin.buf[0] == 0 )
        copy_cJSON(&plugin,cJSON_GetObjectItem(ptr->json,"plugin"));
    copy_cJSON(&method,cJSON_GetObjectItem(ptr->json,"method"));
    localaccess = juint(ptr->json,"localaccess");
    if ( ptr->sock < 0 )
        localaccess = 1;
    timeout = get_API_int(cJSON_GetObjectItem(ptr->json,"timeout"),SUPERNET.PLUGINTIMEOUT);
    broadcaststr = cJSON_str(cJSON_GetObjectItem(ptr->json,"broadcast"));
    if ( ptr->sock >= 0 )
        fprintf(stderr,"sock.%d (%s) API RECV.(%s)\n",ptr->sock,broadcaststr!=0?broadcaststr:"",ptr->jsonstr);
    if ( ptr->sock >= 0 && (ptr->retind= nn_connect(ptr->sock,apitag.buf)) < 0 )
        fprintf(stderr,"error connecting to (%s)\n",apitag.buf);
    else
    {
        destNXT = cJSON_str(cJSON_GetObjectItem(ptr->json,"destNXT"));
        if ( strcmp(plugin.buf,"relay") == 0 || (broadcaststr != 0 && strcmp(broadcaststr,"remoteaccess") == 0) || cJSON_str(cJSON_GetObjectItem(ptr->json,"servicename")) != 0 )
        {
            if ( Debuglevel > 2 )
                printf("call busdata_sync.(%s)\n",ptr->jsonstr);
            //printf("destNXT.(%s)\n",destNXT!=0?destNXT:"");
            str = busdata_sync(&nonce,ptr->jsonstr,broadcaststr,destNXT);
            //printf("got.(%s)\n",str);
        }
        else
        {
            if ( Debuglevel > 2 )
                fprintf(stderr,"call plugin_method.(%s)\n",ptr->jsonstr);
            str = plugin_method(ptr->sock,0,localaccess,plugin.buf,method.buf,0,0,ptr->jsonstr,(int32_t)strlen(ptr->jsonstr)+1,timeout,0);
        }
        if ( str != 0 )
        {
            int32_t busdata_validate(char *forwarder,char *sender,uint32_t *timestamp,uint8_t *databuf,int32_t *datalenp,void *msg,cJSON *json);
            struct destbuf forwarder,sender,methodstr; uint32_t timestamp = 0; uint8_t databuf[8192]; int32_t valid=-1,datalen; cJSON *retjson,*argjson;
            forwarder.buf[0] = sender.buf[0] = 0;
            if ( (retjson= cJSON_Parse(str)) != 0 )
            {
                if ( is_cJSON_Array(retjson) != 0 && cJSON_GetArraySize(retjson) == 2 )
                {
                    argjson = cJSON_GetArrayItem(retjson,0);
                    copy_cJSON(&methodstr,cJSON_GetObjectItem(argjson,"method"));
                    if ( strcmp(methodstr.buf,"busdata") == 0 )
                    {
                        //fprintf(stderr,"call validate\n");
                        if ( (valid= busdata_validate(forwarder.buf,sender.buf,&timestamp,databuf,&datalen,str,retjson)) > 0 )
                        {
                            if ( datalen > 0 )
                            {
                                free(str);
                                str = malloc(datalen);
                                memcpy(str,databuf,datalen);
                            }
                        }
                    }
                }
                free_json(retjson);
            }
            //fprintf(stderr,"sock.%d mainstr.(%s) valid.%d sender.(%s) forwarder.(%s) time.%u\n",ptr->sock,str,valid,sender,forwarder,timestamp);
            if ( str != 0 && (retjson= cJSON_Parse(str)) != 0 )
            {
                if ( juint(retjson,"done") != 0 )
                {
                    cJSON_DeleteItemFromObject(retjson,"daemonid");
                    cJSON_DeleteItemFromObject(retjson,"myid");
                    cJSON_DeleteItemFromObject(retjson,"allowremote");
                    cJSON_DeleteItemFromObject(retjson,"tag");
                    cJSON_DeleteItemFromObject(retjson,"NXT");
                    cJSON_DeleteItemFromObject(retjson,"done");
                    free(str);
                    str = jprint(retjson,0);
                }
                free_json(retjson);
            }
            if ( ptr->sock >= 0 )
            {
                retlen = (int32_t)strlen(str) + 1;
                if ( (checklen= nn_send(ptr->sock,str,retlen,0)) != retlen )
                    fprintf(stderr,"checklen.%d != len.%d for nn_send to (%s)\n",checklen,retlen,apitag.buf);
                free(str), str = 0;
            }
        } else printf("null str returned\n");
    }
    if ( ptr->sock >= 0 )
    {
        //nn_freemsg(ptr->jsonstr);
        nn_shutdown(ptr->sock,ptr->retind);
        if ( str != 0 )
            free(str), str = 0;
    }
    free_json(ptr->json);
    free(ptr);
    return(str);
}

char *process_nn_message(int32_t sock,char *jsonstr)
{
    cJSON *json; struct pending_cgi *ptr; char *retstr = 0;
    if ( (json= cJSON_Parse(jsonstr)) != 0 )
    {
        ptr = calloc(1,sizeof(*ptr));
        ptr->sock = sock;
        ptr->json = json;
        ptr->jsonstr = jsonstr;
        if ( sock >= 0 )
            portable_thread_create((void *)issue_cgicall,ptr);
        else retstr = issue_cgicall(ptr);
    } //else if ( sock >= 0 ) free(jsonstr);
    return(retstr);
}

char *process_jl777_msg(char *buf,int32_t bufsize,char *previpaddr,char *jsonstr,int32_t duration)
{
    char *Pangea_bypass(uint64_t my64bits,uint8_t myprivkey[32],cJSON *json);
    char *process_user_json(char *plugin,char *method,char *cmdstr,int32_t broadcastflag,int32_t timeout);
    struct destbuf plugin,method,request; char *bstr,*retstr=0;
    uint64_t daemonid,instanceid,tag;
    int32_t override=0,broadcastflag = 0;
    cJSON *json;
    if ( previpaddr != 0 && previpaddr[0] != 0 )
        printf("REMOTE SuperNET JSON previpaddr.(%s) (%s)\n",previpaddr!=0?previpaddr:"",jsonstr);
    else printf("SuperNET JSON (%s)\n",jsonstr);
    if ( (json= cJSON_Parse(jsonstr)) != 0 )
    {
        copy_cJSON(&request,cJSON_GetObjectItem(json,"requestType"));
        copy_cJSON(&plugin,cJSON_GetObjectItem(json,"plugin"));
        if ( jstr(json,"plugin") != 0 && jstr(json,"agent") != 0 )
            override = 1;
        //fprintf(stderr,"SuperNET_JSON override.%d\n",override);
        if ( plugin.buf[0] == 0 )
            copy_cJSON(&plugin,cJSON_GetObjectItem(json,"agent"));
        if ( override == 0 && (previpaddr == 0 || previpaddr[0] == 0) && (strcmp(plugin.buf,"InstantDEX") == 0 || strcmp(plugin.buf,"pangea") == 0) )
        {
            if ( strcmp(plugin.buf,"pangea") == 0 )
            {
                if ( (retstr= Pangea_bypass(SUPERNET.my64bits,SUPERNET.myprivkey,json)) != 0 )
                {
                    free(json);
                    return(retstr);
                }
            }
            else if ( (retstr= InstantDEX(jsonstr,0,1)) != 0 )
            {
                free_json(json);
                return(retstr);
            }
            else printf("null return from InstantDEX\n");
        }
        if ( strcmp(request.buf,"install") == 0 && plugin.buf[0] != 0 )
        {
            fprintf(stderr,"call install path\n");
            retstr = SuperNET_install(plugin.buf,jsonstr,json);
            free_json(json);
            return(retstr);
        }
        tag = get_API_nxt64bits(cJSON_GetObjectItem(json,"tag"));
        daemonid = get_API_nxt64bits(cJSON_GetObjectItem(json,"daemonid"));
        instanceid = get_API_nxt64bits(cJSON_GetObjectItem(json,"instanceid"));
        copy_cJSON(&method,cJSON_GetObjectItem(json,"method"));
        if ( (bstr= cJSON_str(cJSON_GetObjectItem(json,"broadcast"))) != 0 )
            broadcastflag = 1;
        else broadcastflag = 0;
        if ( method.buf[0] == 0 )
        {
            strcpy(method.buf,request.buf);
            if ( plugin.buf[0] == 0 && set_first_plugin(plugin.buf,method.buf) < 0 )
                return(clonestr("{\"error\":\"no method or plugin specified, search for requestType failed\"}"));
        }
        if ( strlen(jsonstr) < bufsize )
        {
            strcpy(buf,jsonstr);
            //if ( previpaddr == 0 || previpaddr[0] == 0 )
            //    sprintf(buf + strlen(buf)-1,",\"rand\":\"%d\"}",rand());
            return(process_nn_message(-1,buf));
        } else printf("jsonstr too big %d vs %d\n",(int32_t)strlen(jsonstr),bufsize);
    }
    sprintf(buf,"{\"error\":\"couldnt parse JSON\",\"args\":[\"%s\"]}",jsonstr);
    return(clonestr(buf));
}

char *SuperNET_JSON(char *jsonstr) // BTCD's entry point
{
    char *buf,*retstr; cJSON *json;
    buf = calloc(1,65536);
    retstr = process_jl777_msg(buf,65536,0,jsonstr,60);
    free(buf);
    if ( (json= cJSON_Parse(retstr)) != 0 )
    {
        free(retstr);
        retstr = cJSON_Print(json);
        free_json(json);
    }
    return(retstr);
}

char *call_SuperNET_JSON(char *JSONstr) // sub-plugin's entry point
{
    struct destbuf request,name; char *buf,*retstr = 0;;
    uint64_t daemonid,instanceid;
    cJSON *json;
    fprintf(stderr,"call_SuperNET_JSON\n");
    if ( (json= cJSON_Parse(JSONstr)) != 0 )
    {
        copy_cJSON(&request,cJSON_GetObjectItem(json,"requestType"));
        copy_cJSON(&name,cJSON_GetObjectItem(json,"plugin"));
        if ( name.buf[0] == 0 )
            copy_cJSON(&name,cJSON_GetObjectItem(json,"agent"));
        if ( strcmp(request.buf,"register") == 0 )
        {
            daemonid = get_API_nxt64bits(cJSON_GetObjectItem(json,"daemonid"));
            instanceid = get_API_nxt64bits(cJSON_GetObjectItem(json,"instanceid"));
            retstr = register_daemon(name.buf,daemonid,instanceid,cJSON_GetObjectItem(json,"methods"),cJSON_GetObjectItem(json,"pubmethods"),cJSON_GetObjectItem(json,"authmethods"));
        }
        else
        {
            buf = calloc(1,65536);
            retstr = process_jl777_msg(buf,65536,0,JSONstr,60);
            free(buf);
        }
        free_json(json);
    }
    if ( retstr == 0 )
        retstr = clonestr("{\"result\":\"call_SuperNET_JSON no response\"}");
    return(retstr);
}

char *SuperNET_url()
{
    static char url[64];
    //sprintf(urls[0],"http://127.0.0.1:%d",SUPERNET_PORT+1*0);
    sprintf(url,"https://127.0.0.1:%d",SUPERNET.port);
    return(url);
}

void SuperNET_agentloop(void *ipaddr)
{
    int32_t n = 0;
    while ( 1 )
    {
        if ( poll_daemons() == 0 )
        {
            n++;
            msleep(1000 * (n + 1));
            if ( n > 100 )
                n = 100;
        } else n = 0;
    }
}

void SuperNET_apiloop(void *ipaddr)
{
    struct destbuf plugin; char *jsonstr,*retstr,*msg; int32_t sock,len,retlen,checklen; cJSON *json;
    if ( (sock= nn_socket(AF_SP,NN_PAIR)) >= 0 )
    {
        if ( nn_bind(sock,SUPERNET_APIENDPOINT) < 0 )
            fprintf(stderr,"error binding to relaypoint sock.%d type.%d (%s) %s\n",sock,NN_BUS,SUPERNET_APIENDPOINT,nn_errstr());
        else
        {
            if ( nn_settimeouts(sock,10,1) < 0 )
                fprintf(stderr,"error setting sendtimeout %s\n",nn_errstr());
            fprintf(stderr,"BIND.(%s) sock.%d\n",SUPERNET_APIENDPOINT,sock);
            while ( 1 )
            {
                if ( (len= nn_recv(sock,&msg,NN_MSG,0)) > 0 )
                {
                    jsonstr = clonestr(msg);
                    nn_freemsg(msg);
                    retstr = 0;
                    PostMessage("apirecv.(%s)\n",jsonstr);
                    if ( INSTANTDEX.readyflag != 0 && (json= cJSON_Parse(jsonstr)) != 0 )
                    {
                        copy_cJSON(&plugin,jobj(json,"agent"));
                        if ( plugin.buf[0] == 0 )
                            copy_cJSON(&plugin,jobj(json,"plugin"));
                        if ( strcmp(plugin.buf,"InstantDEX") == 0 )
                        {
                            retstr = clonestr("retstr");
                            if ( (retstr= InstantDEX(jsonstr,jstr(json,"remoteaddr"),juint(json,"localaccess"))) != 0 )
                            {
                                retlen = (int32_t)strlen(retstr) + 1;
                                if ( (checklen= nn_send(sock,retstr,retlen,0)) != retlen )
                                    fprintf(stderr,"checklen.%d != len.%d for nn_send of (%s)\n",checklen,retlen,retstr);
                            }
                        } else fprintf(stderr,">>>>>>>>> request is not InstantDEX (%s) %s\n",plugin.buf,jsonstr);
                        free_json(json);
                    }
                    if ( retstr == 0 && (retstr= process_nn_message(sock,jsonstr)) != 0 )
                        free(retstr);
                }
                msleep(SUPERNET.recvtimeout);
            }
        }
        nn_shutdown(sock,0);
    }
}

cJSON *url_json3(char *url)
{
    char *jsonstr; cJSON *json = 0;
    if ( (jsonstr= issue_curl(url)) != 0 )
    {
        printf("(%s) -> (%s)\n",url,jsonstr);
        json = cJSON_Parse(jsonstr);
        free(jsonstr);
    }
    return(json);
}

uint64_t set_account_NXTSECRET(void *myprivkey,void *mypubkey,char *NXTacct,char *NXTaddr,char *secret,int32_t max,cJSON *argjson,char *coinstr,char *serverport,char *userpass)
{
    uint64_t allocsize,nxt64bits; struct destbuf tmp,myipaddr; char coinaddr[MAX_JSON_FIELD],*str,*privkey;
    SUPERNET.ismainnet = get_API_int(cJSON_GetObjectItem(argjson,"MAINNET"),1);
    SUPERNET.usessl = get_API_int(cJSON_GetObjectItem(argjson,"USESSL"),0);
    SUPERNET.NXTconfirms = get_API_int(cJSON_GetObjectItem(argjson,"NXTconfirms"),10);
    copy_cJSON(&tmp,cJSON_GetObjectItem(argjson,"NXTAPIURL")), safecopy(SUPERNET.NXTAPIURL,tmp.buf,sizeof(SUPERNET.NXTAPIURL));
    if ( SUPERNET.NXTAPIURL[0] == 0 )
    {
        if ( SUPERNET.usessl == 0 )
            strcpy(SUPERNET.NXTAPIURL,"http://127.0.0.1:");
        else strcpy(SUPERNET.NXTAPIURL,"https://127.0.0.1:");
        if ( SUPERNET.ismainnet != 0 )
            strcat(SUPERNET.NXTAPIURL,"7876/nxt");
        else strcat(SUPERNET.NXTAPIURL,"6876/nxt");
    }
    copy_cJSON(&tmp,cJSON_GetObjectItem(argjson,"userdir")), safecopy(SUPERNET.userhome,tmp.buf,sizeof(SUPERNET.userhome));
    if ( SUPERNET.userhome[0] == 0 )
        strcpy(SUPERNET.userhome,"/root");
    strcpy(SUPERNET.NXTSERVER,SUPERNET.NXTAPIURL);
    strcat(SUPERNET.NXTSERVER,"?requestType");
    copy_cJSON(&tmp,cJSON_GetObjectItem(argjson,"myNXTacct")), safecopy(SUPERNET.myNXTacct,tmp.buf,sizeof(SUPERNET.myNXTacct));
    copy_cJSON(&myipaddr,cJSON_GetObjectItem(argjson,"myipaddr"));
    if ( myipaddr.buf[0] != 0 || SUPERNET.myipaddr[0] == 0 )
        safecopy(SUPERNET.myipaddr,myipaddr.buf,sizeof(SUPERNET.myipaddr));
    if ( SUPERNET.myipaddr[0] != 0 )
        SUPERNET.myipbits = (uint32_t)calc_ipbits(SUPERNET.myipaddr);
    NXTaddr[0] = 0;
    extract_cJSON_str(secret,max,argjson,"secret");
    if ( Debuglevel > 2 )
        printf("set_account_NXTSECRET.(%s)\n",secret);
    if ( secret[0] == 0 )
    {
        extract_cJSON_str(coinaddr,sizeof(coinaddr),argjson,"privateaddr");
        if ( strcmp(coinaddr,"privateaddr") == 0 )
        {
            if ( (str= loadfile(&allocsize,"privateaddr")) != 0 )
            {
                if ( allocsize < 128 )
                    strcpy(coinaddr,str);
                free(str);
            }
        }
        if ( coinaddr[0] == 0 )
            extract_cJSON_str(coinaddr,sizeof(coinaddr),argjson,"pubsrvaddr");
        if ( coinaddr[0] != 0 )
        {
            //printf("coinaddr.(%s) nonz.%d\n",coinaddr,coinaddr[0]);
            if ( coinstr == 0 || serverport == 0 || userpass == 0 || (privkey= dumpprivkey(coinstr,serverport,userpass,coinaddr)) == 0 )
                gen_randomacct(33,NXTaddr,secret,"randvals");
            else
            {
                strcpy(secret,privkey);
                free(privkey);
            }
        }
    }
    else if ( strcmp(secret,"randvals") == 0 )
        gen_randomacct(33,NXTaddr,secret,"randvals");
    nxt64bits = conv_NXTpassword(myprivkey,mypubkey,(uint8_t *)secret,(int32_t)strlen(secret));
    expand_nxt64bits(NXTaddr,nxt64bits);
    if ( 1 )
        conv_rsacctstr(NXTacct,nxt64bits);
    char pubkeystr[128];
    init_hexbytes_noT(pubkeystr,mypubkey,32);
    printf("(%s) (%s) (%s) pubkey.(%s) [%02x %02x] NXTAPIURL.[%s]\n",NXTacct,NXTaddr,Debuglevel > 2 ? secret : "<secret>",pubkeystr,*(uint8_t *)myprivkey,*(uint8_t *)mypubkey,SUPERNET.NXTAPIURL);
    if ( 0 )
    {
        int32_t haspubkey; bits256 pub = issue_getpubkey(&haspubkey,NXTaddr);
        init_hexbytes_noT(pubkeystr,pub.bytes,32);
        printf("blockchain haspubkey.%d %s\n",haspubkey,pubkeystr);
        //url_json3("https://blockchain.info/ticker");
        url_json3("http://api.coindesk.com/v1/bpi/historical/close.json");
    }
    return(nxt64bits);
}

void SuperNET_initconf(cJSON *json)
{
    struct destbuf tmp; uint8_t mysecret[32],mypublic[32]; FILE *fp;
    MAX_DEPTH = get_API_int(cJSON_GetObjectItem(json,"MAX_DEPTH"),MAX_DEPTH);
    if ( MAX_DEPTH > _MAX_DEPTH )
        MAX_DEPTH = _MAX_DEPTH;
    SUPERNET.disableNXT = get_API_int(cJSON_GetObjectItem(json,"disableNXT"),0);
    if ( SUPERNET.disableNXT == 0 )
        set_account_NXTSECRET(SUPERNET.myprivkey,SUPERNET.mypubkey,SUPERNET.NXTACCT,SUPERNET.NXTADDR,SUPERNET.NXTACCTSECRET,sizeof(SUPERNET.NXTACCTSECRET)-1,json,0,0,0);
    else strcpy(SUPERNET.NXTADDR,SUPERNET.myNXTacct);
    SUPERNET.my64bits = conv_acctstr(SUPERNET.NXTADDR);
    SUPERNET.iamrelay = get_API_int(cJSON_GetObjectItem(json,"iamrelay"),0);
    copy_cJSON(&tmp,cJSON_GetObjectItem(json,"hostname")), safecopy(SUPERNET.hostname,tmp.buf,sizeof(SUPERNET.hostname));
    SUPERNET.port = get_API_int(cJSON_GetObjectItem(json,"SUPERNET_PORT"),SUPERNET_PORT);
    SUPERNET.serviceport = get_API_int(cJSON_GetObjectItem(json,"serviceport"),SUPERNET_PORT - 2);
    copy_cJSON(&tmp,cJSON_GetObjectItem(json,"transport")), safecopy(SUPERNET.transport,tmp.buf,sizeof(SUPERNET.transport));
    if ( SUPERNET.transport[0] == 0 )
        strcpy(SUPERNET.transport,SUPERNET.UPNP == 0 ? "tcp" : "ws");
    sprintf(SUPERNET.lbendpoint,"%s://%s:%u",SUPERNET.transport,SUPERNET.myipaddr,SUPERNET.port + LB_OFFSET);
    sprintf(SUPERNET.relayendpoint,"%s://%s:%u",SUPERNET.transport,SUPERNET.myipaddr,SUPERNET.port + PUBRELAYS_OFFSET);
    sprintf(SUPERNET.globalendpoint,"%s://%s:%u",SUPERNET.transport,SUPERNET.myipaddr,SUPERNET.port + PUBGLOBALS_OFFSET);
    copy_cJSON(&tmp,cJSON_GetObjectItem(json,"SERVICESECRET")), safecopy(SUPERNET.SERVICESECRET,tmp.buf,sizeof(SUPERNET.SERVICESECRET));
    expand_nxt64bits(SUPERNET.SERVICENXT,conv_NXTpassword(mysecret,mypublic,(uint8_t *)SUPERNET.SERVICESECRET,(int32_t)strlen(SUPERNET.SERVICESECRET)));
    //printf("SERVICENXT.%s\n",SUPERNET.SERVICENXT);
    SUPERNET.automatch = get_API_int(cJSON_GetObjectItem(json,"automatch"),3);
#ifndef __linux__
    SUPERNET.UPNP = 1;
#endif
    SUPERNET.telepathicdelay = get_API_int(cJSON_GetObjectItem(json,"telepathicdelay"),1000);
    SUPERNET.pangeaport = get_API_int(cJSON_GetObjectItem(json,"pangeaport"),0);
    SUPERNET.peggy = get_API_int(cJSON_GetObjectItem(json,"peggy"),0);
    SUPERNET.idlegap = get_API_int(cJSON_GetObjectItem(json,"idlegap"),60);
    SUPERNET.recvtimeout = get_API_int(cJSON_GetObjectItem(json,"recvtimeout"),10);
    SUPERNET.exchangeidle = get_API_int(cJSON_GetObjectItem(json,"exchangeidle"),3);
    SUPERNET.gatewayid = get_API_int(cJSON_GetObjectItem(json,"gatewayid"),-1);
    SUPERNET.numgateways = get_API_int(cJSON_GetObjectItem(json,"numgateways"),3);
    SUPERNET.UPNP = get_API_int(cJSON_GetObjectItem(json,"UPNP"),SUPERNET.UPNP);
    SUPERNET.APISLEEP = get_API_int(cJSON_GetObjectItem(json,"APISLEEP"),DEFAULT_APISLEEP);
    SUPERNET.PLUGINTIMEOUT = get_API_int(cJSON_GetObjectItem(json,"PLUGINTIMEOUT"),10000);
    if ( SUPERNET.APISLEEP <= 1 )
        SUPERNET.APISLEEP = 1;
    copy_cJSON(&tmp,cJSON_GetObjectItem(json,"DATADIR")), safecopy(SUPERNET.DATADIR,tmp.buf,sizeof(SUPERNET.DATADIR));
    if ( SUPERNET.DATADIR[0] == 0 )
        strcpy(SUPERNET.DATADIR,"archive");
    Debuglevel = get_API_int(cJSON_GetObjectItem(json,"debug"),Debuglevel);
    if ( 0 && (fp= fopen("libs/websocketd","rb")) != 0 )
    {
        fclose(fp);
        strcpy(SUPERNET.WEBSOCKETD,"libs/websocketd");
    }
    else strcpy(SUPERNET.WEBSOCKETD,"websocketd");
    copy_cJSON(&tmp,cJSON_GetObjectItem(json,"backups")), safecopy(SUPERNET.BACKUPS,tmp.buf,sizeof(SUPERNET.BACKUPS));
    if ( SUPERNET.BACKUPS[0] == 0 )
        strcpy(SUPERNET.BACKUPS,"/tmp");
    copy_cJSON(&tmp,cJSON_GetObjectItem(json,"DBPATH")), safecopy(SUPERNET.DBPATH,tmp.buf,sizeof(SUPERNET.DBPATH));
    if ( SUPERNET.DBPATH[0] == 0 )
        strcpy(SUPERNET.DBPATH,"./DB");
    os_compatible_path(SUPERNET.DBPATH), ensure_directory(SUPERNET.DBPATH);
#ifdef INSIDE_MGW
    char buf[512];
    copy_cJSON(RAMCHAINS.pullnode,cJSON_GetObjectItem(json,"pullnode"));
    copy_cJSON(SOPHIA.PATH,cJSON_GetObjectItem(json,"SOPHIA"));
    copy_cJSON(SOPHIA.RAMDISK,cJSON_GetObjectItem(json,"RAMDISK"));
    if ( SOPHIA.PATH[0] == 0 )
        strcpy(SOPHIA.PATH,"./DB");
    os_compatible_path(SOPHIA.PATH), ensure_directory(SOPHIA.PATH);
    MGW.port = get_API_int(cJSON_GetObjectItem(json,"MGWport"),7643);
    copy_cJSON(MGW.PATH,cJSON_GetObjectItem(json,"MGWPATH"));
    if ( MGW.PATH[0] == 0 )
        strcpy(MGW.PATH,"/var/www/html/MGW");
    ensure_directory(MGW.PATH);
    sprintf(buf,"%s/msig",MGW.PATH), ensure_directory(buf);
    sprintf(buf,"%s/status",MGW.PATH), ensure_directory(buf);
    sprintf(buf,"%s/sent",MGW.PATH), ensure_directory(buf);
    sprintf(buf,"%s/deposit",MGW.PATH), ensure_directory(buf);
    printf("MGWport.%u >>>>>>>>>>>>>>>>>>> INIT ********************** (%s) (%s) (%s) SUPERNET.port %d UPNP.%d NXT.%s ip.(%s) iamrelay.%d pullnode.(%s)\n",MGW.port,SOPHIA.PATH,MGW.PATH,SUPERNET.NXTSERVER,SUPERNET.port,SUPERNET.UPNP,SUPERNET.NXTADDR,SUPERNET.myipaddr,SUPERNET.iamrelay,RAMCHAINS.pullnode);
#else
    if ( 0 )
    {
        //struct kv777 *kv777_init(char *path,char *name,struct kv777_flags *flags); // kv777_init IS NOT THREADSAFE!
        if ( DB_NXTtrades == 0 )
            DB_NXTtrades = kv777_init(SUPERNET.DBPATH,"NXT_trades",0);
        if ( DB_revNXTtrades == 0 )
            DB_revNXTtrades = kv777_init(SUPERNET.DBPATH,"revNXT_trades",0);
        SUPERNET.NXTaccts = kv777_init(SUPERNET.DBPATH,"NXTaccts",0);
        SUPERNET.NXTtxids = kv777_init(SUPERNET.DBPATH,"NXT_txids",0);
    }
#endif
    if ( SUPERNET.iamrelay != 0 )
    {
        SUPERNET.PM = kv777_init(SUPERNET.DBPATH,"PM",0);
        SUPERNET.alias = kv777_init(SUPERNET.DBPATH,"alias",0);
        SUPERNET.protocols = kv777_init(SUPERNET.DBPATH,"protocols",0);
        SUPERNET.rawPM = kv777_init(SUPERNET.DBPATH,"rawPM",0);
        SUPERNET.services = kv777_init(SUPERNET.DBPATH,"services",0);
        SUPERNET.invoices = kv777_init(SUPERNET.DBPATH,"invoices",0);
    }
}

char *SuperNET_launch_agent(char *name,char *jsonargs,int32_t *readyflagp)
{
    char *str; int32_t ind;
    str = language_func(name,"",0,0,1,name,jsonargs,call_system);
    while ( (readyflagp != 0 && *readyflagp == 0) || find_daemoninfo(&ind,name,0,0) == 0 )
    {
        if ( poll_daemons() > 0 && strcmp(name,"SuperNET") == 0 )
            break;
        msleep(100);
    }
    PostMessage("Launched %s agent\n",name);
    return(str);
}

void nanotests()
{
    int32_t i,seed,errs = 0;
    randombytes((void *)&seed,sizeof(seed));
    srand(seed);
    for (i=0; i<10; i++)
        printf("%u ",rand());
    printf("rands\n");
    int testcmsg(); int testhash(); int testpoll(); int testprio();
    int testseparation(); int testshutdown(); int testemfile(); int testiovec();
    int testsymbol(); int testtimeo(); int testtrie(); int testzerocopy(); int testterm();
    int testlist(); int testmsg(); int testdomain(); int testblock();
    int testdevice(); int testipc(); int testinproc(); int testinproc_shutdown();
    int testipc_shutdown(); int testipc_stress(); int testtcp(); int testtcp_shutdown();
    int testtcpmux(); int testws(); int testpair(); int testbus();
    int testpipeline(); int testpubsub(); int testreqrep(); int testsurvey();
    for (i=0; i<0; i++)
    {
        testhash();
        testtrie();
        testlist();
        testsymbol();
        testiovec();
        testblock();
        testemfile();
        testzerocopy();
        testinproc();
        testinproc_shutdown();
        testtimeo();
        testdomain();
        testprio();
        testshutdown();
        testbus();
        testpipeline();
        testpubsub();
        testpair();
        testsurvey();
        testws();
        testpoll();
        testmsg();
        testcmsg();
        testseparation();
    }
    for (i=0; i<1; i++)
        testipc();
    printf("finished nanotest loop\n"), getchar();
    testreqrep();
testtcp();
    testdevice();
 testtcpmux();
    testipc_shutdown(); testipc_stress();  testtcp_shutdown();
    testterm();

    printf("nanotests num errs.%d\n",errs);
    char *str,*jsonstr = clonestr("{\"plugin\":\"relay\",\"method\":\"busdata\"}"); uint32_t nonce;
    if ( (str= busdata_sync(&nonce,jsonstr,"allnodes",0)) != 0 )
    {
        fprintf(stderr,"busdata.(%s)\n",str);
        free(str);
    }

    getchar();
}

int SuperNET_start(char *fname,char *myip)
{
    void SuperNET_loop(void *_args);
    int32_t init_SUPERNET_pullsock(int32_t sendtimeout,int32_t recvtimeout);
    FILE *fp; char *strs[16],*jsonargs=0,ipaddr[256]; cJSON *json; int32_t i,n = 0; uint64_t allocsize;
    //nanotests();
    randombytes((void *)&i,sizeof(i));
    portable_OS_init();
    parse_ipaddr(ipaddr,myip);
    Debuglevel = 2;
    printf("%p myip.(%s) rand.%llx fname.(%s)\n",myip,myip,(long long)i,fname);
    if ( (jsonargs= loadfile(&allocsize,os_compatible_path(fname))) == 0 )
    {
        printf("ERROR >>>>>>>>>>> (%s) SuperNET.conf file doesnt exist\n",fname);
        jsonargs = clonestr(DEFAULT_SUPERNET_CONF);
    }
    if ( 1 )
    {
        jsonargs = clonestr(DEFAULT_SUPERNET_CONF);
        if ( (fp= fopen(os_compatible_path("SuperNET.conf"),"w")) != 0 )
        {
            fprintf(fp,"%s\n",jsonargs);
            fclose(fp);
        }
    }
    if ( (json= cJSON_Parse(jsonargs)) != 0 )
        SuperNET_initconf(json), free_json(json);
    else
    {
        printf("ERROR >>>>>>>>>>> SuperNET.conf file couldnt be parsed\n");
        exit(-666);
    }
    strcpy(SUPERNET.myipaddr,ipaddr);
    printf("SuperNET_start.(%s) myip.(%s) -> ipaddr.(%s) SUPERNET.port %d\n",jsonargs,myip!=0?myip:"",ipaddr,SUPERNET.port);
    init_SUPERNET_pullsock(10,SUPERNET.recvtimeout);
    busdata_init(10,1,0);
    strs[n++] = SuperNET_launch_agent("SuperNET",jsonargs,&SUPERNET.readyflag);
    strs[n++] = SuperNET_launch_agent("kv777",jsonargs,0);
    strs[n++] = SuperNET_launch_agent("SuperNET",jsonargs,&COINS.readyflag);
    strs[n++] = SuperNET_launch_agent("relay",jsonargs,&RELAYS.readyflag);
    if ( SUPERNET.iamrelay == 0 )
    {
        if ( 0 )
            strs[n++] = SuperNET_launch_agent("jumblr",jsonargs,0);
        if ( 1 )
            strs[n++] = SuperNET_launch_agent("pangea",jsonargs,0);
        if ( 0 )
            strs[n++] = SuperNET_launch_agent("dcnet",jsonargs,0);
        if ( SUPERNET.gatewayid < 0 )
        {
            if ( 0 )
                strs[n++] = SuperNET_launch_agent("prices",jsonargs,&PRICES.readyflag);
            if ( 0 )
                strs[n++] = SuperNET_launch_agent("teleport",jsonargs,&TELEPORT.readyflag);
            if ( 0 )
                strs[n++] = SuperNET_launch_agent("cashier",jsonargs,&CASHIER.readyflag);
            if ( 0 )
                strs[n++] = SuperNET_launch_agent("InstantDEX",jsonargs,&INSTANTDEX.readyflag);
            if ( SUPERNET.peggy != 0 )
            {
                void crypto_update();
                PostMessage("start peggy\n");
                portable_thread_create((void *)crypto_update,myip);
                void idle(); void idle2();
                portable_thread_create((void *)idle,myip);
                portable_thread_create((void *)idle2,myip);
            }
        }
    }
#ifdef INSIDE_MGW
    if ( SUPERNET.gatewayid >= 0 )
    {
        strs[n++] = SuperNET_launch_agent("MGW",jsonargs,&MGW.readyflag);
        strs[n++] = SuperNET_launch_agent("ramchain",jsonargs,&RAMCHAINS.readyflag);
        printf("MGW sock = %d\n",MGW.all.socks.both.bus);
    }
#else
    if ( 0 )
        strs[n++] = SuperNET_launch_agent("ramchain",jsonargs,&RAMCHAINS.readyflag);
#endif
    for (i=0; i<n; i++)
    {
        printf("%s ",strs[i]);
        free(strs[i]);
    }
    printf("num builtin plugin agents.%d\n",n);
    portable_thread_create((void *)SuperNET_loop,myip);
    portable_thread_create((void *)SuperNET_agentloop,myip);
    portable_thread_create((void *)SuperNET_apiloop,myip);
    PostMessage("free jsonargs.%p\n",jsonargs);
    if ( jsonargs != 0 )
        free(jsonargs);
    return(0);
}

#ifdef STANDALONE

int32_t SuperNET_broadcast(char *msg,int32_t duration) { printf(">>>>>>>>> BROADCAST.(%s)\n",msg); return(0); }
int32_t SuperNET_narrowcast(char *destip,unsigned char *msg,int32_t len) { printf(">>>>>>>>>>> NARROWCAST.(%s) -> (%s)\n",msg,destip);  return(0); }

#ifdef __linux__
int main(int argc,const char *argv[])
#else
int SuperNET_init(int argc,const char *argv[])
#endif
{
    uint64_t ipbits; uint32_t i; char _ipaddr[64],*ipaddr = "127.0.0.1:7777";
    i = 1;
    if ( argc > 1 )
    {
        ipbits = calc_ipbits((char *)argv[1]);
        expand_ipbits(_ipaddr,ipbits);
        if ( strcmp(_ipaddr,argv[1]) == 0 )
            ipaddr = (char *)argv[1], i++;
    }
    ipbits = calc_ipbits(ipaddr);
    expand_ipbits(_ipaddr,ipbits);
    printf(">>>>>>>>> call SuperNET_start.(%s)\n",ipaddr);
    SuperNET_start(os_compatible_path("SuperNET.conf"),ipaddr);
    printf("<<<<<<<<< back SuperNET_start\n");
#ifndef __PNACL
    if ( i < argc )
    {
        char *jsonstr; uint64_t allocsize;
        if ( (jsonstr= loadfile(&allocsize,os_compatible_path("SuperNET.conf"))) == 0 )
            jsonstr = clonestr(DEFAULT_SUPERNET_CONF);
        for (; i<argc; i++)
            if ( is_bundled_plugin((char *)argv[i]) != 0 )
                language_func((char *)argv[i],"",0,0,1,(char *)argv[i],jsonstr,call_system);
        free(jsonstr);
    }
    while ( 1 )
    {
        char line[32768];
        line[0] = 0;
        if ( getline777(line,sizeof(line)-1) > 0 )
        {
            //printf("getline777.(%s)\n",line);
            process_userinput(line);
        }
    }
#endif
    return(0);
}
#endif
