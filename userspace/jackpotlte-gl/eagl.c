/* SPDX-License-Identifier: MIT */
/*
 * libjackpotlte-gl.dylib: an OpenGL ES implementation behind iOS's EAGLContext,
 * for the machine CoreAnimation draws on when it has no Metal.
 *
 * CoreAnimation's render server draws with Metal, or else with OpenGL ES, or
 * else with its own software rasteriser (CA::WindowServer::EAGLServer::renderer()).
 * With no Metal plug-in on this phone (docs/jackpotlte-mali.md section 6), it
 * reaches for OpenGL ES, and asks an EAGLContext for seven things.  All seven
 * are replaced here, so the context it gets is one of the library behind this
 * file (jackpotlte_gl_osmesa.c: Mesa's OSMesa with the phone's Mali) rather than
 * Apple's driver:
 *
 *   -initWithAPI:properties:   makes a context of the OpenGL ES version asked for
 *   -API                       that version
 *   -sharegroup                nil: nothing here shares a context (CoreImage asks;
 *                              its own accessor reads ivars of an object this
 *                              shim never initialised, so it must be answered)
 *   -setParameter:to:          noted; the value is not read
 *   -getMacroContextPrivate    the table of functions the caller invokes as
 *                              entry(table[0], arguments...), at the offsets of
 *                              the build's own OpenGLES (eagl_slots.txt); entry
 *                              0 is this context, the rest are gl_slots.c's
 *   -texImageIOSurface:...     the texture bound to the target is to show what
 *                              the IOSurface holds
 *   -attachImage:toCoreSurface:invertedRender:
 *                              the texture bound to the target is to be the
 *                              surface's memory (CoreAnimation then draws into
 *                              it), or to show it once
 *   -dealloc                   the context goes
 *
 * Both surface methods go through the same path: the surface's ID is handed to
 * glEGLImageTargetTexture2DOES, which 0003's OSMesa frontend hook turns into a
 * buffer of that surface (0154's 0x7e) - no copy, the display's memory itself.
 * A surface that cannot be one (not one plane of 32-bit BGRA pixels) is logged
 * and refused rather than copied: a copy would be a different picture and a
 * different bug.
 *
 * Nothing here is called by Apple's code paths that this shim does not replace:
 * the class's ivars are never filled in (there is no call to super's -init), so
 * every method CoreAnimation reaches must be one of the seven above.
 */
#include "jackpotlte_gl.h"

#include <objc/message.h>
#include <objc/runtime.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * IOSurface's accessors, declared here: the iPhoneOS SDK ships no header for
 * them (the framework is there, the header is not), the same arrangement the
 * iOS backend uses for IOKit (userspace/libjackpotlte/jackpotlte_iokit.c).  The
 * names are the framework's and the process that loads this library has it.
 */
typedef struct __IOSurface *IOSurfaceRef;
uint32_t IOSurfaceGetID(IOSurfaceRef surface);
uint32_t IOSurfaceGetPixelFormat(IOSurfaceRef surface);
size_t IOSurfaceGetPlaneCount(IOSurfaceRef surface);
size_t IOSurfaceGetBytesPerElement(IOSurfaceRef surface);
size_t IOSurfaceGetWidth(IOSurfaceRef surface);
size_t IOSurfaceGetHeight(IOSurfaceRef surface);

/* The GL names this file itself uses. */
#define GL_TEXTURE_2D             0x0de1
#define GL_TEXTURE_BINDING_2D     0x8069
#define GL_VENDOR                 0x1f00
#define GL_RENDERER               0x1f01
#define GL_VERSION                0x1f02

/* As many contexts as the display server has been seen to make: the renderer's
 * own and CoreImage's.  A table this small keeps the lookup in context_of() a
 * handful of compares, and a full one is logged instead of growing. */
#define CONTEXTS 8

struct jackpotlte_eagl {
   void *table[JACKPOTLTE_GL_SLOTS];   /* [0] is this: what every entry is handed */
   id object;                          /* the EAGLContext this stands for */
   struct jackpotlte_gl *gl;
   unsigned long api;
};

