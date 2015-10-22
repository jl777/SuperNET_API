/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#include "nacl_io_demo.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <pthread.h>
#include <unistd.h>

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/ppb.h"
#include "ppapi/c/ppb_instance.h"
#include "ppapi/c/ppb_messaging.h"
#include "ppapi/c/ppb_var.h"
#include "ppapi/c/ppb_var_array.h"
#include "ppapi/c/ppb_var_dictionary.h"
#include "ppapi/c/ppp.h"
#include "ppapi/c/ppp_instance.h"
#include "ppapi/c/ppp_messaging.h"
#include "nacl_io/ioctl.h"
#include "nacl_io/nacl_io.h"

#include "handlers.h"
#include "queue.h"

#if defined(WIN32)
#define va_copy(d, s) ((d) = (s))
#endif

#ifndef __PNACL
int32_t PSGetInstanceId()
{
    return(4);
}
#endif

/**
 * The location of MAX is inconsitantly between LIBCs, so instead
 * we define it here for consistency.
 */
static int larger_int_of(int a, int b) {
    if (a > b)
        return a;
    return b;
}

typedef struct {
    const char* name;
    HandleFunc function;
} FuncNameMapping;

static PP_Instance g_instance = 0;
static PPB_GetInterface g_get_browser_interface = NULL;
static PPB_Messaging* g_ppb_messaging = NULL;
PPB_Var* g_ppb_var = NULL;
PPB_VarArray* g_ppb_var_array = NULL;
PPB_VarDictionary* g_ppb_var_dictionary = NULL;
int HandleSuperNET(struct PP_Var params,struct PP_Var *output,const char **out_error);

static FuncNameMapping g_function_map[] = {
    {"SuperNET", HandleSuperNET},
    {"fopen", HandleFopen},
    {"fwrite", HandleFwrite},
    {"fread", HandleFread},
    {"fseek", HandleFseek},
    {"fclose", HandleFclose},
    {"fflush", HandleFflush},
    {"stat", HandleStat},
    {"opendir", HandleOpendir},
    {"readdir", HandleReaddir},
    {"closedir", HandleClosedir},
    {"mkdir", HandleMkdir},
    {"rmdir", HandleRmdir},
    {"chdir", HandleChdir},
    {"getcwd", HandleGetcwd},
    {"getaddrinfo", HandleGetaddrinfo},
    {"gethostbyname", HandleGethostbyname},
    {"connect", HandleConnect},
    {"send", HandleSend},
    {"recv", HandleRecv},
    {"close", HandleClose},
    {NULL, NULL},
};

/** A handle to the thread the handles messages. */

/**
 * Create a new PP_Var from a C string.
 * @param[in] str The string to convert.
 * @return A new PP_Var with the contents of |str|.
 */
struct PP_Var CStrToVar(const char* str) { return g_ppb_var->VarFromUtf8(str, (int32_t)strlen(str)); }

/**
 * Printf to a newly allocated C string.
 * @param[in] format A printf format string.
 * @param[in] args The printf arguments.
 * @return The newly constructed string. Caller takes ownership. */
char *VprintfToNewString(const char* format, va_list args)
{
    va_list args_copy; int length; char *buffer; int result;
    va_copy(args_copy, args);
    length = vsnprintf(NULL, 0, format, args);
    buffer = (char*)malloc(length + 1); // +1 for NULL-terminator.
    result = vsnprintf(&buffer[0], length + 1, format, args_copy);
    if ( result != length )
    {
        assert(0);
        return NULL;
    }
    return buffer;
}

/**
 * Printf to a newly allocated C string.
 * @param[in] format A print format string.
 * @param[in] ... The printf arguments.
 * @return The newly constructed string. Caller takes ownership.
 */
char *PrintfToNewString(const char *format, ...)
{
    va_list args; char *result;
    va_start(args, format);
    result = VprintfToNewString(format, args);
    va_end(args);
    return result;
}

/**
 * Vprintf to a new PP_Var.
 * @param[in] format A print format string.
 * @param[in] va_list The printf arguments.
 * @return A new PP_Var.
 */
