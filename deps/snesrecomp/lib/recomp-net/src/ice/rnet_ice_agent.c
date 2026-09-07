#include "rnet_ice_internal.h"

#if defined(RNET_ENABLE_ICE)

#include <juice/juice.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
typedef CRITICAL_SECTION RNetIceMutex;
static void ice_mutex_init(RNetIceMutex *mutex) { InitializeCriticalSection(mutex); }
static void ice_mutex_destroy(RNetIceMutex *mutex) { DeleteCriticalSection(mutex); }
static void ice_mutex_lock(RNetIceMutex *mutex) { EnterCriticalSection(mutex); }
static void ice_mutex_unlock(RNetIceMutex *mutex) { LeaveCriticalSection(mutex); }
#else
#include <pthread.h>
typedef pthread_mutex_t RNetIceMutex;
static void ice_mutex_init(RNetIceMutex *mutex) { (void)pthread_mutex_init(mutex, NULL); }
static void ice_mutex_destroy(RNetIceMutex *mutex) { (void)pthread_mutex_destroy(mutex); }
static void ice_mutex_lock(RNetIceMutex *mutex) { (void)pthread_mutex_lock(mutex); }
static void ice_mutex_unlock(RNetIceMutex *mutex) { (void)pthread_mutex_unlock(mutex); }
#endif

#define RNET_ICE_RECV_QUEUE 64
#define RNET_ICE_RECV_MAX 2048
#define RNET_ICE_CAND_QUEUE 32
#define RNET_ICE_CAND_MAX 280

typedef struct RNetIceRecvSlot
{
    rnet_u8 data[RNET_ICE_RECV_MAX];
    size_t len;
} RNetIceRecvSlot;

typedef struct RNetIceCandSlot
{
    char sdp[RNET_ICE_CAND_MAX];
} RNetIceCandSlot;

struct RNetIceAgent
{
    juice_agent_t *agent;
    RNetIceMutex mutex;
    RNetIceState state;
    RNetIceSignalEmitFn emit;
    void *user;
    int gathering_done_posted;
    int remote_desc_set;
    RNetIceRecvSlot recv_q[RNET_ICE_RECV_QUEUE];
    unsigned recv_head;
    unsigned recv_tail;
    unsigned recv_count;
    RNetIceCandSlot cand_q[RNET_ICE_CAND_QUEUE];
    unsigned cand_head;
    unsigned cand_tail;
    unsigned cand_count;
    char stun_host[128];
    char turn_host[128];
    char turn_user[192];
    char turn_pass[128];
    char bind_address[64];
    juice_turn_server_t turn;
    int turn_count;
    rnet_u16 stun_port;
    rnet_u16 bind_port;
    int controlling;
    int gather_started;
    int gather_pending;
    int selected_logged;
    int force_relay; /* runtime cfg->force_relay and/or RNET_ICE_FORCE_TURN */
    int relay_fallback_done; /* auto-retried with force_relay once */
    unsigned remote_cand_count; /* trickle remotes seen this gather */
    unsigned remote_public_cand_count; /* remotes with non-private IPv4 */
};

static RNetIceState map_juice_state(juice_state_t st)
{
    switch (st)
    {
    case JUICE_STATE_GATHERING:
        return RNET_ICE_STATE_GATHERING;
    case JUICE_STATE_CONNECTING:
        return RNET_ICE_STATE_CONNECTING;
    case JUICE_STATE_CONNECTED:
        return RNET_ICE_STATE_CONNECTED;
    case JUICE_STATE_COMPLETED:
        return RNET_ICE_STATE_COMPLETED;
    case JUICE_STATE_FAILED:
        return RNET_ICE_STATE_FAILED;
    default:
        return RNET_ICE_STATE_IDLE;
    }
}

static void queue_recv(RNetIceAgent *a, const char *data, size_t size)
{
    if ((data == NULL) || (size == 0) || (size > RNET_ICE_RECV_MAX))
    {
        return;
    }
    if (a->recv_count >= RNET_ICE_RECV_QUEUE)
    {
        a->recv_head = (a->recv_head + 1U) % RNET_ICE_RECV_QUEUE;
        a->recv_count--;
    }
    memcpy(a->recv_q[a->recv_tail].data, data, size);
    a->recv_q[a->recv_tail].len = size;
    a->recv_tail = (a->recv_tail + 1U) % RNET_ICE_RECV_QUEUE;
    a->recv_count++;
}

