/*
 * Xsdl - A kdrive X server that renders into an SDL2 window.
 *
 * Loosely based on the original Xsdl (removed in 2009) and on Xephyr.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  The authors make no representations about the suitability
 * of this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 *
 * THE AUTHORS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE AUTHORS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifdef HAVE_DIX_CONFIG_H
#include <dix-config.h>
#endif

#include <SDL.h>

#include "kdrive.h"
#include "opaque.h"
#include "glx_extinit.h"

/* How often the SDL event queue is polled while the server is idle. */
#define SDL_POLL_MS 10

typedef struct _SdlScrPriv {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    int tex_width, tex_height;
    Uint32 tex_format;

    Uint32 *rgb;                /* converted pixels for depth <= 8 */
    Uint32 palette[256];

    Bool redraw_all;
    ScreenBlockHandlerProcPtr BlockHandler;
} SdlScrPriv;

static KdScreenInfo *sdlScreen;
static KdKeyboardInfo *sdlKeyboard;
static KdPointerInfo *sdlPointer;
static unsigned long sdlButtons;
static float sdlWheelAccum;
static Bool sdlKeyDown[SDL_NUM_SCANCODES];

static int sdlZoom = 1;
static Bool sdlFullscreen;
static Bool sdlHostCursor;
static const char *sdlTitle;

/*
 * SDL scancodes are USB HID usage IDs. The default XKB rules are evdev, so
 * translate to Linux input event codes; the X keycode is that plus 8.
 */