struct PP_Var VprintfToVar(const char* format, va_list args)
{
    struct PP_Var var; char *string = VprintfToNewString(format, args);
    var = g_ppb_var->VarFromUtf8(string, (int32_t)strlen(string));
    free(string);
    return var;
}

/**
 * Convert a PP_Var to a C string.
 * @param[in] var The PP_Var to convert.
 * @return A newly allocated, NULL-terminated string.
 */
static const char *VarToCStr(struct PP_Var var)
{
    uint32_t length; char *new_str; const char *str = g_ppb_var->VarToUtf8(var, &length);
    if ( str == NULL )
        return NULL;
    new_str = (char*)malloc(length + 1);
    memcpy(new_str, str, length); // str is NOT NULL-terminated. Copy using memcpy.
    new_str[length] = 0;
    return new_str;
}

/**
 * Get a value from a Dictionary, given a string key.
 * @param[in] dict The dictionary to look in.
 * @param[in] key The key to look up.
 * @return PP_Var The value at |key| in the |dict|. If the key doesn't exist,
 *     return a PP_Var with the undefined value.
 */
struct PP_Var GetDictVar(struct PP_Var dict, const char* key)
{
    struct PP_Var key_var = CStrToVar(key);
    struct PP_Var value = g_ppb_var_dictionary->Get(dict, key_var);
    g_ppb_var->Release(key_var);
    return value;
}

/**
 * Post a message to JavaScript.
 * @param[in] format A printf format string.
 * @param[in] ... The printf arguments.
 */
void PostMessage(const char* format, ...)
{
    va_list args;
    va_start(args, format);
#ifdef __PNACL
    struct PP_Var var;
    var = VprintfToVar(format, args);
    g_ppb_messaging->PostMessage(g_instance, var);
    g_ppb_var->Release(var);
#else
    printf(format,args);
#endif
    va_end(args);
}

/**
 * Given a message from JavaScript, parse it for functions and parameters.
 *
 * The format of the message is:
 * {
 *  "cmd": <function name>,
 *  "args": [<arg0>, <arg1>, ...]
 * }
 *
 * @param[in] message The message to parse.
 * @param[out] out_function The function name.
 * @param[out] out_params A PP_Var array.
 * @return 0 if successful, otherwise 1.
 */
static int ParseMessage(struct PP_Var message,const char **out_function,struct PP_Var *out_params)
{
    if ( message.type != PP_VARTYPE_DICTIONARY )
        return(1);
    struct PP_Var cmd_value = GetDictVar(message, "cmd");
    *out_function = VarToCStr(cmd_value);
    g_ppb_var->Release(cmd_value);
    *out_params = GetDictVar(message, "args");
    PostMessage("Parse.(%s) cmd.(%s)\n",*out_function,VarToCStr(*out_params));
    if ( cmd_value.type != PP_VARTYPE_STRING )
        return(1);
    if ( out_params->type != PP_VARTYPE_ARRAY )
        return(1);
    return(0);
}

/**
 * Given a function name, look up its handler function.
 * @param[in] function_name The function name to look up.
 * @return The handler function mapped to |function_name|.
 */
static HandleFunc GetFunctionByName(const char* function_name)
{
    FuncNameMapping* map_iter = g_function_map;
    for (; map_iter->name; ++map_iter)
    {
        if (strcmp(map_iter->name, function_name) == 0)
            return map_iter->function;
    }
    return NULL;
}

/**
 * Handle as message from JavaScript on the worker thread.
 *
 * @param[in] message The message to parse and handle.
 */
static void HandleMessage(struct PP_Var message)
{
    const char *function_name,*error; struct PP_Var params,result_var;
    if ( ParseMessage(message, &function_name, &params) != 0 )
    {
        PostMessage("Error: Unable to parse message");
        return;
    }
    HandleFunc function = GetFunctionByName(function_name);
    if ( function == 0 )
    {
        PostMessage("Error: Unknown function \"%s\"", function_name); // Function name wasn't found.
        return;
    }
    // Function name was found, call it.
    int result = (*function)(params, &result_var, &error);
    if ( result != 0 )
    {
        if ( error != NULL )
        {
            PostMessage("Error: \"%s\" failed: %s.", function_name, error);
            free((void*)error);
        }
        else PostMessage("Error: \"%s\" failed.", function_name);
        return;
    }
    // Function returned an output dictionary. Send it to JavaScript.
    g_ppb_messaging->PostMessage(g_instance, result_var);
    g_ppb_var->Release(result_var);
}


