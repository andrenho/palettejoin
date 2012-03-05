#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

#include <png.h>

#define PALETTEJOIN_VERSION "0.0.1"

	/*
	 * Global variables
	 */

	/* 
	 * User options
	 */
	static char** input_files      = NULL;
	static int    output_palette   = 0;
	static int    eliminate_unused = 0;
	static int    backup_old_files = 1;
	static int    verbose          = 0;

	/*
	 * Allocation functions: fail on error.
	 */
	static inline void* MALLOC(size_t size) { void* m = malloc(size); if(!m) abort(); return m; }
	static inline void* CALLOC(size_t nmemb, size_t size) { void* m = calloc(nmemb, size); if(!m) abort(); return m; }
	static inline void* REALLOC(void* ptr, size_t size) { void* m = realloc(ptr, size); if(!m) abort(); return m; }

	/*
	 * Messages to the user.
	 */
	static void help(char* program_name, int exit_status)
	{
		printf("Usage: %s [OPTION]... FILE...\n", program_name);
		printf("Joins the palettes of FILE(s), adapting FILE(s) to the new "
				"palette.\n");
		printf("\n");
		printf("  -p, --output-palette     outputs generated palette to stdout "
				"in PAL format\n");
		printf("  -x, --eliminate-unused   eliminates unused colors\n");
		printf("  -n, --no-backup          don't backup old files\n");
		printf("  -v, --verbose            display debug information\n");
		printf("\n");
		printf("FILEs can be in PNG or PAL format.\n");
		printf("PAL format description is in "
				"<http://www.cryer.co.uk/file-types/p/pal.htm>\n");
		printf("\n");
		printf("palettejoin home page: <http://palettejoin.googlecode.com/>\n");
		exit(exit_status);
	}


	static void version()
	{
		printf("palettejoin %s\n", PALETTEJOIN_VERSION);
		printf("License GPLv3+: GNU GPL version 3 or later "
				"<http://gnu.org/licenses/gpl.html>.\n");
		printf("This is free software: you are free to change and redistribute "
				"it.\n");
		printf("There is NO WARRANTY, to the extent permitted by law.\n");
		printf("\n");
		printf("palettejoin home page: <http://palettejoin.googlecode.com/>\n");
		printf("Written by André Wagner.\n");
		exit(EXIT_SUCCESS);
	}

	/*
	 * Get user options
	 */
	static void get_options(int argc, char* argv[])
	{
		int c;

		while(1)
		{
			int option_index = 0;
			static struct option long_options[] = {
				{ "output-palette", 0, 0, 0 },
				{ "eliminate-unused", 0, 0, 0 },
				{ "no-backup", 0, 0, 0 },
				{ "verbose", 0, 0, 0 },
				{ "help", 0, 0, 0 },
				{ "version", 0, 0, 0 },
				{ 0, 0, 0, 0 }
			};

			if((c = getopt_long(argc, argv, "pxnhv", long_options, 
					&option_index)) == -1)
				break;

			const char* s;
			switch(c)
			{
			case 0:
				s = long_options[option_index].name;
				if(strcmp(s, "version") == 0)
					version();
				else
					help(argv[0], EXIT_FAILURE);
				break;

			case 'p':
				output_palette = 1;
				break;

			case 'x':
				eliminate_unused = 1;
				break;

			case 'n':
				backup_old_files = 0;
				break;

			case 'v':
				verbose = 1;
				break;

			case 'h':
				help(argv[0], EXIT_SUCCESS);
				break;

			case '?':
				break;

			default:
				abort();
			}
		}

		// get filenames
		int i;
		input_files = MALLOC(sizeof(char*) * (argc-optind+1)); 
		for(i=optind; i<argc; i++)
			input_files[i-optind] = argv[i];
		input_files[i-optind] = NULL;
}

/*
 * Main procedure.
 */
int main(int argc, char* argv[])
{
	get_options(argc, argv);
	return 0;
}
