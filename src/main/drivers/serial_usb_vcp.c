/*
 * This file is part of Cleanflight.
 *
 * Cleanflight is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Cleanflight is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Cleanflight.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdint.h>
#include <stdbool.h>

#include "platform.h"

#include "build/build_config.h"

#include "common/utils.h"
#include "drivers/io.h"

#if defined(STM32F4)
#include "usb_core.h"
#include "usbd_cdc_vcp.h"
#include "usb_io.h"
#elif defined(STM32F7)
#include "vcp_hal/usbd_cdc_interface.h"
#include "usb_io.h"
USBD_HandleTypeDef USBD_Device;
#elif defined(XMC4500_F100x1024)
#include "usbd_vcom.h"
#else
#include "usb_core.h"
#include "usb_init.h"
#include "hw_config.h"
#endif

#include "drivers/time.h"

#include "serial.h"
#include "serial_usb_vcp.h"


#define USB_TIMEOUT  50

static vcpPort_t vcpPort;

static void usbVcpSetBaudRate(serialPort_t *instance, uint32_t baudRate)
{
    UNUSED(instance);
    UNUSED(baudRate);

    // TODO implement
}

static void usbVcpSetMode(serialPort_t *instance, portMode_t mode)
{
    UNUSED(instance);
    UNUSED(mode);

    // TODO implement
}

static bool isUsbVcpTransmitBufferEmpty(const serialPort_t *instance)
{
    UNUSED(instance);
    return true;
}

static uint32_t usbVcpAvailable(const serialPort_t *instance)
{
    UNUSED(instance);

#ifndef XMC4500_F100x1024
    return CDC_Receive_BytesAvailable();
#else

    CDC_Device_USBTask(&USBD_VCOM_cdc_interface);

    return USBD_VCOM_BytesReceived();
#endif
}

static uint8_t usbVcpRead(serialPort_t *instance)
{
    UNUSED(instance);

    uint8_t buf[1];

#ifndef XMC4500_F100x1024
    while (true) {
        if (CDC_Receive_DATA(buf, 1))
            return buf[0];
    }
#else
    USBD_VCOM_ReceiveByte((int8_t*)&buf);

    CDC_Device_USBTask(&USBD_VCOM_cdc_interface);

    return buf[0];
#endif
}

static void usbVcpWriteBuf(serialPort_t *instance, const void *data, int count)
{
    UNUSED(instance);

#ifndef XMC4500_F100x1024
    if (!(usbIsConnected() && usbIsConfigured())) {
        return;
    }
#else
    if (!(xmc_device.IsConnected && xmc_device.IsPowered))
    	return;
#endif

#ifndef XMC4500_F100x1024
    uint32_t start = millis();
    const uint8_t *p = data;
    while (count > 0) {

        uint32_t txed = CDC_Send_DATA(p, count);
        count -= txed;
        p += txed;

        if (millis() - start > USB_TIMEOUT) {
            break;
        }
    }
#else
    USBD_VCOM_SendData(data, count);

    CDC_Device_USBTask(&USBD_VCOM_cdc_interface);

#endif
}

static bool usbVcpFlush(vcpPort_t *port)
{
    uint32_t count = port->txAt;
    port->txAt = 0;

    if (count == 0) {
        return true;
    }

#ifndef XMC4500_F100x1024
    if (!usbIsConnected() || !usbIsConfigured()) {
        return false;
    }
#else
    if (!(xmc_device.IsConnected && xmc_device.IsPowered))
    	return false;
#endif

    uint8_t *p = port->txBuf;

#ifndef XMC4500_F100x1024
    uint32_t start = millis();
    while (count > 0) {
        uint32_t txed = CDC_Send_DATA(p, count);

        count -= txed;
        p += txed;

        if (millis() - start > USB_TIMEOUT) {
            break;
        }
    }
    return count == 0;
#else
    USBD_VCOM_SendData((int8_t*)p, count);

    CDC_Device_USBTask(&USBD_VCOM_cdc_interface);

    return 1;
#endif
}

static void usbVcpWrite(serialPort_t *instance, uint8_t c)
{
    vcpPort_t *port = container_of(instance, vcpPort_t, port);

    port->txBuf[port->txAt++] = c;
    if (!port->buffering || port->txAt >= ARRAYLEN(port->txBuf)) {
        usbVcpFlush(port);
    }
}

static void usbVcpBeginWrite(serialPort_t *instance)
{
    vcpPort_t *port = container_of(instance, vcpPort_t, port);
    port->buffering = true;
}

uint32_t usbTxBytesFree()
{
#ifndef XMC4500_F100x1024
    return CDC_Send_FreeBytes();
#else
    return 255;
#endif
}

static void usbVcpEndWrite(serialPort_t *instance)
{
    vcpPort_t *port = container_of(instance, vcpPort_t, port);
    port->buffering = false;
    usbVcpFlush(port);
}

static const struct serialPortVTable usbVTable[] = {
    {
        .serialWrite = usbVcpWrite,
        .serialTotalRxWaiting = usbVcpAvailable,
        .serialTotalTxFree = usbTxBytesFree,
        .serialRead = usbVcpRead,
        .serialSetBaudRate = usbVcpSetBaudRate,
        .isSerialTransmitBufferEmpty = isUsbVcpTransmitBufferEmpty,
        .setMode = usbVcpSetMode,
        .writeBuf = usbVcpWriteBuf,
        .beginWrite = usbVcpBeginWrite,
        .endWrite = usbVcpEndWrite
    }
};

serialPort_t *usbVcpOpen(void)
{
    vcpPort_t *s;

#if defined(STM32F4)
    usbGenerateDisconnectPulse();

    IOInit(IOGetByTag(IO_TAG(PA11)), OWNER_USB, 0);
    IOInit(IOGetByTag(IO_TAG(PA12)), OWNER_USB, 0);
    USBD_Init(&USB_OTG_dev, USB_OTG_FS_CORE_ID, &USR_desc, &USBD_CDC_cb, &USR_cb);
#elif defined(STM32F7)
    usbGenerateDisconnectPulse();

    IOInit(IOGetByTag(IO_TAG(PA11)), OWNER_USB, 0);
    IOInit(IOGetByTag(IO_TAG(PA12)), OWNER_USB, 0);
    /* Init Device Library */
    USBD_Init(&USBD_Device, &VCP_Desc, 0);

    /* Add Supported Class */
    USBD_RegisterClass(&USBD_Device, USBD_CDC_CLASS);

    /* Add CDC Interface Class */
    USBD_CDC_RegisterInterface(&USBD_Device, &USBD_CDC_fops);

    /* Start Device Process */
    USBD_Start(&USBD_Device);
#elif defined(XMC4500_F100x1024)
    USBD_VCOM_Init(&USBD_VCOM_0);
    USBD_VCOM_Connect();
#else
    Set_System();
    Set_USBClock();
    USB_Init();
    USB_Interrupts_Config();
#endif

    s = &vcpPort;
    s->port.vTable = usbVTable;

    return (serialPort_t *)s;
}

//uint32_t usbVcpGetBaudRate(serialPort_t *instance)
//{
//    UNUSED(instance);
//
//    return CDC_BaudRate();
//}