static void queue_cand(RNetIceAgent *a, const char *sdp)
{
    if ((sdp == NULL) || (sdp[0] == '\0'))
    {
        return;
    }
    if (a->cand_count >= RNET_ICE_CAND_QUEUE)
    {
        a->cand_head = (a->cand_head + 1U) % RNET_ICE_CAND_QUEUE;
        a->cand_count--;
    }
    snprintf(a->cand_q[a->cand_tail].sdp, sizeof(a->cand_q[a->cand_tail].sdp), "%s", sdp);
    a->cand_tail = (a->cand_tail + 1U) % RNET_ICE_CAND_QUEUE;
    a->cand_count++;
}

static int ice_candidate_is_relay(const char *sdp)
{
    return sdp != NULL && strstr(sdp, " typ relay") != NULL;
}

static const char *ice_candidate_type(const char *sdp)
{
    if (sdp == NULL || sdp[0] == '\0')
        return "unknown";
    if (strstr(sdp, " typ relay") != NULL)
        return "relay";
    if (strstr(sdp, " typ srflx") != NULL)
        return "srflx";
    if (strstr(sdp, " typ prflx") != NULL)
        return "prflx";
    if (strstr(sdp, " typ host") != NULL)
        return "host";
    return "unknown";
}

static void log_selected_pair(RNetIceAgent *a)
{
    char local_cand[512];
    char remote_cand[512];
    char local_addr[256];
    char remote_addr[256];
    const char *local_ty;
    const char *remote_ty;
    if (a == NULL || a->agent == NULL || a->selected_logged)
        return;
    local_cand[0] = remote_cand[0] = local_addr[0] = remote_addr[0] = '\0';
    if (juice_get_selected_candidates(a->agent, local_cand, sizeof(local_cand),
                                      remote_cand, sizeof(remote_cand)) == 0) {
        fprintf(stderr,
                "rnet_ice: selected candidates\n  local:  %s\n  remote: %s\n",
                local_cand[0] ? local_cand : "(none)",
                remote_cand[0] ? remote_cand : "(none)");
        a->selected_logged = 1;
        if (a->force_relay) {
            local_ty = ice_candidate_type(local_cand);
            remote_ty = ice_candidate_type(remote_cand);
            if (strcmp(local_ty, "relay") != 0 || strcmp(remote_ty, "relay") != 0) {
                fprintf(stderr,
                        "rnet_ice: force_relay but pair is local=%s remote=%s "
                        "(libjuice still gathers local host; non-relay was "
                        "stripped from signaled SDP/trickle)\n",
                        local_ty, remote_ty);
            }
        }
    }
    if (juice_get_selected_addresses(a->agent, local_addr, sizeof(local_addr),
                                     remote_addr, sizeof(remote_addr)) == 0) {
        fprintf(stderr, "rnet_ice: selected addresses local=%s remote=%s\n",
                local_addr[0] ? local_addr : "(none)",
                remote_addr[0] ? remote_addr : "(none)");
    }
}

static void on_state_changed(juice_agent_t *agent, juice_state_t state, void *user_ptr)
{
    RNetIceAgent *a = (RNetIceAgent *)user_ptr;
    RNetIceState mapped;
    (void)agent;
    mapped = map_juice_state(state);
    ice_mutex_lock(&a->mutex);
    a->state = mapped;
    ice_mutex_unlock(&a->mutex);
    if (mapped == RNET_ICE_STATE_CONNECTED || mapped == RNET_ICE_STATE_COMPLETED)
        log_selected_pair(a);
    else if (mapped == RNET_ICE_STATE_FAILED)
        fprintf(stderr, "rnet_ice: state=FAILED (check STUN/TURN / NAT path)\n");
}

static int ice_line_is_candidate(const char *line)
{
    const char *p = line;
    if (p == NULL)
        return 0;
    while (*p == ' ' || *p == '\t')
        p++;
    if (strncmp(p, "a=candidate:", 12) == 0)
        return 1;
    if (strncmp(p, "candidate:", 10) == 0)
        return 1;
    return 0;
}

