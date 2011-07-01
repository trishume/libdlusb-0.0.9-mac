/* $Id: usb_libusb.c,v 1.8 2005/09/13 02:32:14 geni Exp $
 *
 * Copyright (c) 2005 Huidae Cho
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "dlusb.h"

//#define LIBUSB_HAS_DETACH_KERNEL_DRIVER_NP

int
open_dev(dldev_t *dev)
{
	dev->usb.handle = hid_open(VENDOR_ID, PRODUCT_ID, NULL);
	if(dev->usb.handle == NULL) {
		ERROR("could not open device in open_dev()");
		return -1;
	}
	return 0;
}

int
close_dev(dldev_t *dev)
{
	if(!dev->usb.handle){
		ERROR("no device to close");
		return -1;
	}
	hid_close(dev->usb.handle);

	return 0;
}

int
send_usb_packet(dldev_t *dev, u8 *packet, u16 len)
{
	if(hid_write(dev->usb.handle, packet, len) == -1){
		ERROR("could not send packet");
		return -1;
	}

	return 0;
}

int
read_usb_packet(dldev_t *dev, u8 *packet, u16 len)
{
	if(hid_read(dev->usb.handle, packet, len) == -1){
		ERROR("can't read packet");
		return -1;
	}

	return 0;
}
