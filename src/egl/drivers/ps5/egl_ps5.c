/*
 * Copyright © 2026 cybervisi0n
 * SPDX-License-Identifier: MIT
 */

/*
 * EGL driver for PS5 Zink
 */

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <vulkan/vulkan.h>

#include "eglconfig.h"
#include "eglcontext.h"
#include "eglcurrent.h"
#include "egldisplay.h"
#include "egldriver.h"
#include "egllog.h"
#include "eglsurface.h"

#include "frontend/api.h"
#include "kopper_interface.h"
#include "main/glthread.h"
#include "pipe/p_context.h"
#include "pipe/p_defines.h"
#include "pipe/p_screen.h"
#include "pipe/p_state.h"
#include "state_tracker/st_context.h"
#include "util/macros.h"
#include "util/u_atomic.h"
#include "util/u_inlines.h"
#include "zink/zink_kopper.h"
#include "zink/zink_public.h"

#include "prospero/sceVideoOut.h"

_EGL_DRIVER_STANDARD_TYPECASTS(ps5_egl)

struct ps5_egl_display {
   int ref_count;
   struct pipe_frontend_screen fscreen;
};

struct ps5_egl_config {
   _EGLConfig base;
};

struct ps5_egl_context {
   _EGLContext base;
   struct st_context *st;
};

struct ps5_egl_surface {
   _EGLSurface base;
   struct pipe_frontend_drawable drawable;
   struct st_visual visual;
   struct kopper_loader_info kopper_info;
   struct pipe_resource *attachments[ST_ATTACHMENT_COUNT];
};

static uint32_t ps5_egl_next_drawable_id;

static bool
ps5_egl_acquire_gpu_platform(void)
{
   return true;
}

static void
ps5_egl_release_gpu_platform(void)
{
   
}

static int
ps5_egl_get_param(struct pipe_frontend_screen *fscreen,
                      enum st_manager_param param)
{
   return 0;
}

static EGLint
ps5_egl_client_apis(void)
{
   EGLint apis = 0;

   if (_eglIsApiValid(EGL_OPENGL_API))
      apis |= EGL_OPENGL_BIT;
   if (_eglIsApiValid(EGL_OPENGL_ES_API))
      apis |= EGL_OPENGL_ES_BIT | EGL_OPENGL_ES2_BIT | EGL_OPENGL_ES3_BIT_KHR;

   return apis;
}

static void
ps5_egl_fill_config(struct ps5_egl_config *conf, _EGLDisplay *disp,
                        EGLint id, EGLint apis, EGLint depth, EGLint stencil)
{
   _EGLConfig *base = &conf->base;

   _eglInitConfig(base, disp, id);

   base->RedSize = 8;
   base->GreenSize = 8;
   base->BlueSize = 8;
   base->AlphaSize = 8;
   base->BufferSize = 32;
   base->DepthSize = depth;
   base->StencilSize = stencil;
   base->ColorBufferType = EGL_RGB_BUFFER;
   base->ConfigCaveat = EGL_NONE;
   base->TransparentType = EGL_NONE;
   base->NativeRenderable = EGL_TRUE;
   base->NativeVisualID = 0;
   base->NativeVisualType = EGL_NONE;
   base->RenderableType = apis;
   base->Conformant = apis;
   base->SurfaceType = EGL_WINDOW_BIT | EGL_PBUFFER_BIT;
   base->MinSwapInterval = 0;
   base->MaxSwapInterval = 1;
   base->MaxPbufferWidth = _EGL_MAX_PBUFFER_WIDTH;
   base->MaxPbufferHeight = _EGL_MAX_PBUFFER_HEIGHT;
}

