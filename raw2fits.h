#ifndef RAW2FITS_H
#define RAW2FITS_H

extern int verbose;

int print_header(libraw_data_t *rawdata);
int raw2fits(libraw_data_t *rawdata, char **filters,
			 fitsfile **outfile, uint binning);

#endif