static const unsigned char sdlToEvdev[SDL_NUM_SCANCODES] = {
    [SDL_SCANCODE_A] = 30, [SDL_SCANCODE_B] = 48, [SDL_SCANCODE_C] = 46,
    [SDL_SCANCODE_D] = 32, [SDL_SCANCODE_E] = 18, [SDL_SCANCODE_F] = 33,
    [SDL_SCANCODE_G] = 34, [SDL_SCANCODE_H] = 35, [SDL_SCANCODE_I] = 23,
    [SDL_SCANCODE_J] = 36, [SDL_SCANCODE_K] = 37, [SDL_SCANCODE_L] = 38,
    [SDL_SCANCODE_M] = 50, [SDL_SCANCODE_N] = 49, [SDL_SCANCODE_O] = 24,
    [SDL_SCANCODE_P] = 25, [SDL_SCANCODE_Q] = 16, [SDL_SCANCODE_R] = 19,
    [SDL_SCANCODE_S] = 31, [SDL_SCANCODE_T] = 20, [SDL_SCANCODE_U] = 22,
    [SDL_SCANCODE_V] = 47, [SDL_SCANCODE_W] = 17, [SDL_SCANCODE_X] = 45,
    [SDL_SCANCODE_Y] = 21, [SDL_SCANCODE_Z] = 44,

    [SDL_SCANCODE_1] = 2, [SDL_SCANCODE_2] = 3, [SDL_SCANCODE_3] = 4,
    [SDL_SCANCODE_4] = 5, [SDL_SCANCODE_5] = 6, [SDL_SCANCODE_6] = 7,
    [SDL_SCANCODE_7] = 8, [SDL_SCANCODE_8] = 9, [SDL_SCANCODE_9] = 10,
    [SDL_SCANCODE_0] = 11,

    [SDL_SCANCODE_RETURN] = 28, [SDL_SCANCODE_ESCAPE] = 1,
    [SDL_SCANCODE_BACKSPACE] = 14, [SDL_SCANCODE_TAB] = 15,
    [SDL_SCANCODE_SPACE] = 57, [SDL_SCANCODE_MINUS] = 12,
    [SDL_SCANCODE_EQUALS] = 13, [SDL_SCANCODE_LEFTBRACKET] = 26,
    [SDL_SCANCODE_RIGHTBRACKET] = 27, [SDL_SCANCODE_BACKSLASH] = 43,
    [SDL_SCANCODE_NONUSHASH] = 43, [SDL_SCANCODE_SEMICOLON] = 39,
    [SDL_SCANCODE_APOSTROPHE] = 40, [SDL_SCANCODE_GRAVE] = 41,
    [SDL_SCANCODE_COMMA] = 51, [SDL_SCANCODE_PERIOD] = 52,
    [SDL_SCANCODE_SLASH] = 53, [SDL_SCANCODE_CAPSLOCK] = 58,

    [SDL_SCANCODE_F1] = 59, [SDL_SCANCODE_F2] = 60, [SDL_SCANCODE_F3] = 61,
    [SDL_SCANCODE_F4] = 62, [SDL_SCANCODE_F5] = 63, [SDL_SCANCODE_F6] = 64,
    [SDL_SCANCODE_F7] = 65, [SDL_SCANCODE_F8] = 66, [SDL_SCANCODE_F9] = 67,
    [SDL_SCANCODE_F10] = 68, [SDL_SCANCODE_F11] = 87, [SDL_SCANCODE_F12] = 88,

    [SDL_SCANCODE_PRINTSCREEN] = 99, [SDL_SCANCODE_SCROLLLOCK] = 70,
    [SDL_SCANCODE_PAUSE] = 119, [SDL_SCANCODE_INSERT] = 110,
    [SDL_SCANCODE_HOME] = 102, [SDL_SCANCODE_PAGEUP] = 104,
    [SDL_SCANCODE_DELETE] = 111, [SDL_SCANCODE_END] = 107,
    [SDL_SCANCODE_PAGEDOWN] = 109, [SDL_SCANCODE_RIGHT] = 106,
    [SDL_SCANCODE_LEFT] = 105, [SDL_SCANCODE_DOWN] = 108,
    [SDL_SCANCODE_UP] = 103,

    [SDL_SCANCODE_NUMLOCKCLEAR] = 69, [SDL_SCANCODE_KP_DIVIDE] = 98,
    [SDL_SCANCODE_KP_MULTIPLY] = 55, [SDL_SCANCODE_KP_MINUS] = 74,
    [SDL_SCANCODE_KP_PLUS] = 78, [SDL_SCANCODE_KP_ENTER] = 96,
    [SDL_SCANCODE_KP_1] = 79, [SDL_SCANCODE_KP_2] = 80,
    [SDL_SCANCODE_KP_3] = 81, [SDL_SCANCODE_KP_4] = 75,
    [SDL_SCANCODE_KP_5] = 76, [SDL_SCANCODE_KP_6] = 77,
    [SDL_SCANCODE_KP_7] = 71, [SDL_SCANCODE_KP_8] = 72,
    [SDL_SCANCODE_KP_9] = 73, [SDL_SCANCODE_KP_0] = 82,
    [SDL_SCANCODE_KP_PERIOD] = 83, [SDL_SCANCODE_KP_EQUALS] = 117,
    [SDL_SCANCODE_KP_COMMA] = 121,

    [SDL_SCANCODE_NONUSBACKSLASH] = 86, [SDL_SCANCODE_APPLICATION] = 127,
    [SDL_SCANCODE_POWER] = 116,

    [SDL_SCANCODE_F13] = 183, [SDL_SCANCODE_F14] = 184,
    [SDL_SCANCODE_F15] = 185, [SDL_SCANCODE_F16] = 186,
    [SDL_SCANCODE_F17] = 187, [SDL_SCANCODE_F18] = 188,
    [SDL_SCANCODE_F19] = 189, [SDL_SCANCODE_F20] = 190,
    [SDL_SCANCODE_F21] = 191, [SDL_SCANCODE_F22] = 192,
    [SDL_SCANCODE_F23] = 193, [SDL_SCANCODE_F24] = 194,

    [SDL_SCANCODE_HELP] = 138, [SDL_SCANCODE_MENU] = 139,
    [SDL_SCANCODE_STOP] = 128, [SDL_SCANCODE_AGAIN] = 129,
    [SDL_SCANCODE_UNDO] = 131, [SDL_SCANCODE_CUT] = 137,
    [SDL_SCANCODE_COPY] = 133, [SDL_SCANCODE_PASTE] = 135,
    [SDL_SCANCODE_FIND] = 136, [SDL_SCANCODE_MUTE] = 113,
    [SDL_SCANCODE_VOLUMEUP] = 115, [SDL_SCANCODE_VOLUMEDOWN] = 114,

    [SDL_SCANCODE_INTERNATIONAL1] = 89, [SDL_SCANCODE_INTERNATIONAL2] = 93,
    [SDL_SCANCODE_INTERNATIONAL3] = 124, [SDL_SCANCODE_INTERNATIONAL4] = 92,
    [SDL_SCANCODE_INTERNATIONAL5] = 94, [SDL_SCANCODE_INTERNATIONAL6] = 95,
    [SDL_SCANCODE_LANG1] = 122, [SDL_SCANCODE_LANG2] = 123,
    [SDL_SCANCODE_LANG3] = 90, [SDL_SCANCODE_LANG4] = 91,
    [SDL_SCANCODE_LANG5] = 85,

    [SDL_SCANCODE_LCTRL] = 29, [SDL_SCANCODE_LSHIFT] = 42,
    [SDL_SCANCODE_LALT] = 56, [SDL_SCANCODE_LGUI] = 125,
    [SDL_SCANCODE_RCTRL] = 97, [SDL_SCANCODE_RSHIFT] = 54,
    [SDL_SCANCODE_RALT] = 100, [SDL_SCANCODE_RGUI] = 126,
};