/**
 * Helper function used by EchoThread which reads from a file descriptor
 * and writes all the data that it reads back to the same descriptor.
 */
static void EchoInput(int32_t fd)
{
    char buffer[512]; int32_t wrote,rtn;
    while ( 1 )
    {
        if ( (rtn= (int32_t)read(fd,buffer,512)) > 0 )
        {
            if ( (wrote= (int32_t)write(fd,buffer,rtn))  < rtn )
                PostMessage("only wrote %d of %d bytes\n", wrote, rtn);
        }
        else
        {
            if ( rtn < 0 && errno != EAGAIN )
                PostMessage("read failed: %d (%s)\n", errno, strerror(errno));
            break;
        }
    }
}

/**
 * Worker thread that listens for input on JS pipe nodes and echos all input
 * back to the same pipe.
 */
void *EchoThread(void* user_data) {
    int fd1 = open("/dev/jspipe1", O_RDWR | O_NONBLOCK);
    int fd2 = open("/dev/jspipe2", O_RDWR | O_NONBLOCK);
    int fd3 = open("/dev/jspipe3", O_RDWR | O_NONBLOCK);
    int nfds = larger_int_of(fd1, fd2);
    nfds = larger_int_of(nfds, fd3);
    while (1) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(fd1, &readfds);
        FD_SET(fd2, &readfds);
        FD_SET(fd3, &readfds);
        int rtn = select(nfds + 1, &readfds, NULL, NULL, NULL);
        if (rtn < 0 && errno != EAGAIN) {
            PostMessage("select failed: %s\n", strerror(errno));
            break;
        }
        if (rtn > 0) {
            if (FD_ISSET(fd1, &readfds))
                EchoInput(fd1);
            if (FD_ISSET(fd2, &readfds))
                EchoInput(fd2);
            if (FD_ISSET(fd3, &readfds))
                EchoInput(fd3);
        }
        
    }
    close(fd1);
    close(fd2);
    close(fd3);
    return 0;
}

/**
 * A worker thread that handles messages from JavaScript.
 * @param[in] user_data Unused.
 * @return unused.
 */
void *HandleMessageThread(void *user_data)
{
    while ( 1 )
    {
        struct PP_Var message = DequeueMessage();
        HandleMessage(message);
        g_ppb_var->Release(message);
    }
}

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#undef mount
#undef umount

#include "ppapi/c/pp_resource.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_fullscreen.h"
#include "ppapi/c/ppb_graphics_2d.h"
#include "ppapi/c/ppb_image_data.h"
#include "ppapi/c/ppb_input_event.h"
#include "ppapi/c/ppb_instance.h"
#include "ppapi/c/ppb_view.h"

#include "ppapi_simple/ps_event.h"
#include "ppapi_simple/ps_main.h"

PPB_Core* g_pCore;
PPB_Fullscreen* g_pFullscreen;
PPB_Graphics2D* g_pGraphics2D;
PPB_ImageData* g_pImageData;
PPB_Instance* g_pInstance;
PPB_View* g_pView;
PPB_InputEvent* g_pInputEvent;
PPB_KeyboardInputEvent* g_pKeyboardInput;
PPB_MouseInputEvent* g_pMouseInput;
PPB_TouchInputEvent* g_pTouchInput;

struct {
    PP_Resource ctx;
    struct PP_Size size;
    int bound;
    uint8_t* cell_in;
    uint8_t* cell_out;
} g_Context;


const unsigned int kInitialRandSeed = 0xC0DE533D;

/* BGRA helper macro, for constructing a pixel for a BGRA buffer. */
#define MakeBGRA(b, g, r, a)  \
(((a) << 24) | ((r) << 16) | ((g) << 8) | (b))


/*
 * Convert a count value into a live (green) or dead color value.
 */
