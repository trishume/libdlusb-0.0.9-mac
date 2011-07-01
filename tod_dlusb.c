/* $Id: tod_dlusb.c,v 1.6 2005/10/03 18:37:18 geni Exp $
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
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/stat.h>
#include "dlusb.h"
#include "misc.h"

static void usage(void);

static char *dev_file = "/dev/uhid0";

int
main(int argc, char *argv[])
{
	int ret = 1, i;
	char *tz[3], *date = NULL, *hr24 = NULL, *week_no = NULL, *dst = NULL,
	     *euro = NULL, *observe_dst = NULL, *adjust = NULL;
	u8 date_format_;
	time_t dsec[3], adjust_sec = 0;
	time_t tloc, tloc2;
	struct tm *tm;
	dldev_t dev;
	tod_t db;

	/* for data security */
	/*
	umask(S_IRWXG | S_IRWXO);
	*/

	while((i = getopt(argc, argv, "hd:")) != -1){
		switch(i){
		case 'd':
			dev_file = optarg;
			break;
		case 'h':
		default:
			usage();
			break;
		}
	}
	argc -= optind;
	argv += optind;

#ifdef USB_USBHID
	dev.usb.file = dev_file;
#endif

	memset(tz, 0, sizeof(tz));
	BEGIN_OPT()
		OPT("tz1", tz[0])
		OPT("tz2", tz[1])
		OPT("tz3", tz[2])
		OPT("date", date)
		OPT("24hr", hr24)
		OPT("week_no", week_no)
		OPT("dst", dst)
		OPT("euro", euro)
		OPT("observe_dst", observe_dst)
		/* non-standard options */
		OPT("adjust", adjust)
	END_OPT()

	if(date){
		for(i = 0; i < 3 && strcmp(date, date_format[i]); i++);
		if(i == 3){
			fprintf(stderr, "%s: format error!\n", date);
			usage();
		}
		date_format_ = i;
	}else
		date_format_ = mdy;

	for(i = 0; i < 3; i++){
		dsec[i] = 0;
		if(tz[i] && tz[i][0]){
			char buf[10];

			strncpy(buf, tz[i], 9);
			buf[9] = 0;
			if(strlen(buf) != 9 ||
					(buf[3] != '+' && buf[3] != '-')){
				fprintf(stderr, "%s: format error!\n", tz[i]);
				usage();
			}
			buf[6] = 0;
			dsec[i] = atoi(buf+3) * 3600 + atoi(buf+7) * 60;
		}
	}
	if(adjust && adjust[0]){
		char buf[10];

		strncpy(buf, adjust, 9);
		buf[9] = 0;
		if(strlen(buf) != 9 || (buf[0] != '+' && buf[0] != '-')){
			fprintf(stderr, "%s: format error!\n", adjust);
			usage();
		}
		buf[3] = buf[6] = 0;
		adjust_sec = atoi(buf) * 3600 + atoi(buf+4) * 60 + atoi(buf+7);
	}

	if(open_dev(&dev)){
		ERROR("open_dev");
		goto exit;
	}

	if(start_session(&dev)){
		ERROR("read_app_info");
		goto exit;
	}

/******************************************************************************/
#ifdef DEBUG
	for(i = 0; i < NUM_APPS; i++){
		if(!dev.app[i].acd.app_idx)
			continue;
		printf("%2d: %d%d%d%d%d%d%d%d %02x %02x %04x %04x %04x %04x %04x %04x %s\n", i,
				dev.app[i].acd.app_idx,
				dev.app[i].acd.code_loc,
				dev.app[i].acd.db_loc,
				dev.app[i].acd.code_invalid,
				dev.app[i].acd.db_modified,
				dev.app[i].acd.db_invalid,
				dev.app[i].acd.passwd_req,
				dev.app[i].acd.mode_name,

				dev.app[i].acb.app_type,
				dev.app[i].acb.app_inst,
				dev.app[i].acb.asd_addr,
				dev.app[i].acb.add_addr,
				dev.app[i].acb.state_mgr_addr,
				dev.app[i].acb.refresh_addr,
				dev.app[i].acb.banner_addr,
				dev.app[i].acb.code_addr,
				dev.app[i].banner
		);
	}
#endif
/******************************************************************************/

	time(&tloc);

	memset(&db, 0, sizeof(tod_t));

	for(i = 0; i < 3; i++){
		db.tod[i].primary = !i;	/* user */

		db.tod[i].update_tz_id = 1; /* -> tz_id */
		db.tod[i].update_display = 1; /* -> everything else? */
		db.tod[i].update_tz_name = 1; /* -> tz_name */
		db.tod[i].update_hms = 1; /* -> hour/minute/second */
		db.tod[i].update_mdy = 1; /* -> year/month/day */

		/* update_display */
		db.tod[i].date_format = date_format_; /* user */
		db.tod[i].hr24_format = (hr24 && strcmp(hr24, "no")); /* user */
		db.tod[i].display_week_no = (week_no && strcmp(week_no, "no"));
			/* user */
		db.tod[i].tz_in_dst = (dst && strcmp(dst, "no")); /* user */
		db.tod[i].euro_format = (euro && strcmp(euro, "no")); /* user */
		db.tod[i].tz_observes_dst =
			(observe_dst && strcmp(observe_dst, "no")); /* user */

		/* set if update_mdy = 1 */
		db.tod[i].tz_entered_set_state = 1;

		/* update_tz_name */
		if(tz[i] && tz[i][0])
			strncpy(db.tod[i].tz_name, tz[i], 3); /* user */
		else
			strncpy(db.tod[i].tz_name, "UTC", 3); /* user */

		/* update_tz_id */
		db.tod[i].tz_id = 0; /* used for WorldTime WristApp */

		tloc2 = tloc + dsec[i] + adjust_sec;
		tm = gmtime(&tloc2);

		/* update_hms */
		db.tod[i].second = tm->tm_sec; /* user */
		db.tod[i].minute = tm->tm_min; /* user */
		db.tod[i].hour = tm->tm_hour; /* user */

		/* update_mdy */
		db.tod[i].day = tm->tm_mday; /* user */
		db.tod[i].month = tm->tm_mon + 1; /* user */
		db.tod[i].year = tm->tm_year + 1900; /* user */
	}

	if(update_tod(&dev, &db)){
		ERROR("update_tod");
		goto end;
	}

/******************************************************************************/
end:
	if(end_session(&dev)){
		ERROR("end_session");
		goto exit;
	}

	ret = 0;
exit:
	close_dev(&dev);

	exit(ret);
}

static void
usage(void)
{
	fprintf(stderr,
"usage: tod_dlusb [OPTIONS]\n"
"\n"
"  -h		this help\n"
#ifdef USB_USBHID
"  -d device	device file for Timex Data Link USB watch (default: %s)\n"
#endif
"  tz1=...	time zone 1, primary (e.g., CST-06:00)\n"
"  tz2=...	time zone 2\n"
"  tz3=...	time zone 3\n"
"  date=...	date format (dmy, ymd, mdy)\n"
"  24hr=...	24 hour format (yes, no)\n"
"  week_no=...	display week number (yes, no)\n"
/* TODO: dst for each time zone */
#if 0
"  dst=...	dst (yes, no)\n"
"  euro=...	euro format (yes, no)\n"
"  observe_dst=...	observe dst (yes, no)\n"
#endif
"  adjust=...	adjust time (e.g., +00:05:00)\n"
"\n"
"libdlusb version: %s\n",
#ifdef USB_USBHID
	dev_file,
#endif
	VERSION
	);
	exit(1);
}