/* Screen */

static Uint32
sdlTextureFormat(KdScreenInfo *screen)
{
    switch (screen->fb.depth) {
    case 15:
        return SDL_PIXELFORMAT_RGB555;
    case 16:
        return SDL_PIXELFORMAT_RGB565;
    default:
        /* depth 24 directly, depth <= 8 after palette lookup */
        return SDL_PIXELFORMAT_RGB888;
    }
}

static Bool
sdlScreenInitialize(KdScreenInfo *screen)
{
    SdlScrPriv *scrpriv = screen->driver;
    Uint32 format;
    int bpp, i;

    if (screen != sdlScreen && sdlScreen) {
        ErrorF("Xsdl: only one screen is supported\n");
        return FALSE;
    }

    if (!scrpriv) {
        scrpriv = calloc(1, sizeof(SdlScrPriv));
        if (!scrpriv)
            return FALSE;
        screen->driver = scrpriv;
        sdlScreen = screen;
        /* black and white, until the colormap is installed */
        for (i = 1; i < 256; i++)
            scrpriv->palette[i] = 0xffffff;
    }

    if (!screen->width || !screen->height) {
        screen->width = 1024;
        screen->height = 768;
    }

    switch (screen->fb.depth) {
    case 1:
        bpp = 1;
        screen->fb.visuals = (1 << StaticGray);
        break;
    case 8:
        bpp = 8;
        screen->fb.visuals = ((1 << StaticGray) | (1 << GrayScale) |
                              (1 << StaticColor) | (1 << PseudoColor) |
                              (1 << TrueColor) | (1 << DirectColor));
        break;
    case 15:
    case 16:
        bpp = 16;
        screen->fb.visuals = (1 << TrueColor);
        break;
    case 0:
    case 24:
        screen->fb.depth = 24;
        bpp = 32;
        screen->fb.visuals = (1 << TrueColor);
        break;
    default:
        ErrorF("Xsdl: unsupported depth %d (use 1, 8, 15, 16 or 24)\n",
               screen->fb.depth);
        return FALSE;
    }

    screen->fb.bitsPerPixel = bpp;
    screen->fb.redMask = screen->fb.greenMask = screen->fb.blueMask = 0;
    if (screen->fb.depth == 24) {
        screen->fb.redMask = 0xff0000;
        screen->fb.greenMask = 0x00ff00;
        screen->fb.blueMask = 0x0000ff;
    }
    else if (screen->fb.depth == 16) {
        screen->fb.redMask = 0xf800;
        screen->fb.greenMask = 0x07e0;
        screen->fb.blueMask = 0x001f;
    }
    else if (screen->fb.depth == 15) {
        screen->fb.redMask = 0x7c00;
        screen->fb.greenMask = 0x03e0;
        screen->fb.blueMask = 0x001f;
    }
    screen->rate = 60;

    if (!scrpriv->window) {
        Uint32 flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
        char title[64];

        if (sdlFullscreen)
            flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
        if (sdlTitle)
            snprintf(title, sizeof(title), "%s", sdlTitle);
        else
            snprintf(title, sizeof(title), "Xsdl :%s", display);

        scrpriv->window = SDL_CreateWindow(title,
                                           SDL_WINDOWPOS_UNDEFINED,
                                           SDL_WINDOWPOS_UNDEFINED,
                                           screen->width * sdlZoom,
                                           screen->height * sdlZoom, flags);
        if (!scrpriv->window) {
            ErrorF("Xsdl: SDL_CreateWindow: %s\n", SDL_GetError());
            return FALSE;
        }
        scrpriv->renderer = SDL_CreateRenderer(scrpriv->window, -1, 0);
        if (!scrpriv->renderer) {
            ErrorF("Xsdl: SDL_CreateRenderer: %s\n", SDL_GetError());
            return FALSE;
        }
        if (!sdlHostCursor)
            SDL_ShowCursor(SDL_DISABLE);
    }

    format = sdlTextureFormat(screen);
    if (!scrpriv->texture || scrpriv->tex_width != screen->width ||
        scrpriv->tex_height != screen->height ||
        scrpriv->tex_format != format) {
        if (scrpriv->texture)
            SDL_DestroyTexture(scrpriv->texture);
        scrpriv->texture = SDL_CreateTexture(scrpriv->renderer, format,
                                             SDL_TEXTUREACCESS_STREAMING,
                                             screen->width, screen->height);
        if (!scrpriv->texture) {
            ErrorF("Xsdl: SDL_CreateTexture: %s\n", SDL_GetError());
            return FALSE;
        }
        scrpriv->tex_width = screen->width;
        scrpriv->tex_height = screen->height;
        scrpriv->tex_format = format;
        SDL_RenderSetLogicalSize(scrpriv->renderer,
                                 screen->width, screen->height);
    }

    /* the X server draws into a shadow buffer, damage is copied to SDL */
    if (!KdShadowFbAlloc(screen, FALSE))
        return FALSE;
    free(scrpriv->rgb);
    scrpriv->rgb = NULL;
    if (bpp <= 8) {
        scrpriv->rgb = calloc(screen->width * screen->height, sizeof(Uint32));
        if (!scrpriv->rgb)
            return FALSE;
    }
    scrpriv->redraw_all = TRUE;

    return TRUE;
}

