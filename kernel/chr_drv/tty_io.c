#include <linux/tty.h>

void tty_init() {
    con_init();
}
/// @brief 向 TTY 设备写入数据 @param channel TTY 通道号 @param buf 要写入的缓冲区 @param nr 字节数
void tty_write(unsigned channel, char* buf, int nr) {
    console_print(buf, nr);
}