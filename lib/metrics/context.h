#if !defined(OGS_METRICS_INSIDE) && !defined(OGS_METRICS_COMPILATION)
#error "This header cannot be included directly."
#endif

#ifndef OGS_METRICS_CONTEXT_H
#define OGS_METRICS_CONTEXT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <microhttpd.h>

#define OGS_METRICS_DEFAULT_HTTP_PORT       9090
#define OGS_METRICS_DEFAULT_UPDATE_INTERVAL 10


typedef struct ogs_metrics_context_s
{
    /* configuration */
    ogs_sockaddr_t *sockaddr;
    ogs_time_t update_interval;

    /* runtime data */
    ogs_list_t metrics;
    struct MHD_Daemon *mhd_daemon;
    ogs_timer_t *update_timer;
    ogs_metrics_update_cb_t update_cb;
    ogs_socknode_t node;
    void *http_resp;

} ogs_metrics_context_t;


void ogs_metrics_context_init(void);
void ogs_metrics_context_final(void);
ogs_metrics_context_t *ogs_metrics_self(void);
int ogs_metrics_context_parse_config(void);

#ifdef __cplusplus
}
#endif

#endif /* OGS_METRICS_CONTEXT_H */
