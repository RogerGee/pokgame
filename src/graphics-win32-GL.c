/* graphics-win32-GL.c - pokgame */
#include <Windows.h>
#include <gl/GL.h>

#define POKGAME_WINDOW_CLASS "pokgame"

/* functions */
VOID CreateMainWindow(struct pok_graphics_subsystem* sys);
VOID EditMainWindow(struct pok_graphics_subsystem* sys);
VOID DestroyMainWindow(struct pok_graphics_subsystem* sys);
DWORD WINAPI RenderLoop(struct pok_graphics_subsystem* sys);
LRESULT CALLBACK WindowProc(HWND, UINT, WPARAM, LPARAM);
static void gl_init( /* implemented in graphics-GL.c (included later in this file) */
    int32_t viewWidth,
    int32_t viewHeight);
static void gl_create_textures(struct pok_graphics_subsystem* sys);

struct _pok_graphics_subsystem_impl
{
    HDC hDC;
    HGLRC hOpenGLContext;
    HWND hWnd;
    HANDLE mutex;
    HANDLE hThread;

    DWORD textureAlloc, textureCount;
    GLuint* textureNames;

    /* shared variable flags */
    volatile BOOLEAN rendering;     /* is the system rendering the window frame? */
    volatile BOOLEAN gameRendering; /* is the system invoking game rendering? */
    volatile BOOLEAN editFrame;     /* request to reinitialize frame */
    volatile BOOLEAN doShow;        /* request that the window frame be shown to screen */
    volatile BOOLEAN doHide;        /* request that the window frame be closed */
    volatile int texinfoCount;      /* texture information; if set then the rendering thread loads new textures */
    volatile struct texture_info* texinfo;
};

bool_t impl_new(struct pok_graphics_subsystem* sys)
{
    sys->impl = malloc(sizeof(struct _pok_graphics_subsystem_impl));
    if (sys->impl == NULL) {
        pok_exception_flag_memory_error();
        return FALSE;
    }
    /* create mutex for impl object */
    sys->impl->mutex = CreateMutex(NULL, FALSE, NULL);
    if (sys->impl->mutex == NULL)
        pok_error(pok_error_fatal, "fail CreateMutex()");
    /* allocate space to store texture names */
    sys->impl->textureAlloc = 32;
    sys->impl->textureCount = 0;
    sys->impl->textureNames = malloc(sizeof(GLuint) * sys->impl->textureAlloc);
    if (sys->impl->textureNames == NULL) {
        pok_exception_flag_memory_error();
        free(sys->impl);
        return FALSE;
    }
    /* prepare action shared variable */
    sys->impl->rendering = TRUE;
    sys->impl->gameRendering = TRUE;
    sys->impl->editFrame = FALSE;
    sys->impl->doShow = FALSE;
    sys->impl->doHide = FALSE;
    sys->impl->texinfo = NULL;
    sys->impl->texinfoCount = 0;
    /* spawn render loop thread; set 'sys->background' to TRUE to mean that we run
       the window on a background thread */
    sys->background = TRUE;
    sys->impl->hThread = CreateThread(
        NULL,
        0,
        RenderLoop,
        sys,
        0,
        NULL);
    if (sys->impl->hThread == NULL)
        pok_error(pok_error_fatal, "fail CreateThread()");
    return TRUE;
}
void impl_free(struct pok_graphics_subsystem* sys)
{
    /* flag that the rendering thread should quit, then
       wait for the thread to return */
    sys->impl->rendering = FALSE;
    WaitForSingleObject(sys->impl->hThread, INFINITE);
    CloseHandle(sys->impl->hThread);
    free(sys->impl->textureNames);
    free(sys->impl);
    sys->impl = NULL;
}
void impl_reload(struct pok_graphics_subsystem* sys)
{
    sys->impl->editFrame = TRUE;
}
void impl_load_textures(struct pok_graphics_subsystem* sys, struct texture_info* info, int count)
{
    do {
        WaitForSingleObject(sys->impl->mutex,INFINITE);
        /* make sure a request is not already being processed */
        if (sys->impl->texinfo != NULL) {
            ReleaseMutex(sys->impl->mutex);
            continue;
        }
    } while (FALSE);
    sys->impl->texinfo = info;
    sys->impl->texinfoCount = count;
    ReleaseMutex(sys->impl->mutex);
}
void impl_set_game_state(struct pok_graphics_subsystem* sys, bool_t state)
{
    sys->impl->gameRendering = state;
}
void impl_map_window(struct pok_graphics_subsystem* sys)
{
    sys->impl->doShow = TRUE;
}
void impl_unmap_window(struct pok_graphics_subsystem* sys)
{
    sys->impl->doHide = TRUE;
}
void impl_lock(struct pok_graphics_subsystem* sys)
{
    WaitForSingleObject(sys->impl->mutex, INFINITE);
}
void impl_unlock(struct pok_graphics_subsystem* sys)
{
    ReleaseMutex(sys->impl->mutex);
}

