/* $Id: schedule.c,v 1.7 2005/10/06 19:21:22 geni Exp $
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
#include <ctype.h>
#include "dlusb.h"

static char *schedule_msg = NULL;
static u8 grp_type = 0;

void
set_schedule_msg(char *msg)
{
	int l;

	if(schedule_msg)
		free(schedule_msg);
	l = strlen(msg);
	l = (l > 14 ? 14 : l);
	schedule_msg = (char *)malloc(l + 1);
	strncpy(schedule_msg, msg, l);
	schedule_msg[l] = 0;

	return;
}

void
set_schedule_grp_type(u8 type)
{
	if(type > 0 && type < 4)
		grp_type = type;

	return;
}

void
update_schedule_msg(schedule_t *db)
{
	int i, l;

	if(!schedule_msg){
		schedule_msg = (char *)malloc(9);
		strcpy(schedule_msg, "SCHEDULE");
	}

	l = strlen(schedule_msg);
	l = (l > 14 ? 14 : l);
	db->hdr.msg_len = l;
	for(i = 0; i < l; i++)
		db->hdr.msg[i] = rdmi(schedule_msg[i]);
	for(; i < 14; i++)
		db->hdr.msg[i] = 0;

	return;
}

void
update_schedule_grp_type(schedule_t *db)
{
	/* user error! */
	if(db->hdr.grp_type && db->hdr.grp_type != grp_type &&
			db->hdr.num_grps){
		grp_type = db->hdr.grp_type;
		return;
	}

	db->hdr.grp_type = grp_type;

	return;
}

void
create_schedule(schedule_t *db, u8 **data, u16 *len)
{
	int i, j;
	u16 offset, offset2, reserved_size;
	schedule_grp_t *grp;
	schedule_rec_t *rec;

	*len = db->hdr.alloc_size;
	*data = (u8 *)malloc(*len);
	memcpy(*data, &db->hdr, 27);

	offset = 2 * db->hdr.num_grps;
	for(i = 0, grp = db->head; grp; i++, grp = grp->next){
		memcpy(*data+27+2*i, &offset, 2);
		reserved_size = offset;
		memcpy(*data+27+offset, &grp->hdr, 8);
		offset += 8;
		memcpy(*data+27+offset, grp->hdr.msg, grp->hdr.msg_len);
		offset += grp->hdr.msg_len;
		offset2 = 2 * grp->hdr.num_recs;
		for(j = 0, rec = grp->head; rec; j++, rec = rec->next){
			memcpy(*data+27+offset+2*j, &offset2, 2);
			memcpy(*data+27+offset+offset2, rec, 7);
			offset2 += 7;
			memcpy(*data+27+offset+offset2, rec->msg, rec->msg_len);
			offset2 += rec->msg_len;
		}
		offset += offset2;
		reserved_size = grp->hdr.alloc_size - (offset - reserved_size);
		memset(*data+27+offset, 0, reserved_size);
		offset += reserved_size;
	}

	return;
}

int
add_schedule_file(schedule_t *db, FILE *fp)
{
	int ret = -1;
	char *line;
	schedule_data_t data;

	while((line = read_line(fp))){
		if(read_schedule_line(&data, line)){
			int i, j;
			char msg[15];

			for(i = j = 0; line[i] && line[i] != '\t'; i++){
				if(i < 14)
					msg[j++] = line[i];
			}
			msg[j] = 0;
			if(line[i]){
				for(j = 0; j < 3 && strcmp(line+i+1,
						schedule_grp_type[j]); j++);
				if(j < 3 && (!db->hdr.grp_type ||
						db->hdr.grp_type == j + 1 ||
						!db->hdr.num_grps)){
					set_schedule_msg(msg);
					set_schedule_grp_type(j + 1);
					update_schedule_msg(db);
					update_schedule_grp_type(db);
				}
			}

			free(line);
			continue;
		}
		if(find_schedule(db, &data) < 0){
			add_schedule(db, &data);
			ret = 0;
		}
		free(line);
	}

	return ret;
}

int
del_schedule_file(schedule_t *db, FILE *fp)
{
	int ret = -1, i;
	char *line;
	schedule_data_t data;

	while((line = read_line(fp))){
		if(read_schedule_line(&data, line)){
			free(line);
			continue;
		}
		if((i = find_schedule(db, &data)) >= 0 && !del_schedule(db, i))
			ret = 0;
		free(line);
	}

	return ret;
}

