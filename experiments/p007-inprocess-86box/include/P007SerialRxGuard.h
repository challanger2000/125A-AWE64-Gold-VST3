#ifndef P007_SERIAL_RX_GUARD_H
#define P007_SERIAL_RX_GUARD_H
/* P007BA: receive-side flow control for the ORIGINAL emulated 16550.
 * The plugin's COM2 wire may buffer far more than 16 bytes. The adapter
 * must NOT dequeue another byte while the real 16550 receive FIFO is
 * already full. Never modify the original UART or Creative driver.
 * This helper is intentionally C/C++ compatible for deterministic tests.
 */
static inline int p007_serial_rx_has_room(
    int fifo_enabled,unsigned fifo_count,
    int data_ready,int staging_pending) {
    if(staging_pending)return 0;
    if(fifo_enabled)return fifo_count<16u;
    return !data_ready;
}
#endif
