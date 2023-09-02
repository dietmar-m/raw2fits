#include <stdio.h>
#include <time.h>
#include <libraw/libraw.h>
#include <fitsio.h>

#include "raw2fits.h"

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
	printf("filters    = 0x%08x\n",rawdata->idata.filters);
	printf("cdesc      = %s\n",rawdata->idata.cdesc);
	printf("cameratemp = %.2f\n",rawdata->other.CameraTemperature);
	printf("sensortemp = %.2f\n",rawdata->other.SensorTemperature);
	printf("sensortemp2= %.2f\n",rawdata->other.SensorTemperature2);
	return 0;
}

static int write_header(libraw_data_t *rawdata, fitsfile *outfile)
{
	int err=0;
	char buffer[128];
	struct tm *tm;

	sprintf(buffer, "%s %s", rawdata->idata.make, rawdata->idata.model);
	fits_write_key_str(outfile, "INSTRUME", buffer,
					   "instrument used for acquisition", &err);
	fits_write_key_lng(outfile, "EXPTIME", (long)rawdata->other.shutter,
					   "exposure time [s]", &err);
	fits_write_key_lng(outfile, "ISOSPEED", (long)rawdata->other.iso_speed,
					   "ISO speed", &err);
	tm=gmtime(&rawdata->other.timestamp);
	sprintf(buffer,"%04d-%02d-%02dT%02d:%02d:%02d",
			tm->tm_year+1900,tm->tm_mon+1,tm->tm_mday,
			tm->tm_hour,tm->tm_min,tm->tm_sec);
	fits_write_key_str(outfile, "DATE-OBS", buffer,
					   "UTC start of observation", &err);
	fits_write_key_str(outfile, "OBSERVER", rawdata->other.artist,
					   "who took the image",&err);
	fits_write_key_fixflt(outfile, "CAM-TEMP", rawdata->other.CameraTemperature,
						  2, "camera temperature [C]", &err);
	fits_write_date(outfile, &err);

	return err;
}


static int get_matrix(char *bayer, int filter, matrix_t **mat)
{
	static matrix_t BGGR[][2]={
		{{1,1},{-1}},
		{{1,0},{0,1}},
		{{0,0},{-1}},
	};
	static matrix_t GRBG[][2]={
		{{1,0},{-1}},
		{{0,0},{1,1}},
		{{0,1},{-1}},
	};
	static matrix_t GBRG[][2]={
		{{0,1},{-1}},
		{{0,0},{1,1}},
		{{1,0},{-1}},
	};
	static matrix_t RGGB[][2]={
		{{0,0},{-1}},
		{{1,0},{0,1}},
		{{1,1},{-1}},
	};

	if(!strcmp(bayer,"BGGR"))
		*mat=BGGR[filter];
	else if(!strcmp(bayer,"GRBG"))
		*mat=GRBG[filter];
	else if(!strcmp(bayer,"GBRG"))
		*mat=GBRG[filter];
	else if(!strcmp(bayer,"RGGB"))
		*mat=RGGB[filter];
	else
		return -EINVAL;

	if((*mat)[1].dx>=0)
		return 2;
	return 1;
}


// save some typing
#define RAW_PIXEL(row, col, dx, dy) \
	rawdata->rawdata.raw_image[(row*2+rawdata->sizes.top_margin+dy)*\
							   rawdata->sizes.raw_width+col*2+\
							   rawdata->sizes.left_margin+dx]


int raw2fits(libraw_data_t *rawdata, char **filters, char *bayer,
			 fitsfile **outfile, uint binning)
{
	int err=0;
	size_t f; // filter
	// image dimensions
	int naxis=2;
	int width=rawdata->sizes.width/(2*binning);
	int height=rawdata->sizes.height/(2*binning);
	long naxes[]={width, height};
	ushort *data; // image data
	matrix_t *mat; // bayer pattern

	data=malloc(width*height*sizeof(*data));
	if(!data)
		return -ENOMEM;

	for(f=0; filters[f]; f++)
	{
		int mat_c=get_matrix(bayer,f,&mat);
		if(mat_c<0)
		{
			err=mat_c;
			break;
		}

		fits_create_img(outfile[f], SHORT_IMG, naxis, naxes, &err);
		if(!err)
		{
			fits_write_key_str(outfile[f], "FILTER", filters[f],
							   "filter", &err);
			fits_write_key_lng(outfile[f], "XBINNING", binning,
							   "binning factor x", &err);
			fits_write_key_lng(outfile[f], "YBINNING", binning,
							   "binning factor y", &err);
		}

		if(!err)
		{
			size_t row,col;
			uint bv,bh;
			int pixel=0;
			int c;

			for(row=0; row<height; row++)
				for(col=0; col<width; col++)
				{
					pixel=0;
					for(bv=0; bv<binning; bv++)
						for(bh=0; bh<binning; bh++)
							for(c=0; c<mat_c; c++)
								pixel+=RAW_PIXEL(row*binning,col*binning,
												 mat[c].dx+2*bh,
												 mat[c].dy+2*bv);
					pixel/=mat_c*binning*binning;
					data[row*width+col]=(ushort)(pixel > USHRT_MAX ?
												 USHRT_MAX : pixel);
				}
			fits_write_img(outfile[f], TUSHORT, 1,
						   width*height, data, &err);
			if(!err)
				err=write_header(rawdata,outfile[f]);
		}

		if(err)
			break;
	}

	free(data);
	if(err)
		fits_report_error(stderr, err);

	return err;
}