/* Expand indexed pixels in a box through the palette into scrpriv->rgb. */
static void
sdlConvertIndexed(KdScreenInfo *screen, SdlScrPriv *scrpriv, const BoxRec *box)
{
    int x, y;

    for (y = box->y1; y < box->y2; y++) {
        const CARD8 *src = screen->fb.frameBuffer + y * screen->fb.byteStride;
        Uint32 *dst = scrpriv->rgb + y * screen->width;

        if (screen->fb.bitsPerPixel == 8) {
            for (x = box->x1; x < box->x2; x++)
                dst[x] = scrpriv->palette[src[x]];
        }
        else {
            /* fb packs 1bpp LSB first within each 32-bit word */
            const FbBits *bits = (const FbBits *) src;

            for (x = box->x1; x < box->x2; x++)
                dst[x] = scrpriv->palette[(bits[x >> FB_SHIFT] >>
                                           (x & FB_MASK)) & 1];
        }
    }
}

static void
sdlUploadBox(KdScreenInfo *screen, SdlScrPriv *scrpriv, const BoxRec *box)
{
    SDL_Rect rect = {
        .x = box->x1, .y = box->y1,
        .w = box->x2 - box->x1, .h = box->y2 - box->y1,
    };

    if (rect.w <= 0 || rect.h <= 0)
        return;

    if (scrpriv->rgb) {
        sdlConvertIndexed(screen, scrpriv, box);
        SDL_UpdateTexture(scrpriv->texture, &rect,
                          scrpriv->rgb + rect.y * screen->width + rect.x,
                          screen->width * sizeof(Uint32));
    }
    else {
        SDL_UpdateTexture(scrpriv->texture, &rect,
                          screen->fb.frameBuffer +
                          rect.y * screen->fb.byteStride +
                          rect.x * (screen->fb.bitsPerPixel / 8),
                          screen->fb.byteStride);
    }
}

static void
sdlPresent(KdScreenInfo *screen, SdlScrPriv *scrpriv)
{
    SDL_SetRenderDrawColor(scrpriv->renderer, 0, 0, 0, 255);
    SDL_RenderClear(scrpriv->renderer);
    SDL_RenderCopy(scrpriv->renderer, scrpriv->texture, NULL, NULL);
    SDL_RenderPresent(scrpriv->renderer);
}

static void
sdlUploadAll(KdScreenInfo *screen, SdlScrPriv *scrpriv)
{
    BoxRec all = { 0, 0, screen->width, screen->height };

    sdlUploadBox(screen, scrpriv, &all);
    scrpriv->redraw_all = FALSE;
}

