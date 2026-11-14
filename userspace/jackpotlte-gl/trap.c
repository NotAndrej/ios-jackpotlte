/* SPDX-License-Identifier: MIT */
/*
 * The crash trap (bring-up aid).
 *
 * The kernel's crash report prints sig, the faulting pc/lr/sp/fp and a raw
 * frame-pointer walk, but neither the image an address is in nor that image's
 * base.  On the phone there is no atos and no way to take the report off the
 * device, so those numbers alone name nothing: `pc 0x1ef9e9e70` is only a
 * class of bug, not a place in a file.
 *
 * This library is injected into the display server (0159's jtexecinject), so
 * it can answer that question from inside the dying process: it installs a
 * SIGSEGV/SIGBUS/SIGILL handler that walks the loaded images, writes every
 * address of the fault out as "image + offset", then puts the signal back to
 * its default and raises it again, so the kernel's own report still follows.
 *
 * The output goes to stderr, which for a launchd daemon is the console - the
 * same place this library's other lines reach the panel.  A signal handler may
 * only call async-signal-safe functions, and snprintf is not one of them; it is
 * used anyway, because the alternative is a hand-written formatter and this
 * code runs once, in a process that is already dying.
 */
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ucontext.h>
#include <unistd.h>

#define FRAMES 24

/* Where `address` lives: the image's mapped __TEXT and `address - that`. */
static int
image_of(uintptr_t address, uintptr_t *base, uintptr_t *offset, const char **name)
{
   uint32_t i, n = _dyld_image_count();

   for (i = 0; i < n; i++) {
      const struct mach_header_64 *header =
          (const struct mach_header_64 *)_dyld_get_image_header(i);
      const char *where = _dyld_get_image_name(i);
      intptr_t slide = _dyld_get_image_vmaddr_slide(i);
      const uint8_t *command, *end;

      if (header == NULL || where == NULL || header->magic != MH_MAGIC_64) {
         continue;
      }
      command = (const uint8_t *)(header + 1);
      end = command + header->sizeofcmds;
      while (command + sizeof(struct load_command) <= end) {
         const struct load_command *lc = (const struct load_command *)command;

         if (lc->cmd == LC_SEGMENT_64) {
            const struct segment_command_64 *seg =
                (const struct segment_command_64 *)command;

            if (memcmp(seg->segname, "__TEXT", 7) == 0) {
               uintptr_t start = (uintptr_t)seg->vmaddr + (uintptr_t)slide;
               uintptr_t length = (uintptr_t)seg->vmsize;

               if (address >= start && address - start < length) {
                  *base = start;
                  *offset = address - start;
                  *name = where;
                  return 1;
               }
               break;          /* one __TEXT a Mach-O */
            }
         }
         if (lc->cmdsize < sizeof(*lc)) {
            break;
         }
         command += lc->cmdsize;
      }
   }
   return 0;
}

static void
emit(const char *label, int index, uintptr_t address)
{
   uintptr_t base = 0, offset = 0;
   const char *name = NULL;
   const char *leaf = NULL;
   char line[400];
   int length;

   if (address != 0 && image_of(address, &base, &offset, &name)) {
      leaf = strrchr(name, '/');
      leaf = leaf != NULL ? leaf + 1 : name;
      length = snprintf(line, sizeof(line),
          "JTTRAP %s %u %#llx = %s + %#llx (base %#llx)\n", label, index,
          (unsigned long long)address, leaf, (unsigned long long)offset,
          (unsigned long long)base);
   } else {
      length = snprintf(line, sizeof(line), "JTTRAP %s %u %#llx = no image\n",
          label, index, (unsigned long long)address);
   }
   if (length > 0) {
      ssize_t wrote = write(2, line,
          (size_t)(length < (int)sizeof(line) ? length : (int)sizeof(line) - 1));

      (void)wrote;
   }
}

static void
trap(int sig, siginfo_t *info, void *context)
{
   ucontext_t *uc = (ucontext_t *)context;
   uint64_t pc = 0, far = 0, fp = 0;
   struct sigaction restore;
   int frame;

   if (uc != NULL) {
      pc = (uint64_t)uc->uc_mcontext->__ss.__pc;
      fp = (uint64_t)uc->uc_mcontext->__ss.__fp;
      emit("lr", 0, (uintptr_t)uc->uc_mcontext->__ss.__lr);
      emit("sp", 0, (uintptr_t)uc->uc_mcontext->__ss.__sp);
   }
   far = (uint64_t)(uintptr_t)(info != NULL ? info->si_addr : NULL);
   emit("far", 0, (uintptr_t)far);
   emit("pc", 0, (uintptr_t)pc);

   /* AArch64 frame records: [fp] is the caller's fp, [fp + 8] its return
    * address.  Both must move up the stack for the walk to make sense. */
   for (frame = 1; frame <= FRAMES && fp >= 0x1000; frame++) {
      uint64_t next = ((const uint64_t *)(uintptr_t)fp)[0];
      uint64_t ret = ((const uint64_t *)(uintptr_t)fp)[1];

      emit("frame", frame, (uintptr_t)ret);
      if (next <= fp || (next & 0xf) != 0) {
         break;
      }
      fp = next;
   }

   memset(&restore, 0, sizeof(restore));
   restore.sa_handler = SIG_DFL;
   sigemptyset(&restore.sa_mask);
   (void)sigaction(sig, &restore, NULL);
   (void)raise(sig);
   /* If raise() came back, the default action was not the death this signal
    * normally causes; returning would re-run the faulting instruction, so
    * wait instead and let the kernel's report come from another thread. */
   for (;;) {
      pause();
   }
}

__attribute__((constructor)) static void
install_trap(void)
{
   static char alternate[64 * 1024];
   stack_t stack;
   struct sigaction action;
   int signals[] = { SIGSEGV, SIGBUS, SIGILL };
   unsigned i;

   stack.ss_sp = alternate;
   stack.ss_size = sizeof(alternate);
   stack.ss_flags = 0;
   (void)sigaltstack(&stack, NULL);

   memset(&action, 0, sizeof(action));
   action.sa_sigaction = trap;
   action.sa_flags = SA_SIGINFO | SA_ONSTACK;
   sigemptyset(&action.sa_mask);
   for (i = 0; i < sizeof(signals) / sizeof(signals[0]); i++) {
      (void)sigaction(signals[i], &action, NULL);
   }
}