void
add_schedule(schedule_t *db, schedule_data_t *data)
{
	int i, c, l, alloc_size;
	u8 *m, buf1[6], buf2[6];
	schedule_grp_t *grp, *g;
	schedule_rec_t *rec, *r;

	l = strlen(data->grp);
	l = (l > MAX_MSG_LEN ? MAX_MSG_LEN : l);
	m = (u8 *)malloc(l+1);
	for(i = 0; i < l; i++)
		m[i] = rdmi(data->grp[i]);
	m[i] = DM_SENTINEL;

	for(grp = db->head; grp && memcmp(grp->hdr.msg, m, l+1);
			grp = grp->next);

	if(grp)
		free(m);
	else{
		grp = (schedule_grp_t *)malloc(sizeof(schedule_grp_t));
		grp->head = grp->tail = NULL;
		grp->prev = grp->next = NULL;
		grp->hdr.db_size = 8 + l + 1;
		grp->hdr.hdr_size = 3 + l + 1;
		grp->hdr.num_recs = 0;
		grp->hdr.msg_len = l + 1;
		grp->hdr.msg = m;
		grp->hdr.alloc_size = ((grp->hdr.db_size-1) / PAGE_SIZE + 1) *
			PAGE_SIZE;

		/* sort */
		for(g = db->head; g; g = g->next){
			/* DM_SENTINEL is ignored when comparing strings */
			l = (grp->hdr.msg_len < g->hdr.msg_len ?
					grp->hdr.msg_len : g->hdr.msg_len) - 1;
			c = memcmp(grp->hdr.msg, g->hdr.msg, l);
			if((!c && grp->hdr.msg_len < g->hdr.msg_len) || c < 0)
				break;
		}

		if(g){
			grp->prev = g->prev;
			if(grp->prev)
				grp->prev->next = grp;
			else
				db->head = grp;
			grp->next = g;
			grp->next->prev = grp;
		}else{
			grp->prev = db->tail;
			if(grp->prev)
				grp->prev->next = grp;
			else
				db->head = grp;
			grp->next = NULL;
			db->tail = grp;
		}

		db->hdr.num_grps++;
		db->hdr.db_size += 2 + grp->hdr.alloc_size;
	}

	/* 2: record offset, 7: record header */
	grp->hdr.db_size += 2 + 7;

	rec = (schedule_rec_t *)malloc(sizeof(schedule_rec_t));

	l = strlen(data->msg);
	l = (l > MAX_MSG_LEN ? MAX_MSG_LEN : l);
	rec->msg_len = l + 1;
	rec->msg = (u8 *)malloc(rec->msg_len);
	for(i = 0; i < l; i++)
		rec->msg[i] = rdmi(data->msg[i]);
	rec->msg[i] = DM_SENTINEL;

	switch(grp_type){
	case date:
		rec->dow = rec->hour = rec->minute = 0;
		rec->year = bcd(data->year % 100);
		rec->month = bcd(data->month);
		rec->day = bcd(data->day);
		break;
	case dow_time:
		rec->year = rec->month = rec->day = 0;
		rec->dow = data->dow;
		rec->hour = bcd(data->hour);
		rec->minute = bcd(data->minute);
		break;
	case date_time:
		rec->dow = 0;
		rec->year = bcd(data->year % 100);
		rec->month = bcd(data->month);
		rec->day = bcd(data->day);
		rec->hour = bcd(data->hour);
		rec->minute = bcd(data->minute);
		break;
	}

	grp->hdr.db_size += rec->msg_len;

	/* sort */
	buf1[0] = rec->year;
	buf1[1] = rec->month;
	buf1[2] = rec->day;
	buf1[3] = rec->dow;
	buf1[4] = rec->hour;
	buf1[5] = rec->minute;

	/* TODO: order by time? */
	for(r = grp->head; r; r = r->next){
		buf2[0] = r->year;
		buf2[1] = r->month;
		buf2[2] = r->day;
		buf2[3] = r->dow;
		buf2[4] = r->hour;
		buf2[5] = r->minute;
		c = memcmp(buf1, buf2, 6);
		if(c < 0)
			break;
		if(c > 0)
			continue;
		/* DM_SENTINEL is ignored when comparing strings */
		l = (rec->msg_len < r->msg_len ? rec->msg_len : r->msg_len) - 1;
		c = memcmp(rec->msg, r->msg, l);
		if((!c && rec->msg_len < r->msg_len) || c < 0)
			break;
	}

	if(r){
		rec->prev = r->prev;
		if(rec->prev)
			rec->prev->next = rec;
		else
			grp->head = rec;
		rec->next = r;
		rec->next->prev = rec;
	}else{
		rec->prev = grp->tail;
		if(rec->prev)
			rec->prev->next = rec;
		else
			grp->head = rec;
		rec->next = NULL;
		grp->tail = rec;
	}

	grp->hdr.num_recs++;
	alloc_size = grp->hdr.alloc_size;
	grp->hdr.alloc_size = ((grp->hdr.db_size-1) / PAGE_SIZE + 1)*PAGE_SIZE;

	db->hdr.db_size += grp->hdr.alloc_size - alloc_size;
	db->hdr.alloc_size = ((db->hdr.db_size-1) / PAGE_SIZE + 1) * PAGE_SIZE;

	return;
}