static EGLBoolean
ps5_egl_add_configs(_EGLDisplay *disp, EGLint apis)
{
   static const struct {
      EGLint depth;
      EGLint stencil;
   } formats[] = {
      { 0, 0 },
      { 24, 8 },
   };
   struct ps5_egl_config *configs[ARRAY_SIZE(formats)] = { NULL };
   EGLint linked = 0;

   for (unsigned i = 0; i < ARRAY_SIZE(formats); i++) {
      configs[i] = calloc(1, sizeof(*configs[i]));
      if (!configs[i]) {
         for (unsigned j = 0; j < i; j++)
            free(configs[j]);
         return _eglError(EGL_BAD_ALLOC, "eglInitialize");
      }
   }

   for (unsigned i = 0; i < ARRAY_SIZE(formats); i++) {
      struct ps5_egl_config *conf = configs[i];

      ps5_egl_fill_config(conf, disp, linked + 1, apis, formats[i].depth,
                              formats[i].stencil);

      if (!_eglValidateConfig(&conf->base, EGL_FALSE)) {
         _eglLog(_EGL_DEBUG, "ps5: skipping invalid config %u", i);
         free(conf);
         continue;
      }

      if (!_eglLinkConfig(&conf->base)) {
         free(conf);
         continue;
      }

      linked++;
   }

   if (linked == 0)
      return _eglError(EGL_NOT_INITIALIZED, "eglInitialize");

   return EGL_TRUE;
}

static void
ps5_egl_set_display_features(_EGLDisplay *disp, struct pipe_screen *screen,
                                 EGLint apis)
{
   disp->ClientAPIs = apis;

   disp->Extensions.KHR_create_context = EGL_TRUE;
   disp->Extensions.KHR_no_config_context = EGL_TRUE;
   disp->Extensions.KHR_surfaceless_context = EGL_TRUE;
   disp->Extensions.EXT_create_context_robustness =
      screen->caps.device_reset_status_query;
   disp->RobustBufferAccess = screen->caps.robust_buffer_access_behavior;
}

static EGLBoolean
ps5_egl_initialize(_EGLDisplay *disp)
{
   struct ps5_egl_display *display = ps5_egl_display(disp);
   struct pipe_screen *screen;
   EGLint apis;

   if (display) {
      p_atomic_inc(&display->ref_count);
      return EGL_TRUE;
   }

   if (disp->Platform != _EGL_PLATFORM_PROSPERO)
      return _eglError(EGL_NOT_INITIALIZED, "eglInitialize");

   display = calloc(1, sizeof(*display));
   if (!display)
      return _eglError(EGL_BAD_ALLOC, "eglInitialize");

   if (!ps5_egl_acquire_gpu_platform()) {
      _eglError(EGL_NOT_INITIALIZED, "eglInitialize");
      goto fail_free;
   }

   screen = zink_create_screen(NULL, NULL);
   if (!screen) {
      _eglError(EGL_NOT_INITIALIZED, "eglInitialize");
      goto fail_release;
   }

   apis = ps5_egl_client_apis();
   if (!ps5_egl_add_configs(disp, apis))
      goto fail_screen;

   display->ref_count = 1;
   display->fscreen.screen = screen;
   display->fscreen.get_param = ps5_egl_get_param;

   ps5_egl_set_display_features(disp, screen, apis);
   disp->DriverData = display;

   return EGL_TRUE;

fail_screen:
   screen->destroy(screen);
fail_release:
   ps5_egl_release_gpu_platform();
fail_free:
   free(display);
   return EGL_FALSE;
}

static EGLBoolean
ps5_egl_terminate(_EGLDisplay *disp)
{
   struct ps5_egl_display *display = ps5_egl_display(disp);
   struct pipe_screen *screen;

   if (!display)
      return EGL_TRUE;

   assert(display->ref_count > 0);
   if (!p_atomic_dec_zero(&display->ref_count))
      return EGL_TRUE;

   screen = display->fscreen.screen;
   st_screen_destroy(&display->fscreen);
   screen->destroy(screen);
   ps5_egl_release_gpu_platform();

   free(display);
   disp->DriverData = NULL;

   return EGL_TRUE;
}

static struct ps5_egl_surface *
ps5_egl_surface_from_drawable(struct pipe_frontend_drawable *drawable)
{
   return container_of(drawable, struct ps5_egl_surface, drawable);
}

static struct pipe_screen *
ps5_egl_surface_screen(const struct ps5_egl_surface *surface)
{
   return surface->drawable.fscreen->screen;
}

static bool
ps5_egl_surface_is_window(const struct ps5_egl_surface *surface)
{
   return surface->base.Type == EGL_WINDOW_BIT;
}

