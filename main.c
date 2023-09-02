#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <libgen.h>
#include <sys/stat.h>
#include <libraw/libraw.h>
#include <fitsio.h>

#include "raw2fits.h"


int verbose=0;

int usage(char *argv0)
{
	fprintf(stderr, "USAGE:\n"
			"%s [OPTIONS] <filename>...\n"
			"Split the raw DSLR files given on the commandline into monochrome FITS files\n"
			"(one per input file and color) WITHOUT interpolating them.\n"
			"That means the output images will be half of the size of the input images.\n"
			"Any usefull information in the EXIF header will be preserved.\n\n"
			"OPTIONS can be:\n"
			" -v\t\tincrease verbose level\n"
			" -D <dir>\tdirectory to write the files to, defaults to \".\"\n"
			" -B <bayer>\tbayer pattern to use, defaults to RGGB\n"
			" -b <factor>\tbinning factor, defaults to 1\n"
			" -?\t\tthis text\n",
			argv0);
	return 0;
}

int main(int argc, char **argv)
{
	int err=0;
	int opt;
	char *destdir=".";
	char *p;
	char *bayer="RGGB";
	int binning=1;
	struct stat st;
	int flags=0;
	libraw_data_t *rawdata;
	ssize_t n;
	char *filters[]={"R","G","B",NULL};
	char outname[NAME_MAX];
	fitsfile *outfile[3];


	if(argc < 2)
	{
		usage(argv[0]);
		return -1;
	}

	while((opt=getopt(argc,argv,"vD:B:b:?"))>=0)
		switch(opt)
		{
		case 'v':
			verbose++;
			break;
		case 'D':
			destdir=optarg;
			break;
		case 'B':
			if(strlen(optarg)!=4 || !strchr(optarg,'R') ||
			   !strchr(optarg,'G') || !strchr(optarg,'B'))
				fprintf(stderr,"Invalid bayer pattern, using default (%s)\n",
						bayer);
			else
				bayer=optarg;
			break;
		case 'b':
			binning=(int)strtol(optarg, &p, 10);
			if(p==optarg)
			{
				usage(argv[0]);
				return -1;
			}
			break;
		case '?':
		default:
			usage(argv[0]);
			return -1;
		}

	if(stat(destdir, &st) && mkdir(destdir, (mode_t)0755))
	{
		fprintf(stderr, "Directory \"%s\" does not exist and "
				"cannot be created.\n", destdir);
		return -1;
	}

	rawdata=libraw_init(flags);
	if(!rawdata)
	{
		fprintf(stderr, "Error initializing Libraw\n");
		return -1;
	}

	for(; optind<argc; optind++)
	{
		err=libraw_open_file(rawdata,argv[optind]);
		if(!err)
		{
			if(verbose>1)
				print_header(rawdata);
			p=strrchr(argv[optind],'.');
			if(p)
				*p=0;

			for(n=0; n<3; n++)
			{
				sprintf(outname, "!%s/%s-%s.fits", destdir,
						basename(argv[optind]), filters[n]);
				if(verbose>0)
					printf("writing %s\n",outname);
				if(fits_create_file(&outfile[n], outname, &err))
				{
					fits_report_error(stderr, err);
					break;
				}
			}

			if(n==rawdata->idata.colors)
			{
				err=libraw_unpack(rawdata);
				if(!err)
					raw2fits(rawdata,filters,bayer,outfile,binning);
				else
					fprintf(stderr,"%s\n",libraw_strerror(err));
			}

			for(n--; n>=0; n--)
				fits_close_file(outfile[n],&err);
		}
		else
			fprintf(stderr, "%s\n", libraw_strerror(err));
	
		libraw_recycle(rawdata);
	}

	libraw_close(rawdata);

	return err;
}
