#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <libraw/libraw.h>
#include <fitsio.h>

#include "raw2fits.h"


int usage(char *argv0)
{
	fprintf(stderr, "USAGE:\n");
	fprintf(stderr, "%s <filename>\n",argv0);
	return 0;
}

int main(int argc, char **argv)
{
	libraw_data_t *rawdata;
	int flags=0;
	int err=0;

	if(argc != 2)
	{
		usage(argv[0]);
		return -1;
	}
	rawdata=libraw_init(flags);
	if(!rawdata)
	{
		fprintf(stderr, "Error initializing Libraw\n");
		return -1;
	}
	err=libraw_open_file(rawdata,argv[1]);
	if(!err)
	{
		char outname[NAME_MAX];
		char *p;
		char *filters[]={"R","G","B",NULL};
		ssize_t n;
		fitsfile *outfile[3];

		print_header(rawdata);
		p=strrchr(argv[1],'.');
		if(p)
			*p=0;

		for(n=0; n<3; n++)
		{
			sprintf(outname, "!%s-%s.fits", argv[1], filters[n]);
			//printf("%s\n",outname);
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
				raw2fits(rawdata,filters,outfile);
			else
				fprintf(stderr,"%s\n",libraw_strerror(err));
		}

		for(n--; n>=0; n--)
			fits_close_file(outfile[n],&err);
	}
	else
		fprintf(stderr, "%s\n", libraw_strerror(err));
	
	libraw_close(rawdata);
	return err;
}