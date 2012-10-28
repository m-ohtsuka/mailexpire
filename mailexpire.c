/*

  mailexpire - mail expire tool

  Usage: mailexpire spoolfile [day]

  Copyright (C) 2000 Masahito Ohtsuka <negi@KU3G.org>

*/

#include "config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#define BUFSIZE 1001 /* cf RFC821 */
#define EXPIRE_DAYS 90

static char rcsid[] = "$Id: mailexpire.c,v 1.9 2000/11/26 12:59:12 negi Exp $";

void usage(void)
{
    fprintf(stderr, "%s version %s\n", PACKAGE, VERSION);
    fprintf(stderr, "Usage: %s <spoolfile> [day(1-365)]\n", PACKAGE);
}

int mon2num(char *s)
{
    if(strcmp("Jan", s) == 0){
	return 0;
    } else if(strcmp("Feb", s) == 0){
	return 1;
    } else if(strcmp("Mar", s) == 0){
	return 2;
    } else if(strcmp("Apr", s) == 0){
	return 3;
    } else if(strcmp("May", s) == 0){
	return 4;
    } else if(strcmp("Jun", s) == 0){
	return 5;
    } else if(strcmp("Jul", s) == 0){
	return 6;
    } else if(strcmp("Aug", s) == 0){
	return 7;
    } else if(strcmp("Sep", s) == 0){
	return 8;
    } else if(strcmp("Oct", s) == 0){
	return 9;
    } else if(strcmp("Nov", s) == 0){
	return 10;
    } else if(strcmp("Dec", s) == 0){
	return 11;
    }
    return 13;
}

time_t from_time(char *s)
{
    struct tm tm;
    char p[BUFSIZE];
    char day[3], mon[4], year[5], time[9], hour[3], min[3];

    strncpy(p, s, BUFSIZE);

    strtok(p, " ");                      /* From  */
    strtok(NULL, " ");                   /* mail  */
    strtok(NULL, " ");                   /* week  */
    strncpy(mon, strtok(NULL, " "),4);   /* month */
    strncpy(day, strtok(NULL, " "),3);   /* day   */
    strncpy(time, strtok(NULL, " "),9);  /* time  */
    strncpy(year, strtok(NULL, "\n"),5); /* year  */
    strncpy(hour, strtok(time, ":"),3);  /* hour  */
    strncpy(min, strtok(NULL, ":"),3);   /* min   */

    tm.tm_sec = 0;
    tm.tm_min = atoi(min);
    tm.tm_hour = atoi(hour);
    tm.tm_mday = atoi(day);
    tm.tm_mon = mon2num(mon);
    tm.tm_year = atoi(year) - 1900;
    tm.tm_isdst = 0;
    return mktime(&tm);
}

int from_check(char *buf, int days)
{
    time_t t;
    t = from_time(buf);
    if((time(0) - days * 86400) > t && t > 0){
	return 1;
    } else {
	return 0;
    }
}

int file_check(FILE *fp, int days)
{
    char buf[BUFSIZE];
    char prv = '\n';
    static int del_msg = 0;

    while(fgets(buf, BUFSIZE, fp) != NULL){
	if(prv == '\n' && strncmp("From ", buf, 5) == 0){
	    if(from_check(buf, days)){
		del_msg++;
	    } else {
		fseek(fp, -strlen(buf), SEEK_CUR);
		break;
	    }
	}
	prv = *buf;
    }
    return del_msg;
}

int lockmbox(int fd)
{
    int ret;
#ifdef linux
    struct flock buf;
    buf.l_type = F_WRLCK;
    buf.l_whence = 0;
    buf.l_start = 0;
    buf.l_len = 0;
    ret = fcntl(fd, F_SETLK, &buf);
#else
    ret = flock(fd, LOCK_EX | LOCK_NB);
#endif
    return ret;
}

int unlockmbox(int fd)
{
    int ret;
#ifdef linux
    struct flock buf;
    buf.l_type = F_UNLCK;
    buf.l_whence = 0;
    buf.l_start = 0;
    buf.l_len = 0;
    ret = fcntl(fd, F_SETLK, &buf); 
#else
    ret = flock(fd, LOCK_UN);
#endif
    return ret;
}

int main(int argc, char **argv)
{
    int days = EXPIRE_DAYS;
    int fd, msg;
    FILE *fp, *tmp;
    char buf[BUFSIZE];
    char tmpfile[64];
    struct stat st;

    if(argc < 2){
	usage();
	exit(1);
    }
    if((fd = open(argv[1], O_RDWR)) == -1){
        perror(argv[1]);
        exit(1);
    }
    if(argc == 3){
	days = atoi(argv[2]);
	if(366 < days && days > 1){
	    fprintf(stderr, "The range of the value is invalid: %d\n", days);
	    exit(1);
	}
    }
    fstat(fd, &st);
    if(lockmbox(fd) == -1){
	perror(argv[1]);
	exit(1);
    }
    fp = fdopen(fd, "r+");
    if(msg = file_check(fp, days)){
	fprintf(stderr, "Now Rewriting %s ... ", argv[1]);
	sprintf(tmpfile, "%s/.mailexpire.%d", MAILDIR, getpid());
	if((tmp = fopen(tmpfile, "w")) == NULL){
	    perror(tmpfile);
	    exit(1);
	}
	while((fgets(buf, BUFSIZE, fp)) != NULL){
	    fputs(buf, tmp);
	}
	if(rename(tmpfile, argv[1]) == -1){
	    perror(argv[1]);
	    exit(1);
	}
	fprintf(stderr, "done\n");
    }
    if(chown(argv[1], st.st_uid, st.st_gid) == -1){
	perror(argv[1]);
	exit(1);
    }
    if(chmod(argv[1], st.st_mode) == -1){
	perror(argv[1]);
	exit(1);
    }	
    unlockmbox(fd);
    fclose(fp);
    close(fd);
    fprintf(stderr, "%d message(s) have been deleted\n", msg);
    exit(0);
}