static struct jackpotlte_eagl *contexts[CONTEXTS];

/* The library's functions this file calls itself (never by name: see
 * jackpotlte_gl_osmesa.c). */
static struct {
   void (*get_integerv)(unsigned pname, int *params);
   const unsigned char *(*get_string)(unsigned name);
} helpers;

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

/* A slot the build exports no _gl* name for was called (gl_slots.c). */
void
jackpotlte_gl_note_unsupported(unsigned offset)
{
   static unsigned noted;

   if (noted < 32) {
      say("slot %#06x has no name in this build: a call to it did nothing", offset);
      noted++;
   }
}

/*
 * Every entry of the table calls this before it passes its call on: the entry's
 * first argument is the context the caller means, and a GL call runs on the
 * thread where its context is current.  CoreAnimation's renderer is not always
 * the thread that made the context.
 */
void
jackpotlte_gl_use(void *context)
{
   struct jackpotlte_eagl *e = context;

   if (e != NULL && e->gl != NULL)
      jackpotlte_gl_make_current(e->gl);
}

static struct jackpotlte_eagl *
context_of(id object)
{
   unsigned i;

   for (i = 0; i < CONTEXTS; i++) {
      if (contexts[i] != NULL && contexts[i]->object == object)
         return contexts[i];
   }
   return NULL;
}

static int
resolve_helpers(void)
{
   if (helpers.get_integerv != NULL)
      return 1;
   helpers.get_integerv = (void (*)(unsigned, int *))
      jackpotlte_gl_proc("glGetIntegerv");
   helpers.get_string = (const unsigned char *(*)(unsigned))
      jackpotlte_gl_proc("glGetString");
   return helpers.get_integerv != NULL;
}

/* ---- EAGLContext --------------------------------------------------------- */

/* The one initialiser all three selectors end in. */
static id
make_context(id self, unsigned long api)
{
   struct jackpotlte_eagl *e;
   unsigned place;

   for (place = 0; place < CONTEXTS && contexts[place] != NULL; place++)
      ;
   if (api < 2 || api > 3 || place == CONTEXTS) {
      say("no context of API %lu (%u of %u in use)", api, place, (unsigned)CONTEXTS);
      return nil;
   }
   e = calloc(1, sizeof(*e));
   if (e == NULL)
      return nil;
   e->gl = jackpotlte_gl_create((int)api);
   if (e->gl == NULL || !jackpotlte_gl_make_current(e->gl)) {
      say("%s gives no OpenGL ES %lu context", jackpotlte_gl_library(), api);
      jackpotlte_gl_destroy(e->gl);
      free(e);
      return nil;
   }
   e->object = self;
   e->api = api;
   e->table[0] = e;
   jackpotlte_gl_fill_table(e->table);
   contexts[place] = e;

   if (resolve_helpers()) {
      say("an OpenGL ES %lu context for CoreAnimation by %s: %s; %s", api,
          jackpotlte_gl_library(), (const char *)helpers.get_string(GL_VERSION),
          (const char *)helpers.get_string(GL_RENDERER));
   } else {
      say("an OpenGL ES %lu context for CoreAnimation by %s; its glGetString is "
          "missing", api, jackpotlte_gl_library());
   }
   return self;
}

/*
 * Three ways in.  -initWithAPI:properties: is the one CoreAnimation's renderer
 * uses; the other two are the public ones, replaced so that a context made with
 * either is also one of ours - an object made by Apple's initialiser would have
 * none of this file's state, and -getMacroContextPrivate would answer NULL.
 */
static id
my_initWithAPI_properties(id self, SEL selector, unsigned long api, id properties)
{
   (void)selector;
   (void)properties;
   return make_context(self, api);
}

static id
my_initWithAPI(id self, SEL selector, unsigned long api)
{
   (void)selector;
   return make_context(self, api);
}

static id
my_initWithAPI_sharegroup(id self, SEL selector, unsigned long api, id sharegroup)
{
   (void)selector;
   (void)sharegroup;
   return make_context(self, api);
}

