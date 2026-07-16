/*
 * winemac.drv Cocoa/Metal bridge export for native D3D->Metal backends (DXMT).
 *
 * Copyright 2026 the proton-mac project
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

/*
 * proton-mac (Spike 2 D6b): a native-arm64 unixlib graphics backend that renders D3D->Metal
 * (DXMT's winemetal.so) needs winemac's Cocoa Metal-view bridge to turn an HWND into a
 * CAMetalLayer. DXMT resolves that bridge with dlsym(RTLD_DEFAULT, "macdrv_functions") and,
 * failing that, per-function dlsyms (see third_party/dxmt/src/winemetal/unix/winemetal_unix.c
 * _CreateMetalViewFromHWND / _ReleaseMetalView, contract at winemetal_unix.c:1583-1594). Those
 * winemac functions already exist in winemac.so but are hidden (local) symbols, so the dlsyms
 * fail. This TU exports ONE default-visibility dispatch struct populated with the existing
 * functions -- additive, winemac-local, LGPL, and separately-replaceable (R1b-scoped; does not
 * touch the loader core). The struct layout mirrors DXMT's macdrv_functions_t member order
 * EXACTLY; members are void* so this file needs no coupling to DXMT's private type. Slots DXMT
 * never calls on the present/release path (init_display_devices, on_main_thread) stay NULL.
 *
 * Pinned to DXMT contract at third_party/dxmt @ submodule SHA in .gitmodules; if that struct
 * shape changes upstream, update the order here to match.
 */

#if 0
#pragma makedep unix
#endif

#include "config.h"

#include "macdrv.h"

/* Layout MUST match struct macdrv_functions_t in winemetal_unix.c:1583-1594 (member order). */
struct macdrv_functions_export
{
    void *macdrv_init_display_devices;      /* 1  (unused by DXMT present path) */
    void *get_win_data;                     /* 2  HWND -> struct macdrv_win_data* */
    void *release_win_data;                 /* 3 */
    void *macdrv_get_cocoa_window;          /* 4 */
    void *macdrv_create_metal_device;       /* 5 */
    void *macdrv_release_metal_device;      /* 6 */
    void *macdrv_view_create_metal_view;    /* 7 */
    void *macdrv_view_get_metal_layer;      /* 8 */
    void *macdrv_view_release_metal_view;   /* 9 */
    void *on_main_thread;                    /* 10 (unused by DXMT present path) */
};

__attribute__((visibility("default")))
struct macdrv_functions_export macdrv_functions =
{
    NULL,                                    /* macdrv_init_display_devices */
    (void *)get_win_data,
    (void *)release_win_data,
    (void *)macdrv_get_cocoa_window,
    (void *)macdrv_create_metal_device,
    (void *)macdrv_release_metal_device,
    (void *)macdrv_view_create_metal_view,
    (void *)macdrv_view_get_metal_layer,
    (void *)macdrv_view_release_metal_view,
    NULL,                                    /* on_main_thread */
};

/*
 * proton-mac (Spike 2 D6b): winemac materializes a window's client NSView (`data->client_view`) LAZILY, only
 * via its own GL/Vulkan surface paths (the single writer is macdrv_client_surface_present, window.c:1135,
 * reached only from macdrv_client_surface_create, window.c:1148, whose only callers are opengl.c:1475 /
 * vulkan.c:53). A native D3D->Metal backend (DXMT) is a *third* accelerated path winemac doesn't know about,
 * so for a plain Win32 window it reads `client_view` cold and gets NULL. This helper creates that client view
 * on demand the first time the metal bridge asks, mirroring the view setup in macdrv_client_surface_create
 * (macdrv_create_view/set_view_frame/set_view_superview/set_view_hidden) but WITHOUT the refcounted
 * client_surface object -- the view is owned solely by `data->client_view` and disposed on window destroy,
 * so there is no create-then-release lifecycle to manage. Additive; composes existing exported primitives.
 */
__attribute__((visibility("default")))
macdrv_view macdrv_get_or_create_client_view(HWND hwnd)
{
    struct macdrv_win_data *data = get_win_data(hwnd);
    macdrv_view view;

    if (!data) return NULL;

    if (!data->client_view)
    {
        HWND toplevel = NtUserGetAncestor(hwnd, GA_ROOT);
        RECT rect;

        NtUserGetClientRect(hwnd, &rect, NtUserGetWinMonitorDpi(hwnd, MDT_RAW_DPI));
        NtUserMapWindowPoints(hwnd, toplevel, (POINT *)&rect, 2, NtUserGetWinMonitorDpi(toplevel, MDT_RAW_DPI));

        view = macdrv_create_view(cgrect_from_rect(rect));
        OffsetRect(&rect, data->rects.client.left - data->rects.visible.left,
                          data->rects.client.top  - data->rects.visible.top);
        macdrv_set_view_frame(view, cgrect_from_rect(rect));
        macdrv_set_view_superview(view, toplevel == hwnd ? NULL : data->client_view,
                                  data->cocoa_window, NULL, NULL);
        macdrv_set_view_hidden(view, FALSE);
        data->client_view = view;
    }

    view = data->client_view;
    release_win_data(data);
    return view;
}
