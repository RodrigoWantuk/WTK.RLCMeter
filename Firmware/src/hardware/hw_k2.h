#ifndef WTK_HW_K2_H
#define WTK_HW_K2_H

#include <stdbool.h>

#include "bsp/bsp_status.h"

typedef enum
{
    HW_LOWZ_BANK_MODE_FIXED_0R_LINK = 0,
} hw_lowz_bank_mode_t;

typedef struct
{
    bool populated;
    hw_lowz_bank_mode_t lowz_bank_mode;
} hw_k2_topology_t;

typedef bsp_status_t (*hw_k2_write_cmd_fn)(bool high, void *user_data);

typedef struct
{
    hw_k2_write_cmd_fn write_cmd;
    void *user_data;
} hw_k2_io_t;

typedef struct
{
    hw_k2_io_t io;
    hw_k2_topology_t topology;
} hw_k2_t;

bsp_status_t hw_k2_init(hw_k2_t *k2, const hw_k2_io_t *io);
bsp_status_t hw_k2_request_physical_switch(hw_k2_t *k2);
hw_k2_topology_t hw_k2_topology(const hw_k2_t *k2);
const char *hw_lowz_bank_mode_string(hw_lowz_bank_mode_t mode);

#endif
