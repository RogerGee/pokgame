/* graphics-X.c - implement graphics subsystem using POSIX and X11 */
#include "graphics-impl.h"
#include "error.h"
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>

/* functions */
static void do_x_init();
static void do_x_close();
static void make_frame(struct pok_graphics_subsystem* sys);
static void edit_frame(struct pok_graphics_subsystem* sys);
static void close_frame(struct pok_graphics_subsystem* sys);
static void* graphics_loop(struct pok_graphics_subsystem* sys);

/* globals */
static int screen;          /* X session screen */
static Display* display;    /* X session display connection */
static XVisualInfo* visual; /* visual info for X session */
static int xref = 0;        /* reference (how many times have we used X in the program's lifetime?) */
static pthread_mutex_t xlib_mutex = PTHREAD_MUTEX_INITIALIZER; /* we control concurrent access to xlib startup calls */

struct _pok_graphics_subsystem_impl
{
    Atom del;                          /* our window's delete property identifier (used to detect window close messages) */
    Window window;                     /* X window handle */
    GLXContext context;                /* GLX rendering context */
    char keys[32];                     /* bitmask of keyboard keys used when asynchronously querying the keyboard state */
    pthread_t tid;                     /* the rendering routine runs on the thread represented by this process id */
    pthread_mutex_t mutex;             /* this locks the renderer; used for synchronizing updating and rendering */
    struct gl_texture_info gltexinfo;  /* OpenGL texture information */
 
    /* shared variable flags */
    volatile bool_t rendering;     /* is the system rendering the window frame? */
    volatile bool_t gameRendering; /* is the system invoking game rendering? */
    volatile bool_t editFrame;     /* request to reinitialize frame */
    volatile bool_t doMap;         /* request that the window frame be mapped to screen */
    volatile bool_t doUnmap;       /* request that the window frame be unmapped from screen */
    volatile int texinfoCount;     /* texture information; if set then the rendering thread loads new textures */
    volatile struct texture_info* texinfo;
};

#ifdef POKGAME_DEBUG

static void check_impl(struct pok_graphics_subsystem* sys)
{
    if (sys->impl == NULL)
        pok_error(pok_error_fatal,"graphics_subsystem was not configured properly!");
}

#endif