int
del_schedule(schedule_t *db, int idx)
{
	int i;
	schedule_grp_t *grp;
	schedule_rec_t *rec;

#if 0
	int num_recs;

	num_recs = 0;
	for(grp = db->head; grp; grp = grp_next){
		grp_next = grp->next;
		num_recs += grp->hdr.num_recs;
	}
	if(idx < 0 || idx >= num_recs)
		return -1;
#endif

	for(grp = db->head, i = 0; grp; grp = grp->next){
		for(rec = grp->head; rec && i != idx; rec = rec->next, i++);
		if(rec)
			break;
	}
	if(!grp || !rec){
		if(db->head)
			ERROR("DATA CORRUPTED: THIS SHOULD NOT HAPPEN!");
		return -1;
	}

	/* 2: record offset, 7: record header */
	grp->hdr.db_size -= 2 + 7;

	free(rec->msg);
	grp->hdr.db_size -= rec->msg_len;

	if(rec == grp->head){
		grp->head = rec->next;
		if(grp->head)
			grp->head->prev = NULL;
	}else
		rec->prev->next = rec->next;
	if(rec == grp->tail){
		grp->tail = rec->prev;
		if(grp->tail)
			grp->tail->next = NULL;
	}else
		rec->next->prev = rec->prev;

	free(rec);

	grp->hdr.num_recs--;
	if(!grp->hdr.num_recs){
		if(grp == db->head){
			db->head = grp->next;
			if(db->head)
				db->head->prev = NULL;
		}else
			grp->prev->next = grp->next;
		if(grp == db->tail){
			db->tail = grp->prev;
			if(db->tail)
				db->tail->next = NULL;
		}else
			grp->next->prev = grp->prev;

		db->hdr.db_size -= 2 + grp->hdr.alloc_size;

		free(grp->hdr.msg);
		free(grp);

		db->hdr.num_grps--;
	}else{
		int alloc_size;

		alloc_size = grp->hdr.alloc_size;
		grp->hdr.alloc_size = ((grp->hdr.db_size-1) / PAGE_SIZE + 1) *
			PAGE_SIZE;
		db->hdr.db_size += grp->hdr.alloc_size - alloc_size;
	}

	db->hdr.alloc_size = ((db->hdr.db_size-1) / PAGE_SIZE + 1) * PAGE_SIZE;

	return 0;
}