static unsigned long
my_API(id self, SEL selector)
{
   struct jackpotlte_eagl *e = context_of(self);

   (void)selector;
   return e != NULL ? e->api : 0;
}

/*
 * Nothing here shares a context with anybody, and this object has none of
 * Apple's ivars, so nil is the only answer that is true and safe. CoreImage
 * makes a CIContext over an EAGLContext by asking this first.
 */
static id
my_sharegroup(id self, SEL selector)
{
   static int said;

   (void)self;
   (void)selector;
   if (!said) {
      said = 1;
      say("-sharegroup asked for: nil (nothing here shares a context)");
   }
   return nil;
}

/* The value is not read: what the caller passes it as has not been established
 * on this build, and a guess would be a read through it. */
static unsigned long
my_setParameter(id self, SEL selector, unsigned long parameter, void *value)
{
   static unsigned noted;

   (void)self;
   (void)selector;
   if (noted < 16) {
      say("-setParameter:%#lx to:%p", parameter, value);
      noted++;
   }
   return 0;
}

static void *
my_getMacroContextPrivate(id self, SEL selector)
{
   struct jackpotlte_eagl *e = context_of(self);

   (void)selector;
   return e != NULL ? e->table : NULL;
}

/* The name of the texture bound to GL_TEXTURE_2D, or 0. */
static int
bound_texture(struct jackpotlte_eagl *e)
{
   int name = 0;

   if (!resolve_helpers() || !jackpotlte_gl_make_current(e->gl))
      return 0;
   helpers.get_integerv(GL_TEXTURE_BINDING_2D, &name);
   return name;
}

/*
 * The texture bound to the target is to be the surface's memory.  Returns NO
 * when the surface is not one this can be done for, and says why.
 */
static BOOL
surface_is_texture(struct jackpotlte_eagl *e, IOSurfaceRef surface,
                   unsigned long target, const char *method)
{
   uint32_t pixels = IOSurfaceGetPixelFormat(surface);
   int texture = bound_texture(e);
   static unsigned shown;

   if (target != GL_TEXTURE_2D) {
      say("%s: target %#lx is not GL_TEXTURE_2D", method, target);
      return NO;
   }
   if (texture == 0) {
      say("%s: no texture is bound", method);
      return NO;
   }
   if (pixels != 'BGRA' || IOSurfaceGetPlaneCount(surface) != 0 ||
       IOSurfaceGetBytesPerElement(surface) != 4) {
      say("%s: surface %u is '%.4s', %zu planes, %zu bytes a pixel: not a "
          "texture here", method, (unsigned)IOSurfaceGetID(surface),
          (const char *)&(uint32_t){ __builtin_bswap32(pixels) },
          IOSurfaceGetPlaneCount(surface), IOSurfaceGetBytesPerElement(surface));
      return NO;
   }
   if (!jackpotlte_gl_surface_texture(e->gl, (uint32_t)IOSurfaceGetID(surface)))
      return NO;
   if (shown++ < 16) {
      say("%s: texture %d is surface %u, %zu x %zu, memory shared", method,
          texture, (unsigned)IOSurfaceGetID(surface), IOSurfaceGetWidth(surface),
          IOSurfaceGetHeight(surface));
   }
   return YES;
}

/* (the last three arguments arrive on the stack: their sizes and order are the
 * method's own) */
static BOOL
my_texImageIOSurface(id self, SEL selector, IOSurfaceRef surface,
                     unsigned long target, unsigned long internal, int width,
                     int height, unsigned long format, unsigned long type,
                     unsigned int plane, BOOL invert)
{
   struct jackpotlte_eagl *e = context_of(self);

   (void)selector;
   (void)internal;
   (void)format;
   (void)type;
   (void)plane;
   (void)invert;
   (void)width;
   (void)height;
   if (e == NULL || surface == NULL)
      return NO;
   return surface_is_texture(e, surface, target, "-texImageIOSurface:");
}

