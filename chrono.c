/* $Id: chrono.c,v 1.6 2005/10/06 08:49:49 geni Exp $
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

static int unused_recs = UNUSED_CHRONO_RECS;
static u8 display_format = lap_split;

static void update_chrono(chrono_t *db);

void
set_chrono_unused_recs(int recs)
{
	unused_recs = (recs < 5 ? 5 : (recs > 250 ? 250 : recs));

	return;
}

void
set_chrono_display_format(u8 display)
{
	if(display < 4)
		display_format = display;

	return;
}

void
update_chrono_display_format(chrono_t *db)
{
	db->hdr.display_format = display_format;

	return;
}

void
create_chrono(chrono_t *db, u8 **data, u16 *len)
{
	int i;
	chrono_rec_t *rec;
	chrono_lap_t *lap;

	update_chrono(db);

	*len = db->hdr.alloc_size;
	*data = (u8 *)malloc(*len);
	memcpy(*data, &db->hdr, 17);

	for(rec = db->head, i = 0; rec; rec = rec->next){
		memcpy(*data+17+5*(i++), rec, 5);
		for(lap = rec->head; lap; lap = lap->next)
			memcpy(*data+17+5*(i++), lap, 5);
	}
	if(i < db->hdr.num_recs)
		memset(*data+17+5*i, 0, (db->hdr.num_recs - i) * 5);

	return;
}

int
add_chrono_file(chrono_t *db, FILE *fp)
{
	int ret = -1;
	char *line;
	chrono_data_t data;

	while((line = read_line(fp))){
		if(read_chrono_line(&data, line)){
			int i;

			for(i = 0; i < 4 &&
				strcmp(line, chrono_display_format[i]); i++);
			if(i < 4){
				set_chrono_display_format(i);
				update_chrono_display_format(db);
			}
			free(line);
			continue;
		}
		if(find_chrono(db, &data) < 0){
			add_chrono(db, &data);
			ret = 0;
		}
		free(line);
	}

	return ret;
}

int
del_chrono_file(chrono_t *db, FILE *fp)
{
	int ret = -1, i;
	char *line;
	chrono_data_t data;

	while((line = read_line(fp))){
		if(read_chrono_line(&data, line)){
			free(line);
			continue;
		}
		if((i = find_chrono(db, &data)) >= 0 && !del_chrono(db, i))
			ret = 0;
		free(line);
	}

	return ret;
}

void
add_chrono(chrono_t *db, chrono_data_t *data)
{
	int i, t, dt, min_dt, best_lap;
	chrono_rec_t *rec;
	chrono_lap_t *lap;

	/* TODO: sort? */

	for(rec = db->head, i = 1; rec && i != data->rec; rec = rec->next, i++);
	if(!rec){
		rec = (chrono_rec_t *)malloc(sizeof(chrono_rec_t));

		rec->year = bcd(data->year % 100);
		rec->month = bcd(data->month);
		rec->day = bcd(data->day);
		rec->dow = data->dow;
		rec->best_lap = rec->num_laps = 0;

		rec->head = rec->tail = NULL;

		rec->prev = db->tail;
		if(rec->prev)
			rec->prev->next = rec;
		else
			db->head = rec;
		rec->next = NULL;
		db->tail = rec;

		db->hdr.used_recs++;
	}

	lap = (chrono_lap_t *)malloc(sizeof(chrono_lap_t));

	lap->id = rec->num_laps + 1;
	lap->hours = bcd(data->hours);
	lap->minutes = bcd(data->minutes);
	lap->seconds = bcd(data->seconds);
	lap->hundredths = bcd(data->hundredths);

	lap->prev = rec->tail;
	if(lap->prev)
		lap->prev->next = lap;
	else
		rec->head = lap;
	lap->next = NULL;
	rec->tail = lap;

	t = 0;
	min_dt = -1;
	best_lap = 0;
	for(lap = rec->head, i = 1; lap; lap = lap->next, i++){
		dt = debcd(lap->hours) * 360000 + debcd(lap->minutes) * 6000 +
			debcd(lap->seconds) * 100 + debcd(lap->hundredths) - t;
		if(min_dt < 0){
			min_dt = dt;
			best_lap = i;
		}else
		if(dt < min_dt){
			min_dt = dt;
			best_lap = i;
		}
		t += dt;
	}
	rec->best_lap = best_lap;

	rec->num_laps++;
	db->hdr.used_recs++;

	return;
}

