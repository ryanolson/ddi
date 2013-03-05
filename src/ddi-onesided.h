#ifndef __DDI_ONESIDED_H__
#define __DDI_ONESIDED_H__

#include "onesided.h"

#define DDI_MAX_DESCRIPTORS     128
#define DDI_MAX_REQUEST_SIZE    1024

#define DBUFFER

typedef struct ddi_onesided_msg_tag_s {
        cos_mdesc_t response_mdesc;
} msg_tag_t;


cos_request_t cos_req[DDI_MAX_DESCRIPTORS];


void ddi_onesided_init();
void ddi_onesided_finalize();

#endif