static void
ps5_egl_fill_template(struct pipe_resource *templ,
                          const struct ps5_egl_surface *surface,
                          enum pipe_format format, unsigned bind)
{
   memset(templ, 0, sizeof(*templ));
   templ->target = PIPE_TEXTURE_2D;
   templ->format = format;
   templ->width0 = surface->base.Width;
   templ->height0 = surface->base.Height;
   templ->depth0 = 1;
   templ->array_size = 1;
   templ->last_level = 0;
   templ->usage = PIPE_USAGE_DEFAULT;
   templ->bind = bind;
}

static struct pipe_resource *
ps5_egl_create_color_buffer(struct ps5_egl_surface *surface)
{
   struct pipe_screen *screen = ps5_egl_surface_screen(surface);
   unsigned bind = PIPE_BIND_RENDER_TARGET | PIPE_BIND_SAMPLER_VIEW;
   struct pipe_resource templ;

   if (!ps5_egl_surface_is_window(surface)) {
      ps5_egl_fill_template(&templ, surface, surface->visual.color_format,
                                bind);
      return screen->resource_create(screen, &templ);
   }

   ps5_egl_fill_template(&templ, surface, surface->visual.color_format,
                             bind | PIPE_BIND_DISPLAY_TARGET);
   return screen->resource_create_drawable(screen, &templ,
                                           &surface->kopper_info);
}

static struct pipe_resource *
ps5_egl_create_depth_stencil_buffer(struct ps5_egl_surface *surface)
{
   struct pipe_screen *screen = ps5_egl_surface_screen(surface);
   enum pipe_format format = surface->visual.depth_stencil_format;
   struct pipe_resource templ;

   if (format == PIPE_FORMAT_NONE)
      return NULL;

   ps5_egl_fill_template(&templ, surface, format,
                             PIPE_BIND_DEPTH_STENCIL);
   return screen->resource_create(screen, &templ);
}

static struct pipe_resource *
ps5_egl_surface_attachment(struct ps5_egl_surface *surface,
                               enum st_attachment_type type)
{
   if (type < 0 || type >= ST_ATTACHMENT_COUNT)
      return NULL;

   if (surface->attachments[type])
      return surface->attachments[type];

   switch (type) {
   case ST_ATTACHMENT_FRONT_LEFT:
   case ST_ATTACHMENT_BACK_LEFT:
      surface->attachments[type] = ps5_egl_create_color_buffer(surface);
      break;
   case ST_ATTACHMENT_DEPTH_STENCIL:
      surface->attachments[type] =
         ps5_egl_create_depth_stencil_buffer(surface);
      break;
   default:
      break;
   }

   return surface->attachments[type];
}

static void
ps5_egl_surface_track_resize(struct ps5_egl_surface *surface)
{
   struct pipe_resource *back = surface->attachments[ST_ATTACHMENT_BACK_LEFT];

   if (!ps5_egl_surface_is_window(surface) || !back)
      return;

   if (back->width0 == (unsigned)surface->base.Width &&
       back->height0 == (unsigned)surface->base.Height)
      return;

   surface->base.Width = back->width0;
   surface->base.Height = back->height0;
   pipe_resource_reference(&surface->attachments[ST_ATTACHMENT_DEPTH_STENCIL],
                           NULL);
}

static bool
ps5_egl_validate(struct st_context *st,
                     struct pipe_frontend_drawable *drawable,
                     const enum st_attachment_type *statts, unsigned count,
                     struct pipe_resource **out,
                     struct pipe_resource **resolve)
{
   struct ps5_egl_surface *surface =
      ps5_egl_surface_from_drawable(drawable);

   ps5_egl_surface_track_resize(surface);

   for (unsigned i = 0; i < count; i++) {
      out[i] = NULL;
      pipe_resource_reference(&out[i],
                              ps5_egl_surface_attachment(surface,
                                                             statts[i]));
   }

   if (resolve)
      *resolve = NULL;

   return true;
}

static bool
ps5_egl_flush_front(struct st_context *st,
                        struct pipe_frontend_drawable *drawable,
                        enum st_attachment_type statt)
{
   return true;
}

static bool
ps5_egl_flush_swapbuffers(struct st_context *st,
                              struct pipe_frontend_drawable *drawable)
{
   return true;
}

static void
ps5_egl_surface_release_attachments(struct ps5_egl_surface *surface)
{
   for (unsigned i = 0; i < ST_ATTACHMENT_COUNT; i++)
      pipe_resource_reference(&surface->attachments[i], NULL);
}

