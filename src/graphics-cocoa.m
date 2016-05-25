/* graphics-cocoa.m - pokgame */
#include "graphics-impl.h"
#include "error.h"
#include <unistd.h>
#include <pthread.h>
#import <Cocoa/Cocoa.h>
#import <OpenGL/gl.h>

/* this data structure stores graphics subsystem info specific to the Cocoa implementation */
struct _pok_graphics_subsystem_impl
{
    /* this mutex synchronizes graphical rendering and updates */
    pthread_mutex_t graphicsLock;

    /* OpenGL texture information */
    struct gl_texture_info gltexinfo;
    
    /* keyboard input table; this table stores key input for game input keys; we assign a non-zero 
       value for each 'pok_input_key' if it is down */
    char keytable[24];

    /* these flags control the window */
    volatile bool_t rendering,
                    gameRendering,
                    editWindow,
                    showWindow,
                    hideWindow;
    volatile bool_t texinfoLoad;
    volatile int texinfoCount;
    volatile struct texture_info* texinfo;
};

/* implement the interface from graphics-impl.h */

bool_t impl_new(struct pok_graphics_subsystem* sys)
{
    /* initialize a new impl object for the graphics subsystem */
    sys->impl = malloc(sizeof(struct _pok_graphics_subsystem_impl));
    if (sys->impl == NULL) {
        pok_exception_flag_memory_error();
        return FALSE;
    }
    sys->impl->gltexinfo.textureAlloc = 32;
    sys->impl->gltexinfo.textureCount = 0;
    sys->impl->gltexinfo.textureNames = malloc(sizeof(uint32_t) * sys->impl->gltexinfo.textureAlloc);
    if (sys->impl->gltexinfo.textureNames == NULL) {
        pok_exception_flag_memory_error();
        free(sys->impl);
        return FALSE;
    }
    
    /* set key table to default (no keys pressed) */
    memset(sys->impl->keytable,0,sizeof(sys->impl->keytable));

    /* since Cocoa UIs must run on the main thread, we don't spawn a new thread to run
       the UI; instead we mark 'sys->background' to be FALSE to indicate that the UI
       will not be run on a background thread */
    sys->background = FALSE;
    sys->impl->rendering = TRUE;
    sys->impl->gameRendering = TRUE;
    sys->impl->editWindow = FALSE;
    sys->impl->showWindow = FALSE;
    sys->impl->hideWindow = FALSE;
    pthread_mutex_init(&sys->impl->graphicsLock,NULL);
    return TRUE;
}

void impl_free(struct pok_graphics_subsystem* sys)
{
    if (sys->impl->texinfo != NULL)
        free((struct texture_info*)sys->impl->texinfo);
    free(sys->impl->gltexinfo.textureNames);
    free(sys->impl);
    sys->impl = NULL;
}

inline void impl_reload(struct pok_graphics_subsystem* sys)
{
    sys->impl->editWindow = TRUE;
}

void impl_load_textures(struct pok_graphics_subsystem* sys,struct texture_info* info,int count)
{
    do {
        pthread_mutex_lock(&sys->impl->graphicsLock);
        if (sys->impl->texinfo != NULL) {
            pthread_mutex_unlock(&sys->impl->graphicsLock);
            continue;
        }
    } while (FALSE);
    sys->impl->texinfoLoad = TRUE;
    sys->impl->texinfo = info;
    sys->impl->texinfoCount = count;
    pthread_mutex_unlock(&sys->impl->graphicsLock);
}

void impl_delete_textures(struct pok_graphics_subsystem* sys,struct  texture_info* info,int count)
{
    do {
        pthread_mutex_lock(&sys->impl->graphicsLock);
        if (sys->impl->texinfo != NULL) {
            pthread_mutex_unlock(&sys->impl->graphicsLock);
            continue;
        }
    } while (FALSE);
    sys->impl->texinfoLoad = FALSE;
    sys->impl->texinfo = info;
    sys->impl->texinfoCount = count;
    pthread_mutex_unlock(&sys->impl->graphicsLock);
}

inline void impl_set_game_state(struct pok_graphics_subsystem* sys,bool_t state)
{
    sys->impl->gameRendering = state;
}

inline void impl_map_window(struct pok_graphics_subsystem* sys)
{
    sys->impl->showWindow = TRUE;
}

inline void impl_unmap_window(struct pok_graphics_subsystem* sys)
{
    sys->impl->hideWindow = TRUE;
}

