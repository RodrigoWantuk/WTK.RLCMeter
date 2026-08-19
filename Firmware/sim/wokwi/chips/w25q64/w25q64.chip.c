#include "wokwi-api.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum
{
    W25Q64_CAPACITY_BYTES = 8u * 1024u * 1024u,
    W25Q64_SECTOR_SIZE = 4096u,
    W25Q64_PAGE_SIZE = 256u,
    W25Q64_TEST_SECTOR = W25Q64_CAPACITY_BYTES - W25Q64_SECTOR_SIZE,
    W25Q64_CMD_READ_JEDEC_ID = 0x9fu,
    W25Q64_CMD_READ_STATUS1 = 0x05u,
    W25Q64_CMD_WRITE_ENABLE = 0x06u,
    W25Q64_CMD_READ_DATA = 0x03u,
    W25Q64_CMD_FAST_READ = 0x0bu,
    W25Q64_CMD_PAGE_PROGRAM = 0x02u,
    W25Q64_CMD_SECTOR_ERASE = 0x20u,
    W25Q64_STATUS_BUSY = 0x01u,
    W25Q64_STATUS_WEL = 0x02u,
    W25Q64_PROGRAM_BUSY_US = 3000u,
    W25Q64_ERASE_BUSY_US = 15000u,
};

typedef enum
{
    PHASE_COMMAND = 0,
    PHASE_JEDEC,
    PHASE_STATUS,
    PHASE_ADDR,
    PHASE_FAST_DUMMY,
    PHASE_READ,
    PHASE_PROGRAM,
    PHASE_IGNORE,
} command_phase_t;

typedef struct
{
    pin_t cs;
    spi_dev_t spi;
    timer_t busy_timer;
    uint32_t jedec_mode_attr;
    uint32_t no_response_attr;
    uint8_t transfer_byte;
    uint8_t command;
    uint8_t address_bytes;
    uint8_t jedec_index;
    uint8_t status;
    uint8_t page_offset;
    uint16_t program_count;
    uint32_t address;
    command_phase_t phase;
    uint8_t sector[W25Q64_SECTOR_SIZE];
    uint8_t program_data[W25Q64_PAGE_SIZE];
} chip_state_t;

static bool no_response(const chip_state_t *chip)
{
    return attr_read(chip->no_response_attr) != 0u;
}

static void busy_done(void *user_data)
{
    chip_state_t *chip = (chip_state_t *)user_data;
    chip->status &= (uint8_t)~W25Q64_STATUS_BUSY;
}

static bool in_test_sector(uint32_t address)
{
    return (address >= W25Q64_TEST_SECTOR) && (address < W25Q64_CAPACITY_BYTES);
}

static uint8_t read_byte(const chip_state_t *chip, uint32_t address)
{
    if (in_test_sector(address))
    {
        return chip->sector[address - W25Q64_TEST_SECTOR];
    }

    return 0xffu;
}

static void start_busy(chip_state_t *chip, uint32_t micros)
{
    chip->status |= W25Q64_STATUS_BUSY;
    timer_start(chip->busy_timer, micros, false);
}

static void execute_erase(chip_state_t *chip)
{
    if (((chip->status & W25Q64_STATUS_WEL) == 0u) || ((chip->status & W25Q64_STATUS_BUSY) != 0u) ||
        (chip->address_bytes < 3u))
    {
        return;
    }

    const uint32_t sector_address = chip->address & ~(uint32_t)(W25Q64_SECTOR_SIZE - 1u);
    if (sector_address == W25Q64_TEST_SECTOR)
    {
        memset(chip->sector, 0xff, sizeof(chip->sector));
    }
    chip->status &= (uint8_t)~W25Q64_STATUS_WEL;
    start_busy(chip, W25Q64_ERASE_BUSY_US);
}

static void execute_program(chip_state_t *chip)
{
    if (((chip->status & W25Q64_STATUS_WEL) == 0u) || ((chip->status & W25Q64_STATUS_BUSY) != 0u) ||
        (chip->address_bytes < 3u) || (chip->program_count == 0u))
    {
        return;
    }

    for (uint16_t i = 0u; i < chip->program_count; i++)
    {
        const uint32_t address = chip->address + i;
        if (in_test_sector(address))
        {
            chip->sector[address - W25Q64_TEST_SECTOR] &= chip->program_data[i];
        }
    }
    chip->status &= (uint8_t)~W25Q64_STATUS_WEL;
    start_busy(chip, W25Q64_PROGRAM_BUSY_US);
}

static uint8_t jedec_byte(const chip_state_t *chip)
{
    static const uint8_t normal_jedec[3] = {0xefu, 0x40u, 0x17u};
    static const uint8_t bad_jedec[3] = {0x00u, 0x00u, 0x00u};
    const uint8_t *jedec = (attr_read(chip->jedec_mode_attr) == 0u) ? normal_jedec : bad_jedec;
    const uint8_t value = jedec[chip->jedec_index % 3u];
    return value;
}