/*
 * Surfaces
 */

static void
ps5_egl_init_visual(struct st_visual *visual, bool depth_stencil)
{
   visual->buffer_mask = ST_ATTACHMENT_BACK_LEFT_MASK;
   visual->color_format = PIPE_FORMAT_R8G8B8A8_UNORM;
   visual->depth_stencil_format = PIPE_FORMAT_NONE;
   visual->accum_format = PIPE_FORMAT_NONE;
   visual->samples = 1;

   if (depth_stencil) {
      visual->buffer_mask |= ST_ATTACHMENT_DEPTH_STENCIL_MASK;
      visual->depth_stencil_format = PIPE_FORMAT_Z24_UNORM_S8_UINT;
   }
}

static void
ps5_egl_init_drawable(struct ps5_egl_surface *surface,
                          struct ps5_egl_display *display)
{
   struct pipe_frontend_drawable *drawable = &surface->drawable;

   drawable->visual = &surface->visual;
   drawable->fscreen = &display->fscreen;
   drawable->ID = p_atomic_inc_return(&ps5_egl_next_drawable_id);
   drawable->stamp = 1;
   drawable->validate = ps5_egl_validate;
   drawable->flush_front = ps5_egl_flush_front;
   drawable->flush_swapbuffers = ps5_egl_flush_swapbuffers;
}

/* Describes the sceVideoOut for Zink, which keys its display target by it. */
static void
ps5_egl_init_kopper_info(struct ps5_egl_surface *surface)
{
   struct kopper_loader_info *info = &surface->kopper_info;

   // TODO: PS5 specific window surface stuff
   memset(info, 0, sizeof(*info));
   info->has_alpha = surface->base.Config->AlphaSize > 0;
   info->initial_swap_interval = 1;
   info->present_opaque = surface->base.PresentOpaque;
}

static EGLBoolean
ps5_egl_init_window(struct ps5_egl_surface *surface)
{
   // TODO query this from sceVideoOut

   surface->base.Width = 1920;
   surface->base.Height = 1080;
   ps5_egl_init_kopper_info(surface);

   return EGL_TRUE;
}

static _EGLSurface *
ps5_egl_create_surface(_EGLDisplay *disp, EGLint type, _EGLConfig *conf,
                           void *native_window, const EGLint *attrib_list)
{
   struct ps5_egl_display *display = ps5_egl_display(disp);
   struct ps5_egl_surface *surface;

   surface = calloc(1, sizeof(*surface));
   if (!surface) {
      _eglError(EGL_BAD_ALLOC, "eglCreateSurface");
      return NULL;
   }

   if (!_eglInitSurface(&surface->base, disp, type, conf, attrib_list, native_window))
      goto fail;

   if (type == EGL_WINDOW_BIT && !ps5_egl_init_window(surface))
      goto fail;

   ps5_egl_init_visual(&surface->visual,
                           conf->DepthSize > 0 || conf->StencilSize > 0);
   ps5_egl_init_drawable(surface, display);

   return &surface->base;

fail:
   free(surface);
   return NULL;
}

static _EGLSurface *
ps5_egl_create_window_surface(_EGLDisplay *disp, _EGLConfig *conf,
                                  void *native_window,
                                  const EGLint *attrib_list)
{
   return ps5_egl_create_surface(disp, EGL_WINDOW_BIT, conf,
                                     native_window, attrib_list);
}

static _EGLSurface *
ps5_egl_create_pbuffer_surface(_EGLDisplay *disp, _EGLConfig *conf,
                                   const EGLint *attrib_list)
{
   return ps5_egl_create_surface(disp, EGL_PBUFFER_BIT, conf, NULL,
                                     attrib_list);
}

static _EGLSurface *
ps5_egl_create_pixmap_surface(_EGLDisplay *disp, _EGLConfig *conf,
                                  void *native_pixmap,
                                  const EGLint *attrib_list)
{
   return NULL;
}

static EGLBoolean
ps5_egl_destroy_surface(_EGLDisplay *disp, _EGLSurface *surf)
{
   struct ps5_egl_surface *surface = ps5_egl_surface(surf);

   if (!_eglPutSurface(surf))
      return EGL_TRUE;

   ps5_egl_surface_release_attachments(surface);
   st_api_destroy_drawable(&surface->drawable);
   free(surface);

   return EGL_TRUE;
}