int
find_schedule(schedule_t *db, schedule_data_t *data)
{
	int i, l;
	u8 *m, dow, year, month, day, hour, minute;
	schedule_grp_t *grp;
	schedule_rec_t *rec = NULL;

	l = strlen(data->grp);
	l = (l > MAX_MSG_LEN ? MAX_MSG_LEN : l);
	m = (u8 *)malloc(l+1);
	for(i = 0; i < l; i++)
		m[i] = rdmi(data->grp[i]);
	m[i] = DM_SENTINEL;

	for(i = 0, grp = db->head; grp && memcmp(grp->hdr.msg, m, l+1);
			i += grp->hdr.num_recs, grp = grp->next);

	free(m);

	if(!grp)
		return -1;

	l = strlen(data->msg);
	l = (l > MAX_MSG_LEN ? MAX_MSG_LEN : l);
	m = (u8 *)malloc(l+1);
	for(i = 0; i < l; i++)
		m[i] = rdmi(data->msg[i]);
	m[i] = DM_SENTINEL;

	switch(grp_type){
	case date:
		year = bcd(data->year % 100);
		month = bcd(data->month);
		day = bcd(data->day);
		for(i = 0, rec = grp->head; rec; i++, rec = rec->next){
			if(rec->year == year && rec->month == month &&
					rec->day == day &&
					!memcmp(rec->msg, m, l+1))
				break;
		}
		break;
	case dow_time:
		dow = data->dow;
		hour = bcd(data->hour);
		minute = bcd(data->minute);
		for(i = 0, rec = grp->head; rec; i++, rec = rec->next){
			if(rec->dow == dow && rec->hour == hour &&
					rec->minute == minute &&
					!memcmp(rec->msg, m, l+1))
				break;
		}
		break;
	case date_time:
		year = bcd(data->year % 100);
		month = bcd(data->month);
		day = bcd(data->day);
		hour = bcd(data->hour);
		minute = bcd(data->minute);
		for(i = 0, rec = grp->head; rec; i++, rec = rec->next){
			if(rec->year == year && rec->month == month &&
					rec->day == day && rec->hour == hour &&
					rec->minute == minute &&
					!memcmp(rec->msg, m, l+1))
				break;
		}
		break;
	}

	free(m);

	return (rec ? i : -1);
}

void
read_schedule(schedule_t *db, u8 *data)
{
	int i, j, k;
	u16 offset, offset2;
	schedule_grp_t *grp;
	schedule_rec_t *rec;

	memcpy(&db->hdr, data, 27);

	db->head = db->tail = NULL;
	for(i = 0; i < db->hdr.num_grps; i++){
		grp = (schedule_grp_t *)malloc(sizeof(schedule_grp_t));

		memcpy(&offset, data+27+2*i, 2);
		offset += 27;
		memcpy(&grp->hdr, data+offset, 8);

		grp->hdr.msg = (u8 *)malloc(grp->hdr.msg_len);
		memcpy(grp->hdr.msg, data+offset+8, grp->hdr.msg_len);
		/* TODO: Timex software bug?  Some character values are
		 * unpredictably greater than 0x69 and it should be &
		 * ~(1 << 7) to be compared with dl's index. */
		for(k = 0; k < grp->hdr.msg_len; k++)
			grp->hdr.msg[k] &= ~(1 << 7);

		grp->head = grp->tail = NULL;
		for(j = 0; j < grp->hdr.num_recs; j++){
			rec = (schedule_rec_t *)malloc(sizeof(schedule_rec_t));

			memcpy(&offset2, data+offset+8+grp->hdr.msg_len+2*j, 2);
			offset2 += offset + 8 + grp->hdr.msg_len;

			memcpy(rec, data+offset2, 7);

			rec->msg = (u8 *)malloc(rec->msg_len);
			memcpy(rec->msg, data+offset2+7, rec->msg_len);
			/* TODO: Timex software bug?  Some character values are
			 * unpredictably greater than 0x69 and it should be &
			 * ~(1 << 7) to be compared with dl's index. */
			for(k = 0; k < rec->msg_len &&
					rec->msg[k] != DM_SENTINEL; k++)
				rec->msg[k] &= ~(1 << 7);
			/* recalculate the length of the message */
			rec->msg_len = k + 1;

			rec->prev = rec->next = NULL;
			if(grp->head){
				rec->prev = grp->tail;
				grp->tail->next = rec;
				grp->tail = rec;
			}else
				grp->head = grp->tail = rec;
		}
		grp->prev = grp->next = NULL;
		if(db->head){
			grp->prev = db->tail;
			db->tail->next = grp;
			db->tail = grp;
		}else
			db->head = db->tail = grp;
	}

	grp_type = db->hdr.grp_type;

	return;
}