static uint8_t process_byte(chip_state_t *chip, uint8_t value)
{
    if (no_response(chip))
    {
        return 0xffu;
    }

    switch (chip->phase)
    {
    case PHASE_COMMAND:
        chip->command = value;
        chip->address = 0u;
        chip->address_bytes = 0u;
        chip->jedec_index = 0u;
        chip->program_count = 0u;
        if ((chip->status & W25Q64_STATUS_BUSY) != 0u)
        {
            if (value == W25Q64_CMD_READ_STATUS1)
            {
                chip->phase = PHASE_STATUS;
                return chip->status;
            }
            chip->command = 0u;
            chip->phase = PHASE_IGNORE;
            return 0xffu;
        }
        if (value == W25Q64_CMD_READ_JEDEC_ID)
        {
            chip->phase = PHASE_JEDEC;
            const uint8_t response = jedec_byte(chip);
            chip->jedec_index++;
            return response;
        }
        else if (value == W25Q64_CMD_READ_STATUS1)
        {
            chip->phase = PHASE_STATUS;
            return chip->status;
        }
        else if (value == W25Q64_CMD_WRITE_ENABLE)
        {
            chip->status |= W25Q64_STATUS_WEL;
        }
        else if ((value == W25Q64_CMD_READ_DATA) || (value == W25Q64_CMD_FAST_READ) ||
                 (value == W25Q64_CMD_PAGE_PROGRAM) || (value == W25Q64_CMD_SECTOR_ERASE))
        {
            chip->phase = PHASE_ADDR;
        }
        return 0xffu;
    case PHASE_JEDEC:
    {
        const uint8_t response = jedec_byte(chip);
        chip->jedec_index++;
        return response;
    }
    case PHASE_STATUS:
        return chip->status;
    case PHASE_ADDR:
        chip->address = ((chip->address << 8u) | (uint32_t)value) & 0x00ffffffu;
        chip->address_bytes++;
        if (chip->address_bytes >= 3u)
        {
            if (chip->command == W25Q64_CMD_FAST_READ)
            {
                chip->phase = PHASE_FAST_DUMMY;
            }
            else if (chip->command == W25Q64_CMD_READ_DATA)
            {
                chip->phase = PHASE_READ;
                const uint8_t response = read_byte(chip, chip->address);
                chip->address = (chip->address + 1u) & 0x00ffffffu;
                return response;
            }
            else if (chip->command == W25Q64_CMD_PAGE_PROGRAM)
            {
                chip->phase = PHASE_PROGRAM;
                chip->page_offset = (uint8_t)(chip->address & (W25Q64_PAGE_SIZE - 1u));
            }
        }
        return 0xffu;
    case PHASE_FAST_DUMMY:
        (void)value;
        chip->phase = PHASE_READ;
        {
            const uint8_t response = read_byte(chip, chip->address);
            chip->address = (chip->address + 1u) & 0x00ffffffu;
            return response;
        }
    case PHASE_READ:
    {
        (void)value;
        const uint8_t response = read_byte(chip, chip->address);
        chip->address = (chip->address + 1u) & 0x00ffffffu;
        return response;
    }
    case PHASE_PROGRAM:
        if ((chip->program_count < (uint16_t)W25Q64_PAGE_SIZE) &&
            ((uint16_t)chip->page_offset + chip->program_count < (uint16_t)W25Q64_PAGE_SIZE))
        {
            chip->program_data[chip->program_count] = value;
            chip->program_count++;
        }
        return 0xffu;
    case PHASE_IGNORE:
        return 0xffu;
    default:
        return 0xffu;
    }
}

static void reset_transaction(chip_state_t *chip)
{
    chip->transfer_byte = 0xffu;
    chip->command = 0u;
    chip->address = 0u;
    chip->address_bytes = 0u;
    chip->jedec_index = 0u;
    chip->page_offset = 0u;
    chip->program_count = 0u;
    chip->phase = PHASE_COMMAND;
}

static void spi_done(void *user_data, uint8_t *buffer, uint32_t count)
{
    chip_state_t *chip = (chip_state_t *)user_data;
    if ((count == 1u) && (pin_read(chip->cs) == LOW))
    {
        chip->transfer_byte = process_byte(chip, buffer[0]);
        buffer[0] = chip->transfer_byte;
        spi_start(chip->spi, buffer, 1u);
    }
}

static void cs_changed(void *user_data, pin_t pin, uint32_t value)
{
    (void)pin;
    chip_state_t *chip = (chip_state_t *)user_data;
    if (value == LOW)
    {
        reset_transaction(chip);
        chip->transfer_byte = 0xffu;
        spi_start(chip->spi, &chip->transfer_byte, 1u);
    }
    else
    {
        spi_stop(chip->spi);
        if ((chip->command == W25Q64_CMD_PAGE_PROGRAM) && (chip->address_bytes >= 3u))
        {
            execute_program(chip);
        }
        else if ((chip->command == W25Q64_CMD_SECTOR_ERASE) && (chip->address_bytes >= 3u))
        {
            execute_erase(chip);
        }
        reset_transaction(chip);
    }
}

void chip_init(void)
{
    chip_state_t *chip = malloc(sizeof(*chip));
    if (chip == NULL)
    {
        return;
    }

    memset(chip, 0, sizeof(*chip));
    memset(chip->sector, 0xff, sizeof(chip->sector));

    chip->cs = pin_init("CS", INPUT_PULLUP);
    chip->jedec_mode_attr = attr_init("jedecMode", 0u);
    chip->no_response_attr = attr_init("noResponse", 0u);
    reset_transaction(chip);

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

    timer_config_t timer_config = {
        .callback = busy_done,
        .user_data = chip,
    };
    chip->busy_timer = timer_init(&timer_config);
}
