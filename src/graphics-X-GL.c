/* graphics-X-GL.c - implement graphics subsystem using POSIX, XLib and GL */
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <unistd.h>
#include <pthread.h>

/* functions */
static void do_x_init();
static void do_x_close();
static void make_frame(struct pok_graphics_subsystem* sys);
static void edit_frame(struct pok_graphics_subsystem* sys);
static void close_frame(struct pok_graphics_subsystem* sys);
static void* graphics_loop(struct pok_graphics_subsystem* sys);

/* globals */
static int screen;
static Display* display;
static XVisualInfo* visual;
static int xref = 0;
static pthread_mutex_t xlib_mutex = PTHREAD_MUTEX_INITIALIZER; /* we must control concurrent access to xlib calls */

struct _pok_graphics_subsystem_impl
{
    Atom del;
    Window window;
    GLXContext context;
    char keys[32];
    pthread_t tid;
    pthread_mutex_t mutex;

    /* shared variable flags */
    volatile bool_t rendering;     /* is the system rendering the window frame? */
    volatile bool_t gameRendering; /* is the system invoking game rendering? */
    volatile bool_t editFrame;     /* request to reinitialize frame */
    volatile bool_t doMap;         /* request that the window frame be mapped to screen */
    volatile bool_t doUnmap;       /* request that the window frame be unmapped from screen */
};

bool_t impl_new(struct pok_graphics_subsystem* sys)
{
    sys->impl = malloc(sizeof(struct _pok_graphics_subsystem_impl));
    if (sys->impl == NULL) {
        pok_exception_flag_memory_error();
        return FALSE;
    }
    /* prepare the rendering thread */
    sys->impl->rendering = TRUE;
    sys->impl->gameRendering = TRUE;
    sys->impl->editFrame = FALSE;
    sys->impl->doMap = FALSE;
    sys->impl->doUnmap = FALSE;
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
    free(sys->impl);
    sys->impl = NULL;
}

inline void impl_reload(struct pok_graphics_subsystem* sys)
{
    sys->impl->editFrame = TRUE;
}

inline void impl_map_window(struct pok_graphics_subsystem* sys)
{
    /* flag that we want the frame to be mapped to screen */
    sys->impl->doMap = TRUE;
}

inline void impl_unmap_window(struct pok_graphics_subsystem* sys)
{
    sys->impl->doUnmap = TRUE;
}

inline void impl_lock(struct pok_graphics_subsystem* sys)
{
    pthread_mutex_lock(&sys->impl->mutex);
}

inline void impl_unlock(struct pok_graphics_subsystem* sys)
{
    pthread_mutex_unlock(&sys->impl->mutex);
}

/* define keyboard input functions */
static int pok_input_key_to_keycode(enum pok_input_key key)
{
    switch (key) {
    case pok_input_key_ABUTTON:
        return XK_Z;
    case pok_input_key_BBUTTON:
        return XK_X;
    case pok_input_key_ENTER:
        return XK_Return;
    case pok_input_key_ALT:
        return XK_BackSpace;
    case pok_input_key_UP:
        return XK_Up;
    case pok_input_key_DOWN:
        return XK_Down;
    case pok_input_key_LEFT:
        return XK_Left;
    case pok_input_key_RIGHT:
        return XK_Right;
    }
    return -1;
}
bool_t pok_graphics_subsystem_keyboard_query(struct pok_graphics_subsystem* sys,enum pok_input_key key,bool_t refresh)
{
    if (display != NULL) {
        /* asynchronously get the state of the keyboard from the X server; return TRUE if the specified
           normal input key was pressed in this instance; only grab the keyboard state if the user specified
           'refresh' so that we can process the keys in a single instance */
        int keycode;
        keycode = pok_input_key_to_keycode(key);
        if (refresh)
            XQueryKeymap(display,sys->impl->keys);
        return sys->impl->keys[keycode/8] & (0x01 << keycode%8) ? TRUE : FALSE;
    }
    return FALSE;
}