/* too slow */
int
read_schedule_mem(dldev_t *dev, schedule_t *db)
{
	int i, j, k;
	u16 add_addr, offset, offset2;
	schedule_grp_t *grp;
	schedule_rec_t *rec;

	/* find the application */
	if((i = find_app(dev, "SCHEDULE")) < 0){
		ERROR("SCHEDULE application not found");
		return -1;
	}

	/* application database data */
	add_addr = dev->app[i].acb.add_addr;
	/* allocation size */
	if(read_abs_addr(dev, add_addr, ext_mem, (u8 *)&db->hdr, 27)){
		ERROR("read_abs_addr");
		return -1;
	}

	db->head = db->tail = NULL;
	for(i = 0; i < db->hdr.num_grps; i++){
		grp = (schedule_grp_t *)malloc(sizeof(schedule_grp_t));
		if(read_abs_addr(dev, add_addr+27+2*i, ext_mem, (u8 *)&offset,
					2)){
			ERROR("read_abs_addr");
			return -1;
		}
		offset += add_addr + 27;
		if(read_abs_addr(dev, offset, ext_mem, (u8 *)&grp->hdr, 8)){
			ERROR("read_abs_addr");
			return -1;
		}
		grp->hdr.msg = (u8 *)malloc(grp->hdr.msg_len);
		if(read_abs_addr(dev, offset+8, ext_mem, grp->hdr.msg,
					grp->hdr.msg_len)){
			ERROR("read_abs_addr");
			return -1;
		}
		/* TODO: Timex software bug?  Some character values are
		 * unpredictably greater than 0x69 and it should be &
		 * ~(1 << 7) to be compared with dl's index. */
		for(k = 0; k < grp->hdr.msg_len; k++)
			grp->hdr.msg[k] &= ~(1 << 7);

		grp->head = grp->tail = NULL;
		for(j = 0; j < grp->hdr.num_recs; j++){
			rec = (schedule_rec_t *)malloc(sizeof(schedule_rec_t));
			if(read_abs_addr(dev, offset+8+grp->hdr.msg_len+2*j,
						ext_mem, (u8 *)&offset2,
						2)){
				ERROR("read_abs_addr");
				return -1;
			}
			offset2 += offset + 8 + grp->hdr.msg_len;
			if(read_abs_addr(dev, offset2, ext_mem, (u8 *)rec, 7)){
				ERROR("read_abs_addr");
				return -1;
			}
			rec->msg = (u8 *)malloc(rec->msg_len);
			if(read_abs_addr(dev, offset2+7, ext_mem, rec->msg,
						rec->msg_len)){
				ERROR("read_abs_addr");
				return -1;
			}
			/* TODO: Timex software bug?  Some character values are
			 * unpredictably greater than 0x69 and it should be &
			 * ~(1 << 7) to be compared with dl's index. */
			for(k = 0; k < rec->msg_len &&
					rec->msg[k] != DM_SENTINEL; k++)
				rec->msg[k] &= ~(1 << 7);
			/* recalculate the length of the message */
			rec->msg_len = k + 1;

			rec->prev = rec->next = NULL;
			if(grp->head){
				rec->prev = grp->tail;
				grp->tail->next = rec;
				grp->tail = rec;
			}else
				grp->head = grp->tail = rec;
		}
		grp->prev = grp->next = NULL;
		if(db->head){
			grp->prev = db->tail;
			db->tail->next = grp;
			db->tail = grp;
		}else
			db->head = db->tail = grp;
	}

	grp_type = db->hdr.grp_type;

	return 0;
}

void
print_schedule(schedule_t *db, FILE *fp)
{
	schedule_grp_t *grp;
	schedule_rec_t *rec;

	print_rdm(db->hdr.msg, db->hdr.msg_len, fp);
	fprintf(fp, "\t%s\n", get_schedule_grp_type_str(db->hdr.grp_type));

	for(grp = db->head; grp; grp = grp->next){
		for(rec = grp->head; rec; rec = rec->next){
			print_rdm(grp->hdr.msg, grp->hdr.msg_len, fp);
			fprintf(fp, "\t");
			print_rdm(rec->msg, rec->msg_len, fp);
			fprintf(fp, "\t");

			switch(db->hdr.grp_type){
			case date:
				fprintf(fp, "%d-%02d-%02d",
						2000 + debcd(rec->year),
						debcd(rec->month),
						debcd(rec->day));
				break;
			case dow_time:
				fprintf(fp, "%s %02d:%02d",
						schedule_dow[rec->dow],
						debcd(rec->hour),
						debcd(rec->minute));
				break;
			case date_time:
				fprintf(fp, "%d-%02d-%02d %02d:%02d",
						2000 + debcd(rec->year),
						debcd(rec->month),
						debcd(rec->day),
						debcd(rec->hour),
						debcd(rec->minute));
				break;
			}
			fprintf(fp, "\n");
		}
	}

	return;
}

