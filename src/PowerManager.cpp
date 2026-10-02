// C library headers
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
// Linux headers
#include <fcntl.h>   // Contains file controls like O_RDWR
#include <errno.h>   // Error integer and strerror() function
#include <termios.h> // Contains POSIX terminal control definitions
#include <unistd.h>  // write(), read(), close()
#include <sys/select.h>

static int serial_fd = -1;

/*
 * The STC8 returns battery voltage in millivolts.  Set these at build time
 * after comparing the display with a multimeter:
 *   displayed = raw * BATTERY_VOLTAGE_SCALE_PERMILLE / 1000
 *             + BATTERY_VOLTAGE_OFFSET_MV
 */
#ifndef BATTERY_VOLTAGE_SCALE_PERMILLE
#define BATTERY_VOLTAGE_SCALE_PERMILLE 1000
#endif
#ifndef BATTERY_VOLTAGE_OFFSET_MV
#define BATTERY_VOLTAGE_OFFSET_MV 0
#endif

int serialSetup()
{
    int serial_port;
    serial_port = open("/dev/ttyS3", O_RDWR | O_NOCTTY);
    if (serial_port < 0)
    {
        perror("open uart device error\n");
        return -1;
    }
    // Create new termios struct, we call it 'tty' for convention
    // No need for "= {0}" at the end as we'll immediately write the existing
    // config to this struct
    struct termios tty;

    // Read in existing settings, and handle any error
    // NOTE: This is important! POSIX states that the struct passed to tcsetattr()
    // must have been initialized with a call to tcgetattr() overwise behaviour
    // is undefined
    if (tcgetattr(serial_port, &tty) != 0)
    {
        printf("Error %i from tcgetattr: %s\n", errno, strerror(errno));
        close(serial_port);
        return -1;
    }
    tty.c_iflag &= ~(IGNBRK | BRKINT | ICRNL |
        INLCR | PARMRK | INPCK | ISTRIP | IXON);


    tty.c_oflag = 0;
    //
    // No line processing
    //
    // echo off, echo newline off, canonical mode off,
    // extended input processing off, signal chars off
    //
    tty.c_lflag &= ~(ECHO | ECHONL | ICANON | IEXTEN | ISIG);

    //
    // Turn off character processing
    //
    // clear current char size mask, no parity checking,
    // no output processing, force 8 bit input
    //
    tty.c_cflag &= ~(CSIZE | PARENB);
    tty.c_cflag |= CS8;

    //
    // One input byte is enough to return from read()
    // Inter-character timer off
    //
    tty.c_cc[VMIN] = 1;
    tty.c_cc[VTIME] = 0;
    cfsetispeed(&tty, B115200);
    cfsetospeed(&tty, B115200);
    if (tcsetattr(serial_port, TCSAFLUSH, &tty) != 0)
    {
        printf("Error %i from tcsetattr: %s\n", errno, strerror(errno));
        close(serial_port);
        return -1;
    }
    tcflush(serial_port, TCIOFLUSH); // Flush serial buffer
    return serial_port;
}

static int serialWrite(int fd, uint8_t command)
{
    if (fd < 0)
        return 0;
    int len;
    len = write(fd, &command, 1);
    if (len < 0)
        return 0;
    return 1;
}

static bool serialReadExact(uint8_t *buf, size_t size)
{
    if (serial_fd < 0)
        return false;

    size_t received = 0;
    while (received < size)
    {
        fd_set set;
        struct timeval timeout;
        FD_ZERO(&set);
        FD_SET(serial_fd, &set);
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000;

        int select_result = select(serial_fd + 1, &set, NULL, NULL, &timeout);
        if (select_result <= 0)
            return false;

        ssize_t len = read(serial_fd, buf + received, size - received);
        if (len <= 0)
            return false;
        received += (size_t)len;
    }
    return true;
}

#define SERIAL_CMD_READ_ADC 0x58
#define SERIAL_CMD_POWEROFF 0x6a
#define SERIAL_CMD_USBMODE_NORMAL 0x11
#define SERIAL_CMD_USBMODE_WIFI_CAM 0x12
#define SERIAL_CMD_USBMODE_DIRECT 0x13
#define SERIAL_CMD_IS_CHARGING 0x59

int16_t PowerManager_getBatteryVoltage()
{
    uint8_t buf[2];
    if (serial_fd < 0)
        return -1;

    /* Drop stale reply bytes only.  TCIOFLUSH here can discard the command. */
    tcflush(serial_fd, TCIFLUSH);
    if (!serialWrite(serial_fd, SERIAL_CMD_READ_ADC) || !serialReadExact(buf, 2))
        return -1;

    int32_t voltage = ((int32_t)buf[0] << 8) | buf[1];
    voltage = voltage * BATTERY_VOLTAGE_SCALE_PERMILLE / 1000 + BATTERY_VOLTAGE_OFFSET_MV;
    if (voltage < 0)
        voltage = 0;
    if (voltage > INT16_MAX)
        voltage = INT16_MAX;
    return (int16_t)voltage;
}

bool PowerManager_isCharging()
{
    uint8_t buf = 0;
    if (serial_fd < 0)
        return false;

    tcflush(serial_fd, TCIFLUSH);
    if (!serialWrite(serial_fd, SERIAL_CMD_IS_CHARGING) || !serialReadExact(&buf, 1))
        return false;
    return buf == 1;
}

void PowerManager_init()
{
    serial_fd = serialSetup();
}

#include <signal.h>
void PowerManager_powerOff()
{
    if (serial_fd < 0)
        return;
    serialWrite(serial_fd, SERIAL_CMD_POWEROFF);
    system("echo 1 > /tmp/poweroff");
    system("poweroff");
    // stop here
    exit(0);
}