static BOOL
my_attachImage(id self, SEL selector, unsigned long target, IOSurfaceRef surface,
               BOOL inverted)
{
   struct jackpotlte_eagl *e = context_of(self);

   (void)selector;
   (void)inverted;
   if (e == NULL || surface == NULL)
      return NO;
   return surface_is_texture(e, surface, target, "-attachImage:");
}

/*
 * objc_msgSendSuper, read as what is wanted here.  <objc/message.h> declares it
 * as `void objc_msgSendSuper(void)` - Apple's form, which says the arguments
 * are the caller's to know - so this is the one place the two are written out:
 * the declared function pointer, and the call.
 */
static void
send_to_super(struct objc_super *super, SEL selector)
{
   union {
      void (*as_declared)(void);
      void (*as_called)(struct objc_super *, SEL);
   } send;

   send.as_declared = objc_msgSendSuper;
   send.as_called(super, selector);
}

static void
my_dealloc(id self, SEL selector)
{
   struct objc_super super = { self, class_getSuperclass((Class)objc_getClass("EAGLContext")) };
   struct jackpotlte_eagl *e = context_of(self);
   unsigned i;

   if (e != NULL) {
      for (i = 0; i < CONTEXTS; i++) {
         if (contexts[i] == e)
            contexts[i] = NULL;
      }
      jackpotlte_gl_destroy(e->gl);
      free(e);
   }
   send_to_super(&super, selector);
}

/*
 * The methods and the implementations that replace them.  A union, not a cast:
 * the runtime is handed an IMP and the implementations have their own
 * signatures, and reading one function pointer as another is what this says.
 */
union method_prototype {
   IMP imp;
   id (*initialise)(id, SEL, unsigned long, id);          /* with properties or a sharegroup */
   id (*initialise_simple)(id, SEL, unsigned long);
   unsigned long (*api)(id, SEL);
   id (*sharegroup)(id, SEL);
   unsigned long (*parameter)(id, SEL, unsigned long, void *);
   void *(*macro_context)(id, SEL);
   BOOL (*image)(id, SEL, IOSurfaceRef, unsigned long, unsigned long, int, int,
                 unsigned long, unsigned long, unsigned int, BOOL);
   BOOL (*attach)(id, SEL, unsigned long, IOSurfaceRef, BOOL);
   void (*dispose)(id, SEL);
};

__attribute__((constructor)) static void
install(void)
{
   static const struct {
      const char *name;
      union method_prototype ours;
   } methods[] = {
      { "initWithAPI:properties:", { .initialise = my_initWithAPI_properties } },
      { "initWithAPI:", { .initialise_simple = my_initWithAPI } },
      { "initWithAPI:sharegroup:", { .initialise = my_initWithAPI_sharegroup } },
      { "API", { .api = my_API } },
      { "sharegroup", { .sharegroup = my_sharegroup } },
      { "setParameter:to:", { .parameter = my_setParameter } },
      { "getMacroContextPrivate", { .macro_context = my_getMacroContextPrivate } },
      { "texImageIOSurface:target:internalFormat:width:height:format:type:plane:invert:",
        { .image = my_texImageIOSurface } },
      { "attachImage:toCoreSurface:invertedRender:", { .attach = my_attachImage } },
      { "dealloc", { .dispose = my_dealloc } },
   };
   Class eagl = (Class)objc_getClass("EAGLContext");
   unsigned i, installed = 0;

   if (eagl == NULL) {
      say("no EAGLContext class: this library is loaded where OpenGL ES is not");
      return;
   }
   for (i = 0; i < sizeof(methods) / sizeof(methods[0]); i++) {
      SEL selector = sel_registerName(methods[i].name);
      Method method = class_getInstanceMethod(eagl, selector);

      /* Only methods that are there: the implementations are installed, and the
       * encodings stay the class's own.  A selector iOS does not have is noted,
       * not added with a guess at its signature. */
      if (method != NULL) {
         method_setImplementation(method, methods[i].ours.imp);
         installed++;
      } else {
         say("EAGLContext has no -%s", methods[i].name);
      }
   }
   say("EAGLContext is ours: %u of %u methods", installed,
       (unsigned)(sizeof(methods) / sizeof(methods[0])));
}
