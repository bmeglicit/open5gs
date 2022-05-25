#ifndef OGS_METRICS_H
#define OGS_METRICS_H

#ifdef __cplusplus
extern "C" {
#endif


typedef void (*ogs_metrics_update_cb_t)(void);

typedef enum
{
    OGS_METRICS_TYPE_COUNTER,
    OGS_METRICS_TYPE_GAUGE,
    OGS_METRICS_TYPE_HISTOGRAM,
    OGS_METRICS_TYPE_SUMMARY,
} ogs_metrics_type_e;

typedef struct ogs_metrics_sample_s
{
    bool valid;
    ssize_t val_gauge;
} ogs_metrics_sample_t;


typedef struct ogs_metrics_label_s
{
    ogs_lnode_t lnode;

    char *label_name;
    char *label_value;
} ogs_metrics_label_t;

typedef struct ogs_metrics_label_list_s
{
    ogs_lnode_t lnode;

    ogs_list_t labels;
    ogs_metrics_sample_t sample;
} ogs_metrics_label_list_t;

typedef struct ogs_metrics_metric_s
{
    ogs_lnode_t lnode;

    ogs_metrics_type_e type;
    char *name;
    char *help;

    ogs_metrics_sample_t sample;

    ogs_list_t label_list;
} ogs_metrics_metric_t;


#define OGS_METRICS_INSIDE
#include "metrics/context.h"
#undef OGS_METRICS_INSIDE


/**
 * @brief Initialize metrics
 *
 */
int ogs_metrics_init(void);

/**
 * @brief Deinitialize metrics and cleanup resources
 *
 */
void ogs_metrics_deinit(void);

/**
 * @brief Start metrics server
 *
 */
int ogs_metrics_start(void);

/**
 * @brief Stop the metrics server
 *
 */
void ogs_metrics_close(void);

/**
 * @brief Set the callback function for updating metrics data
 *
 */
void ogs_metrics_set_update_cb(ogs_metrics_update_cb_t cb);

/**
 * @brief Create a new metric
 *
 * @param type metric type
 * @param name name of the metric
 * @param help help message for the metric
 * @param label_cnt number of labels
 * @param labels array of 2-element array of labels
 * @return ogs_metrics_sample_t* pointer to metric sample
 */
ogs_metrics_sample_t *ogs_metrics_create(ogs_metrics_type_e type,
        const char *name, const char *help,
        unsigned int label_cnt, const char *labels[][2]);
/**
 * @brief Update the value used by metric sample
 *
 */
void ogs_metrics_gauge_set(ogs_metrics_sample_t *sample, ssize_t value);

/**
 * @brief Return the value stored in metric sample
 *
 */
ssize_t ogs_metrics_gauge_get(ogs_metrics_sample_t *sample);


extern int __ogs_metrics_domain;

#undef OGS_LOG_DOMAIN
#define OGS_LOG_DOMAIN __ogs_metrics_domain


#ifdef __cplusplus
}
#endif

#endif
