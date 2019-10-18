/*
 * WARNING: be careful changing this code, it is very timing dependent
 */

#include "quantum.h"
#include "serial.h"
#include "wait.h"

#include "ch.h"
#include "hal.h"

// default wait implementation cannot be called within interrupt
// this method seems to be more accurate than GPT timers
#undef wait_us
#define wait_us(x) chSysPolledDelayX(US2RTC(STM32_SYSCLK, x))

// helper to convert GPIOB to EXT_MODE_GPIOB
#define ExtModePort(pin) (((uint32_t)PAL_PORT(pin) & 0x0000FF00U) >> 6)

// Serial pulse period in microseconds. Its probably a bad idea to lower this value.
#define SERIAL_DELAY 48
#define SERIAL_FUDGE 2

inline static void serial_delay(void) { wait_us(SERIAL_DELAY); }
inline static void serial_delay_half(void) { wait_us(SERIAL_DELAY / 2); }
inline static void serial_delay_blip(void) { wait_us(1); }
inline static void serial_output(void) { setPinOutput(SOFT_SERIAL_PIN); }
inline static void serial_input(void) { setPinInputHigh(SOFT_SERIAL_PIN); }
inline static bool serial_read_pin(void) { return !!readPin(SOFT_SERIAL_PIN); }
inline static void serial_low(void) { writePinLow(SOFT_SERIAL_PIN); }
inline static void serial_high(void) { writePinHigh(SOFT_SERIAL_PIN); }

void interrupt_handler(EXTDriver *extp, expchannel_t channel);

static SSTD_t *Transaction_table      = NULL;
static uint8_t Transaction_table_size = 0;

void soft_serial_initiator_init(SSTD_t *sstd_table, int sstd_table_size) {
    Transaction_table      = sstd_table;
    Transaction_table_size = (uint8_t)sstd_table_size;

    serial_output();
    serial_high();
}

void soft_serial_target_init(SSTD_t *sstd_table, int sstd_table_size) {
    Transaction_table      = sstd_table;
    Transaction_table_size = (uint8_t)sstd_table_size;

    serial_input();

    static EXTConfig extcfg = {0};

    static EXTChannelConfig ext_clock_channel_config = {EXT_CH_MODE_FALLING_EDGE | EXT_CH_MODE_AUTOSTART | ExtModePort(SOFT_SERIAL_PIN), interrupt_handler};
    extStart(&EXTD1, &extcfg); /* activate config, to be able to select the appropriate channel */
    extSetChannelModeI(&EXTD1, PAL_PAD(SOFT_SERIAL_PIN), &ext_clock_channel_config);
}

// Used by the master to synchronize timing with the slave.
static void sync_recv(void) {
    serial_input();
    // This shouldn't hang if the slave disconnects because the
    // serial line will float to high if the slave does disconnect.
    while (!serial_read_pin()) {
    }

    serial_delay_half();
}

// Used by the slave to send a synchronization signal to the master.
static void sync_send(void) {
    serial_output();

    serial_low();
    serial_delay();

    serial_high();
}

// Reads a byte from the serial line
static uint8_t serial_read_byte(void) {
    uint8_t byte = 0;
    serial_input();
    for (uint8_t i = 0; i < 8; ++i) {
        byte = (byte << 1) | serial_read_pin();
        wait_us(SERIAL_DELAY);
    }

    return byte;
}

// Sends a byte with MSB ordering
static void serial_write_byte(uint8_t data) {
    uint8_t b = 8;
    serial_output();
    while (b--) {
        if (data & (1 << b)) {
            serial_high();
        } else {
            serial_low();
        }
        serial_delay();
        wait_us(SERIAL_FUDGE);
    }
}

// interrupt handle to be used by the slave device
void interrupt_handler(EXTDriver *extp, expchannel_t channel) {
    chSysLockFromISR();
    extChannelDisableI(&EXTD1, PAL_PAD(SOFT_SERIAL_PIN));

    sync_send();

    uint8_t checksum = 0;
    for (int i = 0; i < Transaction_table->target2initiator_buffer_size; ++i) {
        serial_write_byte(Transaction_table->initiator2target_buffer[i]);
        sync_send();
        checksum += Transaction_table->target2initiator_buffer[i];
    }
    serial_write_byte(checksum ^ 7);
    sync_send();

    // wait for the sync to finish sending
    serial_delay();

    // end transaction
    serial_input();

    extChannelEnableI(&EXTD1, PAL_PAD(SOFT_SERIAL_PIN));
    chSysUnlockFromISR();
}

/////////
//  start transaction by initiator
//
// int  soft_serial_transaction(int sstd_index)
//
// Returns:
//    TRANSACTION_END
//    TRANSACTION_NO_RESPONSE
//    TRANSACTION_DATA_ERROR
// this code is very time dependent, so we need to disable interrupts
int soft_serial_transaction(void) {
    // this code is very time dependent, so we need to disable interrupts
    chSysLock();

    // signal to the slave that we want to start a transaction
    serial_output();
    serial_low();
    serial_delay_blip();

    // wait for the slaves response
    serial_input();
    serial_high();
    serial_delay();

    // check if the slave is present
    if (serial_read_pin()) {
        // slave failed to pull the line low, assume not present
        chSysUnlock();
        return TRANSACTION_NO_RESPONSE;
    }

    // if the slave is present syncronize with it
    sync_recv();

    uint8_t checksum_computed = 0;
    // receive data from the slave
    for (int i = 0; i < Transaction_table->target2initiator_buffer_size; ++i) {
        Transaction_table->initiator2target_buffer[i] = serial_read_byte();
        sync_recv();
        // dprintf("serial::data[%08b]\n", serial_slave_buffer[i]);
        checksum_computed += Transaction_table->initiator2target_buffer[i];
    }
    checksum_computed ^= 7;
    uint8_t checksum_received = serial_read_byte();

    sync_recv();

    if ((checksum_computed) != (checksum_received)) {
        dprintf("serial::FAIL[%u,%u]\n", checksum_computed, checksum_received);

        serial_output();
        serial_high();

        chSysUnlock();
        return TRANSACTION_DATA_ERROR;
    }

    // always, release the line when not in use
    serial_output();
    serial_high();

    chSysUnlock();
    return TRANSACTION_END;
}
