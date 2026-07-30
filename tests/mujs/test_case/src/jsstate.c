#include "jsi.h"

#include <assert.h>

static int js_ptry(js_State *J) {
	if (J->trytop == JS_TRYLIMIT) {
		J->stack[J->top].t.type = JS_TLITSTR;
		J->stack[J->top].u.litstr = "exception stack overflow";
		++J->top;
		return 1;
	}
	return 0;
}

static void *js_defaultalloc(void *actx, void *ptr, int size)
{
	if (size == 0) {
		free(ptr);
		return NULL;
	}
	return realloc(ptr, (size_t)size);
}

static void js_defaultreport(js_State *J, const char *message)
{
	fputs(message, stderr);
	fputc('\n', stderr);
}

static void js_defaultpanic(js_State *J)
{
	js_report(J, "uncaught exception");
	/* return to javascript to abort */
}

int js_ploadstring(js_State *J, const char *filename, const char *source)
{
	if (js_ptry(J))
		return 1;
	if (js_try(J))
		return 1;
	js_loadstring(J, filename, source);
	js_endtry(J);
	return 0;
}

const char *js_trystring(js_State *J, int idx, const char *error)
{
	const char *s;
	if (js_ptry(J)) {
		js_pop(J, 1);
		return error;
	}
	if (js_try(J)) {
		js_pop(J, 1);
		return error;
	}
	s = js_tostring(J, idx);
	js_endtry(J);
	return s;
}

double js_trynumber(js_State *J, int idx, double error)
{
	double v;
	if (js_ptry(J)) {
		js_pop(J, 1);
		return error;
	}
	if (js_try(J)) {
		js_pop(J, 1);
		return error;
	}
	v = js_tonumber(J, idx);
	js_endtry(J);
	return v;
}

int js_tryinteger(js_State *J, int idx, int error)
{
	int v;
	if (js_ptry(J)) {
		js_pop(J, 1);
		return error;
	}
	if (js_try(J)) {
		js_pop(J, 1);
		return error;
	}
	v = js_tointeger(J, idx);
	js_endtry(J);
	return v;
}

int js_tryboolean(js_State *J, int idx, int error)
{
	int v;
	if (js_ptry(J)) {
		js_pop(J, 1);
		return error;
	}
	if (js_try(J)) {
		js_pop(J, 1);
		return error;
	}
	v = js_toboolean(J, idx);
	js_endtry(J);
	return v;
}

static void js_loadstringx(js_State *J, const char *filename, const char *source, int iseval)
{
	js_Ast *P;
	js_Function *F;

	if (js_try(J)) {
		jsP_freeparse(J);
		js_throw(J);
	}

	P = jsP_parse(J, filename, source);
	F = jsC_compilescript(J, P, iseval ? J->strict : J->default_strict);
	jsP_freeparse(J);
	js_newscript(J, F, iseval ? (J->strict ? J->E : NULL) : J->GE);

	js_endtry(J);
}

void js_loadeval(js_State *J, const char *filename, const char *source)
{
	js_loadstringx(J, filename, source, 1);
}

void js_loadstring(js_State *J, const char *filename, const char *source)
{
	js_loadstringx(J, filename, source, 0);
}

int js_dostring(js_State *J, const char *source)
{
	if (js_ptry(J)) {
		js_report(J, "exception stack overflow");
		js_pop(J, 1);
		return 1;
	}
	if (js_try(J)) {
		js_report(J, js_trystring(J, -1, "Error"));
		js_pop(J, 1);
		return 1;
	}
	js_loadstring(J, "[string]", source);
	js_pushundefined(J);
	js_call(J, 0);
	js_pop(J, 1);
	js_endtry(J);
	return 0;
}

js_Panic js_atpanic(js_State *J, js_Panic panic)
{
	js_Panic old = J->panic;
	J->panic = panic;
	return old;
}

void js_report(js_State *J, const char *message)
{
	if (J->report)
		J->report(J, message);
}

void js_setreport(js_State *J, js_Report report)
{
	J->report = report;
}

void js_setcontext(js_State *J, void *uctx)
{
	J->uctx = uctx;
}

void *js_getcontext(js_State *J)
{
	return J->uctx;
}

js_State *js_newstate(js_Alloc alloc, void *actx, int flags)
{
	js_State *J;

	assert(sizeof(js_Value) == 16);
	assert(soffsetof(js_Value, t.type) == 15);

	if (!alloc)
		alloc = js_defaultalloc;

	J = alloc(actx, NULL, sizeof *J);
	if (!J)
		return NULL;
	memset(J, 0, sizeof(*J));
	J->actx = actx;
	J->alloc = alloc;

	if (flags & JS_STRICT)
		J->strict = J->default_strict = 1;

	J->trace[0].name = "-top-";
	J->trace[0].file = "native";
	J->trace[0].line = 0;

	J->report = js_defaultreport;
	J->panic = js_defaultpanic;

	J->stack = alloc(actx, NULL, JS_STACKSIZE * sizeof *J->stack);
	if (!J->stack) {
		alloc(actx, J, 0);
		return NULL;
	}

	J->gcmark = 1;
	J->nextref = 0;
	J->gcthresh = 0; /* reaches stability within ~ 2-5 GC cycles */

	if (js_try(J)) {
		js_freestate(J);
		return NULL;
	}

	J->R = jsV_newobject(J, JS_COBJECT, NULL);
	J->G = jsV_newobject(J, JS_COBJECT, NULL);
	J->E = jsR_newenvironment(J, J->G, NULL);
	J->GE = J->E;

	jsB_init(J);

	js_endtry(J);
	return J;
}
