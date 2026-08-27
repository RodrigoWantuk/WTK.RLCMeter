#include "app/app_io_workspace.h"

#include <stdbool.h>
#include <stdio.h>

static int expect_true(bool condition, const char *message)
{
    if (!condition)
    {
        (void)fprintf(stderr, "FAIL: %s\n", message);
        return 1;
    }
    return 0;
}

static int expect_u32(uint32_t actual, uint32_t expected, const char *message)
{
    if (actual != expected)
    {
        (void)fprintf(stderr,
                      "FAIL: %s (got %lu expected %lu)\n",
                      message,
                      (unsigned long)actual,
                      (unsigned long)expected);
        return 1;
    }
    return 0;
}

int main(void)
{
    int failures = 0;
    app_io_workspace_t workspace;
    app_io_workspace_init(&workspace);

    failures += expect_u32(app_io_workspace_storage_size_bytes(),
                           HW_METROLOGY_RAW_BUFFER_BYTES,
                           "workspace storage is one canonical 3 KiB buffer");
    failures += expect_true(app_io_workspace_context_size_bytes() <=
                                (HW_METROLOGY_RAW_BUFFER_BYTES + 8u),
                            "workspace context has only small ownership overhead");
    failures += expect_true(app_io_workspace_owner(&workspace) == APP_IO_WORKSPACE_OWNER_FREE,
                            "workspace starts free");
    failures += expect_true(app_io_workspace_metrology_raw_words(&workspace) != NULL,
                            "raw view available");
    failures += expect_true(app_io_workspace_calibration_frame(&workspace) != NULL,
                            "calibration view available");
    failures += expect_u32((uint32_t)app_io_workspace_metrology_word_count(),
                           HW_METROLOGY_RAW_WORD_COUNT,
                           "raw word count exported");
    failures += expect_u32((uint32_t)app_io_workspace_calibration_frame_bytes(),
                           MEASUREMENT_CAL_MAX_FRAME_BYTES,
                           "calibration frame size exported");

    failures += expect_true(app_io_workspace_acquire(&workspace,
                                                     APP_IO_WORKSPACE_OWNER_METROLOGY) == BSP_STATUS_OK,
                            "metrology acquire succeeds");
    failures += expect_true(app_io_workspace_acquire(&workspace,
                                                     APP_IO_WORKSPACE_OWNER_CALIBRATION_STORE) == BSP_STATUS_BUSY,
                            "store blocked while metrology owns workspace");
    failures += expect_true(app_io_workspace_release(&workspace,
                                                     APP_IO_WORKSPACE_OWNER_CALIBRATION_STORE) == BSP_STATUS_BUSY,
                            "wrong owner cannot release");
    failures += expect_true(app_io_workspace_release(&workspace,
                                                     APP_IO_WORKSPACE_OWNER_METROLOGY) == BSP_STATUS_OK,
                            "metrology release succeeds");
    failures += expect_true(app_io_workspace_acquire(&workspace,
                                                     APP_IO_WORKSPACE_OWNER_CALIBRATION_STORE) == BSP_STATUS_OK,
                            "store acquire succeeds after release");
    failures += expect_true(app_io_workspace_acquire(&workspace,
                                                     APP_IO_WORKSPACE_OWNER_METROLOGY) == BSP_STATUS_BUSY,
                            "metrology blocked while store owns workspace");
    failures += expect_true(app_io_workspace_release(&workspace,
                                                     APP_IO_WORKSPACE_OWNER_CALIBRATION_STORE) == BSP_STATUS_OK,
                            "store release succeeds");

    return failures;
}
