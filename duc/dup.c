
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


struct dup_options
{
	long minbytes;
	int matchbysize;
	int matchbyname;
	int folderscan;
};

struct dup_totals
{
	long long totalsize;
	long totalmatches;
};

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

/*
static struct duc_dir *getdirbyino (duc *duc, duc_dir *dir, ino_t ino, dev_t dev)
{
	//this approach isn't working - perhaps i should just store the pointer to the dir along with the dirent
	struct duc_dirent *e;
	struct duc_dir *ret;
	printf ("%s\n", duc_dir_get_path(dir));
        while( (e = duc_dir_read(dir)) != NULL) {

                if(e->mode == DUC_MODE_DIR) {
                        duc_dir *dir_child = duc_dir_openent(dir, e);
                        if(dir_child) {
                         	ret = getdirbyino(duc, dir_child, ino, dev);
				if (ret) return ret;
                        }
                } else {
			printf ("%ld,%ld,%ld,%ld\n", e->dev, e->ino, dev, ino);
                        if ((e->ino == ino)&&(e->dev == dev)) {
				printf ("MATCH:%ld,%ld", e->ino, ino);
				fflush(stdout);
				return dir;
                        }
			else return 0;
                }
	}
	return 0;
} 

*/



static void dump(duc *duc, duc_dir *dir, int depth, long *entry_num, struct duc_dirent *(*entlist)[], struct dup_options *options, struct dup_totals *totals)
{
	struct duc_dirent *e;

	while( (e = duc_dir_read(dir)) != NULL) {

		if(e->mode == DUC_MODE_DIR) {
			duc_dir *dir_child = duc_dir_openent(dir, e);
			if(dir_child) {
				dump(duc, dir_child, depth + 1, entry_num, entlist, options, totals);
			}
		}
		if ((e->mode == DUC_MODE_DIR) && (!options->folderscan)) {continue;}
		if ((e->mode == DUC_MODE_REG) && (options->folderscan)) {break;}
		
		
			(*entlist)[*entry_num]=e;

			if (e->size > options->minbytes) {
				int i;
				int priormatch = 0;
				//look for matches
				for (i=0;i<*entry_num;i++)
				{
					//HOW CAN I FIND THE MATCH PARENT DIR??

					int matchtype = 0;

					if (((*entlist)[i]->size == e->size) && !strcmp((*entlist)[i]->name,e->name)) {
						if (e->mode == DUC_MODE_DIR) {
							printf("MATCHDIR(SIZE+NAME): ");
						}
						else {
							printf("MATCHFIL(SIZE+NAME): ");
						}
						matchtype = 1;
						}
					else if (options->matchbyname) {

							if (!strcmp((*entlist)[i]->name,e->name)) {
                                        		printf("MATCH(NAME): ");
                                        		matchtype = 2;
							}
					}
					else if (options->matchbysize) {
							if ((*entlist)[i]->size == e->size) {
                                        		printf("MATCH(SIZE): ");
							matchtype = 3;
							}
						}

					if (matchtype) {
						if (!priormatch) {
							totals->totalmatches++;
                                          		totals->totalsize+=e->size;
							}
	                                       	printf("%s/%s (%s)  = unknown/%s (%s)\n", duc_dir_get_path(dir) , e->name, 
								duc_human_size(e->size),(*entlist)[i]->name, duc_human_size((*entlist)[i]->size));
						printf ("totals: %ld, %s\n", totals->totalmatches, duc_human_size(totals->totalsize));
						priormatch = 1;
					}
					
				}
			}
			(*entry_num)++;
		//}

	}
}


static int dup_main(int argc, char **argv)
{
	int c;
	char *path_db = NULL;
	duc_log_level loglevel = DUC_LOG_WRN;

	
	struct dup_options opts; 

	
	struct dup_totals tots = {
		0, 0
		};

	struct dup_options *options = &opts;
	struct dup_totals  *totals  = &tots;

	
	options->minbytes = 0;
	options->matchbysize = 0;
	options->matchbyname = 0;
	options->folderscan = 0;


	totals->totalsize = 0;
	totals->totalmatches = 0;
 
	struct option longopts[] = {
		{ "database",       required_argument, NULL, 'd' },
		{ "megabytes", 	    optional_argument, NULL, 'm' },
		{ NULL }
	};

	while( ( c = getopt_long(argc, argv, "d:m:qvsnf", longopts, NULL)) != EOF) {

		switch(c) {
			case 'd':
				path_db = optarg;
				break;
			case 'm':
				options->minbytes = atol(optarg)*1000000;
			case 'q':
				loglevel = DUC_LOG_FTL;
				break;
			case 'v':
				if(loglevel < DUC_LOG_DMP) loglevel ++;
				break;
			case 's':
				options->matchbysize = 1;
				break;
			case 'n':
				options->matchbyname = 1;
				break;
			case 'f':
				options->folderscan = 1;
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

	//simpler way?
	long e = 0;
	long* en = &e;

	struct duc_dirent *el[entcount];
	int j;
	for (j=0; j<entcount; j++) {
		el[j] = malloc(sizeof(struct duc_dirent));
	}

	struct duc_dirent *(*p)[] = &el;
	
	printf("Starting dup scan\n");
	printf("Options:\n");
	printf("Min bytes: %ld; \n", options->minbytes);
	printf("Folder Scan: %d\n", options->folderscan);
	printf("*********\n");

		

	dump(duc, dir, 1, en, p, options, totals);
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
		"  -m, --megabytes=ARG     minimum filesize in megabytes to include in comparison\n"
		"  -q, --quiet             quiet mode, do not print any warnings\n"
		"  -n, --name              return name only matches\n"
		"  -f, --folders           return folder (directory level) matches\n"
		"  -s, --size              return size only matches\n",

	.main = dup_main
};


/*
 * End
 */