/* Keep ice-ufrag/pwd/etc.; drop host/srflx/prflx candidate lines. */
static int ice_sdp_strip_non_relay(const char *in, char *out, size_t out_len)
{
    const char *p;
    size_t o = 0;

    if (out == NULL || out_len == 0)
        return -1;
    out[0] = '\0';
    if (in == NULL)
        return 0;

    p = in;
    while (*p)
    {
        const char *eol = p;
        size_t n;
        int keep;

        while (*eol && *eol != '\n' && *eol != '\r')
            eol++;
        n = (size_t)(eol - p);
        keep = 1;
        if (n > 0 && ice_line_is_candidate(p))
            keep = ice_candidate_is_relay(p);
        if (keep)
        {
            if (o + n + 2 >= out_len)
                return -1;
            memcpy(out + o, p, n);
            o += n;
            out[o++] = '\n';
            out[o] = '\0';
        }
        p = eol;
        if (*p == '\r')
            p++;
        if (*p == '\n')
            p++;
    }
    return 0;
}

void rnet_ice_agent_selected_info(const RNetIceAgent *agent, char *path_out, size_t path_len,
                                  char *local_addr_out, size_t local_addr_len,
                                  char *remote_addr_out, size_t remote_addr_len)
{
    char local_cand[512];
    char remote_cand[512];
    const char *local_ty;
    const char *remote_ty;
    const char *path = "unknown";

    if (path_out && path_len)
        path_out[0] = '\0';
    if (local_addr_out && local_addr_len)
        local_addr_out[0] = '\0';
    if (remote_addr_out && remote_addr_len)
        remote_addr_out[0] = '\0';
    if (agent == NULL || agent->agent == NULL)
        return;

    local_cand[0] = remote_cand[0] = '\0';
    if (juice_get_selected_candidates(agent->agent, local_cand, sizeof(local_cand),
                                      remote_cand, sizeof(remote_cand)) == 0) {
        local_ty = ice_candidate_type(local_cand);
        remote_ty = ice_candidate_type(remote_cand);
        /* Prefer remote type for NAT path; escalate to relay if either side is. */
        if (strcmp(local_ty, "relay") == 0 || strcmp(remote_ty, "relay") == 0)
            path = "relay";
        else if (strcmp(remote_ty, "unknown") != 0)
            path = remote_ty;
        else
            path = local_ty;
    }
    if (path_out && path_len)
        snprintf(path_out, path_len, "%s", path);

    if (local_addr_out && local_addr_len && remote_addr_out && remote_addr_len) {
        (void)juice_get_selected_addresses(agent->agent, local_addr_out, local_addr_len,
                                           remote_addr_out, remote_addr_len);
    } else if (local_addr_out && local_addr_len) {
        char discard[256];
        (void)juice_get_selected_addresses(agent->agent, local_addr_out, local_addr_len,
                                           discard, sizeof(discard));
    } else if (remote_addr_out && remote_addr_len) {
        char discard[256];
        (void)juice_get_selected_addresses(agent->agent, discard, sizeof(discard),
                                           remote_addr_out, remote_addr_len);
    }
}

static void on_candidate(juice_agent_t *agent, const char *sdp, void *user_ptr)
{
    RNetIceAgent *a = (RNetIceAgent *)user_ptr;
    (void)agent;
    if (a->force_relay && !ice_candidate_is_relay(sdp))
        return; /* Drop host/srflx so ICE must use TURN */
    ice_mutex_lock(&a->mutex);
    queue_cand(a, sdp);
    ice_mutex_unlock(&a->mutex);
}

static void on_gathering_done(juice_agent_t *agent, void *user_ptr)
{
    RNetIceAgent *a = (RNetIceAgent *)user_ptr;
    (void)agent;
    ice_mutex_lock(&a->mutex);
    a->gathering_done_posted = 1;
    ice_mutex_unlock(&a->mutex);
}

static void on_recv(juice_agent_t *agent, const char *data, size_t size, void *user_ptr)
{
    RNetIceAgent *a = (RNetIceAgent *)user_ptr;
    (void)agent;
    ice_mutex_lock(&a->mutex);
    queue_recv(a, data, size);
    ice_mutex_unlock(&a->mutex);
}