const uint32_t kNeighborColors[] = {
    MakeBGRA(0x00, 0x00, 0x00, 0xFF),
    MakeBGRA(0x00, 0x00, 0x00, 0xFF),
    MakeBGRA(0x00, 0x00, 0x00, 0xFF),
    MakeBGRA(0x00, 0x00, 0x00, 0xFF),
    MakeBGRA(0x00, 0x00, 0x00, 0xFF),
    MakeBGRA(0x00, 0xFF, 0x00, 0xFF),
    MakeBGRA(0x00, 0xFF, 0x00, 0xFF),
    MakeBGRA(0x00, 0xFF, 0x00, 0xFF),
    MakeBGRA(0x00, 0x00, 0x00, 0xFF),
    MakeBGRA(0x00, 0x00, 0x00, 0xFF),
    MakeBGRA(0x00, 0x00, 0x00, 0xFF),
    MakeBGRA(0x00, 0x00, 0x00, 0xFF),
    MakeBGRA(0x00, 0x00, 0x00, 0xFF),
    MakeBGRA(0x00, 0x00, 0x00, 0xFF),
    MakeBGRA(0x00, 0x00, 0x00, 0xFF),
    MakeBGRA(0x00, 0x00, 0x00, 0xFF),
    MakeBGRA(0x00, 0x00, 0x00, 0xFF),
    MakeBGRA(0x00, 0x00, 0x00, 0xFF),
};

/*
 * These represent the new health value of a cell based on its neighboring
 * values.  The health is binary: either alive or dead.
 */
const uint8_t kIsAlive[] = {
    0, 0, 0, 0, 0, 1, 1, 1, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0
};

void UpdateContext(uint32_t width, uint32_t height) {
    if (width != g_Context.size.width || height != g_Context.size.height) {
        size_t size = width * height;
        size_t index;
        
        free(g_Context.cell_in);
        free(g_Context.cell_out);
        
        /* Create a new context */
        g_Context.cell_in = (uint8_t*) malloc(size);
        g_Context.cell_out = (uint8_t*) malloc(size);
        
        memset(g_Context.cell_out, 0, size);
        for (index = 0; index < size; index++) {
            g_Context.cell_in[index] = rand() & 1;
        }
    }
    
    /* Recreate the graphics context on a view change */
    g_pCore->ReleaseResource(g_Context.ctx);
    g_Context.size.width = width;
    g_Context.size.height = height;
    g_Context.ctx =
    g_pGraphics2D->Create(PSGetInstanceId(), &g_Context.size, PP_TRUE);
    g_Context.bound =
    g_pInstance->BindGraphics(PSGetInstanceId(), g_Context.ctx);
}

void DrawCell(int32_t x, int32_t y) {
    int32_t width = g_Context.size.width;
    int32_t height = g_Context.size.height;
    
    if (!g_Context.cell_in) return;
    
    if (x > 0 && x < width - 1 && y > 0 && y < height - 1) {
        g_Context.cell_in[x - 1 + y * width] = 1;
        g_Context.cell_in[x + 1 + y * width] = 1;
        g_Context.cell_in[x + (y - 1) * width] = 1;
        g_Context.cell_in[x + (y + 1) * width] = 1;
    }
}

void ProcessTouchEvent(PSEvent* event) {
    uint32_t count = g_pTouchInput->GetTouchCount(event->as_resource,
                                                  PP_TOUCHLIST_TYPE_TOUCHES);
    uint32_t i, j;
    for (i = 0; i < count; i++) {
        struct PP_TouchPoint touch = g_pTouchInput->GetTouchByIndex(
                                                                    event->as_resource, PP_TOUCHLIST_TYPE_TOUCHES, i);
        int radius = (int)touch.radius.x;
        int x = (int)touch.position.x;
        int y = (int)touch.position.y;
        /* num = 1/100th the area of touch point */
        int num = (int)(M_PI * radius * radius / 100.0f);
        for (j = 0; j < num; j++) {
            int dx = rand() % (radius * 2) - radius;
            int dy = rand() % (radius * 2) - radius;
            /* only plot random cells within the touch area */
            if (dx * dx + dy * dy <= radius * radius)
                DrawCell(x + dx, y + dy);
        }
    }
}