int
del_chrono(chrono_t *db, int idx)
{
	int i;
	chrono_rec_t *rec;
	chrono_lap_t *lap;

	for(rec = db->head, i = 0; rec; rec = rec->next){
		for(lap = rec->head; lap && i != idx; lap = lap->next, i++);
		if(lap)
			break;
	}
	if(!rec || !lap){
		if(db->head)
			ERROR("DATA CORRUPTED: THIS SHOULD NOT HAPPEN!");
		return -1;
	}

	if(lap == rec->head){
		rec->head = lap->next;
		if(rec->head)
			rec->head->prev = NULL;
	}else
		lap->prev->next = lap->next;
	if(lap == rec->tail){
		rec->tail = lap->prev;
		if(rec->tail)
			rec->tail->next = NULL;
	}else
		lap->next->prev = lap->prev;

	free(lap);

	rec->num_laps--;

	if(rec->num_laps){
		int t, dt, min_dt, best_lap;

		for(lap = lap->next; lap; lap = lap->next)
			lap->id--;

		t = 0;
		min_dt = -1;
		best_lap = 0;
		for(lap = rec->head, i = 1; lap; lap = lap->next, i++){
			dt = debcd(lap->hours) * 360000 +
				debcd(lap->minutes) * 6000 +
				debcd(lap->seconds) * 100 +
				debcd(lap->hundredths) - t;
			if(min_dt < 0){
				min_dt = dt;
				best_lap = i;
			}else
			if(dt < min_dt){
				min_dt = dt;
				best_lap = i;
			}
			t += dt;
		}
		rec->best_lap = best_lap;
	}else{
		if(rec == db->head){
			db->head = rec->next;
			if(db->head)
				db->head->prev = NULL;
		}else
			rec->prev->next = rec->next;
		if(rec == db->tail){
			db->tail = rec->prev;
			if(db->tail)
				db->tail->next = NULL;
		}else
			rec->next->prev = rec->prev;

		free(rec);

		db->hdr.used_recs--;
	}

	db->hdr.used_recs--;

	return 0;
}

int
find_chrono(chrono_t *db, chrono_data_t *data)
{
	int i, j;
	chrono_rec_t *rec;
	chrono_lap_t *lap;

	for(rec = db->head, i = 1, j = 0; rec && i != data->rec;
			i++, j += rec->num_laps, rec = rec->next);
	if(!rec)
		return -1;

	for(lap = rec->head; lap && lap->id != data->lap_id;
			lap = lap->next, j++);

	return (lap ? j : -1);
}

void
read_chrono(chrono_t *db, u8 *data)
{
	int i, j;
	chrono_rec_t *rec;
	chrono_lap_t *lap;

	memcpy(&db->hdr, data, 17);

	db->head = db->tail = NULL;
	for(i = 0; i < db->hdr.used_recs; ){
		rec = (chrono_rec_t *)malloc(sizeof(chrono_rec_t));
		memcpy(rec, data+17+5*i, 5);

		rec->head = rec->tail = NULL;
		for(j = 0, i++; j < rec->num_laps; j++, i++){
			lap = (chrono_lap_t *)malloc(sizeof(chrono_lap_t));
			memcpy(lap, data+17+5*i, 5);

			lap->prev = lap->next = NULL;
			if(rec->head){
				lap->prev = rec->tail;
				rec->tail->next = lap;
				rec->tail = lap;
			}else
				rec->head = rec->tail = lap;
		}

		rec->prev = rec->next = NULL;
		if(db->head){
			rec->prev = db->tail;
			db->tail->next = rec;
			db->tail = rec;
		}else
			db->head = db->tail = rec;
	}

	return;
}