static void emit_signal(RNetIceAgent *a, RNetSignalType type, const char *text, rnet_u8 flag)
{
    RNetSignal msg;
    if (a->emit == NULL)
    {
        return;
    }
    memset(&msg, 0, sizeof(msg));
    msg.type = type;
    msg.flag = flag;
    if (text != NULL)
    {
        snprintf(msg.text, sizeof(msg.text), "%s", text);
    }
    a->emit(&msg, a->user);
}

static void ice_reset_runtime(RNetIceAgent *a)
{
    if (a->agent != NULL)
    {
        juice_destroy(a->agent);
        a->agent = NULL;
    }
    ice_mutex_lock(&a->mutex);
    a->recv_head = a->recv_tail = a->recv_count = 0;
    a->cand_head = a->cand_tail = a->cand_count = 0;
    a->gathering_done_posted = 0;
    ice_mutex_unlock(&a->mutex);
    a->remote_desc_set = 0;
    a->gather_started = 0;
    a->gather_pending = 0;
    a->selected_logged = 0;
    a->remote_cand_count = 0;
    a->remote_public_cand_count = 0;
    a->state = RNET_ICE_STATE_IDLE;
}

static int ice_create_juice(RNetIceAgent *a)
{
    juice_config_t jcfg;

    memset(&jcfg, 0, sizeof(jcfg));
    /* force_relay: skip STUN so we do not gather srflx (host still gathered). */
    if (a->stun_host[0] != '\0' && !a->force_relay)
    {
        jcfg.stun_server_host = a->stun_host;
        jcfg.stun_server_port = a->stun_port ? a->stun_port : 3478;
    }
    if (a->turn_count > 0)
    {
        a->turn.host = a->turn_host;
        a->turn.username = a->turn_user;
        a->turn.password = a->turn_pass;
        jcfg.turn_servers = &a->turn;
        jcfg.turn_servers_count = 1;
    }
    if (a->bind_address[0] != '\0')
        jcfg.bind_address = a->bind_address;
    if (a->bind_port != 0)
    {
        jcfg.local_port_range_begin = a->bind_port;
        jcfg.local_port_range_end = a->bind_port;
    }
    jcfg.cb_state_changed = on_state_changed;
    jcfg.cb_candidate = on_candidate;
    jcfg.cb_gathering_done = on_gathering_done;
    jcfg.cb_recv = on_recv;
    jcfg.user_ptr = a;

    a->agent = juice_create(&jcfg);
    return a->agent != NULL ? 0 : -1;
}

RNetIceAgent *rnet_ice_agent_create(const RNetIceConfig *cfg, RNetIceSignalEmitFn emit, void *user)
{
    RNetIceAgent *a;
    if (cfg == NULL)
    {
        return NULL;
    }
    a = (RNetIceAgent *)calloc(1, sizeof(*a));
    if (a == NULL)
    {
        return NULL;
    }
    ice_mutex_init(&a->mutex);
    a->emit = emit;
    a->user = user;
    a->state = RNET_ICE_STATE_IDLE;

    if (cfg->stun_host != NULL && cfg->stun_host[0] != '\0')
    {
        snprintf(a->stun_host, sizeof(a->stun_host), "%s", cfg->stun_host);
        a->stun_port = cfg->stun_port ? cfg->stun_port : 3478;
    }
    if (cfg->turn_host != NULL && cfg->turn_host[0] != '\0' && cfg->turn_user != NULL &&
        cfg->turn_pass != NULL)
    {
        memset(&a->turn, 0, sizeof(a->turn));
        snprintf(a->turn_host, sizeof(a->turn_host), "%s", cfg->turn_host);
        snprintf(a->turn_user, sizeof(a->turn_user), "%s", cfg->turn_user);
        snprintf(a->turn_pass, sizeof(a->turn_pass), "%s", cfg->turn_pass);
        a->turn.port = cfg->turn_port ? cfg->turn_port : 3478;
        a->turn_count = 1;
    }
    if (cfg->bind_address != NULL && cfg->bind_address[0] != '\0')
        snprintf(a->bind_address, sizeof(a->bind_address), "%s", cfg->bind_address);
    a->bind_port = cfg->bind_port;

    a->force_relay = cfg->force_relay ? 1 : 0;
#if defined(RNET_ICE_FORCE_TURN)
    a->force_relay = 1;
#endif
    /* libjuice has no public set_ice_controlling; role follows offer/answer:
     * controlling gathers immediately; controlled waits for remote SDP. */
    a->controlling = cfg->controlling ? 1 : 0;

    if (ice_create_juice(a) != 0)
    {
        ice_mutex_destroy(&a->mutex);
        free(a);
        return NULL;
    }
    if (a->force_relay)
    {
        if (a->turn_count == 0)
        {
            fprintf(stderr,
                    "rnet_ice: force_relay set but no TURN server in "
                    "RNetIceConfig — agent created without relay\n");
        }
        else
        {
            fprintf(stderr,
                    "rnet_ice: force_relay — strip non-relay from SDP/trickle "
                    "(no STUN/srflx; libjuice may still gather local host)\n");
        }
    }
    return a;
}

