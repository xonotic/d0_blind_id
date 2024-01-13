/*
 * FILE:	d0_bignum-openssl.c
 * AUTHOR:	Rudolf Polzer - divVerent@xonotic.org
 * 
 * Copyright (c) 2010, Rudolf Polzer
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTOR(S) ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTOR(S) BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Format:commit %H$
 * $Id$
 */

/* NOTE: this file links against openssl (http://www.openssl.org), which is
 * under the OpenSSL License. You may have to abide to its terms too if you use
 * this file.
 * To alternatively link to GMP, provide the option --without-openssl to
 * ./configure.
 */

#include "d0_bignum.h"

#include <assert.h>
#include <string.h>
#include <openssl/bn.h>
#include <openssl/rand.h>


struct d0_bignum_s
{
	BIGNUM *z;
};

static d0_bignum_t temp;
static BN_CTX *ctx;
static unsigned char numbuf[65536];
static void *tempmutex = NULL; // hold this mutex when using ctx or temp or numbuf

#include <time.h>
#include <stdio.h>

static void **locks;

void locking_function(int mode, int l, const char *file, int line)
{
	void *m = locks[l];
	if(mode & CRYPTO_LOCK)
		d0_lockmutex(m);
	else
		d0_unlockmutex(m);
}

typedef struct CRYPTO_dynlock_value
{
	void *m;
};

struct CRYPTO_dynlock_value *dyn_create_function(const char *file, int line)
{
	return (struct CRYPTO_dynlock_value *) d0_createmutex();
}

void dyn_lock_function(int mode, struct CRYPTO_dynlock_value *l, const char *file, int line)
{
	void *m = (void *) l;
	if(mode & CRYPTO_LOCK)
		d0_lockmutex(m);
	else
		d0_unlockmutex(m);
}

void dyn_destroy_function(struct CRYPTO_dynlock_value *l, const char *file, int line)
{
	void *m = (void *) l;
	d0_destroymutex(l);
}