void ProcessEvent(PSEvent* event) {
    switch(event->type) {
            /* If the view updates, build a new Graphics 2D Context */
        case PSE_INSTANCE_DIDCHANGEVIEW: {
            struct PP_Rect rect;
            
            g_pView->GetRect(event->as_resource, &rect);
            UpdateContext(rect.size.width, rect.size.height);
            break;
        }
            
        case PSE_INSTANCE_HANDLEINPUT: {
            PP_InputEvent_Type type = g_pInputEvent->GetType(event->as_resource);
            PP_InputEvent_Modifier modifiers =
            g_pInputEvent->GetModifiers(event->as_resource);
            
            switch(type) {
                case PP_INPUTEVENT_TYPE_MOUSEDOWN:
                case PP_INPUTEVENT_TYPE_MOUSEMOVE: {
                    struct PP_Point location =
                    g_pMouseInput->GetPosition(event->as_resource);
                    /* If the button is down, draw */
                    if (modifiers & PP_INPUTEVENT_MODIFIER_LEFTBUTTONDOWN) {
                        DrawCell(location.x, location.y);
                    }
                    break;
                }
                    
                case PP_INPUTEVENT_TYPE_TOUCHSTART:
                case PP_INPUTEVENT_TYPE_TOUCHMOVE:
                    ProcessTouchEvent(event);
                    break;
                    
                case PP_INPUTEVENT_TYPE_KEYDOWN: {
                    PP_Bool fullscreen = g_pFullscreen->IsFullscreen(PSGetInstanceId());
                    g_pFullscreen->SetFullscreen(PSGetInstanceId(),
                                                 fullscreen ? PP_FALSE : PP_TRUE);
                    break;
                }
                    
                default:
                    break;
            }
            /* case PSE_INSTANCE_HANDLEINPUT */
            break;
        }
            
        default:
            break;
    }
}


void Stir(uint32_t width, uint32_t height) {
    int i;
    if (g_Context.cell_in == NULL || g_Context.cell_out == NULL)
        return;
    
    for (i = 0; i < width; ++i) {
        g_Context.cell_in[i] = rand() & 1;
        g_Context.cell_in[i + (height - 1) * width] = rand() & 1;
    }
    for (i = 0; i < height; ++i) {
        g_Context.cell_in[i * width] = rand() & 1;
        g_Context.cell_in[i * width + (width - 1)] = rand() & 1;
    }
}

void Render()
{
    struct PP_Size* psize = &g_Context.size;
    PP_ImageDataFormat format = PP_IMAGEDATAFORMAT_BGRA_PREMUL;
    
    /*
     * Create a buffer to draw into.  Since we are waiting until the next flush
     * chrome has an opportunity to cache this buffer see ppb_graphics_2d.h.
     */
    PP_Resource image = g_pImageData->Create(PSGetInstanceId(), format, psize, PP_FALSE);
    uint8_t* pixels = g_pImageData->Map(image);
    struct PP_ImageDataDesc desc;
    uint8_t* cell_temp;
    uint32_t x, y;
    
    /* If we somehow have not allocated these pointers yet, skip this frame. */
    if (!g_Context.cell_in || !g_Context.cell_out) return;
    
    /* Get the stride. */
    g_pImageData->Describe(image, &desc);
    
    /* Stir up the edges to prevent the simulation from reaching steady state. */
    Stir(desc.size.width, desc.size.height);
    
    /* Do neighbor summation; apply rules, output pixel color. */
    for (y = 1; y < desc.size.height - 1; ++y) {
        uint8_t *src0 = (g_Context.cell_in + (y - 1) * desc.size.width) + 1;
        uint8_t *src1 = src0 + desc.size.width;
        uint8_t *src2 = src1 + desc.size.width;
        int count;
        uint32_t color;
        uint8_t *dst = (g_Context.cell_out + y * desc.size.width) + 1;
        uint32_t *pixel_line =  (uint32_t*) (pixels + y * desc.stride);
        
        for (x = 1; x < (desc.size.width - 1); ++x) {
            /* Jitter and sum neighbors. */
            count = src0[-1] + src0[0] + src0[1] +
            src1[-1] +         + src1[1] +
            src2[-1] + src2[0] + src2[1];
            /* Include center cell. */
            count = count + count + src1[0];
            /* Use table lookup indexed by count to determine pixel & alive state. */
            color = kNeighborColors[count];
            *pixel_line++ = color;
            *dst++ = kIsAlive[count];
            ++src0;
            ++src1;
            ++src2;
        }
    }
    
    cell_temp = g_Context.cell_in;
    g_Context.cell_in = g_Context.cell_out;
    g_Context.cell_out = cell_temp;
    
    /* Unmap the range, we no longer need it. */
    g_pImageData->Unmap(image);
    
    /* Replace the contexts, and block until it's on the screen. */
    g_pGraphics2D->ReplaceContents(g_Context.ctx, image);
    g_pGraphics2D->Flush(g_Context.ctx, PP_BlockUntilComplete());
    
    /* Release the image data, we no longer need it. */
    g_pCore->ReleaseResource(image);
}