/* keyboard input functions */
bool_t pok_graphics_subsystem_keyboard_query(struct pok_graphics_subsystem* sys, enum pok_input_key key, bool_t refresh)
{
    if (GetForegroundWindow() == sys->impl->hWnd) {
        int vkey;
        switch (key) {
        case pok_input_key_ABUTTON:
            vkey = 0x5A;
            break;
        case pok_input_key_BBUTTON:
            vkey = 0x58;
            break;
        case pok_input_key_ENTER:
            vkey = VK_RETURN;
            break;
        case pok_input_key_ALT:
            vkey = VK_BACK;
            break;
        case pok_input_key_UP:
            vkey = VK_UP;
            break;
        case pok_input_key_DOWN:
            vkey = VK_DOWN;
            break;
        case pok_input_key_LEFT:
            vkey = VK_LEFT;
            break;
        case pok_input_key_RIGHT:
            vkey = VK_RIGHT;
            break;
        default:
            return FALSE;
        }
        return (GetAsyncKeyState(vkey) & 0x8000) != 0;
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
    WaitForSingleObject(sys->impl->mutex, INFINITE);
}
void pok_graphics_subsystem_unlock(struct pok_graphics_subsystem* sys)
{
    ReleaseMutex(sys->impl->mutex);
}

DWORD WINAPI RenderLoop(struct pok_graphics_subsystem* sys)
{
    int framerate = 0;
    DWORD sleepamt = 0;

    /* make window and OpenGL context; bind the context to this thread */
    CreateMainWindow(sys);
    wglMakeCurrent(sys->impl->hDC, sys->impl->hOpenGLContext);

    /* call function to setup OpenGL */
    gl_init(sys->wwidth, sys->wheight);

    while (sys->impl->rendering) {
        MSG msg;

        /* handle window messages from the operating system; use
           PeekMessage so that we don't have to wait on messages 
           but can go on to do more important things; handle all
           messages on this thread (not just for the specific
           window) */
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                sys->impl->rendering = FALSE;
                sys->impl->gameRendering = FALSE;
                break;
            }
            else {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }

        /* check for event notification from another pokgame thread */
        if (sys->impl->editFrame) {
            WaitForSingleObject(sys->impl->mutex, INFINITE);
            EditMainWindow(sys);
            sys->impl->editFrame = FALSE;
            ReleaseMutex(sys->impl->mutex);
        }
        if (sys->impl->doShow) {
            WaitForSingleObject(sys->impl->mutex, INFINITE);
            ShowWindow(sys->impl->hWnd, SW_SHOWNORMAL);
            sys->impl->doShow = FALSE;
            ReleaseMutex(sys->impl->mutex);
        }
        if (sys->impl->doHide) {
            WaitForSingleObject(sys->impl->mutex, INFINITE);
            ShowWindow(sys->impl->hWnd, SW_HIDE);
            sys->impl->doHide = FALSE;
            ReleaseMutex(sys->impl->mutex);
        }
        if (sys->impl->texinfo != NULL && sys->impl->texinfoCount > 0) {
            WaitForSingleObject(sys->impl->mutex, INFINITE);
            gl_create_textures(sys);
            free((struct texture_info*)sys->impl->texinfo);
            sys->impl->texinfo = NULL;
            sys->impl->texinfoCount = 0;
            ReleaseMutex(sys->impl->mutex);
        }

        /* clear the screen */
        glClear(GL_COLOR_BUFFER_BIT);

        /* rendering */
        if (sys->impl->gameRendering) {
            uint16_t index;
            /* go through and call each render function; make sure to
               obtain a lock so that we can synchronize with the update
               thread */
            WaitForSingleObject(sys->impl->mutex,INFINITE);
            for (index = 0; index < sys->routinetop; ++index)
                (*sys->routines[index])(sys, sys->contexts[index]);
            /* expose the backbuffer */
            SwapBuffers(sys->impl->hDC);
            ReleaseMutex(sys->impl->mutex);
        }
        else
            /* expose the blank backbuffer */
            SwapBuffers(sys->impl->hDC);

        /* check for framerate change */
        if (framerate != sys->framerate) {
            framerate = sys->framerate;
            sleepamt = 1000 / framerate;
        }

        Sleep(sleepamt);
    }

    /* cleanup */
    if (sys->impl->textureCount > 0)
        glDeleteTextures(sys->impl->textureCount, sys->impl->textureNames);
    DestroyMainWindow(sys);
    return 0;
}

