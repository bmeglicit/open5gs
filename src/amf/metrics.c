#include "ogs-app.h"
#include "context.h"

#include "metrics.h"


static ogs_metrics_sample_t *sample1;
static ogs_metrics_sample_t *sample2;
static ogs_metrics_sample_t *sample3;

static ogs_metrics_sample_t *gauge_gnb_cnt;
static ogs_metrics_sample_t *gauge_ran_ue_cnt;

static int counter = 1;


static void metrics_update(void)
{
    ogs_trace("metrics_update");

    ogs_metrics_gauge_set(sample1, counter);
    ogs_metrics_gauge_set(sample2, counter*10);
    ogs_metrics_gauge_set(sample3, (counter*10) / 2);

    ogs_metrics_gauge_set(gauge_gnb_cnt, ogs_list_count(&amf_self()->gnb_list));
    ogs_metrics_gauge_set(gauge_ran_ue_cnt, ogs_list_count(&amf_self()->amf_ue_list));

    counter++;
}


int metrics_open(void)
{
    int rv;

    ogs_metrics_init();

    ogs_metrics_set_update_cb(&metrics_update);

    rv = ogs_metrics_start();
    if (rv != OGS_OK) {
        ogs_error("Could not start metrics server");
        return rv;
    }

    sample3 = ogs_metrics_create(OGS_METRICS_TYPE_GAUGE,
        "gauge_name", "gauge help",
        0, NULL);
    ogs_assert(sample3);
    ogs_metrics_gauge_set(sample3, 0);

    sample2 = ogs_metrics_create(OGS_METRICS_TYPE_GAUGE,
        "gauge_name", "gauge help",
        1, (const char * [][2]){
        { "label", "value" },
        });
    ogs_assert(sample2);
    ogs_metrics_gauge_set(sample2, 0);

    sample1 = ogs_metrics_create(OGS_METRICS_TYPE_GAUGE,
        "gauge_name", "gauge help",
        2, (const char * [][2]){
        { "label", "value" },
        { "label2", "value2" }
        });
    ogs_assert(sample1);
    ogs_metrics_gauge_set(sample1, 0);

    gauge_gnb_cnt = ogs_metrics_create(OGS_METRICS_TYPE_GAUGE,
        "amf_gnb_cnt", "Count of gNB's connected", 
        1, (const char * [][2]){
        {"amf_name", amf_self()->amf_name }
        });
    ogs_assert(gauge_gnb_cnt);

    gauge_ran_ue_cnt = ogs_metrics_create(OGS_METRICS_TYPE_GAUGE,
            "amf_ran_ue_cnt", "Count of RAN UE's",
        1, (const char * [][2]){
        {"amf_name", amf_self()->amf_name }
        });
    ogs_assert(gauge_ran_ue_cnt);


    return 0;
}