/*
 * Starting point for the module.  We do not use main since it would
 * collide with main in libppapi_cpp.
 */
int example_main()
{
    PostMessage("Started example main.\n");
    
    //g_pInputEvent = (PPB_InputEvent*) PSGetInterface(PPB_INPUT_EVENT_INTERFACE);
    //g_pKeyboardInput = (PPB_KeyboardInputEvent*)PSGetInterface(PPB_KEYBOARD_INPUT_EVENT_INTERFACE);
    //g_pMouseInput = (PPB_MouseInputEvent*) PSGetInterface(PPB_MOUSE_INPUT_EVENT_INTERFACE);
    //g_pTouchInput = (PPB_TouchInputEvent*) PSGetInterface(PPB_TOUCH_INPUT_EVENT_INTERFACE);
    //PSEventSetFilter(PSE_ALL);
    while (1) {
        /* Process all waiting events without blocking
        PSEvent* event;
        while ((event = PSEventTryAcquire()) != NULL) {
            ProcessEvent(event);
            PSEventRelease(event);
        }*/
        
        // Render a frame, blocking until complete.
        if ( g_Context.bound )
            Render();
    }
    return 0;
}

PSMainFunc_t PSUserMainGet()
{
    return(example_main);
}

//void *bindloop(void *args);
void init_InstantDEX(void *json);
int SuperNET_init(int argc,const char *argv[]);
//int32_t pp_connect(char *hostname,uint16_t port);
void msleep(uint32_t milliseconds);

void *SuperNET(void *threads)
{
    static const char *argv[2] = { "127.0.0.1" }; // int32_t sock2;
    SuperNET_init(1,argv);
    //init_InstantDEX(0);
    //sock2 = pp_connect("127.0.0.1",8000);
    //printf("sock2.%d\n",sock2);
    //example_main();
    while ( 1 )
    {
        msleep(1000);
    }
    return(0);
}

static PP_Bool Instance_DidCreate(PP_Instance instance,uint32_t argc,const char* argn[],const char* argv[])
{
#ifndef __APPLE__
    static pthread_t g_handle_message_thread;
    //static pthread_t g_echo_thread;
    static pthread_t SuperNET_thread;//,bind_thread;
    g_instance = instance;
    // By default, nacl_io mounts / to pass through to the original NaCl
    // filesystem (which doesn't do much). Let's remount it to a memfs
    // filesystem.
    InitializeMessageQueue();
    pthread_create(&g_handle_message_thread, NULL, &HandleMessageThread, NULL);
    pthread_create(&SuperNET_thread,NULL,&SuperNET,NULL);
    //pthread_create(&bind_thread,NULL,&bindloop,NULL);
    //pthread_create(&g_echo_thread, NULL, &EchoThread, NULL);
    nacl_io_init_ppapi(instance, g_get_browser_interface);
    umount("/");
    mount("", "/", "memfs", 0, "");
    
    mount("",                                       /* source */
          "/persistent",                            /* target */
          "html5fs",                                /* filesystemtype */
          0,                                        /* mountflags */
          "type=PERSISTENT,expected_size=1073741824"); /* data */
    
    mount("",       /* source. Use relative URL */
          "/http",  /* target */
          "httpfs", /* filesystemtype */
          0,        /* mountflags */
          "");      /* data */
    //g_pCore = (PPB_Core*)PSGetInterface(PPB_CORE_INTERFACE);
    //g_pFullscreen = (PPB_Fullscreen*)PSGetInterface(PPB_FULLSCREEN_INTERFACE);
    //g_pGraphics2D = (PPB_Graphics2D*)PSGetInterface(PPB_GRAPHICS_2D_INTERFACE);
    //g_pInstance = (PPB_Instance*)PSGetInterface(PPB_INSTANCE_INTERFACE);
    //g_pImageData = (PPB_ImageData*)PSGetInterface(PPB_IMAGEDATA_INTERFACE);
    //g_pView = (PPB_View*)PSGetInterface(PPB_VIEW_INTERFACE);
#endif
    return PP_TRUE;
}