/* too slow */
int
read_chrono_mem(dldev_t *dev, chrono_t *db)
{
	int i, j;
	u16 add_addr;
	chrono_rec_t *rec;
	chrono_lap_t *lap;

	/* find the application */
	if((i = find_app(dev, "CHRONO")) < 0){
		ERROR("CHRONO application not found");
		return -1;
	}

	/* application database data */
	add_addr = dev->app[i].acb.add_addr;
	/* allocation size */
	if(read_abs_addr(dev, add_addr, ext_mem, (u8 *)&db->hdr, 17)){
		ERROR("read_abs_addr");
		return -1;
	}

	db->head = db->tail = NULL;
	for(i = 0; i < db->hdr.used_recs; ){
		rec = (chrono_rec_t *)malloc(sizeof(chrono_rec_t));
		if(read_abs_addr(dev, add_addr+17+5*i, ext_mem, (u8 *)rec, 5)){
			ERROR("read_abs_addr");
			return -1;
		}

		rec->head = rec->tail = NULL;
		for(j = 0, i++; j < rec->num_laps; j++, i++){
			lap = (chrono_lap_t *)malloc(sizeof(chrono_lap_t));
			if(read_abs_addr(dev, add_addr+17+5*i, ext_mem,
						(u8 *)lap, 5)){
				ERROR("read_abs_addr");
				return -1;
			}

			lap->prev = lap->next = NULL;
			if(rec->head){
				lap->prev = rec->tail;
				rec->tail->next = lap;
				rec->tail = lap;
			}else
				rec->head = rec->tail = lap;
		}

		rec->prev = rec->next = NULL;
		if(db->head){
			rec->prev = db->tail;
			db->tail->next = rec;
			db->tail = rec;
		}else
			db->head = db->tail = rec;
	}

	return 0;
}

void
print_chrono(chrono_t *db, FILE *fp)
{
	int i;
	chrono_rec_t *rec;
	chrono_lap_t *lap;

	fprintf(fp, "%s\n", chrono_display_format[db->hdr.display_format]);
	for(rec = db->head, i = 1; rec; rec = rec->next, i++){
		for(lap = rec->head; lap; lap = lap->next){
			fprintf(fp, "%d\t%d\t%d-%02d-%02d %s\t"
					"%02d:%02d:%02d.%02d",
					i, lap->id, 2000+debcd(rec->year),
					debcd(rec->month), debcd(rec->day),
					get_chrono_dow_str(rec->dow),
					debcd(lap->hours), debcd(lap->minutes),
					debcd(lap->seconds),
					debcd(lap->hundredths));
			if(lap->id == rec->best_lap)
				fprintf(fp, "\tbest lap");
			fprintf(fp, "\n");
		}
	}

	return;
}

void
init_chrono(chrono_t *db)
{
	db->hdr.db_size = 17 + (unused_recs + 1) * 5;
	db->hdr.hdr_size = 12;
	db->hdr.num_recs = unused_recs + 1;
	db->hdr.rec_size = 5;
	db->hdr.display_format = display_format;
	db->hdr.system_flag = 0; /* TODO */
	db->hdr.count = 1;
	db->hdr.unused_recs = db->hdr.num_recs;
	db->hdr.max_recs = db->hdr.num_recs;
	db->hdr.used_recs = 0;
	db->hdr.last_workout = 0;
	db->hdr.stored_workout = db->hdr.used_recs;
	db->hdr.reserved_16 = 0;
	db->hdr.alloc_size = ((db->hdr.db_size-1) / PAGE_SIZE + 1) * PAGE_SIZE;
	db->head = db->tail = NULL;

	return;
}

void
free_chrono(chrono_t *db)
{
	chrono_rec_t *rec, *rec_next;
	chrono_lap_t *lap, *lap_next;

	for(rec = db->head; rec; rec = rec_next){
		rec_next = rec->next;
		for(lap = rec->head; lap; lap = lap_next){
			lap_next = lap->next;
			free(lap);
		}
		free(rec);
	}

	return;
}

int
init_chrono_app(dldev_t *dev, u8 *data, u16 len)
{
	return init_int_app(dev, POR_CHRONO, data, len);
}