/* x11 functions */
void do_x_init()
{
    static bool_t flag = FALSE;
    pthread_mutex_lock(&xlib_mutex);
    ++xref;
    if (!flag) {
        /* connect to the X server (now it gets real) */
        int dum;
        static int GLX_VISUAL[] = {GLX_RGBA,GLX_DEPTH_SIZE,24,GLX_DOUBLEBUFFER,None};
        display = XOpenDisplay(NULL);
        screen = DefaultScreen(display);
        if ( !glXQueryExtension(display,&dum,&dum) )
            pok_error(pok_error_fatal,"glx extension not supported");
        /* obtain OpenGL-capable visual */
        visual = glXChooseVisual(display,screen,GLX_VISUAL);
        flag = TRUE;
    }
    pthread_mutex_unlock(&xlib_mutex);
}
void do_x_close()
{
    pthread_mutex_lock(&xlib_mutex);
    if (--xref <= 0) {
        XFree(visual);
        XCloseDisplay(display);
    }
    pthread_mutex_unlock(&xlib_mutex);
}
void make_frame(struct pok_graphics_subsystem* sys)
{
    unsigned int vmask;
    XSetWindowAttributes attrs;
    Colormap cmap;
    /* prepare X window parameters */
    cmap = XCreateColormap(display,RootWindow(display,visual->screen),visual->visual,AllocNone);
    vmask = CWBorderPixel | CWColormap | CWEventMask;
    attrs.border_pixel = 0;
    attrs.colormap = cmap;
    attrs.event_mask = ExposureMask | StructureNotifyMask | KeyPressMask;
    /* create the X Window */
    sys->impl->window = XCreateWindow(display,
        RootWindow(display,visual->screen),
        0, 0,
        sys->dimension * sys->windowSize.columns,
        sys->dimension * sys->windowSize.rows,
        5, visual->depth,
        InputOutput,
        visual->visual,
        vmask,
        &attrs);
    /* catch delete messages (enable X button) */
    sys->impl->del = XInternAtom(display,"WM_DELETE_WINDOW",False);
    XSetWMProtocols(display,sys->impl->window,&sys->impl->del,1);
    /* set title bar text */
    XStoreName(display,sys->impl->window,sys->title.buf);
    /* create the GL rendering context */
    sys->impl->context = glXCreateContext(display,visual,None,True);
    if (sys->impl->context == NULL)
        pok_error(pok_error_fatal,"cannot create OpenGL context");
}
void edit_frame(struct pok_graphics_subsystem* sys)
{
    /* edit the frame size */
    XResizeWindow(display,
        sys->impl->window,
        sys->dimension * sys->windowSize.columns,
        sys->dimension * sys->windowSize.rows);
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
    /* initialize global X11 connection */
    do_x_init();
    /* make the frame */
    make_frame(sys);
    /* make the context current on this thread */
    if ( !glXMakeCurrent(display,sys->impl->window,sys->impl->context) )
        pok_error(pok_error_fatal,"fail glkMakeCurrent()");
    /* setup OpenGL */
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0,sys->dimension*sys->windowSize.columns,0.0,sys->dimension*sys->windowSize.rows,-1.0,1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
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

            }
        }

        /* check for event notifications from another thread */
        if (sys->impl->editFrame) {
            edit_frame(sys);
            sys->impl->editFrame = FALSE;
        }
        if (sys->impl->doMap) {
            XMapWindow(display,sys->impl->window);
            sys->impl->doMap = FALSE;
        }
        if (sys->impl->doUnmap) {
            XUnmapWindow(display,sys->impl->window);
            sys->impl->doUnmap = FALSE;
        }

        glClearColor(12,12,12,0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (sys->impl->gameRendering) {
            /* test */
            glBegin(GL_QUADS);
            glColor3b(0,0,127);
            glVertex2i(10,10);
            glVertex2i(10,40);
            glVertex2i(40,40);
            glVertex2i(40,10);
            glEnd();

            glBegin(GL_QUADS);
            glColor3b(127,0,127);
            glVertex2i(100,100);
            glVertex2i(100,280);
            glVertex2i(280,280);
            glVertex2i(280,100);
            glEnd();
        }

        /* expose the backbuffer */
        glXSwapBuffers(display,sys->impl->window);

        /* put the thread to sleep to produce a frame rate */
        usleep(100000);
    }

done:
    close_frame(sys);
    do_x_close();
    return NULL;
}
