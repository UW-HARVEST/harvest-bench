/* Link-level shim for running the upstream tests against a single-threaded
 * library build.
 *
 * Upstream always compiles fuzzer.c / zstreamtest.c with ZSTD_MULTITHREAD and
 * unconditionally exercises the MT-only parameters (ZSTD_c_nbWorkers,
 * ZSTD_c_jobSize, ZSTD_c_overlapLog, ZSTD_c_rsyncable), including set/get
 * round-trip checks. A single-threaded libzstd rejects those parameters with
 * "Unsupported parameter", and the library under test here is single-threaded
 * by contract.
 *
 * This TU interposes the parameter entry points (a strong symbol in the
 * executable wins over the shared library) and emulates an MT build's
 * parameter STORAGE for those four parameters: values are recorded per
 * CCtx/CCtxParams object inside the shim and read back on get, while the
 * real library only ever sees the value 0 (synchronous mode). Creation,
 * free, and reset entry points are interposed as well so storage lifetime
 * follows object lifetime. Compressed-output semantics are unchanged; only
 * MT job scheduling goes unexercised. The #ifdef ZSTD_MULTITHREAD test
 * sections (threadPoolTests etc.) compile out instead.
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stddef.h>
#include <string.h>

#define ZSTD_STATIC_LINKING_ONLY
#include "zstd.h"

/* ---- per-object storage for the four MT parameters ---- */

#define MT_SLOTS 256

typedef struct {
    const void* obj;
    int nbWorkers, jobSize, overlapLog, rsyncable;
} mt_entry;

static mt_entry mt_table[MT_SLOTS];

static mt_entry* mt_find(const void* obj)
{
    int i;
    for (i = 0; i < MT_SLOTS; i++)
        if (mt_table[i].obj == obj) return &mt_table[i];
    return NULL;
}

static mt_entry* mt_find_or_add(const void* obj)
{
    mt_entry* e = mt_find(obj);
    if (e) return e;
    e = mt_find(NULL);
    if (!e) e = &mt_table[0]; /* overwrite arbitrarily; tests use few objects */
    memset(e, 0, sizeof(*e));
    e->obj = obj;
    return e;
}

static void mt_drop(const void* obj)
{
    mt_entry* e = mt_find(obj);
    if (e) memset(e, 0, sizeof(*e));
}

static int* mt_field(mt_entry* e, ZSTD_cParameter param)
{
    switch (param) {
        case ZSTD_c_nbWorkers:  return &e->nbWorkers;
        case ZSTD_c_jobSize:    return &e->jobSize;
        case ZSTD_c_overlapLog: return &e->overlapLog;
        case ZSTD_c_rsyncable:  return &e->rsyncable;
        default:                return NULL;
    }
}

static int is_mt_param(ZSTD_cParameter param)
{
    return param == ZSTD_c_nbWorkers || param == ZSTD_c_jobSize
        || param == ZSTD_c_overlapLog || param == ZSTD_c_rsyncable;
}