inline void impl_lock(struct pok_graphics_subsystem* sys)
{
    pthread_mutex_lock(&sys->impl->graphicsLock);
}

inline void impl_unlock(struct pok_graphics_subsystem* sys)
{
    pthread_mutex_unlock(&sys->impl->graphicsLock);
}

/* Cocoa subsystem implementation */

enum pok_input_key CocoaKeyCodeToKeyFlag(UInt16 keyCode)
{
    /* I know, this is nasty, but I can't find any enumerations or constants that
       symbolically refer to the key codes */
    switch (keyCode) {
        case 0x7E:
            return pok_input_key_UP;
        case 0x7D:
            return pok_input_key_DOWN;
        case 0x7B:
            return pok_input_key_LEFT;
        case 0x7C:
            return pok_input_key_RIGHT;
        case 0x06: /* Z */
            return pok_input_key_ABUTTON;
        case 0x07: /* X */
            return pok_input_key_BBUTTON;
        case 0x24:
            return pok_input_key_ENTER;
        case 0x33:
            return pok_input_key_BACK;
        case 0x75:
            return pok_input_key_DEL;
    }
    return pok_input_key_unknown;
}

@interface PokGameWindowDelegate : NSObject

@property (readonly) BOOL receivedClose;

- (id)init;
- (BOOL)windowShouldClose:(id)sender;
@end

@implementation PokGameWindowDelegate

- (id)init
{
    self = [super init];
    _receivedClose = NO;
    return self;
}

- (BOOL)windowShouldClose:(id)sender
{
    _receivedClose = YES;
    return YES;
}
@end

@interface PokGameContentView : NSView
{
    struct pok_graphics_subsystem* sys;
}

- (id)initWithSys:(struct pok_graphics_subsystem*)subsys;
@end

@implementation PokGameContentView

- (id)initWithSys:(struct pok_graphics_subsystem*)subsys
{
    self = [super init];
    sys = subsys;
    return self;
}

- (BOOL)canBecomeKeyView
{
    return YES;
}

- (BOOL)acceptsFirstResponder
{
    return YES;
}

-(BOOL)isOpaque
{
    return YES;
}

-(void)keyDown:(NSEvent*)event
{
    /* determine if the key is a 'pok_input_key'; if so, then flag its state
       in the key table; the keyUp functionality will reset the state later */

    sys->impl->keytable[CocoaKeyCodeToKeyFlag([event keyCode])] = 1;
}

-(void)keyUp:(NSEvent*)event
{
    /* we mark the key as being released; this allows the implementation to query
       against the key table at any point between the keyDown and keyUp to retrieve
       the key state */
    NSString* ascii;
    enum pok_input_key key;
    
    key = CocoaKeyCodeToKeyFlag([event keyCode]);
    ascii = [event characters];
    sys->impl->keytable[key] = 0;

    /* handle keyboard string input */
    if (key != pok_input_key_unknown) {
        for (int i = 0;i < sys->keyupHook.top;++i)
            if (sys->keyupHook.routines[i] != NULL)
                sys->keyupHook.routines[i](key,sys->keyupHook.contexts[i]);
    }
    if ([ascii length] > 0) {
        for (int i = 0;i < sys->textentryHook.top;++i)
            if (sys->textentryHook.routines[i])
                sys->textentryHook.routines[i]([ascii characterAtIndex:0],sys->textentryHook.contexts[i]);
    }
}
@end

@interface PokGameAppDelegate : NSObject

@end

@implementation PokGameAppDelegate

/* the app delegate should end the run loop after it finishes launching */

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender
{
    /* disable the application from terminating */
    return NSTerminateCancel;
}

- (void)applicationDidFinishLaunching:(NSNotification *)notification
{
    /* stop the main application run loop as soon as it starts; we'll perform the loop's
       messaging operations ourselves somewhere else */
    [NSApp stop:nil];
}

@end

/* this class will provide an interface to the Cocoa objects we need to spawn a window,
   create an OpenGL context, perform rendering and handle window events */
@interface PokCocoaSubsystem : NSObject
{
    id window;
    id view;
    id delegate;
    struct pok_graphics_subsystem* sys;
    id glContext;
}

+ (void)initApp;
+ (void)eventPoll;

- (id)initWithSys:(struct pok_graphics_subsystem*)subsys;
- (void)cleanup;
- (BOOL)isFinished;
- (void)showWindow;
- (void)hideWindow;
- (void)editWindow;
- (void)callRenderRoutines;
- (void)present;