void
init_schedule(schedule_t *db)
{
	int i, l;

	db->hdr.db_size = 27;
	db->hdr.hdr_size = 22;
	db->hdr.num_grps = 0;
	db->hdr.grp_type = grp_type;
	/* TODO: LCD address */
	db->hdr.msg1_addr = 0xf900 + 2;
	db->hdr.msg2_addr = 0xfa00 + 2;
	if(!schedule_msg){
		schedule_msg = (char *)malloc(9);
		strcpy(schedule_msg, "SCHEDULE");
	}
	l = strlen(schedule_msg);
	l = (l > 14 ? 14 : l);
	db->hdr.msg_len = l;
	for(i = 0; i < l; i++)
		db->hdr.msg[i] = rdmi(schedule_msg[i]);
	for(; i < 14; i++)
		db->hdr.msg[i] = 0;
	db->hdr.alloc_size = ((db->hdr.db_size-1) / PAGE_SIZE + 1) * PAGE_SIZE;
	db->head = db->tail = NULL;

	return;
}

void
free_schedule(schedule_t *db)
{
	schedule_grp_t *grp, *grp_next;
	schedule_rec_t *rec, *rec_next;

	for(grp = db->head; grp; grp = grp_next){
		grp_next = grp->next;
		free(grp->hdr.msg);
		for(rec = grp->head; rec; rec = rec_next){
			rec_next = rec->next;
			free(rec->msg);
			free(rec);
		}
		free(grp);
	}

	return;
}

int
init_schedule_app(dldev_t *dev, u8 *data, u16 len)
{
	return init_int_app(dev, POR_SCHEDULE, data, len);
}

int
print_all_schedules(dldev_t *dev, char *file)
{
	FILE *fp;
	int i, j;
	u16 add_addr, offset, offset2;
	schedule_hdr_t hdr;
	schedule_grp_hdr_t grp_hdr;
	schedule_rec_t rec;
	u8 grp[101], msg[101];

	/* find the application */
	if((i = find_app(dev, "SCHEDULE")) < 0){
		ERROR("SCHEDULE application not found");
		return -1;
	}

	if(!(fp = fopen(file, "w"))){
		ERROR("%s: open failed", file);
		return -1;
	}

	/* application database data */
	add_addr = dev->app[i].acb.add_addr;
	if(read_abs_addr(dev, add_addr, ext_mem, (u8 *)&hdr, 27)){
		ERROR("read_abs_addr");
		return -1;
	}
	print_rdm(hdr.msg, hdr.msg_len, fp);
	fprintf(fp, "\t%s\n", get_schedule_grp_type_str(hdr.grp_type));

	for(i = 0; i < hdr.num_grps; i++){
		if(read_abs_addr(dev, add_addr+5+22+2*i, ext_mem, (u8 *)&offset,
					2)){
			ERROR("read_abs_addr");
			return -1;
		}
		offset += add_addr + 27;
		if(read_abs_addr(dev, offset, ext_mem, (u8 *)&grp_hdr, 8)){
			ERROR("read_abs_addr");
			return -1;
		}
		if(read_abs_addr(dev, offset+8, ext_mem, grp, grp_hdr.msg_len)){
			ERROR("read_abs_addr");
			return -1;
		}

		for(j = 0; j < grp_hdr.num_recs; j++){
			if(read_abs_addr(dev, offset+8+grp_hdr.msg_len+2*j,
						ext_mem, (u8 *)&offset2,
						2)){
				ERROR("read_abs_addr");
				return -1;
			}
			offset2 += offset + 8 + grp_hdr.msg_len;
			if(read_abs_addr(dev, offset2, ext_mem, (u8 *)&rec, 7)){
				ERROR("read_abs_addr");
				return -1;
			}
			if(read_abs_addr(dev, offset2+7, ext_mem, msg,
						rec.msg_len)){
				ERROR("read_abs_addr");
				return -1;
			}

			print_rdm(grp, grp_hdr.msg_len, fp);
			fprintf(fp, "\t");
			print_rdm(msg, rec.msg_len, fp);
			fprintf(fp, "\t");

			switch(hdr.grp_type){
			case date:
				fprintf(fp, "%d-%02d-%02d",
						2000 + debcd(rec.year),
						debcd(rec.month),
						debcd(rec.day));
				break;
			case dow_time:
				fprintf(fp, "%s %02d:%02d",
						schedule_dow[rec.dow],
						debcd(rec.hour),
						debcd(rec.minute));
				break;
			case date_time:
				fprintf(fp, "%d-%02d-%02d %02d:%02d",
						2000 + debcd(rec.year),
						debcd(rec.month),
						debcd(rec.day), debcd(rec.hour),
						debcd(rec.minute));
				break;
			}
			fprintf(fp, "\n");
		}
	}

	fclose(fp);

	return 0;
}