VOID CreateMainWindow(struct pok_graphics_subsystem* sys)
{
    /* !!! oh the horror of the Win32 API !!! */

    RECT winrect;
    DWORD dwStyle;
    HANDLE hInst;
    WNDCLASS wc;
    int pixelFormat;
    static PIXELFORMATDESCRIPTOR pfd = {
        sizeof(PIXELFORMATDESCRIPTOR),
        1,
        PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
        PFD_TYPE_RGBA, 
        32,
        0, 0, 0, 0, 0, 0,
        0,
        0,
        0,
        0, 0, 0, 0,
        24,
        8,
        0,
        PFD_MAIN_PLANE,
        0,
        0, 0, 0
    };

    /* create the window class */
    hInst = GetModuleHandle(NULL);
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hbrBackground = NULL;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = NULL; /* TODO: load pokgame icon */
    wc.hInstance = hInst;
    wc.lpfnWndProc = WindowProc;
    wc.lpszClassName = POKGAME_WINDOW_CLASS;
    wc.lpszMenuName = NULL; 
    wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    if (!RegisterClass(&wc))
        pok_error(pok_error_fatal, "fail RegisterClass()");

    /* calculate window size */
    winrect.left = 0;
    winrect.right = sys->wwidth;
    winrect.top = 0;
    winrect.bottom = sys->wheight;
    dwStyle = WS_OVERLAPPEDWINDOW ^ WS_SIZEBOX ^ WS_MAXIMIZEBOX;
    AdjustWindowRectEx(&winrect, dwStyle, FALSE, WS_EX_CLIENTEDGE);

    /* create the main window */
    if (!(sys->impl->hWnd = CreateWindowEx(
                                WS_EX_CLIENTEDGE,
                                POKGAME_WINDOW_CLASS,
                                sys->title.buf,
                                dwStyle,
                                CW_USEDEFAULT,
                                CW_USEDEFAULT,
                                winrect.right - winrect.left,
                                winrect.bottom - winrect.top,
                                NULL,
                                NULL,
                                hInst,
                                sys)))
        pok_error(pok_error_fatal, "fail CreateWindowEx()");

    /* setup window device context */
    sys->impl->hDC = GetDC(sys->impl->hWnd);
    if (!(pixelFormat = ChoosePixelFormat(sys->impl->hDC, &pfd)))
        pok_error(pok_error_fatal, "fail ChoosePixelFormat()");
    if (!SetPixelFormat(sys->impl->hDC, pixelFormat, &pfd))
        pok_error(pok_error_fatal, "fail SetPixelFormat()");

    /* create OpenGL rendering context */
    sys->impl->hOpenGLContext = wglCreateContext(sys->impl->hDC);
    if (!sys->impl->hOpenGLContext)
        pok_error(pok_error_fatal, "fail wglCreateContext()");
}

VOID EditMainWindow(struct pok_graphics_subsystem* sys)
{
    sys->wwidth = sys->windowSize.columns * sys->dimension;
    sys->wheight = sys->windowSize.columns * sys->dimension;
    /* resize window */
    SetWindowPos(sys->impl->hWnd, NULL, 0, 0, sys->wwidth, sys->wheight, SWP_NOZORDER | SWP_NOMOVE);
    /* reset window text */
    SetWindowText(sys->impl->hWnd, sys->title.buf);
}

VOID DestroyMainWindow(struct pok_graphics_subsystem* sys)
{
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(sys->impl->hOpenGLContext);
    ReleaseDC(sys->impl->hWnd, sys->impl->hDC);
    DestroyWindow(sys->impl->hWnd);
    UnregisterClass(POKGAME_WINDOW_CLASS, GetModuleHandle(NULL));
}

LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    static struct pok_graphics_subsystem* sys = NULL;
    switch (uMsg) {
    case WM_CREATE:
        /* cache the graphics subsystem for later use; we trust
           that other code doesn't destroy it while the window
           message handler is running */
        sys = ((CREATESTRUCT*)lParam)->lpCreateParams;
        break;
    case WM_CLOSE:
        /* our X button was clicked */
        PostQuitMessage(0);
        break;
    case WM_KEYUP:
        if (sys->keyup != NULL) {
            enum pok_input_key key = pok_input_key_unknown;
            switch (wParam) {
            case VK_UP:
                key = pok_input_key_UP;
                break;
            case VK_DOWN:
                key = pok_input_key_DOWN;
                break;
            case VK_LEFT:
                key = pok_input_key_LEFT;
                break;
            case VK_RIGHT:
                key = pok_input_key_RIGHT;
                break;
            case VK_RETURN:
                key = pok_input_key_ENTER;
                break;
            case VK_BACK:
                key = pok_input_key_ALT;
                break;
            case 0x5A:  /* Z key */
                key = pok_input_key_ABUTTON;
                break;
            case 0x58:    /* X key */
                key = pok_input_key_BBUTTON;
                break;
            }
            (*sys->keyup)(key);
        }
        break;
    default:
        /* default message handler */
        return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }
    return 0;
}

/* include misc. graphics routines (they don't require the Win32 API) */
#include "graphics-GL.c"
