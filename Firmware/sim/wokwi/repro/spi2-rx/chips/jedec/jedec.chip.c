#include "wokwi-api.h"

#include <stdint.h>
#include <stdio.h>

static const uint8_t k_jedec[3] = {0xEFu, 0x40u, 0x17u};

typedef struct
{
    pin_t cs;
    spi_dev_t spi;
    uint8_t transfer_byte;
    uint8_t index;
    bool saw_command;
} chip_state_t;

static chip_state_t g_chip;

static void spi_done(void *user_data, uint8_t *buffer, uint32_t count)
{
    chip_state_t *chip = (chip_state_t *)user_data;
    if (pin_read(chip->cs) != LOW)
    {
        return;
    }

    for (uint32_t i = 0; i < count; i++)
    {
        const uint8_t rx = buffer[i];
        uint8_t tx_next = 0xFFu;
        if (!chip->saw_command)
        {
            chip->saw_command = (rx == 0x9Fu);
            tx_next = chip->saw_command ? k_jedec[0] : 0xFFu;
            chip->index = 0u;
        }
        else if (chip->index < 2u)
        {
            chip->index++;
            tx_next = k_jedec[chip->index];
        }
        printf("RX[%u] = %02X\n", (unsigned)i, (unsigned)rx);
        printf("TX_NEXT = %02X\n", (unsigned)tx_next);
        buffer[i] = tx_next;
        chip->transfer_byte = tx_next;
    }

    spi_start(chip->spi, &chip->transfer_byte, 1u);
}

static void cs_changed(void *user_data, pin_t pin, uint32_t value)
{
    (void)pin;
    chip_state_t *chip = (chip_state_t *)user_data;
    if (value == LOW)
    {
        chip->saw_command = false;
        chip->index = 0u;
        chip->transfer_byte = 0xFFu;
        printf("CS LOW\n");
        spi_start(chip->spi, &chip->transfer_byte, 1u);
    }
    else
    {
        spi_stop(chip->spi);
        printf("CS HIGH\n");
    }
}

void chip_init(void)
{
    chip_state_t *chip = &g_chip;
    chip->cs = pin_init("CS", INPUT_PULLUP);
    spi_config_t spi_config = {
        .sck = pin_init("SCK", INPUT),
        .mosi = pin_init("MOSI", INPUT),
        .miso = pin_init("MISO", INPUT),
        .mode = 0,
        .done = spi_done,
        .user_data = chip,
    };
    chip->spi = spi_init(&spi_config);
    pin_watch_config_t watch_config = {
        .edge = BOTH,
        .pin_change = cs_changed,
        .user_data = chip,
    };
    pin_watch(chip->cs, &watch_config);
}