static void Instance_DidDestroy(PP_Instance instance) { }

static void Instance_DidChangeView(PP_Instance instance,PP_Resource view_resource) { }

static void Instance_DidChangeFocus(PP_Instance instance, PP_Bool has_focus) { }

static PP_Bool Instance_HandleDocumentLoad(PP_Instance instance,PP_Resource url_loader)
{
    // NaCl modules do not need to handle the document load function.
    return PP_FALSE;
}

static void Messaging_HandleMessage(PP_Instance instance,struct PP_Var message)
{
    if ( message.type != PP_VARTYPE_DICTIONARY ) // Special case for jspipe input handling
    {
        PostMessage("Got unexpected message type: %d\n", message.type);
        return;
    }
    struct PP_Var pipe_var = CStrToVar("pipe");
    struct PP_Var pipe_name = g_ppb_var_dictionary->Get(message, pipe_var);
    g_ppb_var->Release(pipe_var);
    if ( pipe_name.type == PP_VARTYPE_STRING ) // Special case for jspipe input handling
    {
        char file_name[PATH_MAX];
        snprintf(file_name, PATH_MAX, "/dev/%s", VarToCStr(pipe_name));
        int fd = open(file_name, O_RDONLY);
        g_ppb_var->Release(pipe_name);
        if ( fd < 0 )
        {
            PostMessage("Warning: opening %s failed.", file_name);
            goto done;
        }
        if ( ioctl(fd, NACL_IOC_HANDLEMESSAGE, &message) != 0 )
            PostMessage("Error: ioctl on %s failed: %s", file_name, strerror(errno));
        close(fd);
        goto done;
    }
    g_ppb_var->AddRef(message);
    if ( !EnqueueMessage(message) )
    {
        g_ppb_var->Release(message);
        PostMessage("Warning: dropped message because the queue was full.");
    }
done:
    g_ppb_var->Release(pipe_name);
}

#define GET_INTERFACE(var, type, name)            \
var = (type*)(get_browser(name));               \
if (!var) {                                     \
printf("Unable to get interface " name "\n"); \
return PP_ERROR_FAILED;                       \
}

PP_EXPORT int32_t PPP_InitializeModule(PP_Module a_module_id,PPB_GetInterface get_browser)
{
    g_get_browser_interface = get_browser;
    GET_INTERFACE(g_ppb_messaging, PPB_Messaging, PPB_MESSAGING_INTERFACE);
    GET_INTERFACE(g_ppb_var, PPB_Var, PPB_VAR_INTERFACE);
    GET_INTERFACE(g_ppb_var_array, PPB_VarArray, PPB_VAR_ARRAY_INTERFACE);
    GET_INTERFACE(g_ppb_var_dictionary, PPB_VarDictionary, PPB_VAR_DICTIONARY_INTERFACE);
    return PP_OK;
}

PP_EXPORT const void *PPP_GetInterface(const char* interface_name)
{
    if ( strcmp(interface_name,PPP_INSTANCE_INTERFACE) == 0 )
    {
        static PPP_Instance instance_interface =
        {
            &Instance_DidCreate,
            &Instance_DidDestroy,
            &Instance_DidChangeView,
            &Instance_DidChangeFocus,
            &Instance_HandleDocumentLoad,
        };
        return &instance_interface;
    }
    else if ( strcmp(interface_name, PPP_MESSAGING_INTERFACE) == 0 )
    {
        static PPP_Messaging messaging_interface = { &Messaging_HandleMessage };
        return &messaging_interface;
    }
    return NULL;
}

PP_EXPORT void PPP_ShutdownModule() { }

