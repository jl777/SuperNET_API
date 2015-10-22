// deprecated

/*

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include "../libjl777/plugins/includes/cJSON.h"

void *poll_for_broadcasts(void *args);
extern int32_t SuperNET_retval,did_SuperNET_init;
char SuperNET_url[512];
int32_t decode_hex(unsigned char *bytes,int32_t n,char *hex);
char *bitcoind_RPC(char **retstrp,char *debugstr,char *url,char *userpass,char *command,char *args);

int8_t portable_spawn(char *os, char *cmd, char *arg) //TODO: extend for other OSes
{
    int8_t status = 0;
    if(strcmp("_WIN32",os)==0)
    {
#ifdef _WIN32
        if ( _spawnl(_P_NOWAIT, cmd, cmd, arg, NULL) != 0 )
            status = 1;
        else status = 0;
#endif
    } 
    else if(strcmp("__linux__",os)==0)
    {
#ifndef _WIN32
        pid_t pid = 0;
        pid = fork();
        if ( pid == 0 ) //child process
        {
            if ( execl(cmd, arg, NULL) )
		        status = 1;
	        else status = 0;
        }
#endif
    }
    else
    {
#ifndef _WIN32
        char *cmdArgs = (char*)malloc(strlen(cmd)+strlen(arg)+16);
        strcpy(cmdArgs, cmd);
        strcat(cmdArgs, " ");
        strcat(cmdArgs, arg);
	    pid_t pid = 0;
	    pid = fork();
	    if (pid==0)//child process
	    {
            if ( system(cmd) != 0 )
                status = 1;
            else
                status = 0;
	    }
        free(cmdArgs);
#else
		status = 1;
#endif
    }
    return status;
}

int32_t set_SuperNET_url(char *url)
{
    FILE *fp;
    int32_t retval=0,usessl=0,port=0;
    while ( (fp= fopen("horrible.hack","rb")) == 0 )
        sleep(1);
    if ( fread(&retval,1,sizeof(retval),fp) != sizeof(retval) )
        retval = -2;
    fclose(fp);
    if ( retval > 0 )
    {
        usessl = (retval & 1);
        port = (retval >> 1) & 0xffff;
        if ( port < (1 << 16) )
        {
            sprintf(SuperNET_url,"http%s://127.0.0.1:%d",usessl==0?"":"s",port);// + !usessl);
            retval >>= 17;
            printf("retval.%d port.%d usessl.%d\n",retval,port,usessl);
        }
        else retval = -3;
    }
    switch ( retval )
    {
        case 0: printf("SuperNET file found!\n"); break;
        case -1: printf("SuperNET file NOT found. It must be in the same directory as SuperNET\n"); break;
        case -2: printf("handshake file error\n"); break;
        case -3: printf("illegal supernet port.%d usessl.%d\n",port,usessl); break;
    }
    return(retval);
}

void *_launch_SuperNET(void *_myip)
{
    char *myip = _myip;
    FILE *fp;
    char cmd[128],*osstr,*rmstr,*cmdstr,*msgfname = "horrible.hack";
    int32_t retval;
    void *processptr = 0;
#ifdef _WIN32
    cmdstr = "SuperNET.exe";
    osstr = "_WIN32";
    rmstr = "del"
#else
    cmdstr = "./SuperNET";
    osstr = "__linux__";
    rmstr = "rm";
#endif
   if ( (fp= fopen(msgfname,"rb")) != 0 )
    {
        fclose(fp);
        sprintf(cmd,"%s %s",rmstr,msgfname);
        system(cmd);
    }
    if ( portable_spawn(osstr,cmdstr,myip) != 0 )
        printf("error launching (%s)\n",cmdstr);
    else
    {
        retval = set_SuperNET_url(SuperNET_url);
        if ( retval >= 0 )
        {
            processptr = portable_thread_create(poll_for_broadcasts,0);
            did_SuperNET_init = 1;
        }
        SuperNET_retval = retval;
        printf("myip.(%s) SuperNET_retval = %d\n",myip,SuperNET_retval);
    }
    return(processptr);
}

int32_t launch_SuperNET(char *myip)
{
    static char ipaddr[64];
    if ( myip != 0 )
    {
        strcpy(ipaddr,myip);
        myip = ipaddr;
    }
    void *processptr;
    printf("call launch_SuperNET with ip.(%s)\n",ipaddr);
    processptr = portable_thread_create(_launch_SuperNET,ipaddr);
    return(0);
}


int32_t Pending_RPC,SuperNET_retval,did_SuperNET_init;
extern char SuperNET_url[512];
char *SuperNET_JSON(char *JSONstr)
{
    char *retstr,*jsonstr,params[MAX_JSON_FIELD],result[MAX_JSON_FIELD],request[MAX_JSON_FIELD];
    cJSON *json;
    long len;
    if ( SuperNET_retval < 0 )
        return(0);
    // static char *gotnewpeer[] = { (char *)gotnewpeer_func, "gotnewpeer", "ip_port", 0 };
    if ( 1 && Pending_RPC != 0 )
    {
        sprintf(result,"{\"error\":\"Pending_RPC.%d please resubmit request\"}",Pending_RPC);
    return_result:
        len = strlen(result)+1;
        retstr = (char *)malloc(len);
        memcpy(retstr,result,len);
        return(retstr);
    }
    //while ( Pending_RPC != 0 )
    // {
    // fprintf(stderr,".");
    // sleep(1);
    // }
    memset(params,0,sizeof(params));
    if ( (json= cJSON_Parse(JSONstr)) != 0 )
    {
        copy_cJSON(request,cJSON_GetObjectItem(json,"requestType"));
        if ( strcmp(request,"stop") == 0 )
        {
            Pending_RPC = 0;
            did_SuperNET_init = 0;
            sprintf(result,"{\"result\":\"stopped\"}");
            //free_json(json);
            //goto return_result;
        }
        else if ( strcmp(request,"start") == 0 && did_SuperNET_init == 0 )
        {
            fprintf(stderr,"start again\n");
            launch_SuperNET(0);
            sprintf(result,"{\"result\":\"started\",\"retval\":%d}",SuperNET_retval);
            free_json(json);
            goto return_result;
        }
        else Pending_RPC++;
        free_json(json);
    }
    else
    {
        fprintf(stderr,"SuperNET RPC: malformed JSON.(%s)\n",JSONstr);
        return(0);
    }
    jsonstr = stringifyM(JSONstr);
    sprintf(params,"{\"requestType\":\"BTCDjson\",\"json\":%s}",jsonstr);
    retstr = bitcoind_RPC(0,(char *)"BTCD",SuperNET_url,(char *)"",(char *)"SuperNET",params);
    if ( retstr != 0 )
    {
        if ( (json= cJSON_Parse(retstr)) != 0 )
        {
            copy_cJSON(result,cJSON_GetObjectItem(json,"result"));
            if ( strcmp(result,"pending SuperNET API call") != 0 )
                Pending_RPC = 0;
            free_json(json);
        }
        //fprintf(stderr,"<<<<<<<<<<<<< SuperNET_JSON RET.(%s) for (%s) result.(%s)\n",retstr,jsonstr,result);
    }
    else
    {
        retstr = (char *)malloc(strlen("{\"result\":null}") + 1);
        strcpy(retstr,"{\"result\":null}");
    }
    free(jsonstr);
    return(retstr);
}

int32_t issue_gotnewpeer(char *ip_port)
{
    char *retstr,params[MAX_JSON_FIELD];
    if ( 1 || SuperNET_retval < 0 )
        return(-1);
    memset(params,0,sizeof(params));
    sprintf(params,"{\"requestType\":\"gotnewpeer\",\"ip_port\":\"%s\"}",ip_port);
    retstr = bitcoind_RPC(0,(char *)"BTCD",SuperNET_url,(char *)"",(char *)"SuperNET",params);
    if ( retstr != 0 )
    {
        fprintf(stderr,"<<<<<<<<<<<<< RET.(%s) for (%s)\n",retstr,ip_port);
        free(retstr);
        return(0);
    }
    return(-1);
}

int32_t got_newpeer(const char *ip_port)
{
    static int numearly;
    static char **earlybirds;
    int32_t i;
    // static char *gotnewpeer[] = { (char *)gotnewpeer_func, "gotnewpeer", "ip_port", 0 };
    while ( did_SuperNET_init == 0 )
    {
        printf("got_newpeer(%s) %d before initialized\n",ip_port,numearly);
        numearly++;
        earlybirds = (char **)realloc(earlybirds,(numearly+1) * sizeof(*earlybirds));
        earlybirds[numearly] = 0;
        earlybirds[numearly-1] = (char *)malloc(strlen(ip_port)+1);
        strcpy(earlybirds[numearly-1],ip_port);
        return(0);
    }
    if ( earlybirds != 0 )
    {
        for (i=0; i<numearly; i++)
            if ( earlybirds[i] != 0 )
            {
                issue_gotnewpeer(earlybirds[i]);
                free(earlybirds[i]);
            }
        free(earlybirds);
        earlybirds = 0;
        numearly = 0;
    }
    issue_gotnewpeer((char *)ip_port);
    return(0);
}

char *process_jl777_msg(char *from_ipaddr,char *msg, int32_t duration)
{
	//char *SuperNET_gotpacket(char *msg,int32_t duration,char *ip_port);
    static long retlen;
	static char *retbuf;
	int32_t len;
    char *retstr,params[MAX_JSON_FIELD*2],*str;
    //fprintf(stderr,"in process_jl777_msg(%s) dur.%d | retval.%d\n",msg,duration,SuperNET_retval);
    if ( SuperNET_retval <= 0 )
        return(0);
	if ( msg == 0 || msg[0] == 0 )
	{
		printf("no point to process null msg.%p\n",msg);
		return((char *)"{\"result\":null}");
	}
    memset(params,0,sizeof(params));
	//retstr = SuperNET_gotpacket(msg,duration,from_ipaddr);//(char *)from->addr.ToString().c_str());
    // static char *gotpacket[] = { (char *)gotpacket_func, "gotpacket", "", "msg", "dur", "ip", 0 };
    str = stringifyM(msg);
    sprintf(params,"{\"requestType\":\"gotpacket\",\"msg\":%s,\"dur\":%d,\"ip_port\":\"%s\"}",str,duration,from_ipaddr);
    free(str);
    retstr = bitcoind_RPC(0,(char *)"BTCD",SuperNET_url,(char *)"",(char *)"SuperNET",params);
    if ( retstr == 0 )
    {
        retstr = (char *)malloc(16);
        strcpy(retstr,"{\"result\":null}");
    }
	if ( retstr != 0 )
	{
		if ( (len= (int32_t)strlen(retstr)) >= retlen )
		{
			retlen = len + 1;
			retbuf = (char *)realloc(retbuf,len+1);
		}
		strcpy(retbuf,retstr);
		//fprintf(stderr,"\n<<<<<<<<<<<<< BTCD received message. msg: %s from %s retstr.(%s)\n",msg,from->addr.ToString().c_str(),retbuf);
		free(retstr);
	}
	return(retbuf);
}

void *poll_for_broadcasts(void *args)
{
    int32_t SuperNET_broadcast(char *msg,int32_t duration);
    int32_t SuperNET_narrowcast(char *destip,unsigned char *msg,int32_t len);
    cJSON *json;
    int32_t duration,len,sleeptime = 1;
    unsigned char data[4098];
    char params[4096],buf[8192],destip[1024],txidstr[64],*retstr;
    while ( did_SuperNET_init != 0 )
    {
        sleep(sleeptime++);
        //printf("ISSUE BTCDpoll\n");
        sprintf(params,"{\"requestType\":\"BTCDpoll\"}");
        retstr = bitcoind_RPC(0,(char *)"BTCD",SuperNET_url,(char *)"",(char *)"SuperNET",params);
        //fprintf(stderr,"<<<<<<<<<<< BTCD poll_for_broadcasts: issued bitcoind_RPC params.(%s) -> retstr.(%s)\n",params,retstr);
        if ( retstr != 0 )
        {
            if ( (json= cJSON_Parse(retstr)) != 0 )
            {
                duration = (int32_t)get_API_int(cJSON_GetObjectItem(json,"duration"),-1);
                copy_cJSON(destip,cJSON_GetObjectItem(json,"ip_port"));
                if ( destip[0] != 0 && duration < 0 )
                {
                    sleeptime = 1;
                    copy_cJSON(buf,cJSON_GetObjectItem(json,"hex"));
                    len = ((int32_t)strlen(buf) >> 1);
                    decode_hex(data,len,buf);
                    //fprintf(stderr,"<<<<<<<<<<< BTCD poll_for_broadcasts: narrowcast %d bytes to %s\n",len,destip);
                    SuperNET_narrowcast(destip,data,len); //Send a PubAddr message to a specific peer
                }
                else if ( duration >= 0 )
                {
                    copy_cJSON(buf,cJSON_GetObjectItem(json,"msg"));
                    if ( buf[0] != 0 )
                    {
                        sleeptime = 1;
                        unstringify(buf);
                        //fprintf(stderr,"<<<<<<<<<<< BTCD poll_for_broadcasts: SuperNET_broadcast(%s) dur.%d\n",buf,duration);
                        SuperNET_broadcast(buf,duration);
                    }
                }
                else
                {
                    copy_cJSON(buf,cJSON_GetObjectItem(json,"result"));
                    if ( buf[0] != 0 )
                    {
                        Pending_RPC = 0;
                        unstringify(buf);
                        copy_cJSON(txidstr,cJSON_GetObjectItem(json,"txid"));
                        sleeptime = 1;
                        if ( 0 && txidstr[0] != 0 )
                            fprintf(stderr,"<<<<<<<<<<< BTCD poll_for_broadcasts: (%s) for [%s]\n",buf,txidstr);
                    }
                    //fprintf(stderr,"<<<<<<<<<<< BTCD poll_for_broadcasts: unrecognised case duration.%d destip.(%s)\n",duration,destip);
                }
                free_json(json);
            } else fprintf(stderr,"<<<<<<<<<<< BTCD poll_for_broadcasts: PARSE_ERROR.(%s)\n",retstr);
            free(retstr);
        } //else fprintf(stderr,"<<<<<<<<<<< BTCD poll_for_broadcasts: bitcoind_RPC returns null\n");
    }
    return(0);
}
*/

