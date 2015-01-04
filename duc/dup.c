
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
	int matchbyS;
	int matchbyN;
	int matchbyES;
	int folderscan;
};

struct dup_totals
{
	//sum of all
	long long total_size;
	long total_num;

	//name + size
	long long matchNS_size;
	long matchNS_num;

	//size only
	long long matchS_size;
	long matchS_num;

	//name only
	long long matchN_size;
	long matchN_num;

	//extension + size
	long long matchES_size;
	long matchES_num;
};

struct duc_dirent2 {
	char *name;                 /* File name */
	off_t size;                 /* File size */
	duc_dirent_mode mode;       /* File mode */
	dev_t dev;                  /* ID of device containing file */
	ino_t ino;                  /* inode number */
	char *path;		    /* full path */
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

static const char *get_filename_ext(const char *filename) {
    const char *dot = strrchr(filename, '.');
    if(!dot || dot == filename) return "";
    return dot + 1;
}


static void dump(duc *duc, duc_dir *dir, int depth, long *entry_num, struct duc_dirent2 *(*entlist)[], struct dup_options *options, struct dup_totals *totals)
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

			char fullpath[PATH_MAX];
			strcpy (fullpath , duc_dir_get_path(dir));
			strcat (fullpath , "/");
			strcat (fullpath , e->name);
			//printf ("fullpath:%s\n", fullpath);

			//copy ent to ent2 to add path
			struct duc_dirent2 *cmp = (*entlist)[*entry_num];

			cmp->name = e->name;
			cmp->size = e->size;
			cmp->mode = e->mode;
			cmp->dev  = e->dev;
			cmp->ino  = e->ino;
			cmp->path = strdup(fullpath);



