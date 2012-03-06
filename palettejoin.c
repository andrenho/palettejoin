#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <png.h>

#define PALETTEJOIN_VERSION "0.0.1"

struct Palette {
	png_color* palette;
	int        n_colors;
	int        valid;
};

/*
 * Global variables
 */
static struct Palette* palette       = NULL;               // images palettes
static struct Palette  final         = { .n_colors = 0 };  // joined palette
static int             n_input_files = 0;                  // # of total files

/* 
 * User options
 */
static char** input_files      = NULL;  // names of the files
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
	printf("FILEs can be in PNG or GPL (Gimp palette) format.\n");
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
	printf("Compiled with libpng %s (using libpng %s).\n",
			PNG_LIBPNG_VER_STRING, png_libpng_ver);
	printf("Compiled with zlib %s (using zlib %s).\n",
			ZLIB_VERSION, zlib_version);
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
	if(optind == argc)
		help(argv[0], EXIT_FAILURE);
	input_files = MALLOC(sizeof(char*) * (argc-optind));
	int i;
	for(i=optind; i<argc; i++)
		input_files[i-optind] = argv[i];
	n_input_files = i-optind;
}

/*
 * Read palettes
 */
void read_palette_png(int n, char* filename)
{
	palette[n].valid = 0;

	// open file
	FILE* f = fopen(filename, "rb");
	if(!f)
	{
		perror(filename);
		return;
	}

	// read PNG header
	uint8_t sig[8];
	(void) fread(sig, 1, 8, f);
	if(!png_check_sig(sig, 8))
	{
		fprintf(stderr, "%s: not a valid PNG file.\n", filename);
		return;
	}

	// prepare
	png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, 
			NULL, NULL);
	if(!png_ptr)
		abort();
	png_infop info_ptr = png_create_info_struct(png_ptr);

	// handle errors
	if(setjmp(png_ptr->jmpbuf))
	{
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		fprintf(stderr, "%s: something went wrong while reading PNG "
				"file.\n", filename);
		return;
	}

	// read PNG file info
	png_init_io(png_ptr, f);
	png_set_sig_bytes(png_ptr, 8);
	png_read_info(png_ptr, info_ptr);
	if(png_get_color_type(png_ptr, info_ptr) != PNG_COLOR_TYPE_PALETTE)
	{
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		fprintf(stderr, "%s: only paletted images are supported.\n",
			       	filename);
		return;
	}
	//printf("%d\n", png_get_bit_depth(png_ptr, info_ptr));
	
	// read palette
	png_color* p;
	png_get_PLTE(png_ptr, info_ptr, &p, &palette[n].n_colors);

	// copy colors
	palette[n].palette = malloc(sizeof(png_color) * 256);
	memcpy(palette[n].palette, p, sizeof(png_color) * 256);

	// close
	if(png_ptr && info_ptr)
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

	palette[n].valid = 1;
}


void read_palette_gpl(int n, char* filename)
{
	(void) n, (void) filename;
	// TODO
}


void read_palette(int n, char* filename)
{
	int s = strlen(filename);
	if(s < 4)
		goto invalid_file;
	else if(strcmp(&filename[s-4], ".png") == 0)
		read_palette_png(n, filename);
	else if(strcmp(&filename[s-4], ".gpl") == 0)
		read_palette_gpl(n, filename);
	else
	{
invalid_file:
		fprintf(stderr, "%s: invalid image or palette.\n", filename);
		palette[n].valid = 0;
	}
}

/*
 * Merge palettes
 */
static inline int colorcmp(png_color* a, png_color* b)
{
	return (a->red == b->red && a->green == b->green && a->blue == b->blue);
}

void merge_palettes()
{
	int i, j, k; // loop counters

	for(i=0; i<n_input_files; i++)
		if(palette[i].valid) for(j=0; j<palette[i].n_colors; j++)
		{
			int found = 0;
			for(k=0; k<final.n_colors; k++)
				if(colorcmp(&final.palette[k], 
							&palette[i].palette[j]))
					found = 1;
			if(!found) // TODO - check unused colors
			{
				if(final.n_colors <= 255)
				{
					struct Palette* p = &palette[i];
					png_color* pc = p->palette;
					png_color c = pc[j];
					final.palette[final.n_colors] = c;
				}
				final.n_colors++;
			}
		}
	if(final.n_colors > 255)
	{
		fprintf(stderr, "The joined palette of these images resulted "
				"in %d colors, and a new palette can have "
				"256 colors at most.\n", final.n_colors);
	}
}

/*
 * Rewrite images
 */
int backup(int n)
{
	int from, to;
	char buf[4096];
	ssize_t nread;
	char* dest = alloca(strlen(input_files[n]) + 5);
	sprintf(dest, "%s.bak", input_files[n]);

	from = open(input_files[n], O_RDONLY);
	if(from < 0)
	{
		perror(input_files[n]);
		return 0;
	}

	to = open(dest, O_WRONLY | O_CREAT | O_EXCL, 0644);
	if(to < 0)
	{
		perror(dest);
		return 0;
	}

	while(nread = read(from, buf, sizeof buf), nread > 0)
	{
		char *out_ptr = buf;
		ssize_t nwritten;

		do
		{
			nwritten = write(to, out_ptr, nread);
			if(nwritten >= 0)
			{
				nread -= nwritten;
				out_ptr += nwritten;
			}
			else if(errno != EINTR)
				goto out_error;
		} while(nread > 0);
		if(nread == 0)
		{
			if(close(to) < 0)
			{
				to = -1;
				goto out_error;
			}
			close(from);
			return -1; // success
		}
	}

	int saved_errno;
out_error:
	saved_errno = errno;
	close(from);
	if(to >= 0)
		close(to);
	errno = saved_errno;
	perror(input_files[n]);
	return 0;
}


void rewrite_image(int n)
{
	// tries to backup the file and, if it fails, doesn't continue
	if(backup_old_files)
		if(!backup(n))
			exit(EXIT_FAILURE);


}

/*
 * Output new palette
 */
void output_new_palette()
{
	printf("GIMP Palette\n");
	printf("Name: palettejoin\n");
	printf("Columns: 16\n");
	printf("#\n");
	int i;
	for(i=0; i<final.n_colors; i++)
		printf("%3d %3d %3d %s\n", final.palette[i].red, 
				           final.palette[i].green,
				           final.palette[i].blue,
					   "Untitled"); // TODO - use X color name?
}

/*
 * Main procedure.
 */
int main(int argc, char* argv[])
{
	final.palette = calloc(sizeof(png_color), 256);

	// read options
	get_options(argc, argv);

	// read palettes
	palette = calloc(sizeof(struct Palette), n_input_files);
	int i;
	for(i=0; i<n_input_files; i++)
		read_palette(i, input_files[i]);
	
	// merge palettes
	merge_palettes();
	
	// rewrite images
	for(i=0; i<n_input_files; i++)
		if(palette[i].valid)
			rewrite_image(i);
	
	// output new palette
	if(output_palette)
		output_new_palette();

	return EXIT_SUCCESS;
}