static void
sdlShadowUpdate(ScreenPtr pScreen, shadowBufPtr pBuf)
{
    KdScreenPriv(pScreen);
    KdScreenInfo *screen = pScreenPriv->screen;
    SdlScrPriv *scrpriv = screen->driver;
    RegionPtr pRegion = DamageRegion(pBuf->pDamage);
    int nbox = RegionNumRects(pRegion);
    BoxPtr pbox = RegionRects(pRegion);

    if (scrpriv->redraw_all)
        sdlUploadAll(screen, scrpriv);
    else {
        /* many small boxes are cheaper as one upload */
        if (nbox > 32) {
            nbox = 1;
            pbox = RegionExtents(pRegion);
        }
        while (nbox--)
            sdlUploadBox(screen, scrpriv, pbox++);
    }
    sdlPresent(screen, scrpriv);
}

static void *
sdlShadowWindow(ScreenPtr pScreen, CARD32 row, CARD32 offset, int mode,
                CARD32 *size, void *closure)
{
    KdScreenPriv(pScreen);
    KdScreenInfo *screen = pScreenPriv->screen;

    *size = screen->fb.byteStride;
    return screen->fb.frameBuffer + row * screen->fb.byteStride + offset;
}

/* Input */

static void
sdlReleaseAllKeys(void)
{
    int sc;

    for (sc = 0; sc < SDL_NUM_SCANCODES; sc++) {
        if (sdlKeyDown[sc]) {
            sdlKeyDown[sc] = FALSE;
            KdEnqueueKeyboardEvent(sdlKeyboard, sdlToEvdev[sc] + 8, TRUE);
        }
    }
}

static void
sdlKey(const SDL_KeyboardEvent *key)
{
    SDL_Scancode sc = key->keysym.scancode;
    unsigned char code;

    if (key->repeat || sc < 0 || sc >= SDL_NUM_SCANCODES)
        return;     /* the server does its own autorepeat */
    code = sdlToEvdev[sc];
    if (!code)
        return;

#ifdef __APPLE__
    /*
     * macOS reports Caps Lock as down while it is locked and up when it is
     * unlocked, so each transition is one complete press of the key.
     */
    if (sc == SDL_SCANCODE_CAPSLOCK) {
        KdEnqueueKeyboardEvent(sdlKeyboard, code + 8, FALSE);
        KdEnqueueKeyboardEvent(sdlKeyboard, code + 8, TRUE);
        return;
    }
#endif

    sdlKeyDown[sc] = key->type == SDL_KEYDOWN;
    KdEnqueueKeyboardEvent(sdlKeyboard, code + 8, key->type == SDL_KEYUP);
}

static void
sdlMotion(int x, int y)
{
    x = max(0, min(x, sdlScreen->width - 1));
    y = max(0, min(y, sdlScreen->height - 1));
    KdEnqueuePointerEvent(sdlPointer, sdlButtons, x, y, 0);
}

static void
sdlButton(const SDL_MouseButtonEvent *button)
{
    unsigned long bit;

    switch (button->button) {
    case SDL_BUTTON_LEFT:
        bit = KD_BUTTON_1;
        break;
    case SDL_BUTTON_MIDDLE:
        bit = KD_BUTTON_2;
        break;
    case SDL_BUTTON_RIGHT:
        bit = KD_BUTTON_3;
        break;
    default:
        return;
    }
    if (button->type == SDL_MOUSEBUTTONDOWN)
        sdlButtons |= bit;
    else
        sdlButtons &= ~bit;
    sdlMotion(button->x, button->y);
}

static void
sdlWheel(const SDL_MouseWheelEvent *wheel)
{
    int x, y;

    /* buttons 4 and 5 are one click each; collect trackpad fractions */
    sdlWheelAccum += wheel->preciseY;
    SDL_GetMouseState(&x, &y);
    while (sdlWheelAccum >= 1.0f || sdlWheelAccum <= -1.0f) {
        unsigned long bit = sdlWheelAccum > 0 ? KD_BUTTON_4 : KD_BUTTON_5;

        KdEnqueuePointerEvent(sdlPointer, sdlButtons | bit | KD_MOUSE_DELTA,
                              0, 0, 0);
        KdEnqueuePointerEvent(sdlPointer, sdlButtons | KD_MOUSE_DELTA,
                              0, 0, 0);
        sdlWheelAccum += sdlWheelAccum > 0 ? -1.0f : 1.0f;
    }
}