			if (e->size > options->minbytes) {
				int i;
				int priormatch = 0;
				//look for matches
				for (i=0;i< *entry_num;i++)
				{

					int matchtype = 0;
					struct duc_dirent2 *cmp;
					cmp = (*entlist)[i];

					if ((cmp->size == e->size) && !strcasecmp(cmp->name,e->name)) {
						matchtype = 1;

						if (e->mode == DUC_MODE_DIR) {
							printf("MATCHDIR(NAME+SIZE): ");
						}
						else {
							printf("MATCHFIL(NAME+SIZE): ");
						}
						
						if (!priormatch) {
                                                        totals->total_num++;
                                                        totals->total_size+=e->size;
							totals->matchNS_num++;
							totals->matchNS_size+=e->size;
                                                        }

					}

					else if ((options->matchbyES) && (e->mode != DUC_MODE_DIR) && 
							(!strcasecmp(get_filename_ext(cmp->name),get_filename_ext(e->name))) && (cmp->size == e->size)) {
                                                matchtype = 2;
                                
                                                if (e->mode == DUC_MODE_DIR) {
                                                        printf("MATCHDIR(EXT+SIZE):  ");
                                                }
                                                else {
                                                        printf("MATCHFIL(EXT+SIZE):  ");
                                                }
                                                
                                                if (!priormatch) {
                                                        totals->total_num++;
                                                        totals->total_size+=e->size;
                                                        totals->matchES_num++;
                                                        totals->matchES_size+=e->size;
                                                }
                                        }

					else if ((options->matchbyN) && (!strcasecmp(cmp->name,e->name))) {
                                                matchtype = 3;
				
						if (e->mode == DUC_MODE_DIR) {
                                                        printf("MATCHDIR(NAME):      ");
                                                }
                                                else {
                                                        printf("MATCHFIL(NAME):      ");
                                                }
                                                
                                                if (!priormatch) {
                                                	totals->total_num++;
                                                        totals->total_size+=e->size;
                                                        totals->matchN_num++;
                                                        totals->matchN_size+=e->size;
                                                        }
					}

					else if ((options->matchbyS) && (cmp->size == e->size)) {
                                                matchtype = 4;

						if (e->mode == DUC_MODE_DIR) {
                                                        printf("MATCHDIR(SIZE):      ");
                                                }
                                                else {
                                                        printf("MATCHFIL(SIZE):      ");
                                                }
                                                
                                                if (!priormatch) {
                                                        totals->total_num++;
                                                        totals->total_size+=e->size;
                                                        totals->matchS_num++;
                                                        totals->matchS_size+=e->size;
                                                }

					}

					if (matchtype) {
	                                       	printf("%s/%s (%s)  = %s/%s (%s)\n", duc_dir_get_path(dir) , e->name, 
								duc_human_size(e->size), cmp->path, cmp->name, duc_human_size(cmp->size));
						priormatch = 1;
					} 
					
				} //end for (iterate match array)
			} //end if (minsize)	
	 (*entry_num)++;
	} //end while (entries to scan)
} //end function


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
	options->matchbyS = 0;
	options->matchbyN = 0;
	options->matchbyES = 0;
	options->folderscan = 0;


	totals->total_size = 0;
	totals->total_num = 0;
	totals->matchNS_size = 0;
	totals->matchES_size = 0;
	totals->matchS_size = 0;
	totals->matchN_size = 0;
	totals->matchNS_num = 0;
	totals->matchES_num = 0;
	totals->matchS_num = 0;
	totals->matchN_num = 0;

 
	struct option longopts[] = {
		{ "database",       required_argument, NULL, 'd' },
		{ "megabytes", 	    optional_argument, NULL, 'm' },
		{ NULL }
	};

	while( ( c = getopt_long(argc, argv, "d:m:qvsnfe", longopts, NULL)) != EOF) {

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
				options->matchbyS = 1;
				break;
			case 'n':
				options->matchbyN = 1;
				break;
			case 'e':
				options->matchbyES = 1;
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

	//switch to duc_malloc?
	struct duc_dirent2 *el[entcount];
	int j;
	for (j=0; j<entcount; j++) {
		el[j] = malloc(sizeof(struct duc_dirent2));
	}

	struct duc_dirent2 *(*p)[] = &el;


	dump(duc, dir, 1, en, p, options, totals);
	//printf("</duc>\n");

	//TODO:FREE MEMORY!
	printf ("\n\n**********************************************************************\n");
	printf ("Summary of Match Results:\n\n");
	printf ("Sum of All  Matches: %10ld    Size: %s\n", totals->total_num  , duc_human_size(totals->total_size));
	printf ("_________________________________________________\n");
	printf ("(NAME+SIZE) Matches: %10ld    Size: %s\n", totals->matchNS_num, duc_human_size(totals->matchNS_size));

	if (options->matchbyES) { printf ("(EXT+SIZE)  Matches: %10ld    Size: %s\n", totals->matchES_num, duc_human_size(totals->matchES_size)); }
	if (options->matchbyS)  { printf ("(SIZE)      Matches: %10ld    Size: %s\n", totals->matchS_num , duc_human_size(totals->matchS_size));  }
	if (options->matchbyN)  { printf ("(NAME)      Matches: %10ld    Size: %s\n", totals->matchN_num , duc_human_size(totals->matchN_size));  }

	printf ("\n");
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
		"  -f, --folders           return folder (directory level) matches\n"
		"  -e, --extension         return matches for fileext and size = fileext and size\n"
		"  -s, --size              return matches for size = size\n"
		"  -n, --name              return matches for name = name\n"
		"\n"
		"\n"
		"Notes: \n"
		"    Name+Size matches always returned, then precedence is:\n"
		"         File Extension and Size match another item (requires -e) \n"
		"         Size match with another item (requires -s) \n"
		"         Name match with another item (requires -n) \n"
		"\n"
		"    Enabling matching at the folder level (-f) will invalidate \n"
		"         matching by file extension and size (-e) \n"
		"\n",

	.main = dup_main
};


/*
 * End
 */