/* implement the graphics subsystem interface */
bool_t impl_new(struct pok_graphics_subsystem* sys)
{
    /* initialize a new impl object for the graphics subsystem */
    sys->impl = malloc(sizeof(struct _pok_graphics_subsystem_impl));
    if (sys->impl == NULL) {
        pok_exception_flag_memory_error();
        return FALSE;
    }
    sys->impl->window = None;
    sys->impl->gltexinfo.textureAlloc = 32;
    sys->impl->gltexinfo.textureCount = 0;
    sys->impl->gltexinfo.textureNames = malloc(sizeof(GLuint) * sys->impl->gltexinfo.textureAlloc);
    if (sys->impl->gltexinfo.textureNames == NULL) {
        pok_exception_flag_memory_error();
        free(sys->impl);
        return FALSE;
    }
    /* prepare the rendering thread: since we can run the window and renderer on
       a separate thread, we set 'sys->background' to TRUE so the program can know
       to use the main thread for something else */
    sys->background = TRUE;
    sys->impl->rendering = TRUE;
    sys->impl->gameRendering = TRUE;
    sys->impl->editFrame = FALSE;
    sys->impl->doMap = FALSE;
    sys->impl->doUnmap = FALSE;
    sys->impl->texinfo = NULL;
    sys->impl->texinfoCount = 0;
    pthread_mutex_init(&sys->impl->mutex,NULL);
    if (pthread_create(&sys->impl->tid,NULL,(void*(*)(void*))graphics_loop,sys) != 0)
        pok_error(pok_error_fatal,"fail pthread_create()");
    return TRUE;
}
inline void impl_set_game_state(struct pok_graphics_subsystem* sys,bool_t state)
{
    /* this allows the subsystem to effectively pause/resume the game rendering */
    sys->impl->gameRendering = state;
}
void impl_free(struct pok_graphics_subsystem* sys)
{
    /* flag that rendering should stop and join the render thread
       back with this thread */
    sys->impl->rendering = FALSE;
    if (pthread_join(sys->impl->tid,NULL) != 0)
        pok_error(pok_error_fatal,"fail pthread_join()");
    free(sys->impl->gltexinfo.textureNames);
    free(sys->impl);
    sys->impl = NULL;
}
inline void impl_reload(struct pok_graphics_subsystem* sys)
{
#ifdef POKGAME_DEBUG
    check_impl(sys);
#endif

    sys->impl->editFrame = TRUE;
}
void impl_load_textures(struct pok_graphics_subsystem* sys,struct texture_info* info,int count)
{
#ifdef POKGAME_DEBUG
    check_impl(sys);
#endif

    do {
        pthread_mutex_lock(&sys->impl->mutex);
        /* make sure a request is not already being processed */
        if (sys->impl->texinfo != NULL) {
            pthread_mutex_unlock(&sys->impl->mutex);
            continue;
        }
    } while (FALSE);
    sys->impl->texinfo = info;
    sys->impl->texinfoCount = count;
    pthread_mutex_unlock(&sys->impl->mutex);
}
void impl_delete_textures(struct pok_graphics_subsystem* sys,struct texture_info* info,int count)
{
#ifdef POKGAME_DEBUG
    check_impl(sys);
#endif

    pthread_mutex_lock(&sys->impl->mutex);
    gl_delete_textures(&sys->impl->gltexinfo,info,count);
    pthread_mutex_unlock(&sys->impl->mutex);
}
inline void impl_map_window(struct pok_graphics_subsystem* sys)
{
#ifdef POKGAME_DEBUG
    check_impl(sys);
#endif

    /* flag that we want the frame to be mapped to screen; the
       rendering thread will detect this change and reset the flag */
    sys->impl->doMap = TRUE;
}
inline void impl_unmap_window(struct pok_graphics_subsystem* sys)
{
#ifdef POKGAME_DEBUG
    check_impl(sys);
#endif

    /* flag that we want the frame to be unmapped (hidden) from screen; the
       rendering thread will detect this change and reset the flag */
    sys->impl->doUnmap = TRUE;
}
inline void impl_lock(struct pok_graphics_subsystem* sys)
{
#ifdef POKGAME_DEBUG
    check_impl(sys);
#endif

    /* lock the impl object; note: if the game is rendering then this will also
       lock the rendering thread */
    pthread_mutex_lock(&sys->impl->mutex);
}
inline void impl_unlock(struct pok_graphics_subsystem* sys)
{
#ifdef POKGAME_DEBUG
    check_impl(sys);
#endif

    /* unlock the impl object */
    pthread_mutex_unlock(&sys->impl->mutex);
}

