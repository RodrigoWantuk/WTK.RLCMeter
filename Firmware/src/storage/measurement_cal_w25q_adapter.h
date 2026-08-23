#ifndef WTK_MEASUREMENT_CAL_W25Q_ADAPTER_H
#define WTK_MEASUREMENT_CAL_W25Q_ADAPTER_H

#include "drivers/w25q.h"
#include "measurement/measurement_calibration_store.h"

measurement_cal_store_io_t measurement_cal_w25q_store_io(w25q_device_t *flash);
bsp_status_t measurement_cal_w25q_status_to_bsp(w25q_status_t status);

#endif