static void
sdlWindowEvent(const SDL_WindowEvent *window)
{
    SdlScrPriv *scrpriv = sdlScreen->driver;

    switch (window->event) {
    case SDL_WINDOWEVENT_FOCUS_LOST:
        /* don't leave modifiers stuck after e.g. Cmd-Tab */
        sdlReleaseAllKeys();
        break;
    case SDL_WINDOWEVENT_EXPOSED:
    case SDL_WINDOWEVENT_SIZE_CHANGED:
        sdlPresent(sdlScreen, scrpriv);
        break;
    }
}

static void
sdlProcessEvents(void)
{
    SDL_Event event;

    input_lock();
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_QUIT:
            dispatchException |= DE_TERMINATE;
            isItTimeToYield = TRUE;
            break;
        case SDL_WINDOWEVENT:
            sdlWindowEvent(&event.window);
            break;
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            if (sdlKeyboard)
                sdlKey(&event.key);
            break;
        case SDL_MOUSEMOTION:
            if (sdlPointer)
                sdlMotion(event.motion.x, event.motion.y);
            break;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            if (sdlPointer)
                sdlButton(&event.button);
            break;
        case SDL_MOUSEWHEEL:
            if (sdlPointer)
                sdlWheel(&event.wheel);
            break;
        }
    }
    input_unlock();
}

static Bool
sdlEventWorkProc(ClientPtr client, void *closure)
{
    sdlProcessEvents();
    return TRUE;
}

static void
sdlScreenBlockHandler(ScreenPtr pScreen, void *timeout)
{
    KdScreenPriv(pScreen);
    KdScreenInfo *screen = pScreenPriv->screen;
    SdlScrPriv *scrpriv = screen->driver;

    pScreen->BlockHandler = scrpriv->BlockHandler;
    (*pScreen->BlockHandler) (pScreen, timeout);
    scrpriv->BlockHandler = pScreen->BlockHandler;
    pScreen->BlockHandler = sdlScreenBlockHandler;

    /* a palette change recolours pixels that were not damaged */
    if (scrpriv->redraw_all) {
        sdlUploadAll(screen, scrpriv);
        sdlPresent(screen, scrpriv);
    }

    /*
     * SDL has no file descriptor to wait on, and on macOS it must be driven
     * from the main thread, so poll it from here.
     */
    SDL_PumpEvents();
    if (SDL_PeepEvents(NULL, 0, SDL_PEEKEVENT,
                       SDL_FIRSTEVENT, SDL_LASTEVENT) > 0) {
        if (!QueueWorkProc(sdlEventWorkProc, NULL, NULL))
            FatalError("Xsdl: cannot queue event processing\n");
        AdjustWaitForDelay(timeout, 0);
    }
    else
        AdjustWaitForDelay(timeout, SDL_POLL_MS);
}

static Bool
sdlCreateColormap(ColormapPtr pmap)
{
    return fbInitializeColormap(pmap);
}

static Bool
sdlInitScreen(ScreenPtr pScreen)
{
    pScreen->CreateColormap = sdlCreateColormap;
    return TRUE;
}

#ifdef RANDR
/*
 * RandR 1.2 with one CRTC and one output whose only mode is the screen size;
 * the SDL window scales instead.  Toolkits such as GTK need a real output to
 * find a monitor.
 */
static Bool
sdlRandRGetInfo(ScreenPtr pScreen, Rotation *rotations)
{
    *rotations = RR_Rotate_0;
    return TRUE;
}

static Bool
sdlRandRCrtcSet(ScreenPtr pScreen, RRCrtcPtr crtc, RRModePtr mode,
                int x, int y, Rotation rotation,
                int numOutputs, RROutputPtr *outputs)
{
    if (mode && (mode->mode.width != pScreen->width ||
                 mode->mode.height != pScreen->height || x || y ||
                 rotation != RR_Rotate_0))
        return FALSE;
    return RRCrtcNotify(crtc, mode, x, y, rotation, NULL, numOutputs, outputs);
}

static Bool
sdlRandRScreenSetSize(ScreenPtr pScreen, CARD16 width, CARD16 height,
                      CARD32 mmWidth, CARD32 mmHeight)
{
    return width == pScreen->width && height == pScreen->height;
}

