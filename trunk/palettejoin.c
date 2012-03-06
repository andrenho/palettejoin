#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <png.h>

#define PALETTEJOIN_VERSION "1.0.0"

struct Image {
	png_color* palette;
	int        n_colors;
	int        valid;
	int        transparent;
	png_bytep* row_pointers;
	int        w, h;
};

/*
 * Global variables
 */
static struct Image* image         = NULL;               // images
static struct Image  final         = { .n_colors = 0 };  // joined palette
static int           n_input_files = 0;                  // # of total files
static int           c;                                  // parser lookahead

/* 
 * User options
 */
static char** input_files      = NULL;  // names of the files
static int    output_palette   = 0;
static int    eliminate_unused = 0;
static int    backup_old_files = 1;

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
			{ "help", 0, 0, 0 },
			{ "version", 0, 0, 0 },
			{ 0, 0, 0, 0 }
		};

		if((c = getopt_long(argc, argv, "pxnh", long_options, 
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
	image[n].valid = 0;

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
	int bitdepth, color_type;
	png_init_io(png_ptr, f);
	png_set_sig_bytes(png_ptr, 8);
	png_read_info(png_ptr, info_ptr);
	image[n].h = png_get_image_height(png_ptr, info_ptr);
	image[n].w = png_get_image_width(png_ptr, info_ptr);
	bitdepth = png_get_bit_depth(png_ptr, info_ptr);
	color_type = png_get_color_type(png_ptr, info_ptr);
	if(color_type != PNG_COLOR_TYPE_PALETTE || bitdepth != 8)
	{
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		fprintf(stderr, "%s: only 8-bit paletted images are supported.\n",
			       	filename);
		return;
	}
	//printf("%d\n", png_get_bit_depth(png_ptr, info_ptr));
	
	// read palette
	png_color* p;
	png_get_PLTE(png_ptr, info_ptr, &p, &image[n].n_colors);

	// copy colors
	image[n].palette = malloc(sizeof(png_color) * 256);
	memcpy(image[n].palette, p, sizeof(png_color) * 256);

	// get transparent color
	image[n].transparent = -1;
	if(png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
	{
		png_bytep trans_alpha;
		int num_trans;
		png_get_tRNS(png_ptr, info_ptr, &trans_alpha, &num_trans, NULL);
		if(num_trans > 0)
			image[n].transparent = trans_alpha[0];
	}

	// read image data
	image[n].row_pointers = malloc(sizeof(png_bytep) * image[n].h);
	int y;
	for(y=0; y<image[n].h; y++)
		image[n].row_pointers[y] = 
			malloc(png_get_rowbytes(png_ptr, info_ptr));
	png_read_image(png_ptr, image[n].row_pointers);

	// close
	if(png_ptr && info_ptr)
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
	fclose(f);

	image[n].valid = 1;
}


// parsing functions
inline void read_until_next_eol(FILE* f)
{
	do
	{
		c = fgetc(f);
	} while(c != '\n' || c != EOF);
}

void read_palette_gpl(int n, char* filename)
{
	// TODO

	int i = 0;
	char who_cares[2048];
	image[n].valid = 0;

	FILE* f = fopen(filename, "r");
	if(!f)
	{
		perror(filename);
		return;
	}
	c = fgetc(f);
	read_until_next_eol(f);
	read_until_next_eol(f);
	read_until_next_eol(f);
	read_until_next_eol(f);
	while(c != EOF)
		fscanf(f, "%d %d %d %s", (int*)&image[n].palette[i].red,
				         (int*)&image[n].palette[i].green,
				         (int*)&image[n].palette[i].blue,
					 who_cares);
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
		image[n].valid = 0;
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

	final.n_colors = 1; // 0 is used for transparency
	final.palette[0] = (png_color) { 255, 0, 255 };

	for(i=0; i<n_input_files; i++)
		if(image[i].valid) for(j=0; j<image[i].n_colors; j++)
		{
			int found = 0;
			for(k=1; k<final.n_colors; k++)
				if(colorcmp(&final.palette[k], 
						&image[i].palette[j]))
					found = 1;
			if(!found) // TODO - check unused colors
			{
				if(final.n_colors <= 255)
				{
					png_color c = image[i].palette[j];
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


static void replace_colors(int n)
{
	// find correspondant colors
	int x, y, correspondence[256] = { [0 ... 255] -1 } ;
	for(y=0; y<image[n].h; y++)
		for(x=0; x<image[n].w; x++)
		{
			png_byte c = image[n].row_pointers[y][x];
			if(c == image[n].transparent)
				correspondence[c] = 0;
			else if(correspondence[c] == -1)
			{
				int i;
				for(i=0; i<image[n].n_colors; i++)
					if(colorcmp(&final.palette[i],
							&image[n].palette[c]))
					{
						correspondence[c] = i;
						break;
					}
				// sanity
				if(correspondence[c] == -1)
					abort();
			}
		}

	// replace colors
	for(y=0; y<image[n].h; y++)
		for(x=0; x<image[n].w; x++)
		{
			png_byte c = image[n].row_pointers[y][x];
			image[n].row_pointers[y][x] = correspondence[c];
		}
}


static void save_image(int n)
{
	FILE *f = fopen(input_files[n], "wb");
	if(!f)
	{
		perror(input_files[n]);
		return;
	}

	// initialize libpbng
	png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, 
			NULL, NULL, NULL);
	png_infop info_ptr = png_create_info_struct(png_ptr);
	if(!png_ptr || !info_ptr)
		abort();
	png_init_io(png_ptr, f);

	// handle errors
	if(setjmp(png_ptr->jmpbuf))
	{
		png_destroy_write_struct(&png_ptr, &info_ptr);
		fprintf(stderr, "%s: something went wrong while writing PNG "
				"file.\n", input_files[n]);
		return;
	}

	// write header
	png_set_IHDR(png_ptr, info_ptr, image[n].w, image[n].h, 8, 
			PNG_COLOR_TYPE_PALETTE, PNG_INTERLACE_NONE, 
			PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

	// write palette
	png_set_PLTE(png_ptr, info_ptr, final.palette, final.n_colors);

	// set transparent color
	png_byte trans_alpha[256] = { [0 ... 255] 0 };
	png_set_tRNS(png_ptr, info_ptr, trans_alpha, 1, NULL);
	
	// write data
	png_write_info(png_ptr, info_ptr);
	png_write_image(png_ptr, image[n].row_pointers);
	png_write_end(png_ptr, NULL);
	fclose(f);
}


void rewrite_image(int n)
{
	// tries to backup the file and, if it fails, doesn't continue
	if(backup_old_files)
		if(!backup(n))
			exit(EXIT_FAILURE);

	replace_colors(n);

	save_image(n);
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
					   "Untitled");
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
	image = calloc(sizeof(struct Image), n_input_files);
	int i;
	for(i=0; i<n_input_files; i++)
		read_palette(i, input_files[i]);
	
	// merge palettes
	merge_palettes();
	
	// rewrite images
	for(i=0; i<n_input_files; i++)
		if(image[i].valid)
			rewrite_image(i);
	
	// output new palette
	if(output_palette)
		output_new_palette();

	return EXIT_SUCCESS;
}
