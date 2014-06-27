#include <stdlib.h>
#include "http-server.h"
#include "http-bundle.h"
#include "cstringext.h"
#include "sys/sync.h"
#include <memory.h>
#include <assert.h>

void* http_bundle_alloc(size_t sz)
{
	struct http_bundle_t *bundle;

	bundle = malloc(sizeof(struct http_bundle_t) + sz + 1);
	if(!bundle)
		return NULL;

	bundle->ref = 1;
	bundle->len = 0;
	bundle->ptr = bundle + 1;
	bundle->capacity = sz;

#if defined(_DEBUG)
	bundle->magic = 0xABCDEF10;
	((char*)bundle->ptr)[bundle->capacity] = 0xAB;
#endif

	return bundle;
}

int http_bundle_free(void* ptr)
{
	struct http_bundle_t *bundle;
	bundle = (struct http_bundle_t *)ptr;
	http_bundle_release(bundle);
	return 0;
}

void* http_bundle_lock(void* ptr)
{
	struct http_bundle_t *bundle;
	bundle = (struct http_bundle_t *)ptr;
	return bundle->ptr;
}

int http_bundle_unlock(void* ptr, size_t sz)
{
	struct http_bundle_t *bundle;
	bundle = (struct http_bundle_t *)ptr;
	assert(sz <= bundle->capacity);
	bundle->len = sz;
	return 0;
}

int http_bundle_addref(struct http_bundle_t *bundle)
{
    InterlockedIncrement(&bundle->ref);
    return 0;
}

int http_bundle_release(struct http_bundle_t *bundle)
{
	assert(bundle->len > 0);

#if defined(_DEBUG)
	assert(bundle->magic == 0xABCDEF10);
	assert(((char*)bundle->ptr)[bundle->capacity] == (char)0xAB);
#endif

	if(0 == InterlockedDecrement(&bundle->ref))
	{
		free(bundle);
	}

	return 0;
}