static Bool
sdlRandRInit(ScreenPtr pScreen)
{
    rrScrPrivPtr pScrPriv;
    RRCrtcPtr crtc;
    RROutputPtr output;
    RRModePtr mode;
    xRRModeInfo modeInfo;
    char name[32];

    if (!RRScreenInit(pScreen))
        return FALSE;
    pScrPriv = rrGetScrPriv(pScreen);
    pScrPriv->rrGetInfo = sdlRandRGetInfo;
    pScrPriv->rrCrtcSet = sdlRandRCrtcSet;
    pScrPriv->rrScreenSetSize = sdlRandRScreenSetSize;

    RRScreenSetSizeRange(pScreen, pScreen->width, pScreen->height,
                         pScreen->width, pScreen->height);

    crtc = RRCrtcCreate(pScreen, NULL);
    if (!crtc)
        return FALSE;
    /* xrandr complains about outputs without gamma */
    RRCrtcGammaSetSize(crtc, 256);

    output = RROutputCreate(pScreen, "SDL", 3, NULL);
    if (!output ||
        !RROutputSetClones(output, NULL, 0) ||
        !RROutputSetCrtcs(output, &crtc, 1) ||
        !RROutputSetConnection(output, RR_Connected) ||
        !RROutputSetPhysicalSize(output, pScreen->mmWidth, pScreen->mmHeight))
        return FALSE;

    snprintf(name, sizeof(name), "%dx%d", pScreen->width, pScreen->height);
    memset(&modeInfo, 0, sizeof(modeInfo));
    modeInfo.width = pScreen->width;
    modeInfo.height = pScreen->height;
    modeInfo.nameLength = strlen(name);
    mode = RRModeGet(&modeInfo, name);
    if (!mode ||
        !RROutputSetModes(output, &mode, 1, 0) ||
        !RRCrtcNotify(crtc, mode, 0, 0, RR_Rotate_0, NULL, 1, &output))
        return FALSE;
    pScrPriv->primaryOutput = output;
    return TRUE;
}
#endif

static Bool
sdlFinishInitScreen(ScreenPtr pScreen)
{
    KdScreenPriv(pScreen);
    SdlScrPriv *scrpriv = pScreenPriv->screen->driver;

    if (!shadowSetup(pScreen))
        return FALSE;

#ifdef RANDR
    if (!sdlRandRInit(pScreen))
        return FALSE;
#endif

    scrpriv->BlockHandler = pScreen->BlockHandler;
    pScreen->BlockHandler = sdlScreenBlockHandler;
    return TRUE;
}

static Bool
sdlCreateResources(ScreenPtr pScreen)
{
    return KdShadowSet(pScreen, RR_Rotate_0, sdlShadowUpdate, sdlShadowWindow);
}

static void
sdlScreenFini(KdScreenInfo *screen)
{
    SdlScrPriv *scrpriv = screen->driver;

    /* the window survives server resets; the framebuffer is reallocated */
    KdShadowFbFree(screen);
    scrpriv->BlockHandler = NULL;
}

static void
sdlGetColors(ScreenPtr pScreen, int n, xColorItem *pdefs)
{
    KdScreenPriv(pScreen);
    SdlScrPriv *scrpriv = pScreenPriv->screen->driver;

    while (n--) {
        Uint32 rgb = scrpriv->palette[pdefs->pixel & 0xff];

        pdefs->red = ((rgb >> 16) & 0xff) * 0x101;
        pdefs->green = ((rgb >> 8) & 0xff) * 0x101;
        pdefs->blue = (rgb & 0xff) * 0x101;
        pdefs++;
    }
}

static void
sdlPutColors(ScreenPtr pScreen, int n, xColorItem *pdefs)
{
    KdScreenPriv(pScreen);
    SdlScrPriv *scrpriv = pScreenPriv->screen->driver;

    while (n--) {
        if (pdefs->pixel < 256)
            scrpriv->palette[pdefs->pixel] = ((pdefs->red >> 8) << 16) |
                ((pdefs->green >> 8) << 8) | (pdefs->blue >> 8);
        pdefs++;
    }
    /* every pixel may have changed colour, not just damaged ones */
    if (scrpriv->rgb)
        scrpriv->redraw_all = TRUE;
}

static KdCardFuncs sdlFuncs = {
    .scrinit = sdlScreenInitialize,
    .initScreen = sdlInitScreen,
    .finishInitScreen = sdlFinishInitScreen,
    .createRes = sdlCreateResources,
    .scrfini = sdlScreenFini,
    .getColors = sdlGetColors,
    .putColors = sdlPutColors,
};

