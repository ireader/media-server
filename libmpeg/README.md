#PS packer

#TS packer
<pre><code>
struct mpeg_ts_func_t h;
h.alloc = ts_alloc;
h.write = ts_write;
h.free = ts_free;

void* ts = mpeg_ts_create(&h, fp);

while(1)
{
	mpeg_ts_write(param, avtype, pts, dts, data, bytes);
}

mpeg_ts_destroy(ts);
</code></pre>