/* define keyboard input functions */
static KeySym pok_input_key_to_keysym(enum pok_input_key key)
{
    /* convert the pok_input_key enumerator into its X11 key constant equivilent */
    switch (key) {
    case pok_input_key_ABUTTON:
        return XK_Z;
    case pok_input_key_BBUTTON:
        return XK_X;
    case pok_input_key_ENTER:
        return XK_Return;
    case pok_input_key_BACK:
        return XK_BackSpace;
    case pok_input_key_DEL:
        return XK_Delete;
    case pok_input_key_UP:
        return XK_Up;
    case pok_input_key_DOWN:
        return XK_Down;
    case pok_input_key_LEFT:
        return XK_Left;
    case pok_input_key_RIGHT:
        return XK_Right;
    default:
        return -1;
    }
    return -1;
}
static enum pok_input_key pok_input_key_from_keysym(KeySym keysym)
{
    /* convert X11 key constant to pok_input_key enumerator equivilent */
    switch (keysym) {
    case XK_Z:
    case XK_z:
        return pok_input_key_ABUTTON;
    case XK_X:
    case XK_x:
        return pok_input_key_BBUTTON;
    case XK_Return:
        return pok_input_key_ENTER;
    case XK_BackSpace:        
        return pok_input_key_BACK;
    case XK_Delete:
        return pok_input_key_DEL;
    case XK_Up:
        return pok_input_key_UP;
    case XK_Down:
        return pok_input_key_DOWN;
    case XK_Left:
        return pok_input_key_LEFT;
    case XK_Right:
        return pok_input_key_RIGHT;
    }
    return pok_input_key_unknown;
}
bool_t pok_graphics_subsystem_keyboard_query(struct pok_graphics_subsystem* sys,enum pok_input_key key,bool_t refresh)
{
    /* this function will asychronously detect the keyboard state; the main game uses this implementation as opposed to
       window events because we can detect key presses closer to real time in this manner */
#ifdef POKGAME_DEBUG
    check_impl(sys);
#endif

    int dummy;
    Window xwin;
    static int cache[8][2] = {
        {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
        {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}
    };

    /* only query keyboard events if the window has input focus; we have to explicitly check this */
    if (display != NULL && XGetInputFocus(display,&xwin,&dummy) && xwin == sys->impl->window) {
        /* asynchronously get the state of the keyboard from the X server; return TRUE if the specified
           normal input key was pressed in this instance; only grab the keyboard state if the user specified
           'refresh' so that we can process the keys in a single instance */
        int keycode, i, j;
        if (refresh) {
            pthread_mutex_lock(&sys->impl->mutex);
            XQueryKeymap(display,sys->impl->keys);
            pthread_mutex_unlock(&sys->impl->mutex);
            if ((int)key == -1)
                return FALSE;
        }
        if (key < 8) {
            if (cache[key][0] == -1) {
                /* cache the byte position and mask of the key's bit in the bitmask for efficiency */
                if ((keycode = XKeysymToKeycode(display,pok_input_key_to_keysym(key))) == 0)
                    return FALSE;
                cache[key][0] = i = keycode/8;
                cache[key][1] = j = 0x01 << keycode%8;
            }
            else {
                i = cache[key][0];
                j = cache[key][1];
            }
        }
        else {
            /* not a pok_input_key */
            if ((keycode = XKeysymToKeycode(display,pok_input_key_to_keysym(key))) == 0)
                return FALSE;
            i = keycode/8;
            j = 0x01 << keycode%8;
        }
        return sys->impl->keys[i] & j ? TRUE : FALSE;
    }
    return FALSE;
}

/* misc. pok_graphics_subsystem functions */
bool_t pok_graphics_subsystem_is_running(struct pok_graphics_subsystem* sys)
{
    /* the game is running (a.k.a. being rendered) if the 'gameRendering' flag is up */
    return sys->impl != NULL && sys->impl->rendering && sys->impl->gameRendering;
}
bool_t pok_graphics_subsystem_has_window(struct pok_graphics_subsystem* sys)
{
    /* the window is up if the 'rendering' flag is true */
    return sys->impl != NULL && sys->impl->rendering;
}
void pok_graphics_subsystem_lock(struct pok_graphics_subsystem* sys)
{
#ifdef POKGAME_DEBUG
    check_impl(sys);
#endif

    /* this function is the same as 'impl_lock' however it is visible to other
       modules so that other routines can synchronize with the renderer */
    pthread_mutex_lock(&sys->impl->mutex);
}
void pok_graphics_subsystem_unlock(struct pok_graphics_subsystem* sys)
{
#ifdef POKGAME_DEBUG
    check_impl(sys);
#endif

    /* this function is the same as 'impl_unlock' however it is visible to other modules */
    pthread_mutex_unlock(&sys->impl->mutex);
}
void pok_graphics_subsystem_render_loop(struct pok_graphics_subsystem* sys)
{
    graphics_loop(sys);
}

/* x11 functions */
void do_x_init()
{
    /* connect to the X server if we have not already */
    pthread_mutex_lock(&xlib_mutex);
    if (xref++ == 0) {
        /* connect to the X server (now it gets real) */
        int dum;
        static int GLX_VISUAL[] = {GLX_RGBA,GLX_DEPTH_SIZE,24,GLX_DOUBLEBUFFER,None};
        XInitThreads();
        display = XOpenDisplay(NULL);
        screen = DefaultScreen(display);
        if ( !glXQueryExtension(display,&dum,&dum) )
            pok_error(pok_error_fatal,"glx extension not supported");
        /* obtain OpenGL-capable visual */
        visual = glXChooseVisual(display,screen,GLX_VISUAL);
    }
    pthread_mutex_unlock(&xlib_mutex);
}
void do_x_close()
{
    /* close the connection to the X server if we are the last to use it */
    pthread_mutex_lock(&xlib_mutex);
    if (--xref <= 0) {
        XFree(visual);
        XCloseDisplay(display);
        display = NULL;
    }
    pthread_mutex_unlock(&xlib_mutex);
}
void make_frame(struct pok_graphics_subsystem* sys)
{
    /* make a brand new X window; create a GLX context for the window as well */
    unsigned int vmask;
    XSetWindowAttributes attrs;
    Colormap cmap;
    XSizeHints hints;
    XVisualInfo* vi;
    /* prepare X window parameters */
    cmap = XCreateColormap(display,RootWindow(display,visual->screen),visual->visual,AllocNone);
    vmask = CWBorderPixel | CWColormap | CWEventMask;
    attrs.border_pixel = 0;
    attrs.colormap = cmap;
    attrs.event_mask = ExposureMask | StructureNotifyMask | KeyPressMask | KeyReleaseMask;
    /* create the X Window */
    sys->impl->window = XCreateWindow(display,
        RootWindow(display,visual->screen),
        0, 0,
        sys->wwidth,
        sys->wheight,
        5, visual->depth,
        InputOutput,
        visual->visual,
        vmask,
        &attrs);
    /* catch delete messages (enable X button) */
    sys->impl->del = XInternAtom(display,"WM_DELETE_WINDOW",False);
    XSetWMProtocols(display,sys->impl->window,&sys->impl->del,1);
    /* make the frame non-resizable */
    hints.flags = PMaxSize | PMinSize;
    hints.min_width = sys->wwidth; hints.max_width = sys->wwidth;
    hints.min_height = sys->wheight; hints.max_height = sys->wheight;
    XSetWMNormalHints(display,sys->impl->window,&hints);
    /* set title bar text */
    XStoreName(display,sys->impl->window,sys->title.buf);
    /* create the GL rendering context */
    sys->impl->context = glXCreateContext(display,visual,None,True);
    if (sys->impl->context == NULL)
        pok_error(pok_error_fatal,"cannot create OpenGL context");
}
void edit_frame(struct pok_graphics_subsystem* sys)
{
    XSizeHints hints;
    sys->wwidth = sys->dimension * sys->windowSize.columns;
    sys->wheight = sys->dimension * sys->windowSize.rows;
    /* respecify window limits */
    hints.flags = PMaxSize | PMinSize;
    hints.min_width = sys->wwidth; hints.max_width = sys->wwidth;
    hints.min_height = sys->wheight; hints.max_height = sys->wheight;
    XSetWMNormalHints(display,sys->impl->window,&hints);
    /* edit the frame size and title */
    XResizeWindow(display,
        sys->impl->window,
        sys->wwidth,
        sys->wheight);
    XStoreName(display,sys->impl->window,sys->title.buf);
}
void close_frame(struct pok_graphics_subsystem* sys)
{
    /* destroy the window and context */
    glXMakeContextCurrent(display,None,None,NULL);
    glXDestroyContext(display,sys->impl->context);
    XDestroyWindow(display,sys->impl->window);
}

/* graphics rendering loop */
void* graphics_loop(struct pok_graphics_subsystem* sys)
{
    uint16_t index;
    int framerate = 0;
    useconds_t sleepamt = 0;

    /* initialize global X11 connection */
    do_x_init();

    /* make the frame */
    make_frame(sys);

    /* make the context current on this thread and attach it to the window */
    if ( !glXMakeCurrent(display,sys->impl->window,sys->impl->context) )
        pok_error(pok_error_fatal,"fail glXMakeCurrent()");

    /* call function to setup OpenGL */
    gl_init(sys->wwidth,sys->wheight);

    /* call load routine on graphics subsystem (if specified) */
    if (sys->loadRoutine != NULL)
        sys->loadRoutine();

    /* begin rendering loop */
    while (sys->impl->rendering) {
        /* check for events from the XServer */
        while (XPending(display)) { /* call XPending to prevent XNextEvent from blocking; I believe this behavior will be consistent...? */
            XEvent evnt;
            XNextEvent(display,&evnt);
            if (evnt.type == ClientMessage) {
                if ((Atom)evnt.xclient.data.l[0] == sys->impl->del)
                    goto done;
            }
            else if (evnt.type == Expose) {


            }
            else if (evnt.type == ConfigureNotify) {
                /* center the image */
                int32_t x, y;
                x = evnt.xconfigure.width / 2 - sys->wwidth / 2;
                y = evnt.xconfigure.height / 2 - sys->wheight / 2;
                glViewport(x,y,sys->wwidth,sys->wheight);
            }
            else if (evnt.type == KeyRelease) {
                /* handle key release hooks */
                KeySym sym;
                char asciiValue;
                enum pok_input_key gameKey;
                XLookupString(&evnt.xkey,&asciiValue,1,&sym,NULL);
                gameKey = pok_input_key_from_keysym(sym);
                if (gameKey != pok_input_key_unknown)
                    for (index = 0;index < sys->keyupHook.top;++index)
                        if (sys->keyupHook.routines[index])
                            sys->keyupHook.routines[index](gameKey,sys->keyupHook.contexts[index]);
                if (sym >= XK_space && sym <= XK_asciitilde)
                    if (asciiValue != -1)
                        for (index = 0;index < sys->textentryHook.top;++index)
                            if (sys->textentryHook.routines[index])
                                sys->textentryHook.routines[index](asciiValue,sys->textentryHook.contexts[index]);
            }
        }

        /* check for event notifications from another thread */
        if (sys->impl->editFrame) {
            pthread_mutex_lock(&sys->impl->mutex);
            edit_frame(sys);
            sys->impl->editFrame = FALSE;
            pthread_mutex_unlock(&sys->impl->mutex);
        }
        if (sys->impl->doMap) {
            pthread_mutex_lock(&sys->impl->mutex);
            XMapWindow(display,sys->impl->window);
            sys->impl->doMap = FALSE;
            pthread_mutex_unlock(&sys->impl->mutex);
        }
        if (sys->impl->doUnmap) {
            pthread_mutex_lock(&sys->impl->mutex);
            XUnmapWindow(display,sys->impl->window);
            sys->impl->doUnmap = FALSE;
            pthread_mutex_unlock(&sys->impl->mutex);
        }
        if (sys->impl->texinfo != NULL && sys->impl->texinfoCount > 0) {
            pthread_mutex_lock(&sys->impl->mutex);
            gl_create_textures(&sys->impl->gltexinfo,(struct texture_info*)sys->impl->texinfo,sys->impl->texinfoCount);
            free((struct texture_info*)sys->impl->texinfo);
            sys->impl->texinfo = NULL;
            sys->impl->texinfoCount = 0;
            pthread_mutex_unlock(&sys->impl->mutex);
        }

        /* clear the screen */
        glClear(GL_COLOR_BUFFER_BIT);

        /* rendering */
        if (sys->impl->gameRendering) {
            /* go through and call each render function; we need to obtain
               a lock for the right to render; this allows for synchronization
               with the update process */
            pthread_mutex_lock(&sys->impl->mutex);

            for (index = 0;index < sys->routinetop;++index)
                if (sys->routines[index])
                    sys->routines[index](sys,sys->contexts[index]);
            /* expose the backbuffer */
            glXSwapBuffers(display,sys->impl->window);

            pthread_mutex_unlock(&sys->impl->mutex);
        }
        else
            /* expose just a black back buffer */
            glXSwapBuffers(display,sys->impl->window);

        /* check for framerate updates */
        if (sys->framerate != framerate) {
            framerate = sys->framerate;
            /* usleep uses microseconds; there are 10^6 microseconds in a second */
            sleepamt = 1000000 / framerate;
        }

        /* put the thread to sleep to produce a frame rate */
        usleep(sleepamt);
    }

    /* cleanup */
done:
    sys->impl->gameRendering = FALSE;
    sys->impl->rendering = FALSE;
    if (sys->impl->gltexinfo.textureCount > 0) {
        glDeleteTextures(sys->impl->gltexinfo.textureCount,sys->impl->gltexinfo.textureNames);
        sys->impl->gltexinfo.textureCount = 0;
    }
    if (sys->unloadRoutine != NULL)
        sys->unloadRoutine();
    close_frame(sys);
    do_x_close();
    return NULL;
}
