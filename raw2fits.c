#include <stdio.h>
#include <time.h>
#include <libraw/libraw.h>
#include <fitsio.h>

typedef struct { int dx,dy;} matrix_t;

int print_header(libraw_data_t *rawdata)
{
	printf("make       = \"%s\"\n",rawdata->idata.make);
	printf("model      = \"%s\"\n",rawdata->idata.model);
	printf("colors     = %d\n",rawdata->idata.colors);
	printf("raw_height = %d\n",(int)rawdata->sizes.raw_height);
	printf("raw_width  = %d\n",(int)rawdata->sizes.raw_width);
	printf("height     = %d\n",(int)rawdata->sizes.height);
	printf("width      = %d\n",(int)rawdata->sizes.width);
	printf("top        = %d\n",(int)rawdata->sizes.top_margin);
	printf("left       = %d\n",(int)rawdata->sizes.left_margin);
	printf("flip       = %d\n",rawdata->sizes.flip);
	printf("iso_speed  = %.0f\n",rawdata->other.iso_speed);
	printf("shutter    = %.0f\n",rawdata->other.shutter);
	printf("artist     = \"%s\"\n",rawdata->other.artist);
	printf("timestamp  = %s",ctime(&rawdata->other.timestamp));
	printf("cdesc      = %s\n",rawdata->idata.cdesc);
	return 0;
}

static int write_header(libraw_data_t *rawdata, fitsfile *outfile)
{
	int err=0;
	char buffer[128];
	struct tm *tm;

	sprintf(buffer, "%s %s", rawdata->idata.make, rawdata->idata.model);
	fits_write_key_str(outfile, "INSTRUME", buffer,
					   "Instrument used for acquisition", &err);
	fits_write_key_lng(outfile, "EXPTIME", (long)rawdata->other.shutter,
					   "[s] Total Exposure Time", &err);
	tm=gmtime(&rawdata->other.timestamp);
	sprintf(buffer,"%04d-%02d-%02dT%02d:%02d:%02d",
			tm->tm_year+1900,tm->tm_mon+1,tm->tm_mday,
			tm->tm_hour,tm->tm_min,tm->tm_sec);
	fits_write_key_str(outfile, "DATE-OBS", buffer,
					   "UTC start date of observation", &err);
	return err;
}


static matrix_t *get_matrix(libraw_data_t *rawdata, int filter)
{
	static matrix_t RGBG[][2]={
		{{0,0},{-1,-1}},
		{{1,0},{0,1}},
		{{1,1},{-1,-1}},
	};

	if(!strcmp(rawdata->idata.cdesc,"RGBG"))
		return RGBG[filter];

	return NULL;
}


// for convenience
#define RAW_PIXEL(row, col, dx, dy) \
	rawdata->rawdata.raw_image[(row*2+top+dy)*raw_width+col*2+left+dx]

int raw2fits(libraw_data_t *rawdata, char **filters, fitsfile **outfile)
{
	int err=0;
	size_t f; // filter
	// image dimensions
	int naxis=2;
	int width=rawdata->sizes.width/2;
	int height=rawdata->sizes.height/2;
	long naxes[]={width, height};
	ushort *data; // image data
	// save some typing
	ushort raw_width=rawdata->sizes.raw_width;
	ushort top=rawdata->sizes.top_margin;
	ushort left=rawdata->sizes.left_margin;
	// bayer pattern for EOS600D
	/*
	matrix_t mat[][2]={
		{{0,0},{-1,-1}},
		{{1,0},{0,1}},
		{{1,1},{-1,-1}},
	};
	*/

	data=malloc(width*height*sizeof(*data));
	if(!data)
		return -ENOMEM;

	for(f=0; filters[f]; f++)
	{
		matrix_t *mat;

		mat=get_matrix(rawdata,f);
		if(!mat)
		{
			err=-1;
			break;
		}

		fits_create_img(outfile[f], SHORT_IMG, naxis, naxes, &err);
		if(!err)
			fits_write_key(outfile[f], TSTRING, "FILTER",
						   filters[f], NULL, &err);
		if(!err)
		{
			size_t row,col;
			int pixel=0;
			int c;

			for(row=0; row<height; row++)
				for(col=0; col<width; col++)
				{
					/*
					switch(filter)
					{
					case 0: // red
						pixel=RAW_PIXEL(row,col,0,0);
						break;
					case 1: // green
						pixel=RAW_PIXEL(row,col,1,0)+RAW_PIXEL(row,col,0,1);
						pixel/=2;
						break;
					case 2: //blue
						pixel=RAW_PIXEL(row,col,1,1);
						break;
					}
					*/
					pixel=0;
					/*
					for(c=0; c<2 && mat[f][c].dx>=0; c++)
						pixel+=RAW_PIXEL(row,col,mat[f][c].dx,mat[f][c].dy);
					*/
					for(c=0; c<2 && mat[c].dx>=0; c++)
						pixel+=RAW_PIXEL(row,col,mat[c].dx,mat[c].dy);
					pixel/=c;
					data[row*width+col]=(ushort)pixel;
				}
			fits_write_img(outfile[f], TUSHORT, 1,
						   width*height, data, &err);
			if(!err)
				write_header(rawdata,outfile[f]);
		}
		else
			break;
	}

	free(data);
	if(err)
		fits_report_error(stderr, err);

	return err;
}
