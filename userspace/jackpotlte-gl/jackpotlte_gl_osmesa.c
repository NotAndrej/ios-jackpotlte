/* SPDX-License-Identifier: MIT */
/*
 * The OpenGL ES library behind userspace/jackpotlte-gl/eagl.c: Mesa's OSMesa,
 * as tools/mesa-jackpotlte/ builds it (panfrost on Bifrost v6, the phone's
 * Mali-G71), with an OpenGL ES profile (tools/mesa-jackpotlte/0004).
 *
 * This file is all that knows which library draws.  The EAGL face wants four
 * things besides the gl* calls themselves: a context of a version of OpenGL ES,
 * that context made the calling thread's, a context taken away again, and a
 * name for the log.  OSMesa is reached through dlopen because the library is
 * put on the phone beside this one, not linked against it: the build of this
 * file is the same whether the library is there or not, and a phone without it
 * logs that and draws nothing rather than failing to load.
 *
 * The gl* functions are never looked up by name here.  jackpotlte_gl_proc is
 * OSMesa's own OSMesaGetProcAddress: the guest of the table (gl_slots.c) asks
 * for a name and gets the library's function, which is the only way to reach
 * it - a plain dlsym(RTLD_DEFAULT, "glBindTexture") would find the dyld shared
 * cache's OpenGLES.framework first, and Apple's driver knows nothing of these
 * contexts.
 */
#include "jackpotlte_gl.h"

#include <dlfcn.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* GL/osmesa.h's attribute and format values (Mesa 25.0.7, include/GL/osmesa.h;
 * OSMESA_ES_PROFILE is 0004's addition). */
#define OSMESA_FORMAT                 0x22
#define OSMESA_RGBA                   0x1908
#define OSMESA_PROFILE                0x33
#define OSMESA_ES_PROFILE             0x38
#define OSMESA_CONTEXT_MAJOR_VERSION  0x36
#define OSMESA_CONTEXT_MINOR_VERSION  0x37

/* The GL names this file itself uses. */
#define GL_TEXTURE_2D                 0x0de1
#define GL_UNSIGNED_BYTE              0x1401
#define GL_NO_ERROR                   0

/* OSMesa renders into memory it is handed; these are the pixels it may draw
 * into when the caller asks for its own window (nothing here does: what is
 * drawn is drawn into SweetSurfaces, 0003). */
#define WINDOW 16

struct jackpotlte_gl {
   void *context;   /* OSMesaContext */
   void *window;    /* the memory OSMesaMakeCurrent is given */
};

/* The library, opened once. */
static struct {
   void *handle;
   const char *where;
   void *(*create_context)(const int *attribs, void *share);
   void (*destroy_context)(void *context);
   unsigned char (*make_current)(void *context, void *buffer, unsigned type,
                                 int width, int height);
   jackpotlte_gl_func (*get_proc)(const char *name);
} lib;

/* The context this thread's GL calls are for.  OSMesa keeps one per thread. */
static _Thread_local struct jackpotlte_gl *current;

static void
say(const char *format, ...)
{
   va_list arguments;

   fputs("jackpotlte-gl: ", stderr);
   va_start(arguments, format);
   vfprintf(stderr, format, arguments);
   va_end(arguments);
   fputc('\n', stderr);
}

/*
 * Where the library may be: the one beside this one first (the phone's root has
 * both in the same directory), then the same library a directory up, then
 * whatever the loader's own search finds.  JACKPOTLTE_GL_LIBRARY answers for
 * all of them, which is how a build under test is pointed at a new one.
 */
static int
open_library(void)
{
   static const char *const beside[] = {
      "@loader_path/libOSMesa.8.dylib",
      "@loader_path/../lib/libOSMesa.8.dylib",
      "libOSMesa.8.dylib",
   };
   static int tried;
   const char *wanted;
   unsigned i;

   if (tried)
      return lib.handle != NULL ? 0 : -1;
   tried = 1;

   wanted = getenv("JACKPOTLTE_GL_LIBRARY");
   if (wanted != NULL && wanted[0] != '\0') {
      lib.handle = dlopen(wanted, RTLD_NOW | RTLD_LOCAL);
      lib.where = wanted;
   }
   for (i = 0; i < sizeof(beside) / sizeof(beside[0]) && lib.handle == NULL; i++) {
      lib.handle = dlopen(beside[i], RTLD_NOW | RTLD_LOCAL);
      lib.where = beside[i];
   }
   if (lib.handle == NULL) {
      lib.where = "none";
      say("no OpenGL ES library: %s", dlerror());
      return -1;
   }

   lib.create_context = (void *(*)(const int *, void *))
      dlsym(lib.handle, "OSMesaCreateContextAttribs");
   lib.destroy_context = (void (*)(void *))dlsym(lib.handle, "OSMesaDestroyContext");
   lib.make_current = (unsigned char (*)(void *, void *, unsigned, int, int))
      dlsym(lib.handle, "OSMesaMakeCurrent");
   lib.get_proc = (jackpotlte_gl_func (*)(const char *))
      dlsym(lib.handle, "OSMesaGetProcAddress");
   if (!lib.create_context || !lib.destroy_context || !lib.make_current || !lib.get_proc) {
      say("%s is not an OSMesa library: no OSMesaCreateContextAttribs, "
          "OSMesaDestroyContext, OSMesaMakeCurrent or OSMesaGetProcAddress", lib.where);
      dlclose(lib.handle);
      lib.handle = NULL;
      return -1;
   }
   say("the library is %s", lib.where);
   return 0;
}

