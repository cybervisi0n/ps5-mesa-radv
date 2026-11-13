/*
 * Mesa 3-D graphics library
 *
 * Copyright (C) 2014 Adrián Arroyo Calle <adrian.arroyocalle@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <dlfcn.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>


#include "eglconfig.h"
#include "eglcontext.h"
#include "eglcurrent.h"
#include "egldevice.h"
#include "egldisplay.h"
#include "egldriver.h"
#include "eglimage.h"
#include "egllog.h"
#include "eglsurface.h"
#include "egltypedefs.h"

#include "pipe/p_context.h"
#include "pipe/p_defines.h"
#include "state_tracker/st_context.h"
#include "util/u_atomic.h"
#include "mesa/glapi/glapi/glapi.h"

#include "gallium/drivers/zink/zink_kopper.h"

#include "gallium/drivers/zink/zink_public.h"

#include "target-helpers/inline_sw_helper.h"


#if MESA_DEBUG
#define TRACE(x...) printf("egl_ps5: " x)
#define CALLED()    TRACE("CALLED: %s\n", __PRETTY_FUNCTION__)
#else
#define TRACE(x...)
#define CALLED()
#endif
#define ERROR(x...) printf("egl_ps5: " x)

_EGL_DRIVER_STANDARD_TYPECASTS(ps5_egl)

struct ps5_egl_display {
   struct kopper_displaytarget * kopper_display;
   struct pipe_screen * screen;
   int ref_count;
};

struct ps5_egl_config {
   _EGLConfig base;
};

struct ps5_egl_context {
   _EGLContext base;
};

struct ps5_egl_surface {
   _EGLSurface base;
};

static
struct sw_winsys*
ps5_create_sw_winsys()
{
	struct sw_winsys* winsys = CALLOC_STRUCT(sw_winsys);

	if (!winsys)
		return NULL;

	// Attach winsys hooks for Haiku
	//winsys->destroy = hgl_winsys_destroy;
	//winsys->is_displaytarget_format_supported
	//	= hgl_winsys_is_displaytarget_format_supported;
	//winsys->displaytarget_create = hgl_winsys_displaytarget_create;
	//winsys->displaytarget_from_handle = hgl_winsys_displaytarget_from_handle;
	//winsys->displaytarget_get_handle = hgl_winsys_displaytarget_get_handle;
	//winsys->displaytarget_map = hgl_winsys_displaytarget_map;
	//winsys->displaytarget_unmap = hgl_winsys_displaytarget_unmap;
	//winsys->displaytarget_display = hgl_winsys_displaytarget_display;
	//winsys->displaytarget_destroy = hgl_winsys_displaytarget_destroy;

	return winsys;
}

static _EGLSurface *
ps5_create_window_surface(_EGLDisplay *disp, _EGLConfig *conf,
                            void *native_window, const EGLint *attrib_list)
{
   return NULL;
}

static _EGLSurface *
ps5_create_pixmap_surface(_EGLDisplay *disp, _EGLConfig *conf,
                            void *native_pixmap, const EGLint *attrib_list)
{
   return NULL;
}

static _EGLSurface *
ps5_create_pbuffer_surface(_EGLDisplay *disp, _EGLConfig *conf,
                             const EGLint *attrib_list)
{
   return NULL;
}

static EGLBoolean
ps5_destroy_surface(_EGLDisplay *disp, _EGLSurface *surf)
{
   return EGL_TRUE;
}

static EGLBoolean
ps5_swap_buffers(_EGLDisplay *disp, _EGLSurface *surf)
{
   return EGL_TRUE;
}

static EGLBoolean
ps5_initialize(_EGLDisplay *disp)
{
   struct ps5_egl_display *ps5_dpy = ps5_egl_display(disp);

   struct sw_winsys *winsys = ps5_create_sw_winsys();

   struct pipe_screen_config ps5_config = {};

   struct pipe_screen * screen = zink_create_screen(winsys, &ps5_config);
   ps5_dpy->screen = screen;

   return EGL_TRUE;
}

static EGLBoolean
ps5_terminate(_EGLDisplay *disp)
{
   
   return EGL_TRUE;
}

static _EGLContext *
ps5_create_context(_EGLDisplay *disp, _EGLConfig *conf,
                     _EGLContext *share_list, const EGLint *attrib_list)
{
   struct ps5_egl_context *context =
      (struct ps5_egl_context *)calloc(1, sizeof(*context));
   if (!context) {
      _eglError(EGL_BAD_ALLOC, "ps5_create_context");
      return NULL;
   }

   _eglInitContext(&context->base, disp, conf, share_list, attrib_list);

   
   return NULL;
}

static EGLBoolean
ps5_destroy_context(_EGLDisplay *disp, _EGLContext *ctx)
{
   return EGL_TRUE;
}

static EGLBoolean
ps5_make_current(_EGLDisplay *disp, _EGLSurface *dsurf, _EGLSurface *rsurf,
                   _EGLContext *ctx)
{
   return EGL_TRUE;
}

const _EGLDriver _eglDriver = {
   .Initialize = ps5_initialize,
   .Terminate = ps5_terminate,
   .CreateContext = ps5_create_context,
   .DestroyContext = ps5_destroy_context,
   .MakeCurrent = ps5_make_current,
   .CreateWindowSurface = ps5_create_window_surface,
   .CreatePixmapSurface = ps5_create_pixmap_surface,
   .CreatePbufferSurface = ps5_create_pbuffer_surface,
   .DestroySurface = ps5_destroy_surface,
   .SwapBuffers = ps5_swap_buffers,
};