/* True for IPv4 in RFC1918 / link-local / loopback. Public coturn with
 * denied-peer-ip rejects CreatePermission into these ranges (libjuice:
 * "CreatePermission error … code=403"). */
static int ice_ipv4_is_private(const char *ip)
{
    unsigned a = 0, b = 0, c = 0, d = 0;
    if (ip == NULL || ip[0] == '\0')
        return 0;
    if (sscanf(ip, "%u.%u.%u.%u", &a, &b, &c, &d) != 4)
        return 0;
    if (a > 255U || b > 255U || c > 255U || d > 255U)
        return 0;
    if (a == 10U)
        return 1;
    if (a == 127U)
        return 1;
    if (a == 169U && b == 254U)
        return 1;
    if (a == 172U && b >= 16U && b <= 31U)
        return 1;
    if (a == 192U && b == 168U)
        return 1;
    return 0;
}

/* ADDRESS field of an ICE candidate line:
 * [a=]candidate:<foundation> <component> <proto> <priority> <address> <port> … */
static int ice_sdp_candidate_ipv4(const char *sdp, char *out, size_t out_len)
{
    const char *p = sdp;
    unsigned field = 0;
    unsigned a = 0, b = 0, c = 0, d = 0;
    char tok[128];
    size_t n;

    if (sdp == NULL || out == NULL || out_len < 8)
        return 0;
    if (p[0] == 'a' && p[1] == '=')
        p += 2;
    if (strncmp(p, "candidate:", 10) == 0)
        p += 10;
    while (*p)
    {
        while (*p == ' ' || *p == '\t')
            p++;
        if (*p == '\0')
            break;
        n = 0;
        while (p[n] && p[n] != ' ' && p[n] != '\t' && n + 1 < sizeof(tok))
            n++;
        memcpy(tok, p, n);
        tok[n] = '\0';
        p += n;
        /* After stripping "candidate:", fields are foundation(0) … address(4). */
        if (field == 4U)
        {
            if (sscanf(tok, "%u.%u.%u.%u", &a, &b, &c, &d) == 4 && a <= 255U &&
                b <= 255U && c <= 255U && d <= 255U)
            {
                snprintf(out, out_len, "%u.%u.%u.%u", a, b, c, d);
                return 1;
            }
            return 0;
        }
        field++;
    }
    return 0;
}

static int ice_sdp_has_public_ipv4(const char *sdp)
{
    char ip[64];
    if (!ice_sdp_candidate_ipv4(sdp, ip, sizeof(ip)))
        return 0;
    return !ice_ipv4_is_private(ip);
}

static void ice_note_remote_candidate(RNetIceAgent *a, const char *sdp)
{
    if (a == NULL || sdp == NULL || sdp[0] == '\0')
        return;
    a->remote_cand_count++;
    if (ice_sdp_has_public_ipv4(sdp))
        a->remote_public_cand_count++;
}

int rnet_ice_agent_has_turn(const RNetIceAgent *agent)
{
    return agent != NULL && agent->turn_count > 0;
}

/* Remotes seen and every IPv4 address was private (typical CGNAT host
 * candidates before typ relay arrives). Informational for fallback logs. */
