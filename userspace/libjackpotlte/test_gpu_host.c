/* SPDX-License-Identifier: MIT */
/*
 * test_gpu_host: check the request layer against the ABI, on a build machine.
 *
 * There is no GPU here and no IOKit: a fake backend records the selector, the
 * size and the bytes that jt_gpu hands it, and answers the way 0154 does.  So
 * this checks the half of M4 that can be checked without the phone - that the
 * right request carries the right structure with the right fields - and it
 * runs on Linux, so it also runs in the userspace CI job.
 *
 * It is not a test of the kernel.  "Green" here means the layer asks correctly,
 * nothing about whether the GPU answers; that needs the phone.
 */
#include "jackpotlte_gpu.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

static unsigned failures;

#define CHECK(cond, ...) do {                                   \
	if (!(cond)) {                                          \
		failures++;                                     \
		printf("FAIL %s:%d: ", __func__, __LINE__);     \
		printf(__VA_ARGS__);                            \
		printf("\n");                                   \
	}                                                       \
} while (0)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * The fake IOKit.
 */
struct fake {
	unsigned calls;
	uint32_t selector;
	size_t   size;
	unsigned char sent[64];         /* what the layer sent, before any reply */
	uint32_t kr;                    /* what call() answers with */
	uint64_t job_seq;               /* what the "kernel" gives the next job */

	unsigned maps, unmaps, closes;
	uint32_t unmap_handle;          /* the type the unmap came back with */
	void    *map_address;
	uint64_t map_size;
	uint32_t map_kr;

	/* Fill in the kernel's answer for this selector, and only this one: a
	 * reply armed for another selector would write its own structure's
	 * bytes into a request of a different size. */
	void (*reply)(struct fake *f, unsigned char *data, size_t size);
	uint32_t reply_selector;
};

static uint32_t
fake_call(void *ctx, uint32_t selector, void *data, size_t size)
{
	struct fake *f = ctx;

	f->calls++;
	f->selector = selector;
	f->size = size;
	if (size <= sizeof(f->sent)) {
		memcpy(f->sent, data, size);
	}
	if (f->kr == JT_IORETURN_SUCCESS) {
		if (f->reply != NULL && f->selector == f->reply_selector) {
			f->reply(f, data, size);
		}
		/* A submit always comes back with the job's number in `jc`, 0 when
		 * the kernel refused it (0154:541) - so the layer can read 0 as
		 * "not queued" rather than the chain it sent. */
		if (f->selector == JT_PANFROST_SUBMIT && size >= sizeof(struct jt_submit_req)) {
			((struct jt_submit_req *)data)->jc = f->job_seq;
		}
	}
	return f->kr;
}

static uint32_t
fake_map(void *ctx, uint32_t handle, void **address, uint64_t *size)
{
	struct fake *f = ctx;

	f->maps++;
	f->selector = handle;
	if (f->map_kr != JT_IORETURN_SUCCESS) {
		return f->map_kr;
	}
	*address = f->map_address;
	*size = f->map_size;
	return JT_IORETURN_SUCCESS;
}

static void
fake_unmap(void *ctx, uint32_t handle, void *address, uint64_t size)
{
	struct fake *f = ctx;

	(void)address;
	(void)size;
	f->unmaps++;
	f->unmap_handle = handle;
}

static void
fake_close(void *ctx)
{
	struct fake *f = ctx;

	f->closes++;
}

static const struct jt_backend fake_backend = {
	fake_call, fake_map, fake_unmap, fake_close,
};

/* One fake at a time - the tests run one after another, and the layer holds
 * the pointer, so it has to outlive any open. */
static struct fake G;

/* Arm the fake's answer for one selector. */
static void
arm(struct fake *f, uint32_t selector,
    void (*reply)(struct fake *, unsigned char *, size_t))
{
	f->reply = reply;
	f->reply_selector = selector;
}

