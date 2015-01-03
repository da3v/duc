
#include "config.h"

#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>

#include "cmd.h"
#include "duc.h"
#include "db.h"

/*
static void indent(int n)
{
	int i;
	for(i=0; i<n; i++) {
		putchar(' ');
	}
}

static void print_escaped(const char *s)
{
	while(*s) {
		if(isprint(*s)) {
			if(*s == '&') {
				fwrite("&amp;", 1, 5, stdout);
			} else if(*s == '\'') {
				fwrite("&apos;", 1, 6, stdout);
			} else {
				putchar(*s);
			}
		}
		s++;
	}
}

*/

static void dump(duc *duc, duc_dir *dir, int depth, long *entry_num, struct duc_dirent *(*entlist)[])
{
	struct duc_dirent *e;

	while( (e = duc_dir_read(dir)) != NULL) {

		if(e->mode == DUC_MODE_DIR) {
			//indent(depth);
			//printf("<ent type='dir' name='");
			//print_escaped(e->name);
			//printf("' inode='%ld' size='%ld' entries='%ld'>\n", (long) e->ino, (long)e->size, (long)duc_dir_get_count(dir));
			//fflush(stdout);
			duc_dir *dir_child = duc_dir_openent(dir, e);
			if(dir_child) {
				dump(duc, dir_child, depth + 1, entry_num, entlist);
				//indent(depth);
				//printf("</ent>\n");
			}
		} else {
			//indent(depth);
			//printf("<ent name='");
			//print_escaped(e->name);
			//printf("' inode='%ld' size='%ld' entry = '%ld'/>\n", (long) e->ino, (long)e->size, *entry_num);
			(*entlist)[*entry_num]=e;
			long minsize = 1000000;

			if (e->size > minsize) {
				int i;
				for (i=0;i<*entry_num;i++)
				{
					long cmpsize = (*entlist)[i]->size;	
					char *cmphsize = duc_human_size(cmpsize);
			
					if (((*entlist)[i]->size == e->size) && !strcmp((*entlist)[i]->name,e->name)) {
                                        	printf("MATCH(SIZE+NAME): %ld:%s:%ld  =  %ld:%s:%s\n", e->ino, e->name, (long) e->size, (*entlist)[i]->ino,(*entlist)[i]->name, cmphsize);
                                        	}
					/*
					else if (!strcmp((*entlist)[i]->name,e->name)) {
                                        	printf("MATCH(NAME): %ld:%s  :  %ld:%s\n", e->ino, e->name, (*entlist)[i]->ino, (*entlist)[i]->name);
                                        	}
				
					else if ((*entlist)[i]->size == e->size) {
						printf("MATCH(SIZE): %ld:%s  :  %ld:%s\n", e->ino, e->name, (*entlist)[i]->ino, (*entlist)[i]->name);
						}
					*/
				}
			}
			(*entry_num)++;
		}

	}
}


static int dup_main(int argc, char **argv)
{
	int c;
	char *path_db = NULL;
	duc_log_level loglevel = DUC_LOG_WRN;

	struct option longopts[] = {
		{ "database",       required_argument, NULL, 'd' },
		{ NULL }
	};

	while( ( c = getopt_long(argc, argv, "d:qv", longopts, NULL)) != EOF) {

		switch(c) {
			case 'd':
				path_db = optarg;
				break;
			case 'q':
				loglevel = DUC_LOG_FTL;
				break;
			case 'v':
				if(loglevel < DUC_LOG_DMP) loglevel ++;
				break;
			default:
				return -2;
		}
	}

	argc -= optind;
	argv += optind;
	
	char *path = ".";
	if(argc > 0) path = argv[0];
	
	/* Open duc context */
	
	duc *duc = duc_new();
	if(duc == NULL) {
                fprintf(stderr, "Error creating duc context\n");
                return -1;
        }
	duc_set_log_level(duc, loglevel);

	int r = duc_open(duc, path_db, DUC_OPEN_RO);
	if(r != DUC_OK) {
		fprintf(stderr, "%s\n", duc_strerror(duc));
		return -1;
	}

	duc_dir *dir = duc_dir_open(duc, path);
	if(dir == NULL) {
		fprintf(stderr, "%s\n", duc_strerror(duc));
		return -1;
	}
	
	long filecount = 0;
	long dircount = 0;

	struct duc_index_report *report;
	int i = 0;

	while( (report = duc_get_report(duc, i)) != NULL) {

		if (strcmp(path, report->path)==0) {
			filecount = report->file_count;
			dircount = report->dir_count;
		}

		duc_index_report_free(report);
		i++;
	}
	
	long entcount = filecount + dircount;
	//printf("<?xml version='1.0' encoding='UTF-8'?>\n");
	//printf("<duc root='%s' size='%ld' file_count='%ld' dir_count='%ld'>\n", path, (long)duc_dir_get_size(dir), 
	//	(long)filecount, (long)dircount);

	//fflush(stdout);

	//simpler way?
	long e = 0;
	long* en = &e;

	struct duc_dirent *el[entcount];
	int j;
	for (j=0; j<entcount; j++) {
		el[j] = malloc(sizeof(struct duc_dirent));
	}

	struct duc_dirent *(*p)[] = &el;
	

	dump(duc, dir, 1, en, p);
	//printf("</duc>\n");

	//TODO:FREE MEMORY!

	duc_dir_close(dir);
	duc_close(duc);
	duc_del(duc);

	return 0;
}



struct cmd cmd_dup = {
	.name = "dup",
	.description = "List duplicates",
	.usage = "[options] [PATH]",
	.help = 
		"  -d, --database=ARG      use database file ARG [~/.duc.db]\n"
		"  -q, --quiet             quiet mode, do not print any warnings\n",
	.main = dup_main
};


/*
 * End
 */