/* Input drivers */

static Status
sdlKeyboardInit(KdKeyboardInfo *ki)
{
    ki->minScanCode = KD_MIN_KEYCODE;
    ki->maxScanCode = KD_MAX_KEYCODE;
    free(ki->name);
    ki->name = strdup("Xsdl keyboard");
    sdlKeyboard = ki;
    return Success;
}

static Status
sdlKeyboardEnable(KdKeyboardInfo *ki)
{
    return Success;
}

static void
sdlKeyboardFini(KdKeyboardInfo *ki)
{
    sdlKeyboard = NULL;
}

static KdKeyboardDriver sdlKeyboardDriver = {
    .name = "sdl",
    .Init = sdlKeyboardInit,
    .Enable = sdlKeyboardEnable,
    .Fini = sdlKeyboardFini,
};

static Status
sdlMouseInit(KdPointerInfo *pi)
{
    pi->nAxes = 3;
    pi->nButtons = 5;
    free(pi->name);
    pi->name = strdup("Xsdl mouse");
    sdlPointer = pi;
    return Success;
}

static Status
sdlMouseEnable(KdPointerInfo *pi)
{
    return Success;
}

static void
sdlMouseFini(KdPointerInfo *pi)
{
    sdlPointer = NULL;
}

static KdPointerDriver sdlMouseDriver = {
    .name = "sdl",
    .Init = sdlMouseInit,
    .Enable = sdlMouseEnable,
    .Fini = sdlMouseFini,
};

/* DDX entry points */

int
main(int argc, char *argv[], char *envp[])
{
    return dix_main(argc, argv, envp);
}

void
InitCard(char *name)
{
    KdCardInfoAdd(&sdlFuncs, 0);
}

void
InitOutput(ScreenInfo *pScreenInfo, int argc, char **argv)
{
    KdInitOutput(pScreenInfo, argc, argv);
}

void
InitInput(int argc, char **argv)
{
    KdKeyboardInfo *ki;
    KdPointerInfo *pi;

    KdAddKeyboardDriver(&sdlKeyboardDriver);
    KdAddPointerDriver(&sdlMouseDriver);

    ki = KdNewKeyboard();
    if (!ki)
        FatalError("Couldn't create Xsdl keyboard\n");
    ki->driver = &sdlKeyboardDriver;
    KdAddKeyboard(ki);

    pi = KdNewPointer();
    if (!pi)
        FatalError("Couldn't create Xsdl pointer\n");
    pi->driver = &sdlMouseDriver;
    KdAddPointer(pi);

    KdInitInput();
}

void
CloseInput(void)
{
    KdCloseInput();
}

#if INPUTTHREAD
void
ddxInputThreadInit(void)
{
}
#endif

#ifdef DDXBEFORERESET
void
ddxBeforeReset(void)
{
}
#endif

void
ddxUseMsg(void)
{
    KdUseMsg();

    ErrorF("\nXsdl Option Usage:\n");
    ErrorF("-zoom <n>            Scale the initial window size by n\n");
    ErrorF("-fullscreen          Run in a fullscreen window\n");
    ErrorF("-title <title>       Set the window title\n");
    ErrorF("-host-cursor         Show the host cursor over the window\n");
}

int
ddxProcessArgument(int argc, char **argv, int i)
{
    if (!strcmp(argv[i], "-zoom")) {
        if (i + 1 < argc && atoi(argv[i + 1]) > 0) {
            sdlZoom = atoi(argv[i + 1]);
            return 2;
        }
        UseMsg();
        exit(1);
    }
    if (!strcmp(argv[i], "-fullscreen")) {
        sdlFullscreen = TRUE;
        return 1;
    }
    if (!strcmp(argv[i], "-title")) {
        if (i + 1 < argc) {
            sdlTitle = argv[i + 1];
            return 2;
        }
        UseMsg();
        exit(1);
    }
    if (!strcmp(argv[i], "-host-cursor")) {
        sdlHostCursor = TRUE;
        return 1;
    }
    return KdProcessArgument(argc, argv, i);
}

void
OsVendorInit(void)
{
    if (serverGeneration == 1) {
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
        SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");
        if (SDL_Init(SDL_INIT_VIDEO) < 0)
            FatalError("Xsdl: SDL_Init: %s\n", SDL_GetError());
        atexit(SDL_Quit);
    }
}