D0_WARN_UNUSED_RESULT D0_BOOL d0_bignum_INITIALIZE(void)
{
	FILE *f;
	D0_BOOL ret = 1;
	unsigned char buf[256];
	int i, n;

	tempmutex = d0_createmutex();
	d0_lockmutex(tempmutex);

	n = CRYPTO_num_locks();
	locks = d0_malloc(n * sizeof(*locks));
	for(i = 0; i < n; ++i)
		locks[i] = d0_createmutex();

	CRYPTO_set_locking_callback(locking_function);
	CRYPTO_set_dynlock_create_callback(dyn_create_function);
	CRYPTO_set_dynlock_lock_callback(dyn_lock_function);
	CRYPTO_set_dynlock_destroy_callback(dyn_destroy_function);

	ctx = BN_CTX_new();
	d0_bignum_init(&temp);

#ifdef WIN32
	{
		HCRYPTPROV hCryptProv;
		if(CryptAcquireContext(&hCryptProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
		{
			if(!CryptGenRandom(hCryptProv, sizeof(buf), (PBYTE) &buf[0]))
			{
				fprintf(stderr, "WARNING: could not initialize random number generator (CryptGenRandom failed)\n");
				ret = 0;
			}
			CryptReleaseContext(hCryptProv, 0);
		}
		else if(CryptAcquireContext(&hCryptProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | CRYPT_NEWKEYSET))
		{
			if(!CryptGenRandom(hCryptProv, sizeof(buf), (PBYTE) &buf[0]))
			{
				fprintf(stderr, "WARNING: could not initialize random number generator (CryptGenRandom failed)\n");
				ret = 0;
			}
			CryptReleaseContext(hCryptProv, 0);
		}
		else
		{
			fprintf(stderr, "WARNING: could not initialize random number generator (CryptAcquireContext failed)\n");
			ret = 0;
		}
	}
#else
	f = fopen("/dev/urandom", "rb");
	if(!f)
		f = fopen("/dev/random", "rb");
	if(f)
	{
		setbuf(f, NULL);
		if(fread(buf, sizeof(buf), 1, f) != 1)
		{
			fprintf(stderr, "WARNING: could not initialize random number generator (read from random device failed)\n");
			ret = 0;
		}
		fclose(f);
	}
	else
	{
		fprintf(stderr, "WARNING: could not initialize random number generator (no random device found)\n");
		ret = 0;
	}
#endif
	RAND_add(buf, sizeof(buf), sizeof(buf));

	d0_unlockmutex(tempmutex);

	return 1;
	// FIXME initialize the RNG on Windows on UNIX it is done right already
}

void d0_bignum_SHUTDOWN(void)
{
	int i, n;

	d0_lockmutex(tempmutex);

	d0_bignum_clear(&temp);
	BN_CTX_free(ctx);
	ctx = NULL;

	n = CRYPTO_num_locks();
	for(i = 0; i < n; ++i)
		d0_destroymutex(locks[i]);
	d0_free(locks);

	d0_unlockmutex(tempmutex);
	d0_destroymutex(tempmutex);
	tempmutex = NULL;
}

D0_BOOL d0_iobuf_write_bignum(d0_iobuf_t *buf, const d0_bignum_t *bignum)
{
	D0_BOOL ret;
	size_t count = 0;

	d0_lockmutex(tempmutex);
	numbuf[0] = BN_is_zero(bignum->z) ? 0 : BN_is_negative(bignum->z) ? 3 : 1;
	if((numbuf[0] & 3) != 0) // nonzero
	{
		count = BN_num_bytes(bignum->z);
		if(count > sizeof(numbuf) - 1)
		{
			d0_unlockmutex(tempmutex);
			return 0;
		}
		BN_bn2bin(bignum->z, numbuf+1);
	}
	ret = d0_iobuf_write_packet(buf, numbuf, count + 1);
	d0_unlockmutex(tempmutex);
	return ret;
}

d0_bignum_t *d0_iobuf_read_bignum(d0_iobuf_t *buf, d0_bignum_t *bignum)
{
	size_t count = sizeof(numbuf);

	d0_lockmutex(tempmutex);
	if(!d0_iobuf_read_packet(buf, numbuf, &count))
	{
		d0_unlockmutex(tempmutex);
		return NULL;
	}
	if(count < 1)
	{
		d0_unlockmutex(tempmutex);
		return NULL;
	}
	if(!bignum)
		bignum = d0_bignum_new();
	if(!bignum)
	{
		d0_unlockmutex(tempmutex);
		return NULL;
	}
	if(numbuf[0] & 3) // nonzero
	{
		BN_bin2bn(numbuf+1, count-1, bignum->z);
		if(numbuf[0] & 2) // negative
			BN_set_negative(bignum->z, 1);
	}
	else // zero
	{
		BN_zero(bignum->z);
	}
	d0_unlockmutex(tempmutex);
	return bignum;
}

ssize_t d0_bignum_export_unsigned(const d0_bignum_t *bignum, void *buf, size_t bufsize)
{
	size_t count;
	count = BN_num_bytes(bignum->z);
	if(count > bufsize)
		return -1;
	if(bufsize > count)
	{
		// pad from left (big endian numbers!)
		memset(buf, 0, bufsize - count);
		buf += bufsize - count;
	}
	BN_bn2bin(bignum->z, buf);
	return bufsize;
}

d0_bignum_t *d0_bignum_import_unsigned(d0_bignum_t *bignum, const void *buf, size_t bufsize)
{
	size_t count;
	if(!bignum) bignum = d0_bignum_new(); if(!bignum) return NULL;
	BN_bin2bn(buf, bufsize, bignum->z);
	return bignum;
}

d0_bignum_t *d0_bignum_new(void)
{
	d0_bignum_t *b = d0_malloc(sizeof(d0_bignum_t));
	b->z = BN_new();
	return b;
}

void d0_bignum_free(d0_bignum_t *a)
{
	BN_free(a->z);
	d0_free(a);
}

void d0_bignum_init(d0_bignum_t *b)
{
	b->z = BN_new();
}

void d0_bignum_clear(d0_bignum_t *a)
{
	BN_free(a->z);
}

size_t d0_bignum_size(const d0_bignum_t *r)
{
	return BN_num_bits(r->z);
}

int d0_bignum_cmp(const d0_bignum_t *a, const d0_bignum_t *b)
{
	return BN_cmp(a->z, b->z);
}

d0_bignum_t *d0_bignum_rand_range(d0_bignum_t *r, const d0_bignum_t *min, const d0_bignum_t *max)
{
	if(!r) r = d0_bignum_new(); if(!r) return NULL;
	d0_lockmutex(tempmutex);
	BN_sub(temp.z, max->z, min->z);
	BN_rand_range(r->z, temp.z);
	d0_unlockmutex(tempmutex);
	BN_add(r->z, r->z, min->z);
	return r;
}

d0_bignum_t *d0_bignum_rand_bit_atmost(d0_bignum_t *r, size_t n)
{
	if(!r) r = d0_bignum_new(); if(!r) return NULL;
	BN_rand(r->z, n, -1, 0);
	return r;
}

d0_bignum_t *d0_bignum_rand_bit_exact(d0_bignum_t *r, size_t n)
{
	if(!r) r = d0_bignum_new(); if(!r) return NULL;
	BN_rand(r->z, n, 0, 0);
	return r;
}

d0_bignum_t *d0_bignum_zero(d0_bignum_t *r)
{
	if(!r) r = d0_bignum_new(); if(!r) return NULL;
	BN_zero(r->z);
	return r;
}

d0_bignum_t *d0_bignum_one(d0_bignum_t *r)
{
	if(!r) r = d0_bignum_new(); if(!r) return NULL;
	BN_one(r->z);
	return r;
}

d0_bignum_t *d0_bignum_int(d0_bignum_t *r, int n)
{
	if(!r) r = d0_bignum_new(); if(!r) return NULL;
	BN_set_word(r->z, n);
	return r;
}

d0_bignum_t *d0_bignum_mov(d0_bignum_t *r, const d0_bignum_t *a)
{
	if(r == a)
		return r; // trivial
	if(!r) r = d0_bignum_new(); if(!r) return NULL;
	BN_copy(r->z, a->z);
	return r;
}

d0_bignum_t *d0_bignum_neg(d0_bignum_t *r, const d0_bignum_t *a)
{
	if(!r) r = d0_bignum_new(); if(!r) return NULL;
	if(r != a)
		BN_copy(r->z, a->z);
	BN_set_negative(r->z, !BN_is_negative(r->z));
	return r;
}

d0_bignum_t *d0_bignum_shl(d0_bignum_t *r, const d0_bignum_t *a, ssize_t n)
{
	if(!r) r = d0_bignum_new(); if(!r) return NULL;
	if(n > 0)
		BN_lshift(r->z, a->z, n);
	else if(n < 0)
		BN_rshift(r->z, a->z, -n);
	else if(r != a)
		BN_copy(r->z, a->z);
	return r;
}

d0_bignum_t *d0_bignum_add(d0_bignum_t *r, const d0_bignum_t *a, const d0_bignum_t *b)
{
	if(!r) r = d0_bignum_new(); if(!r) return NULL;
	BN_add(r->z, a->z, b->z);
	return r;
}

d0_bignum_t *d0_bignum_sub(d0_bignum_t *r, const d0_bignum_t *a, const d0_bignum_t *b)
{
	if(!r) r = d0_bignum_new(); if(!r) return NULL;
	BN_sub(r->z, a->z, b->z);
	return r;
}

d0_bignum_t *d0_bignum_mul(d0_bignum_t *r, const d0_bignum_t *a, const d0_bignum_t *b)
{
	if(!r) r = d0_bignum_new(); if(!r) return NULL;
	d0_lockmutex(tempmutex);
	BN_mul(r->z, a->z, b->z, ctx);
	d0_unlockmutex(tempmutex);
	return r;
}

d0_bignum_t *d0_bignum_divmod(d0_bignum_t *q, d0_bignum_t *m, const d0_bignum_t *a, const d0_bignum_t *b)
{
	if(!q && !m)
		m = d0_bignum_new();
	d0_lockmutex(tempmutex);
	if(q)
	{
		if(m)
			BN_div(q->z, m->z, a->z, b->z, ctx);
		else
			BN_div(q->z, NULL, a->z, b->z, ctx);
		assert(!"I know this code is broken (rounds towards zero), need handle negative correctly");
	}
	else
		BN_nnmod(m->z, a->z, b->z, ctx);
	d0_unlockmutex(tempmutex);
	if(m)
		return m;
	else
		return q;
}

d0_bignum_t *d0_bignum_mod_add(d0_bignum_t *r, const d0_bignum_t *a, const d0_bignum_t *b, const d0_bignum_t *m)
{
	if(!r) r = d0_bignum_new(); if(!r) return NULL;
	d0_lockmutex(tempmutex);
	BN_mod_add(r->z, a->z, b->z, m->z, ctx);
	d0_unlockmutex(tempmutex);
	return r;
}

d0_bignum_t *d0_bignum_mod_sub(d0_bignum_t *r, const d0_bignum_t *a, const d0_bignum_t *b, const d0_bignum_t *m)
{
	if(!r) r = d0_bignum_new(); if(!r) return NULL;
	d0_lockmutex(tempmutex);
	BN_mod_sub(r->z, a->z, b->z, m->z, ctx);
	d0_unlockmutex(tempmutex);
	return r;
}

d0_bignum_t *d0_bignum_mod_mul(d0_bignum_t *r, const d0_bignum_t *a, const d0_bignum_t *b, const d0_bignum_t *m)
{
	if(!r) r = d0_bignum_new(); if(!r) return NULL;
	d0_lockmutex(tempmutex);
	BN_mod_mul(r->z, a->z, b->z, m->z, ctx);
	d0_unlockmutex(tempmutex);
	return r;
}

d0_bignum_t *d0_bignum_mod_pow(d0_bignum_t *r, const d0_bignum_t *a, const d0_bignum_t *b, const d0_bignum_t *m)
{
	if(!r) r = d0_bignum_new(); if(!r) return NULL;
	d0_lockmutex(tempmutex);
	BN_mod_exp(r->z, a->z, b->z, m->z, ctx);
	d0_unlockmutex(tempmutex);
	return r;
}

D0_BOOL d0_bignum_mod_inv(d0_bignum_t *r, const d0_bignum_t *a, const d0_bignum_t *m)
{
	// here, r MUST be set, as otherwise we cannot return error state!
	int ret;
	d0_lockmutex(tempmutex);
	ret = !!BN_mod_inverse(r->z, a->z, m->z, ctx);
	d0_unlockmutex(tempmutex);
	return ret;
}

int d0_bignum_isprime(const d0_bignum_t *r, int param)
{
	int ret;
	d0_lockmutex(tempmutex);
	if(param <= 0)
		ret = BN_is_prime_fasttest(r->z, 1, NULL, ctx, NULL, 1);
	else
		ret = BN_is_prime(r->z, param, NULL, ctx, NULL);
	d0_unlockmutex(tempmutex);
	return ret;
}

d0_bignum_t *d0_bignum_gcd(d0_bignum_t *r, d0_bignum_t *s, d0_bignum_t *t, const d0_bignum_t *a, const d0_bignum_t *b)
{
	if(!r) r = d0_bignum_new(); if(!r) return NULL;
	if(s)
		assert(!"Extended gcd not implemented");
	else if(t)
		assert(!"Extended gcd not implemented");
	else
	{
		d0_lockmutex(tempmutex);
		BN_gcd(r->z, a->z, b->z, ctx);
		d0_unlockmutex(tempmutex);
	}
	return r;
}

char *d0_bignum_tostring(const d0_bignum_t *x, unsigned int base)
{
	char *s = NULL;
	char *s2;
	size_t n;
	if(base == 10)
		s = BN_bn2dec(x->z);
	else if(base == 16)
		s = BN_bn2hex(x->z);
	else
		assert(!"Other bases not implemented");
	n = strlen(s) + 1;
	s2 = d0_malloc(n);
	memcpy(s2, s, n);
	OPENSSL_free(s);
	return s2;
}
