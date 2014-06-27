#ifndef _http_bundle_h_
#define _http_bundle_h_

struct http_bundle_t
{
	long ref;

	size_t len;  // used size
	void* ptr;
    size_t capacity;

#if defined(_DEBUG)
	int magic;
#endif
};

int http_bundle_addref(struct http_bundle_t *bundle);
int http_bundle_release(struct http_bundle_t *bundle);

#endif /* !_http_bundle_h_ */
