#ifndef __D0_BLIND_ID_H__
#define __D0_BLIND_ID_H__

#include "d0.h"

typedef struct d0_blind_id_s d0_blind_id_t;

EXPORT WARN_UNUSED_RESULT d0_blind_id_t *d0_blind_id_new();
EXPORT void d0_blind_id_free(d0_blind_id_t *a);
EXPORT void d0_blind_id_clear(d0_blind_id_t *ctx);
EXPORT void d0_blind_id_copy(d0_blind_id_t *ctx, const d0_blind_id_t *src);
EXPORT WARN_UNUSED_RESULT BOOL d0_blind_id_generate_private_keys(d0_blind_id_t *ctx, int k);
EXPORT WARN_UNUSED_RESULT BOOL d0_blind_id_read_private_keys(d0_blind_id_t *ctx, const char *inbuf, size_t inbuflen);
EXPORT WARN_UNUSED_RESULT BOOL d0_blind_id_read_public_keys(d0_blind_id_t *ctx, const char *inbuf, size_t inbuflen);
EXPORT WARN_UNUSED_RESULT BOOL d0_blind_id_write_private_keys(d0_blind_id_t *ctx, char *outbuf, size_t *outbuflen);
EXPORT WARN_UNUSED_RESULT BOOL d0_blind_id_write_public_keys(d0_blind_id_t *ctx, char *outbuf, size_t *outbuflen);
EXPORT WARN_UNUSED_RESULT BOOL d0_blind_id_generate_private_id_start(d0_blind_id_t *ctx);
EXPORT WARN_UNUSED_RESULT BOOL d0_blind_id_generate_private_id_request(d0_blind_id_t *ctx, char *outbuf, size_t *outbuflen);
EXPORT WARN_UNUSED_RESULT BOOL d0_blind_id_answer_private_id_request(d0_blind_id_t *ctx, const char *inbuf, size_t inbuflen, char *outbuf, size_t *outbuflen);
EXPORT WARN_UNUSED_RESULT BOOL d0_blind_id_finish_private_id_request(d0_blind_id_t *ctx, const char *inbuf, size_t inbuflen);
EXPORT WARN_UNUSED_RESULT BOOL d0_blind_id_read_private_id(d0_blind_id_t *ctx, const char *inbuf, size_t inbuflen);
EXPORT WARN_UNUSED_RESULT BOOL d0_blind_id_read_public_id(d0_blind_id_t *ctx, const char *inbuf, size_t inbuflen);
EXPORT WARN_UNUSED_RESULT BOOL d0_blind_id_write_private_id(d0_blind_id_t *ctx, char *outbuf, size_t *outbuflen);
EXPORT WARN_UNUSED_RESULT BOOL d0_blind_id_write_public_id(d0_blind_id_t *ctx, char *outbuf, size_t *outbuflen);
EXPORT WARN_UNUSED_RESULT BOOL d0_blind_id_authenticate_with_private_id_start(d0_blind_id_t *ctx, int is_first, char *message, size_t msglen, char *outbuf, size_t *outbuflen);
EXPORT WARN_UNUSED_RESULT BOOL d0_blind_id_authenticate_with_private_id_challenge(d0_blind_id_t *ctx, int is_first, const char *inbuf, size_t inbuflen, char *outbuf, size_t *outbuflen);
EXPORT WARN_UNUSED_RESULT BOOL d0_blind_id_authenticate_with_private_id_response(d0_blind_id_t *ctx, const char *inbuf, size_t inbuflen, char *outbuf, size_t *outbuflen);
EXPORT WARN_UNUSED_RESULT BOOL d0_blind_id_authenticate_with_private_id_verify(d0_blind_id_t *ctx, const char *inbuf, size_t inbuflen, char *msg, ssize_t *msglen);
EXPORT WARN_UNUSED_RESULT BOOL d0_blind_id_fingerprint64_public_id(d0_blind_id_t *ctx, char *outbuf, size_t *outbuflen);
EXPORT 
EXPORT void d0_blind_id_INITIALIZE();
EXPORT void d0_blind_id_SHUTDOWN();

#endif
