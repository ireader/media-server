#include <math.h>
#include <time.h>
#include <stdlib.h>
#include <stdint.h>

#if defined(OS_WINDOWS) || defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#include <wincrypt.h>

uint32_t rtp_ssrc(void)
{
	uint32_t seed;
	HCRYPTPROV provider;

	seed = (uint32_t)rand();
	if (CryptAcquireContext(&provider, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | CRYPT_SILENT)) {
		CryptGenRandom(provider, sizeof(seed), (PBYTE)&seed);
		CryptReleaseContext(provider, 0);
	}
	return seed;
}

#elif defined(OS_LINUX) || defined(OS_MAC)
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

static int read_random(uint32_t *dst, const char *file)
{
	int fd = open(file, O_RDONLY);
	int err = -1;
	if (fd == -1)
		return -1;
	err = (int)read(fd, dst, sizeof(*dst));
	close(fd);
	return err;
}
uint32_t rtp_ssrc(void)
{
	uint32_t seed;
	if (read_random(&seed, "/dev/urandom") == sizeof(seed))
		return seed;
	if (read_random(&seed, "/dev/random") == sizeof(seed))
		return seed;
	return (uint32_t)rand();
}
#else

uint32_t rtp_ssrc(void)
{
	return rand();
}

#endif