#define REAL(name, fn_t)                                        \
    static fn_t real;                                           \
    if (!real) real = (fn_t)dlsym(RTLD_NEXT, #name)

/* ---- parameter entry points ---- */

size_t ZSTD_CCtx_setParameter(ZSTD_CCtx* cctx, ZSTD_cParameter param, int value)
{
    typedef size_t (*fn_t)(ZSTD_CCtx*, ZSTD_cParameter, int);
    REAL(ZSTD_CCtx_setParameter, fn_t);
    if (is_mt_param(param)) {
        *mt_field(mt_find_or_add(cctx), param) = value;
        return real(cctx, param, 0);
    }
    return real(cctx, param, value);
}

size_t ZSTD_CCtxParams_setParameter(ZSTD_CCtx_params* params,
                                    ZSTD_cParameter param, int value)
{
    typedef size_t (*fn_t)(ZSTD_CCtx_params*, ZSTD_cParameter, int);
    REAL(ZSTD_CCtxParams_setParameter, fn_t);
    if (is_mt_param(param)) {
        *mt_field(mt_find_or_add(params), param) = value;
        return real(params, param, 0);
    }
    return real(params, param, value);
}

size_t ZSTD_CCtx_getParameter(const ZSTD_CCtx* cctx, ZSTD_cParameter param,
                              int* value)
{
    typedef size_t (*fn_t)(const ZSTD_CCtx*, ZSTD_cParameter, int*);
    REAL(ZSTD_CCtx_getParameter, fn_t);
    if (is_mt_param(param)) {
        mt_entry* e = mt_find(cctx);
        *value = e ? *mt_field(e, param) : 0;
        return 0;
    }
    return real(cctx, param, value);
}

size_t ZSTD_CCtxParams_getParameter(const ZSTD_CCtx_params* params,
                                    ZSTD_cParameter param, int* value)
{
    typedef size_t (*fn_t)(const ZSTD_CCtx_params*, ZSTD_cParameter, int*);
    REAL(ZSTD_CCtxParams_getParameter, fn_t);
    if (is_mt_param(param)) {
        mt_entry* e = mt_find(params);
        *value = e ? *mt_field(e, param) : 0;
        return 0;
    }
    return real(params, param, value);
}

/* ---- lifetime entry points: keep shim storage in sync ---- */

ZSTD_CCtx* ZSTD_createCCtx(void)
{
    typedef ZSTD_CCtx* (*fn_t)(void);
    REAL(ZSTD_createCCtx, fn_t);
    {   ZSTD_CCtx* const cctx = real();
        mt_drop(cctx);
        return cctx;
    }
}

size_t ZSTD_freeCCtx(ZSTD_CCtx* cctx)
{
    typedef size_t (*fn_t)(ZSTD_CCtx*);
    REAL(ZSTD_freeCCtx, fn_t);
    mt_drop(cctx);
    return real(cctx);
}

size_t ZSTD_CCtx_reset(ZSTD_CCtx* cctx, ZSTD_ResetDirective reset)
{
    typedef size_t (*fn_t)(ZSTD_CCtx*, ZSTD_ResetDirective);
    REAL(ZSTD_CCtx_reset, fn_t);
    if (reset == ZSTD_reset_parameters
        || reset == ZSTD_reset_session_and_parameters)
        mt_drop(cctx);
    return real(cctx, reset);
}

ZSTD_CCtx_params* ZSTD_createCCtxParams(void)
{
    typedef ZSTD_CCtx_params* (*fn_t)(void);
    REAL(ZSTD_createCCtxParams, fn_t);
    {   ZSTD_CCtx_params* const p = real();
        mt_drop(p);
        return p;
    }
}

size_t ZSTD_freeCCtxParams(ZSTD_CCtx_params* params)
{
    typedef size_t (*fn_t)(ZSTD_CCtx_params*);
    REAL(ZSTD_freeCCtxParams, fn_t);
    mt_drop(params);
    return real(params);
}

size_t ZSTD_CCtxParams_reset(ZSTD_CCtx_params* params)
{
    typedef size_t (*fn_t)(ZSTD_CCtx_params*);
    REAL(ZSTD_CCtxParams_reset, fn_t);
    mt_drop(params);
    return real(params);
}

size_t ZSTD_CCtxParams_init(ZSTD_CCtx_params* params, int compressionLevel)
{
    typedef size_t (*fn_t)(ZSTD_CCtx_params*, int);
    REAL(ZSTD_CCtxParams_init, fn_t);
    mt_drop(params);
    return real(params, compressionLevel);
}

size_t ZSTD_CCtxParams_init_advanced(ZSTD_CCtx_params* params,
                                     ZSTD_parameters zparams)
{
    typedef size_t (*fn_t)(ZSTD_CCtx_params*, ZSTD_parameters);
    REAL(ZSTD_CCtxParams_init_advanced, fn_t);
    mt_drop(params);
    return real(params, zparams);
}
