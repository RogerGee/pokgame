/* graphics-win32-gid.c - pokgame */
#include <Windows.h>

/* note: this file mirrors 'graphics-win32-GL.c'; it is very similar except
   it performs software rendering using the Win32 Graphics Display Interface */

#define POKGAME_WINDOW_CLASS "pokgame"

/* functions */
VOID CreateMainWindow(struct pok_graphics_subsystem* sys);
VOID EditMainWindow(struct pok_graphics_subsystem* sys);
VOID DestroyMainWindow(struct pok_graphics_subsystem* sys);
DWORD WINAPI RenderLoop(struct pok_graphics_subsystem* sys);
LRESULT CALLBACK WindowProc(HWND, UINT, WPARAM, LPARAM);

struct _pok_graphics_subsystem_impl
{
    HDC hDC;
    HWND hWnd;
    HDC hMemDC;
    HANDLE mutex;
    HANDLE hThread;
    HBITMAP hBackBuffer;
    HDC hBackBufferDC;
    HBRUSH hClearBrush;

    DWORD textureAlloc, textureCount;
    HBITMAP* textures;

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
    sys->impl->textures = malloc(sizeof(HBITMAP) * sys->impl->textureAlloc);
    if (sys->impl->textures == NULL) {
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
    /* spawn render loop thread */
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
    free(sys->impl->textures);
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
        WaitForSingleObject(sys->impl->mutex, INFINITE);
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

/* OpenGL functions */
void create_textures(struct pok_graphics_subsystem* sys)
{
    int i;
    for (i = 0; i < sys->impl->texinfoCount; ++i) {
        int j;
        for (j = 0; j < sys->impl->texinfo[i].count; ++j) {
            HBITMAP hBitmap;
            struct pok_image* img = sys->impl->texinfo[i].images[j];
            /* append texture name to collection */
            if (sys->impl->textureCount >= sys->impl->textureAlloc) {
                size_t nalloc;
                void* ndata;
                nalloc = sys->impl->textureAlloc << 1;
                ndata = realloc(sys->impl->textures, nalloc * sizeof(HBITMAP));
                if (ndata == NULL) {
                    pok_error(pok_error_warning, "could not allocate memory in create_textures()");
                    return;
                }
                sys->impl->textures = ndata;
                sys->impl->textureAlloc = nalloc;
            }
            /* assign the texture reference to the image (this is
               the index+1 at which to find the Win32 BITMAP object) */
            img->texref = sys->impl->textureCount+1;
            /* create texture object (really just a boring old Win32 BITMAP) */
            if (img->flags & pok_image_flag_alpha)
                hBitmap = CreateBitmap(img->width, img->height, 1, 32, img->pixels.data);
            else {
                /* word align the data */
                uint32_t i, j, m = img->width * img->height;
                byte_t* pixels = malloc(m * 4);
                for (i = 0, j = 0; i < m; ++i) {
                    pixels[j++] = img->pixels.dataRGB[i].r;
                    pixels[j++] = img->pixels.dataRGB[i].g;
                    pixels[j++] = img->pixels.dataRGB[i].b;
                    pixels[j++] = 0;
                }
                hBitmap = CreateBitmap(img->width, img->height, 1, 32, pixels);
                free(pixels);
            }
            sys->impl->textures[sys->impl->textureCount++] = hBitmap;
        }
    }
}

DWORD WINAPI RenderLoop(struct pok_graphics_subsystem* sys)
{
    DWORD i;
    RECT windowDims;

    /* compute window view dimensions */
    windowDims.left = 0;
    windowDims.top = 0;
    windowDims.right = sys->dimension * sys->windowSize.columns;
    windowDims.bottom = sys->dimension * sys->windowSize.rows;

    /* make window and OpenGL context; bind the context to this thread */
    CreateMainWindow(sys);

    /* create back buffer */
    sys->impl->hBackBuffer = CreateCompatibleBitmap(sys->impl->hDC, windowDims.right, windowDims.bottom);
    sys->impl->hBackBufferDC = CreateCompatibleDC(sys->impl->hDC);
    SelectObject(sys->impl->hBackBufferDC, sys->impl->hBackBuffer);

    /* create clear brush */
    sys->impl->hClearBrush = CreateSolidBrush(RGB(blackPixel.r, blackPixel.g, blackPixel.b));
    if (sys->impl->hClearBrush == NULL)
        pok_error(pok_error_fatal, "fail CreateSolidBrush()");

    while (sys->impl->rendering) {
        MSG msg;

        /* handle window messages from the operating system; use
           PeekMessage so that we don't have to wait on messages
           but can go on to do more important things */
        if (PeekMessage(&msg, sys->impl->hWnd, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                sys->impl->rendering = FALSE;
                sys->impl->gameRendering = FALSE;
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
            create_textures(sys);
            free((struct texture_info*)sys->impl->texinfo);
            sys->impl->texinfo = NULL;
            sys->impl->texinfoCount = 0;
            ReleaseMutex(sys->impl->mutex);
        }

        /* clear the back buffer */
        FillRect(sys->impl->hBackBufferDC, &windowDims,sys->impl->hClearBrush);

        /* rendering */
        if (sys->impl->gameRendering) {
            uint16_t index;
            /* go through and call each render function */
            WaitForSingleObject(sys->impl->mutex, INFINITE);
            for (index = 0; index < sys->routinetop; ++index)
                (*sys->routines[index])(sys, sys->contexts[index]);
            ReleaseMutex(sys->impl->mutex);
        }

        /* draw back buffer */
        BitBlt(sys->impl->hDC, 0, 0, windowDims.right, windowDims.bottom, sys->impl->hBackBufferDC, 0, 0, SRCCOPY);

        Sleep(sys->framerate);
    }

    /* cleanup */
    for (i = 0; i < sys->impl->textureCount; ++i)
        DeleteObject(sys->impl->textures[i]);
    DeleteObject(sys->impl->hClearBrush);
    DeleteObject(sys->impl->hBackBufferDC);
    DeleteObject(sys->impl->hBackBuffer);
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

    /* create the window class */
    hInst = GetModuleHandle(NULL);
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hbrBackground = NULL;
    wc.hCursor = LoadCursor(hInst, IDC_ARROW);
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
    winrect.right = sys->windowSize.columns * sys->dimension;
    winrect.top = 0;
    winrect.bottom = sys->windowSize.rows * sys->dimension;
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

    /* setup window device context; create a compatible memory device context
       that we'll use to render bitmaps */
    sys->impl->hDC = GetDC(sys->impl->hWnd);
    sys->impl->hMemDC = CreateCompatibleDC(sys->impl->hDC);
}

VOID EditMainWindow(struct pok_graphics_subsystem* sys)
{
    int wwidth, wheight;
    wwidth = sys->windowSize.columns * sys->dimension;
    wheight = sys->windowSize.columns * sys->dimension;
    /* resize window */
    SetWindowPos(sys->impl->hWnd, NULL, 0, 0, wwidth, wheight, SWP_NOZORDER | SWP_NOMOVE);
    /* reset window text */
    SetWindowText(sys->impl->hWnd, sys->title.buf);
}

VOID DestroyMainWindow(struct pok_graphics_subsystem* sys)
{
    DeleteDC(sys->impl->hMemDC);
    ReleaseDC(sys->impl->hWnd, sys->impl->hDC);
    DestroyWindow(sys->impl->hWnd);
    UnregisterClass(POKGAME_WINDOW_CLASS, GetModuleHandle(NULL));
}

static struct pok_graphics_subsystem* gsys = NULL;
LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
    case WM_CREATE:
        /* cache the graphics subsystem for later use; we trust
        that other code doesn't destroy it while the window
        message handler is running */
        gsys = ((CREATESTRUCT*)lParam)->lpCreateParams;
        break;
    case WM_CLOSE:
        /* our X button was clicked */
        PostQuitMessage(0);
        gsys = NULL;
        break;
    case WM_KEYUP:
        if (gsys->keyup != NULL) {
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
            (*gsys->keyup)(key);
        }
        break;
    default:
        /* default message handler */
        return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }
    return 0;
}

void pok_image_render(struct pok_image* img, int32_t x, int32_t y)
{
    if (gsys != NULL) {
        HBITMAP hOld;
        hOld = SelectObject(gsys->impl->hMemDC, gsys->impl->textures[img->texref - 1]);
        BitBlt(gsys->impl->hBackBufferDC, x, y, img->width, img->height, gsys->impl->hMemDC, 0, 0, SRCCOPY);
        SelectObject(gsys->impl->hMemDC, hOld);
    }
}
