#ifndef RNET_STUN_INTERNAL_H
#define RNET_STUN_INTERNAL_H

#include "recomp_net/types.h"

#include <stddef.h>

#define RNET_STUN_TRANSACTION_ID_SIZE 12

enum {
    RNET_STUN_PARSE_OK = 0,
    RNET_STUN_PARSE_TRANSACTION_MISMATCH = 1,
    RNET_STUN_PARSE_ERROR = -1
};

/* Parse an RFC 5389 Binding Success Response.
 * address_out / port_out are host byte order. port_out may be NULL. */
int rnet_stun_parse_binding_response_ex(
    const rnet_u8 *packet, size_t packet_len,
    const rnet_u8 transaction_id[RNET_STUN_TRANSACTION_ID_SIZE],
    rnet_u32 *address_out, rnet_u16 *port_out);

/* Compat wrapper — discards mapped port. */
int rnet_stun_parse_binding_response(
    const rnet_u8 *packet, size_t packet_len,
    const rnet_u8 transaction_id[RNET_STUN_TRANSACTION_ID_SIZE],
    rnet_u32 *address_out);

#endif /* RNET_STUN_INTERNAL_H */