const char *
jackpotlte_gl_library(void)
{
   return lib.handle != NULL ? lib.where : "none";
}

struct jackpotlte_gl *
jackpotlte_gl_create(int version)
{
   int attribs[] = {
      OSMESA_FORMAT, OSMESA_RGBA,
      OSMESA_PROFILE, OSMESA_ES_PROFILE,
      OSMESA_CONTEXT_MAJOR_VERSION, version,
      OSMESA_CONTEXT_MINOR_VERSION, 0,
      0,
   };
   struct jackpotlte_gl *gl;

   if (version < 2 || version > 3) {
      say("no context of OpenGL ES %d: this shim makes 2 and 3", version);
      return NULL;
   }
   if (open_library() != 0)
      return NULL;

   gl = calloc(1, sizeof(*gl));
   if (gl == NULL)
      return NULL;
   gl->window = calloc((size_t)WINDOW * WINDOW, 4);
   gl->context = lib.create_context(attribs, NULL);
   if (gl->window == NULL || gl->context == NULL) {
      /* The context is NULL when the library does not take the ES profile
       * attribute, which is what Mesa without 0004 does. */
      say("no OpenGL ES %d context from %s", version, jackpotlte_gl_library());
      free(gl->window);
      free(gl);
      return NULL;
   }
   return gl;
}

int
jackpotlte_gl_make_current(struct jackpotlte_gl *gl)
{
   if (gl == current)
      return 1;

   /* NULL, NULL: no context on this thread, which is how OSMesa is told. */
   if (!lib.make_current(gl != NULL ? gl->context : NULL,
                         gl != NULL ? gl->window : NULL,
                         gl != NULL ? GL_UNSIGNED_BYTE : 0,
                         gl != NULL ? WINDOW : 0,
                         gl != NULL ? WINDOW : 0)) {
      say("OSMesaMakeCurrent failed");
      return 0;
   }
   current = gl;
   return 1;
}

void
jackpotlte_gl_destroy(struct jackpotlte_gl *gl)
{
   if (gl == NULL)
      return;
   if (current == gl) {
      lib.make_current(NULL, NULL, 0, 0, 0);
      current = NULL;
   }
   lib.destroy_context(gl->context);
   free(gl->window);
   free(gl);
}

jackpotlte_gl_func
jackpotlte_gl_proc(const char *name)
{
   if (lib.handle == NULL && open_library() != 0)
      return NULL;
   return lib.get_proc(name);
}

int
jackpotlte_gl_surface_texture(struct jackpotlte_gl *gl, uint32_t surface)
{
   static void (*target_texture)(unsigned target, void *image);
   static unsigned (*get_error)(void);
   unsigned error;

   if (gl == NULL || surface == 0)
      return 0;
   if (target_texture == NULL) {
      /* The library's own names for these: the table's entries are resolved the
       * same way, and for the same reason. */
      target_texture = (void (*)(unsigned, void *))
         jackpotlte_gl_proc("glEGLImageTargetTexture2DOES");
      get_error = (unsigned (*)(void))jackpotlte_gl_proc("glGetError");
      if (target_texture == NULL || get_error == NULL) {
         say("the library has no glEGLImageTargetTexture2DOES: a surface cannot "
             "be a texture");
         target_texture = NULL;
         return 0;
      }
   }
   if (current != gl)
      jackpotlte_gl_make_current(gl);

   /* Left over from earlier calls: what this call did is the next one. */
   while (get_error() != GL_NO_ERROR) {
   }
   /* What glEGLImageTargetTexture2DOES is given here is the SweetSurface's ID:
     * with no EGL there is no image object, and 0003's frontend hook reads it as
     * the surface the kernel's driver has a buffer of. */
   target_texture(GL_TEXTURE_2D, (void *)(uintptr_t)surface);
   error = get_error();
   if (error != GL_NO_ERROR) {
      say("surface %u is not a texture here (GL error %#x)", surface, error);
      return 0;
   }
   return 1;
}