@end

@implementation PokCocoaSubsystem

+ (void)initApp
{
    if (NSApp)
        /* already set up */
        return;

    /* setup global shared application object */
    [NSAutoreleasePool new];
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

    /* setup the application delegate */
    [NSApp setDelegate:[[PokGameAppDelegate alloc] init]];

    /* start the application event loop; the delegate will terminate it
     once it starts; we will opt to process events manually elsewhere */
    [NSApp run];
}

+ (void)eventPoll
{
    /* see if the event queue has unprocessed events; if so, then dequeue and send them */
    while (YES) {
        NSEvent* event;

        event = [NSApp nextEventMatchingMask:NSAnyEventMask
                           untilDate:[NSDate distantPast]
                              inMode:NSDefaultRunLoopMode
                             dequeue:YES];
        if (event == nil)
            break;

        [NSApp sendEvent:event];
    }
}

- (id)initWithSys :(struct pok_graphics_subsystem*)subsys
{
    self = [super init];

    /* setup graphics subsystem */
    sys = subsys;
    sys->wwidth = sys->windowSize.columns * sys->dimension;
    sys->wheight = sys->windowSize.rows * sys->dimension;

    /* create window object */
    window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0,0,sys->wwidth,sys->wheight)
                                            styleMask:NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask
                                                backing:NSBackingStoreBuffered
                                                    defer:NO];
    [window setTitle:[NSString stringWithCString:sys->title.buf encoding:NSASCIIStringEncoding]];

    /* create delegate to handle window events */
    delegate = [[PokGameWindowDelegate alloc] init];
    [window setDelegate:delegate];

    /* create view in which to render content */
    view = [[PokGameContentView alloc] initWithSys:subsys];
    [window setContentView:view];

    /* create OpenGL rendering context */
    NSOpenGLPixelFormatAttribute pixelAttrs[] =
    {
        NSOpenGLPFAColorSize, 24,
        NSOpenGLPFAAlphaSize, 8,
        NSOpenGLPFADoubleBuffer,
        NSOpenGLPFAAccelerated,
        0
    };
    NSOpenGLPixelFormat *pixelFormat = [[[NSOpenGLPixelFormat alloc] initWithAttributes:pixelAttrs] autorelease];
    glContext = [[[NSOpenGLContext alloc] initWithFormat:pixelFormat shareContext:NULL] autorelease];
    if (glContext == nil)
        pok_error(pok_error_fatal,"fail NSOpenGLContext()");

    /* associate the context with the window's view and make the context current for the thread */
    [glContext setView:view];
    [glContext makeCurrentContext];

    /* initialize OpenGL */
    gl_init(sys->wwidth,sys->wheight);

    return self;
}

- (void)cleanup
{
    [window orderOut:nil];
    
    [window setDelegate:nil];
    [delegate release];
    delegate = nil;
    
    [glContext release];
    glContext = nil;
    
    [view release];
    view = nil;
    
    [window close];
    window = nil;
}

- (BOOL)isFinished
{
    return [delegate receivedClose];
}

- (void)showWindow
{
    /* make ours the active application */
    [NSApp activateIgnoringOtherApps:YES];

    /* show window (move to front) */
    [window makeKeyAndOrderFront:nil];
}
- (void)hideWindow
{
    [window orderOut:nil];
}
- (void)editWindow
{
    /* this method edits the frame; it may need to be resized or its title bar
       text needs to be updated */

    /* recalculate desired window size */
    sys->wwidth = sys->windowSize.columns * sys->dimension;
    sys->wheight = sys->windowSize.rows * sys->dimension;
    [window setContentSize:NSMakeSize(sys->wwidth,sys->wheight)];

    /* update title bar text */
    [window setTitle:[NSString stringWithCString:sys->title.buf encoding:NSASCIIStringEncoding]];

    /* initialize OpenGL */
    gl_init(sys->wwidth,sys->wheight);
}
- (void)callRenderRoutines
{
    /* go through and call each render routine */
    for (uint16_t i = 0;i < sys->routinetop;++i)
        (*sys->routines[i])(sys,sys->contexts[i]);
}
- (void)present
{
    [glContext flushBuffer];
}

@end

/* implement pok_graphics_subsystem functions that require the impl object */

inline bool_t pok_graphics_subsystem_is_running(struct pok_graphics_subsystem* sys)
{
    return sys->impl != NULL && sys->impl->rendering && sys->impl->gameRendering;
}