int
read_schedule_line(schedule_data_t *data, char *line)
{
	int i, j, k, l, n;

	l = strlen(line);
	for(i = 0; i < l && line[i] != '\t'; i++);
	if(i == l)
		return -1;
	line[(i > MAX_MSG_LEN ? MAX_MSG_LEN : i)] = 0;
	/*
	data->grp = squeeze_spaces(line);
	*/
	data->grp = line;

	for(j = ++i; i < l && line[i] != '\t'; i++);
	if(i == l){
		/* recover the schedule description line just in case */
		line[j-1] = '\t';
		return -1;
	}
	line[(i > j+MAX_MSG_LEN ? j+MAX_MSG_LEN : i)] = 0;
	/*
	data->msg = squeeze_spaces(line + j);
	*/
	data->msg = line + j;

	switch(grp_type){
	case date:
		for(j = ++i; i < l && isdigit(line[i]); i++);
		if(i == l)
			return -1;
		line[(i > j+4 ? j+4 : i)] = 0;
		data->year = atoi(line + j);

		for(j = ++i; i < l && isdigit(line[i]); i++);
		if(i == l)
			return -1;
		line[(i > j+2 ? j+2 : i)] = 0;
		data->month = atoi(line + j);

		for(j = ++i; i < l && isdigit(line[i]); i++);
		line[(i > j+2 ? j+2 : i)] = 0;
		data->day = atoi(line + j);
		break;
	case dow_time:
		for(j = ++i; i < l && isalpha(line[i]); i++);
		if(i == l)
			return -1;
		line[i] = 0;
		n = 7;
		for(k = 0; k < n && strcmp(line + j, schedule_dow[k]); k++);
		if(k == n)
			return -1;
		data->dow = k;

		for(j = ++i; i < l && isdigit(line[i]); i++);
		if(i == l)
			return -1;
		line[(i > j+2 ? j+2 : i)] = 0;
		data->hour = atoi(line + j);

		for(j = ++i; i < l && isdigit(line[i]); i++);
		line[(i > j+2 ? j+2 : i)] = 0;
		data->minute = atoi(line + j);
		break;
	case date_time:
		for(j = ++i; i < l && isdigit(line[i]); i++);
		if(i == l)
			return -1;
		line[(i > j+4 ? j+4 : i)] = 0;
		data->year = atoi(line + j);

		for(j = ++i; i < l && isdigit(line[i]); i++);
		if(i == l)
			return -1;
		line[(i > j+2 ? j+2 : i)] = 0;
		data->month = atoi(line + j);

		for(j = ++i; i < l && isdigit(line[i]); i++);
		line[(i > j+2 ? j+2 : i)] = 0;
		data->day = atoi(line + j);

		for(j = ++i; i < l && isdigit(line[i]); i++);
		if(i == l)
			return -1;
		line[(i > j+2 ? j+2 : i)] = 0;
		data->hour = atoi(line + j);

		for(j = ++i; i < l && isdigit(line[i]); i++);
		line[(i > j+2 ? j+2 : i)] = 0;
		data->minute = atoi(line + j);
		break;
	default:
		return -1;
	}

	return 0;
}