int rnet_ice_agent_remote_only_private(const RNetIceAgent *agent)
{
    return agent != NULL && agent->remote_cand_count > 0U &&
           agent->remote_public_cand_count == 0U;
}

int rnet_ice_agent_is_force_relay(const RNetIceAgent *agent)
{
    return agent != NULL && agent->force_relay != 0;
}

int rnet_ice_agent_relay_fallback_done(const RNetIceAgent *agent)
{
    return agent != NULL && agent->relay_fallback_done != 0;
}

void rnet_ice_agent_mark_relay_fallback_done(RNetIceAgent *agent)
{
    if (agent != NULL)
        agent->relay_fallback_done = 1;
}

int rnet_ice_agent_restart_force_relay(RNetIceAgent *agent)
{
    if (agent == NULL || agent->turn_count == 0 || agent->relay_fallback_done)
        return -1;
    agent->force_relay = 1;
    agent->relay_fallback_done = 1;
    fprintf(stderr,
            "rnet_ice: auto TURN fallback — restarting ICE with force_relay "
            "(STUN/host path failed or stalled)\n");
    ice_reset_runtime(agent);
    if (ice_create_juice(agent) != 0)
        return -1;
    return rnet_ice_agent_start_gathering(agent);
}

void rnet_ice_agent_destroy(RNetIceAgent *agent)
{
    if (agent == NULL)
    {
        return;
    }
    if (agent->agent != NULL)
    {
        juice_destroy(agent->agent);
        agent->agent = NULL;
    }
    ice_mutex_destroy(&agent->mutex);
    free(agent);
}

static int ice_gather_now(RNetIceAgent *agent)
{
    char local_sdp[JUICE_MAX_SDP_STRING_LEN];
    char filtered[JUICE_MAX_SDP_STRING_LEN];
    const char *emit_sdp;
    if (agent->gather_started)
    {
        return 0;
    }
    if (juice_gather_candidates(agent->agent) < 0)
    {
        return -1;
    }
    agent->gather_started = 1;
    agent->gather_pending = 0;
    if (juice_get_local_description(agent->agent, local_sdp, sizeof(local_sdp)) == 0)
    {
        emit_sdp = local_sdp;
        if (agent->force_relay &&
            ice_sdp_strip_non_relay(local_sdp, filtered, sizeof(filtered)) == 0)
            emit_sdp = filtered;
        emit_signal(agent, RNET_SIGNAL_LOCAL_SDP, emit_sdp, 0);
    }
    return 0;
}

int rnet_ice_agent_start_gathering(RNetIceAgent *agent)
{
    if ((agent == NULL) || (agent->agent == NULL))
    {
        return -1;
    }
    if (!agent->controlling && !agent->remote_desc_set)
    {
        /* Answerer: gather after remote offer arrives (see push_signal). */
        agent->gather_pending = 1;
        return 0;
    }
    return ice_gather_now(agent);
}

void rnet_ice_agent_poll(RNetIceAgent *agent)
{
    RNetIceCandSlot drain[RNET_ICE_CAND_QUEUE];
    unsigned count = 0;
    unsigned i;
    int gathering_done = 0;

    if (agent == NULL)
    {
        return;
    }

    ice_mutex_lock(&agent->mutex);
    while (agent->cand_count > 0 && count < RNET_ICE_CAND_QUEUE)
    {
        drain[count++] = agent->cand_q[agent->cand_head];
        agent->cand_head = (agent->cand_head + 1U) % RNET_ICE_CAND_QUEUE;
        agent->cand_count--;
    }
    if (agent->gathering_done_posted)
    {
        gathering_done = 1;
        agent->gathering_done_posted = 0;
    }
    ice_mutex_unlock(&agent->mutex);

    for (i = 0; i < count; ++i)
    {
        emit_signal(agent, RNET_SIGNAL_LOCAL_CANDIDATE, drain[i].sdp, 0);
    }
    if (gathering_done)
    {
        emit_signal(agent, RNET_SIGNAL_GATHERING_DONE, "", 0);
    }
}