inline bool_t pok_graphics_subsystem_has_window(struct pok_graphics_subsystem* sys)
{
    return sys->impl != NULL && sys->impl->rendering;
}

inline void pok_graphics_subsystem_lock(struct pok_graphics_subsystem* sys)
{
    pthread_mutex_lock(&sys->impl->graphicsLock);
}

inline void pok_graphics_subsystem_unlock(struct pok_graphics_subsystem* sys)
{
    pthread_mutex_unlock(&sys->impl->graphicsLock);
}

bool_t pok_graphics_subsystem_keyboard_query(struct pok_graphics_subsystem* sys,
                                             enum pok_input_key key,
                                             bool_t refresh)
{
    /* Cocoa doesn't have a way (or at least one that I can find) to query the asychronous key state
       of the keyboard input device; so, we just have to hack around this by keeping a key array and
       updating it when we receive key events in our NSResponder subclass */
    if (sys->impl != NULL && (int)key >= 0 && (size_t)key < sizeof(sys->impl->keytable))
        return sys->impl->keytable[key];
    return FALSE;
    (void)refresh;
}

void pok_graphics_subsystem_render_loop(struct pok_graphics_subsystem* sys)
{
    int framerate = 0;
    int sleepamt;
    PokCocoaSubsystem* cocoa;

    /* initialize Cocoa (if we are first to do so) and then create main app window */
    [PokCocoaSubsystem initApp];
    cocoa = [[PokCocoaSubsystem alloc] initWithSys:sys];

    /* call load routine */
    if (sys->loadRoutine != NULL)
        sys->loadRoutine();

    while (sys->impl->rendering) {
        /* do event polling */
        [PokCocoaSubsystem eventPoll];
        
        if ([cocoa isFinished])
            break;
            
        /* check for notifications from game threads for various actions */
        if (sys->impl->editWindow) {
            pthread_mutex_lock(&sys->impl->graphicsLock);
            [cocoa editWindow];
            sys->impl->editWindow = FALSE;
            pthread_mutex_unlock(&sys->impl->graphicsLock);
        }
        if (sys->impl->showWindow) {
            pthread_mutex_lock(&sys->impl->graphicsLock);
            [cocoa showWindow];
            sys->impl->showWindow = FALSE;
            pthread_mutex_unlock(&sys->impl->graphicsLock);
        }
        if (sys->impl->hideWindow) {
            pthread_mutex_lock(&sys->impl->graphicsLock);
            [cocoa hideWindow];
            sys->impl->hideWindow = FALSE;
            pthread_mutex_unlock(&sys->impl->graphicsLock);
        }
        if (sys->impl->texinfo != NULL && sys->impl->texinfoCount > 0) {
            pthread_mutex_lock(&sys->impl->graphicsLock);
            if (sys->impl->texinfoLoad) {
                gl_create_textures(
                    &sys->impl->gltexinfo,
                    (struct texture_info*)sys->impl->texinfo,
                    sys->impl->texinfoCount );
            }
            else {
                gl_delete_textures(
                    &sys->impl->gltexinfo,
                    (struct texture_info*)sys->impl->texinfo,
                    sys->impl->texinfoCount );
            }
            free((struct texture_info*)sys->impl->texinfo);
            sys->impl->texinfo = NULL;
            sys->impl->texinfoCount = 0;
            pthread_mutex_unlock(&sys->impl->graphicsLock);
        }

        /* clear the screen */
        glClear(GL_COLOR_BUFFER_BIT);

        /* do rendering */
        if (sys->impl->gameRendering) {
            pthread_mutex_lock(&sys->impl->graphicsLock);
            [cocoa callRenderRoutines];
            [cocoa present];
            pthread_mutex_unlock(&sys->impl->graphicsLock);
        }
        else
            [cocoa present];

        /* check for framerate change */
        if (framerate != sys->framerate) {
            framerate = sys->framerate;
            sleepamt = 1000000 / framerate;
        }

        /* perform timeout */
        usleep(sleepamt);
    }

    /* cleanup */
    sys->impl->gameRendering = FALSE;
    sys->impl->rendering = FALSE;
    if (sys->impl->gltexinfo.textureCount > 0) {
        glDeleteTextures(sys->impl->gltexinfo.textureCount,sys->impl->gltexinfo.textureNames);
        sys->impl->gltexinfo.textureCount = 0;
    }
    if (sys->unloadRoutine != NULL)
        sys->unloadRoutine();
    [cocoa cleanup];
}