/*
 * Contexts
 */

static EGLBoolean
ps5_egl_context_profile(const _EGLContext *ctx, gl_api *profile)
{
   switch (ctx->ClientAPI) {
   case EGL_OPENGL_ES_API:
      *profile = ctx->ClientMajorVersion > 1 ? API_OPENGLES2 : API_OPENGLES;
      return EGL_TRUE;
   case EGL_OPENGL_API:
      if (ctx->Profile == EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR)
         *profile = API_OPENGL_CORE;
      else
         *profile = API_OPENGL_COMPAT;
      return EGL_TRUE;
   default:
      return EGL_FALSE;
   }
}

static void
ps5_egl_context_flags(const _EGLContext *ctx,
                          struct st_context_attribs *attribs)
{
   if (ctx->Flags & EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR)
      attribs->flags |= ST_CONTEXT_FLAG_DEBUG;
   if (ctx->Flags & EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR)
      attribs->flags |= ST_CONTEXT_FLAG_FORWARD_COMPATIBLE;
   if (ctx->NoError)
      attribs->flags |= ST_CONTEXT_FLAG_NO_ERROR;

   if (ctx->Flags & EGL_CONTEXT_OPENGL_ROBUST_ACCESS_BIT_KHR)
      attribs->context_flags |= PIPE_CONTEXT_ROBUST_BUFFER_ACCESS;
   if (ctx->ResetNotificationStrategy != EGL_NO_RESET_NOTIFICATION_KHR)
      attribs->context_flags |= PIPE_CONTEXT_LOSE_CONTEXT_ON_RESET;
}

static _EGLContext *
ps5_egl_create_context(_EGLDisplay *disp, _EGLConfig *conf,
                           _EGLContext *share_list, const EGLint *attrib_list)
{
   struct ps5_egl_display *display = ps5_egl_display(disp);
   struct st_context_attribs attribs = { 0 };
   enum st_context_error error = ST_CONTEXT_SUCCESS;
   struct st_context *shared_st = NULL;
   struct ps5_egl_context *context;

   context = calloc(1, sizeof(*context));
   if (!context) {
      _eglError(EGL_BAD_ALLOC, "eglCreateContext");
      return NULL;
   }

   if (!_eglInitContext(&context->base, disp, conf, share_list, attrib_list))
      goto fail;

   if (!ps5_egl_context_profile(&context->base, &attribs.profile)) {
      _eglError(EGL_BAD_MATCH, "eglCreateContext");
      goto fail;
   }

   attribs.major = context->base.ClientMajorVersion;
   attribs.minor = context->base.ClientMinorVersion;
   ps5_egl_context_flags(&context->base, &attribs);
   /* Independent of the config, so that config-less contexts work. */
   ps5_egl_init_visual(&attribs.visual, true);

   if (share_list)
      shared_st = ps5_egl_context(share_list)->st;

   context->st = st_api_create_context(&display->fscreen, &attribs, &error,
                                       shared_st);
   if (!context->st) {
      _eglError(error == ST_CONTEXT_ERROR_BAD_VERSION ? EGL_BAD_MATCH
                                                      : EGL_BAD_ALLOC,
                "eglCreateContext");
      goto fail;
   }

   return &context->base;

fail:
   free(context);
   return NULL;
}

static EGLBoolean
ps5_egl_destroy_context(_EGLDisplay *disp, _EGLContext *ctx)
{
   struct ps5_egl_context *context = ps5_egl_context(ctx);

   if (!_eglPutContext(ctx))
      return EGL_TRUE;

   st_destroy_context(context->st);
   free(context);

   return EGL_TRUE;
}

static struct pipe_frontend_drawable *
ps5_egl_drawable_or_null(_EGLSurface *surf)
{
   return surf ? &ps5_egl_surface(surf)->drawable : NULL;
}