void rnet_ice_agent_push_signal(RNetIceAgent *agent, const RNetSignal *msg)
{
    if ((agent == NULL) || (msg == NULL))
    {
        return;
    }
    switch (msg->type)
    {
    case RNET_SIGNAL_REMOTE_SDP:
        if (msg->text[0] != '\0')
        {
            /* Peer ICE restart (e.g. their TURN fallback): rebuild local
             * juice so set_remote_description applies a fresh offer. */
            if (agent->remote_desc_set)
            {
                /* Peer restarted ICE (often their TURN fallback). Adopt
                 * force_relay so both sides gather typ relay — including when
                 * prior remotes were RFC1918-only (CGNAT host candidates). */
                if (agent->turn_count > 0 && !agent->force_relay)
                {
                    agent->force_relay = 1;
                    agent->relay_fallback_done = 1;
                    fprintf(stderr,
                            "rnet_ice: peer ICE restart — adopting force_relay\n");
                }
                ice_reset_runtime(agent);
                if (ice_create_juice(agent) != 0)
                    break;
            }
            if (agent->agent == NULL)
                break;
            {
                char filtered[JUICE_MAX_SDP_STRING_LEN];
                const char *remote_sdp = msg->text;
                if (agent->force_relay &&
                    ice_sdp_strip_non_relay(msg->text, filtered, sizeof(filtered)) == 0)
                    remote_sdp = filtered;
                if (juice_set_remote_description(agent->agent, remote_sdp) == 0)
                {
                    agent->remote_desc_set = 1;
                    if (agent->gather_pending || !agent->controlling)
                    {
                        (void)ice_gather_now(agent);
                    }
                }
            }
        }
        break;
    case RNET_SIGNAL_REMOTE_CANDIDATE:
        if (agent->agent == NULL || msg->text[0] == '\0')
            break;
        if (agent->force_relay && !ice_candidate_is_relay(msg->text))
            break;
        ice_note_remote_candidate(agent, msg->text);
        (void)juice_add_remote_candidate(agent->agent, msg->text);
        break;
    case RNET_SIGNAL_GATHERING_DONE:
        if (agent->agent != NULL)
            (void)juice_set_remote_gathering_done(agent->agent);
        break;
    case RNET_SIGNAL_SET_CONTROLLING:
        /* Advisory for gather order; libjuice resolves role via SDP/conflicts. */
        agent->controlling = msg->flag ? 1 : 0;
        if (agent->controlling && agent->gather_pending && agent->agent != NULL)
        {
            (void)ice_gather_now(agent);
        }
        break;
    default:
        break;
    }
}

RNetIceState rnet_ice_agent_state(const RNetIceAgent *agent)
{
    if (agent == NULL)
    {
        return RNET_ICE_STATE_IDLE;
    }
    return agent->state;
}

int rnet_ice_agent_send(RNetIceAgent *agent, const rnet_u8 *buf, size_t len)
{
    int rc;
    RNetIceState st;
    if ((agent == NULL) || (agent->agent == NULL) || (buf == NULL) || (len == 0))
    {
        return -1;
    }
    /* Quietly drop during gather / force_relay restart — avoids libjuice
     * "Send while ICE is not connected" spam before COMPLETED. */
    st = agent->state;
    if (st != RNET_ICE_STATE_CONNECTED && st != RNET_ICE_STATE_COMPLETED)
        return -1;
    rc = juice_send(agent->agent, (const char *)buf, len);
    if (rc < 0)
    {
        return -1;
    }
    return (int)len;
}

int rnet_ice_agent_recv(RNetIceAgent *agent, rnet_u8 *buf, size_t cap, size_t *out_len)
{
    if ((agent == NULL) || (buf == NULL) || (cap == 0) || (out_len == NULL))
    {
        return -1;
    }
    *out_len = 0;
    ice_mutex_lock(&agent->mutex);
    if (agent->recv_count == 0)
    {
        ice_mutex_unlock(&agent->mutex);
        return -1; /* empty */
    }
    {
        RNetIceRecvSlot *slot = &agent->recv_q[agent->recv_head];
        size_t n = slot->len;
        if (n > cap)
        {
            n = cap;
        }
        memcpy(buf, slot->data, n);
        *out_len = n;
        agent->recv_head = (agent->recv_head + 1U) % RNET_ICE_RECV_QUEUE;
        agent->recv_count--;
    }
    ice_mutex_unlock(&agent->mutex);
    return 0;
}

#endif /* RNET_ENABLE_ICE */
