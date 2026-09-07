#ifndef RNET_ADDRESS_INTERNAL_H
#define RNET_ADDRESS_INTERNAL_H

#include "recomp_net/address.h"
#include "recomp_net/types.h"

#include <stddef.h>

typedef struct RNetIpv4Candidate {
    /* Host byte order, so classification and deterministic sorting are simple. */
    rnet_u32 address;
    char interface_label[RNET_INTERFACE_LABEL_MAX];
} RNetIpv4Candidate;

/* Shared by the platform collectors and unit tests. Returns the total unique
 * usable result count and writes at most capacity sorted records. */
int rnet_ipv4_normalize_candidates(const RNetIpv4Candidate *candidates,
                                   size_t candidate_count,
                                   rnet_u32 preferred_address,
                                   RNetIpv4Address *out,
                                   size_t capacity);

#endif /* RNET_ADDRESS_INTERNAL_H */