static EGLBoolean
ps5_egl_make_current(_EGLDisplay *disp, _EGLSurface *dsurf,
                         _EGLSurface *rsurf, _EGLContext *ctx)
{
   _EGLContext *old_ctx;
   _EGLSurface *old_dsurf, *old_rsurf;
   bool ok;

   if (!_eglBindContext(ctx, dsurf, rsurf, &old_ctx, &old_dsurf, &old_rsurf))
      return EGL_FALSE;

   if (old_ctx == ctx && old_dsurf == dsurf && old_rsurf == rsurf) {
      _eglPutSurface(old_dsurf);
      _eglPutSurface(old_rsurf);
      _eglPutContext(old_ctx);
      return EGL_TRUE;
   }

   if (ctx) {
      ok = st_api_make_current(ps5_egl_context(ctx)->st,
                               ps5_egl_drawable_or_null(dsurf),
                               ps5_egl_drawable_or_null(rsurf));
   } else {
      ok = st_api_make_current(NULL, NULL, NULL);
   }

   if (!ok)
      return _eglError(EGL_BAD_MATCH, "eglMakeCurrent");

   if (old_dsurf)
      ps5_egl_destroy_surface(disp, old_dsurf);
   if (old_rsurf)
      ps5_egl_destroy_surface(disp, old_rsurf);
   if (old_ctx)
      ps5_egl_destroy_context(disp, old_ctx);

   return EGL_TRUE;
}

/*
 * Presentation
 */

struct ps5_egl_flush_args {
   struct pipe_context *pipe;
   struct pipe_resource *back;
};

/* Runs after pending GL work is recorded and before the batch is submitted,
 * so the swapchain image becomes presentable in the same submission.
 */
static void
ps5_egl_before_flush(void *data)
{
   struct ps5_egl_flush_args *args = data;

   args->pipe->flush_resource(args->pipe, args->back);
}

static EGLBoolean
ps5_egl_swap_buffers(_EGLDisplay *disp, _EGLSurface *surf)
{
   struct ps5_egl_surface *surface = ps5_egl_surface(surf);
   struct pipe_resource *back = surface->attachments[ST_ATTACHMENT_BACK_LEFT];
   struct pipe_screen *screen = ps5_egl_surface_screen(surface);
   struct ps5_egl_flush_args args;
   struct st_context *st;

   if (!surf->CurrentContext || !back)
      return EGL_FALSE;

   st = ps5_egl_context(surf->CurrentContext)->st;
   args.pipe = st->pipe;
   args.back = back;

   _mesa_glthread_finish(st->ctx);
   st_context_flush(st, ST_FLUSH_END_OF_FRAME, NULL, ps5_egl_before_flush,
                    &args);

   /* Zink presents, then acquires the next image behind the same resource. */
   screen->flush_frontbuffer(screen, st->pipe, back, 0, 0, &surface->drawable,
                             0, NULL);

   if (ps5_egl_surface_is_window(surface) && !zink_kopper_check(back))
      return _eglError(EGL_BAD_SURFACE, "eglSwapBuffers");

   /* The next draw validates again, which picks up a resized back buffer. */
   p_atomic_inc(&surface->drawable.stamp);
   st_context_invalidate_state(st, ST_INVALIDATE_FB_STATE);

   return EGL_TRUE;
}

static EGLBoolean
ps5_egl_swap_interval(_EGLDisplay *disp, _EGLSurface *surf, EGLint interval)
{
   struct ps5_egl_surface *surface = ps5_egl_surface(surf);
   struct pipe_resource *back = surface->attachments[ST_ATTACHMENT_BACK_LEFT];

   /* Used when the swapchain image is created later. */
   surface->kopper_info.initial_swap_interval = interval;

   if (ps5_egl_surface_is_window(surface) && back)
      zink_kopper_set_swap_interval(ps5_egl_surface_screen(surface), back,
                                    interval);

   return EGL_TRUE;
}

const _EGLDriver _eglDriver = {
   .Initialize = ps5_egl_initialize,
   .Terminate = ps5_egl_terminate,
   .CreateContext = ps5_egl_create_context,
   .DestroyContext = ps5_egl_destroy_context,
   .MakeCurrent = ps5_egl_make_current,
   .CreateWindowSurface = ps5_egl_create_window_surface,
   .CreatePixmapSurface = ps5_egl_create_pixmap_surface,
   .CreatePbufferSurface = ps5_egl_create_pbuffer_surface,
   .DestroySurface = ps5_egl_destroy_surface,
   .SwapBuffers = ps5_egl_swap_buffers,
   .SwapInterval = ps5_egl_swap_interval,
};