static struct jt_gpu *
open_with(struct fake **fp)
{
	struct jt_gpu *g = NULL;

	memset(&G, 0, sizeof(G));
	G.kr = JT_IORETURN_SUCCESS;
	G.map_kr = JT_IORETURN_SUCCESS;
	CHECK(jt_gpu_open(&g, &fake_backend, &G) == 0, "open failed");
	*fp = &G;
	return g;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * The kernel's answers.
 */
static void
reply_create(struct fake *f, unsigned char *data, size_t size)
{
	struct jt_create_bo_req *r = (struct jt_create_bo_req *)data;

	(void)f;
	(void)size;
	r->handle = 7;
	r->offset = 0x01000000;         /* JT_FIRST_JOB_GPU: where 0153 starts */
}

static void
reply_param(struct fake *f, unsigned char *data, size_t size)
{
	struct jt_get_param_req *r = (struct jt_get_param_req *)data;

	(void)f;
	(void)size;
	r->value = 0x7010;              /* a plausible id */
}

static void
reply_offset(struct fake *f, unsigned char *data, size_t size)
{
	struct jt_get_bo_offset_req *r = (struct jt_get_bo_offset_req *)data;

	(void)f;
	(void)size;
	r->offset = 0x02000000;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static void
test_open(void)
{
	struct jt_gpu *g = NULL;
	struct fake *f = NULL;
	struct jt_backend broken = fake_backend;

	CHECK(jt_gpu_open(&g, NULL, NULL) == -EINVAL, "a NULL backend was accepted");
	broken.map = NULL;
	CHECK(jt_gpu_open(&g, &broken, NULL) == -EINVAL, "a backend without map was accepted");

	g = open_with(&f);
	CHECK(g != NULL, "open_with returned nothing");
	CHECK(f->closes == 0, "the connection was closed before it was used");
	if (g != NULL) {
		jt_gpu_close(g);
		CHECK(f->closes == 1, "close did not reach the backend");
	}
}

static void
test_create_bo(void)
{
	struct fake *f;
	struct jt_gpu *g = open_with(&f);
	struct jt_create_bo_req sent;
	uint32_t handle = 0;
	uint64_t address = 0;

	arm(f, JT_PANFROST_CREATE_BO, reply_create);
	CHECK(jt_bo_create(g, 16384, &handle, &address) == 0, "create_bo failed");
	memcpy(&sent, f->sent, sizeof(sent));
	CHECK(f->selector == JT_PANFROST_CREATE_BO, "selector %#x, not create_bo", f->selector);
	CHECK(f->size == sizeof(struct jt_create_bo_req), "sent %zu bytes", f->size);
	CHECK(sent.size == 16384, "asked for %u bytes", sent.size);
	CHECK(handle == 7, "handle %u", handle);
	CHECK(address == 0x01000000, "address %#llx", (unsigned long long)address);

	/* A buffer with no address is not usable and must not look like success. */
	f->reply = NULL;
	CHECK(jt_bo_create(g, 4096, &handle, &address) == -EIO,
	    "a handle of 0 was accepted");
	CHECK(jt_bo_create(g, 0, &handle, &address) == -EINVAL, "size 0 was accepted");

	/* The kernel's refusals arrive as errno. */
	f->kr = JT_IORETURN_NOT_READY;
	CHECK(jt_bo_create(g, 4096, &handle, &address) == -ENODEV,
	    "not-ready did not become -ENODEV");
	jt_gpu_close(g);
}

static void
test_map(void)
{
	struct fake *f;
	struct jt_gpu *g = open_with(&f);
	static int memory[4];
	void *address = NULL;
	uint64_t size = 0;

	f->map_address = memory;
	f->map_size = sizeof(memory);
	CHECK(jt_bo_map(g, 3, &address, &size) == 0, "map failed");
	CHECK(f->maps == 1, "%u maps", f->maps);
	CHECK(f->selector == 3, "the client-memory type was %u, not the handle", f->selector);
	CHECK(address == memory && size == sizeof(memory), "address/size wrong");
	CHECK(jt_bo_unmap(g, 3, address, size) == 0, "unmap failed");
	CHECK(f->unmaps == 1, "%u unmaps", f->unmaps);
	CHECK(f->unmap_handle == 3, "the unmap carried type %u, not the handle", f->unmap_handle);

	f->map_kr = JT_IORETURN_NOT_FOUND;
	CHECK(jt_bo_map(g, 4, &address, &size) == -ENOENT,
	    "a missing buffer did not become -ENOENT");
	jt_gpu_close(g);
}

static void
test_submit(void)
{
	struct fake *f;
	struct jt_gpu *g = open_with(&f);
	uint32_t handles[2] = { 7, 8 };
	struct jt_submit_req sent;
	uint64_t seq;
	int error = 0;

	f->job_seq = 42;                        /* the job the "kernel" queues */
	seq = jt_submit(g, 0x01000000, true, handles, 2, &error);
	memcpy(&sent, f->sent, sizeof(sent));
	CHECK(error == 0, "submit reported %d", error);
	CHECK(seq == 42, "job number %llu", (unsigned long long)seq);
	CHECK(f->selector == JT_PANFROST_SUBMIT, "selector %#x", f->selector);
	CHECK(f->size == sizeof(struct jt_submit_req), "sent %zu bytes", f->size);
	CHECK(sent.jc == 0x01000000, "chain %#llx", (unsigned long long)sent.jc);
	CHECK(sent.bo_handles == (uint64_t)(uintptr_t)handles,
	    "the handle array did not travel by address");
	CHECK(sent.bo_handle_count == 2, "count %u", sent.bo_handle_count);
	CHECK((sent.requirements & JT_JD_REQ_FS) != 0, "the fragment bit is missing");

	/* A vertex job must not carry the fragment bit. */
	(void)jt_submit(g, 0x01000000, false, NULL, 0, &error);
	memcpy(&sent, f->sent, sizeof(sent));
	CHECK((sent.requirements & JT_JD_REQ_FS) == 0, "a vertex job asked to be a fragment job");
	CHECK(sent.bo_handle_count == 0 && sent.bo_handles == 0, "no-handle submit is not empty");

	/* The kernel answering "no job number" is a failure, not job 0. */
	f->job_seq = 0;
	seq = jt_submit(g, 0x01000000, false, NULL, 0, &error);
	CHECK(seq == 0 && error == -EIO, "an answer of 0 was taken as a job (error %d, seq %llu)",
	    error, (unsigned long long)seq);

	f->kr = JT_IORETURN_UNSUPPORTED;
	(void)jt_submit(g, 0x01000000, false, NULL, 0, &error);
	CHECK(error == -ENOTTY, "an unsupported submit became %d", error);
	jt_gpu_close(g);
}

static void
test_waits(void)
{
	struct fake *f;
	struct jt_gpu *g = open_with(&f);
	struct jt_wait_bo_req bo;
	struct jt_wait_req seqreq;

	CHECK(jt_wait_bo(g, 5, 1000000000) == 0, "wait_bo failed");
	memcpy(&bo, f->sent, sizeof(bo));
	CHECK(f->selector == JT_PANFROST_WAIT_BO, "selector %#x", f->selector);
	CHECK(bo.handle == 5 && bo.timeout_ns == 1000000000, "wait_bo fields wrong");

	CHECK(jt_wait_seq(g, 42, 2000000000) == 0, "wait_seq failed");
	memcpy(&seqreq, f->sent, sizeof(seqreq));
	CHECK(f->selector == JT_WAIT, "selector %#x", f->selector);
	CHECK(seqreq.seq == 42 && seqreq.timeout_ns == 2000000000, "wait_seq fields wrong");

	f->kr = JT_IORETURN_TIMEOUT;
	CHECK(jt_wait_seq(g, 42, 1) == -ETIMEDOUT, "a timeout did not become -ETIMEDOUT");
	jt_gpu_close(g);
}

static void
test_param_and_release(void)
{
	struct fake *f;
	struct jt_gpu *g = open_with(&f);
	uint64_t value = 0;
	struct jt_get_param_req param;
	struct jt_gem_close close_req;

	arm(f, JT_PANFROST_GET_PARAM, reply_param);
	CHECK(jt_get_param(g, JT_PARAM_GPU_ID_HI, &value) == 0, "get_param failed");
	memcpy(&param, f->sent, sizeof(param));
	CHECK(param.param == JT_PARAM_GPU_ID_HI, "param %u", param.param);
	CHECK(value == 0x7010, "value %#llx", (unsigned long long)value);

	arm(f, JT_PANFROST_GET_BO_OFFSET, reply_offset);
	CHECK(jt_bo_offset(g, 7, &value) == 0 && value == 0x02000000, "get_bo_offset wrong");

	CHECK(jt_bo_release(g, 7) == 0, "release failed");
	memcpy(&close_req, f->sent, sizeof(close_req));
	CHECK(f->selector == JT_DRM_GEM_CLOSE, "selector %#x", f->selector);
	CHECK(close_req.handle == 7, "closed handle %u", close_req.handle);

	f->kr = JT_IORETURN_NOT_FOUND;
	CHECK(jt_bo_release(g, 9) == -ENOENT, "a missing handle did not become -ENOENT");
	jt_gpu_close(g);
}

static void
test_surface(void)
{
	struct fake *f;
	struct jt_gpu *g = open_with(&f);
	struct jt_surface_req info;
	struct jt_surface_req sent;
	uint32_t handle = JT_SURFACE_DESCRIBE;

	/* Describe only: no buffer is wanted, so the describe value travels. */
	f->reply = NULL;
	CHECK(jt_surface_buffer(g, 12, &handle, &info) == 0, "surface describe failed");
	memcpy(&sent, f->sent, sizeof(sent));
	CHECK(f->selector == JT_SURFACE, "selector %#x", f->selector);
	CHECK(sent.surface == 12 && sent.handle == JT_SURFACE_DESCRIBE,
	    "surface %u handle %#x", sent.surface, sent.handle);
	jt_gpu_close(g);
}

static void
test_errno_table(void)
{
	CHECK(jt_errno(JT_IORETURN_SUCCESS) == 0, "success");
	CHECK(jt_errno(JT_IORETURN_BAD_ARGUMENT) == -EINVAL, "bad argument");
	CHECK(jt_errno(JT_IORETURN_UNSUPPORTED) == -ENOTTY, "unsupported");
	CHECK(jt_errno(JT_IORETURN_NOT_READY) == -ENODEV, "not ready");
	CHECK(jt_errno(JT_IORETURN_TIMEOUT) == -ETIMEDOUT, "timeout");
}

int
main(void)
{
	test_open();
	test_create_bo();
	test_map();
	test_submit();
	test_waits();
	test_param_and_release();
	test_surface();
	test_errno_table();

	if (failures != 0) {
		printf("FAIL: %u check%s failed\n", failures, failures == 1 ? "" : "s");
		return 1;
	}
	printf("PASS: the request layer asks 0154 what 0154 expects\n");
	return 0;
}