int
print_all_chronos(dldev_t *dev, char *file)
{
	FILE *fp;
	int i, j, k;
	u16 add_addr;
	chrono_lap_t lap;
	chrono_rec_t rec;
	u8 display, used_recs;

	/* find the application */
	if((i = find_app(dev, "CHRONO")) < 0){
		ERROR("CHRONO application not found");
		return -1;
	}

	if(!(fp = fopen(file, "w"))){
		ERROR("%s: open failed", file);
		return -1;
	}

	/* application database data */
	add_addr = dev->app[i].acb.add_addr;
	if(read_abs_addr(dev, add_addr+8, ext_mem, (u8 *)&display, 1)){
		ERROR("read_abs_addr");
		return -1;
	}
	fprintf(fp, "%s\n", chrono_display_format[display]);

	/* number of records */
	if(read_abs_addr(dev, add_addr+13, ext_mem, (u8 *)&used_recs, 1)){
		ERROR("read_abs_addr");
		return -1;
	}

	for(i = 0, j = 1; i < used_recs; j++){
		if(read_abs_addr(dev, add_addr+17+5*i, ext_mem, (u8 *)&rec, 5)){
			ERROR("read_abs_addr");
			return -1;
		}
		for(k = 0, i++; k < rec.num_laps; k++, i++){
			if(read_abs_addr(dev, add_addr+17+5*i, ext_mem,
						(u8 *)&lap, 5)){
				ERROR("read_abs_addr");
				return -1;
			}
			fprintf(fp, "%d\t%d\t%d-%02d-%02d %s\t"
					"%02d:%02d:%02d.%02d",
					j, lap.id, 2000+debcd(rec.year),
					debcd(rec.month), debcd(rec.day),
					get_chrono_dow_str(rec.dow),
					debcd(lap.hours), debcd(lap.minutes),
					debcd(lap.seconds),
					debcd(lap.hundredths));
			if(lap.id == rec.best_lap)
				fprintf(fp, "\tbest lap");
			fprintf(fp, "\n");
		}
	}

	fclose(fp);

	return 0;
}

int
read_chrono_line(chrono_data_t *data, char *line)
{
	int i, j, l;
	char t;

	l = strlen(line);
	for(i = 0; i < l && line[i] != '\t'; i++);
	if(i == l)
		return -1;
	line[i] = 0;
	data->rec = atoi(line);

	for(j = ++i; i < l && line[i] != '\t'; i++);
	if(i == l)
		return -1;
	line[i] = 0;
	data->lap_id = atoi(line + j);

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
	if(i == l)
		return -1;
	t = line[i];
	line[(i > j+2 ? j+2 : i)] = 0;
	data->day = atoi(line + j);

	if(t != '\t'){
		for(j = ++i; i < l && line[i] != '\t'; i++);
		if(i == l)
			return -1;
		line[i] = 0;
	}
	data->dow = day_of_week(data->year, data->month, data->day);

	for(j = ++i; i < l && isdigit(line[i]); i++);
	if(i == l)
		return -1;
	line[(i > j+2 ? j+2 : i)] = 0;
	data->hours = atoi(line + j);

	for(j = ++i; i < l && isdigit(line[i]); i++);
	if(i == l)
		return -1;
	line[(i > j+2 ? j+2 : i)] = 0;
	data->minutes = atoi(line + j);

	for(j = ++i; i < l && isdigit(line[i]); i++);
	if(i == l)
		return -1;
	line[(i > j+2 ? j+2 : i)] = 0;
	data->seconds = atoi(line + j);

	j = ++i;
	line[(l > j+2 ? j+2 : l)] = 0;
	data->hundredths = atoi(line + j);

	return 0;
}

static void
update_chrono(chrono_t *db)
{
	db->hdr.num_recs = db->hdr.used_recs + unused_recs + 1;
	db->hdr.unused_recs = unused_recs + 1;
	db->hdr.max_recs = db->hdr.num_recs;
	db->hdr.last_workout = 0;
	db->hdr.stored_workout = db->hdr.used_recs;

	db->hdr.db_size = 17 + db->hdr.num_recs * 5;
	db->hdr.alloc_size = ((db->hdr.db_size-1) / PAGE_SIZE + 1) * PAGE_SIZE;

	return;
}
