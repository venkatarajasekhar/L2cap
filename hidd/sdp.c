/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2003-2008  Marcel Holtmann <marcel@holtmann.org>
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/socket.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/l2cap.h>
#include <bluetooth/sdp.h>
#include <bluetooth/sdp_lib.h>
#include <bluetooth/hidp.h>

#include "textfile.h"
#include "hidd.h"

static int store_device_info(const bdaddr_t *src, const bdaddr_t *dst, struct hidp_connadd_req *req)
{
	char filename[PATH_MAX + 1], addr[18], *str, *desc;
	int i, err, size;

	ba2str(src, addr);
	perror("unimplemented");
	return -EIO;
//	create_name(filename, PATH_MAX, STORAGEDIR, addr, "hidd");

	size = 15 + 3 + 3 + 5 + (req->rd_size * 2) + 1 + 9 + strlen(req->name) + 2;
	str = malloc(size);
	if (!str)
		return -ENOMEM;

	desc = malloc((req->rd_size * 2) + 1);
	if (!desc) {
		free(str);
		return -ENOMEM;
	}

	memset(desc, 0, (req->rd_size * 2) + 1);
	for (i = 0; i < req->rd_size; i++)
		sprintf(desc + (i * 2), "%2.2X", req->rd_data[i]);

	snprintf(str, size - 1, "%04X:%04X:%04X %02X %02X %04X %s %08X %s",
			req->vendor, req->product, req->version,
			req->subclass, req->country, req->parser, desc,
			req->flags, req->name);

	free(desc);

	create_file(filename, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

	ba2str(dst, addr);
	err = textfile_put(filename, addr, str);

	free(str);

	return err;
}

int get_stored_device_info(const bdaddr_t *src, const bdaddr_t *dst, struct hidp_connadd_req *req)
{
	char filename[PATH_MAX + 1], addr[18], tmp[3], *str, *desc;
	unsigned int vendor, product, version, subclass, country, parser, pos;
	int i;

	desc = malloc(4096);
	if (!desc)
		return -ENOMEM;

	memset(desc, 0, 4096);

	ba2str(src, addr);
	perror("unimplemented");
	return -EIO;
//	create_name(filename, PATH_MAX, STORAGEDIR, addr, "hidd");

	ba2str(dst, addr);
	str = textfile_get(filename, addr);
	if (!str) {
		free(desc);
		return -EIO;
	}

	sscanf(str, "%04X:%04X:%04X %02X %02X %04X %4095s %08X %n",
			&vendor, &product, &version, &subclass, &country,
			&parser, desc, &req->flags, &pos);

	free(str);

	req->vendor   = vendor;
	req->product  = product;
	req->version  = version;
	req->subclass = subclass;
	req->country  = country;
	req->parser   = parser;

	snprintf(req->name, 128, str + pos);

	req->rd_size = strlen(desc) / 2;
	req->rd_data = malloc(req->rd_size);
	if (!req->rd_data) {
		free(desc);
		return -ENOMEM;
	}

	memset(tmp, 0, sizeof(tmp));
	for (i = 0; i < req->rd_size; i++) {
		memcpy(tmp, desc + (i * 2), 2);
		req->rd_data[i] = (uint8_t) strtol(tmp, NULL, 16);
	}

	free(desc);

	return 0;
}

int get_sdp_device_info(const bdaddr_t *src, const bdaddr_t *dst, struct hidp_connadd_req *req)
{
	struct sockaddr_l2 addr;
	socklen_t addrlen;
	bdaddr_t bdaddr;
	uint32_t range = 0x0000ffff;
	sdp_session_t *s;
	sdp_list_t *search, *attrid, *pnp_rsp, *hid_rsp;
	sdp_record_t *rec;
	sdp_data_t *pdlist, *pdlist2;
	uuid_t svclass;
	int err;

	s = sdp_connect(src, dst, SDP_RETRY_IF_BUSY | SDP_WAIT_ON_CLOSE);
	if (!s)
		return -1;

	sdp_uuid16_create(&svclass, PNP_INFO_SVCLASS_ID);
	search = sdp_list_append(NULL, &svclass);
	attrid = sdp_list_append(NULL, &range);

	err = sdp_service_search_attr_req(s, search,
					SDP_ATTR_REQ_RANGE, attrid, &pnp_rsp);

	sdp_list_free(search, NULL);
	sdp_list_free(attrid, NULL);

	sdp_uuid16_create(&svclass, HID_SVCLASS_ID);
	search = sdp_list_append(NULL, &svclass);
	attrid = sdp_list_append(NULL, &range);

	err = sdp_service_search_attr_req(s, search,
					SDP_ATTR_REQ_RANGE, attrid, &hid_rsp);

	sdp_list_free(search, NULL);
	sdp_list_free(attrid, NULL);

	memset(&addr, 0, sizeof(addr));
	addrlen = sizeof(addr);

	if (getsockname(s->sock, (struct sockaddr *) &addr, &addrlen) < 0)
		bacpy(&bdaddr, src);
	else
		bacpy(&bdaddr, &addr.l2_bdaddr);

	sdp_close(s);

	if (err || !hid_rsp)
		return -1;

	if (pnp_rsp) {
		rec = (sdp_record_t *) pnp_rsp->data;

		pdlist = sdp_data_get(rec, 0x0201);
		req->vendor = pdlist ? pdlist->val.uint16 : 0x0000;

		pdlist = sdp_data_get(rec, 0x0202);
		req->product = pdlist ? pdlist->val.uint16 : 0x0000;

		pdlist = sdp_data_get(rec, 0x0203);
		req->version = pdlist ? pdlist->val.uint16 : 0x0000;

		sdp_record_free(rec);
	}

	rec = (sdp_record_t *) hid_rsp->data;

	pdlist = sdp_data_get(rec, 0x0101);
	pdlist2 = sdp_data_get(rec, 0x0102);
	if (pdlist) {
		if (pdlist2) {
			if (strncmp(pdlist->val.str, pdlist2->val.str, 5)) {
				strncpy(req->name, pdlist2->val.str, sizeof(req->name) - 1);
				strcat(req->name, " ");
			}
			strncat(req->name, pdlist->val.str,
					sizeof(req->name) - strlen(req->name));
		} else
			strncpy(req->name, pdlist->val.str, sizeof(req->name));
	} else {
		pdlist2 = sdp_data_get(rec, 0x0100);
		if (pdlist2)
			strncpy(req->name, pdlist2->val.str, sizeof(req->name));
	}

	pdlist = sdp_data_get(rec, 0x0201);
	req->parser = pdlist ? pdlist->val.uint16 : 0x0100;

	pdlist = sdp_data_get(rec, 0x0202);
	req->subclass = pdlist ? pdlist->val.uint8 : 0;

	pdlist = sdp_data_get(rec, 0x0203);
	req->country = pdlist ? pdlist->val.uint8 : 0;

	pdlist = sdp_data_get(rec, 0x0206);
	if (pdlist) {
		pdlist = pdlist->val.dataseq;
		pdlist = pdlist->val.dataseq;
		pdlist = pdlist->next;

		req->rd_data = malloc(pdlist->unitSize);
		if (req->rd_data) {
			memcpy(req->rd_data, (unsigned char *) pdlist->val.str, pdlist->unitSize);
			req->rd_size = pdlist->unitSize;
		}
	}

	sdp_record_free(rec);

	if (bacmp(&bdaddr, BDADDR_ANY))
		store_device_info(&bdaddr, dst, req);

	return 0;
}
