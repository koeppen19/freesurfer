/**
 * @file  mri_nlfilter.c
 * @brief REPLACE_WITH_ONE_LINE_SHORT_DESCRIPTION
 *
 * REPLACE_WITH_LONG_DESCRIPTION_OR_REFERENCE
 */
/*
 * Original Author: REPLACE_WITH_FULL_NAME_OF_CREATING_AUTHOR 
 * CVS Revision Info:
 *    $Author: nicks $
 *    $Date: 2011/03/02 00:04:23 $
 *    $Revision: 1.14 $
 *
 * Copyright © 2011 The General Hospital Corporation (Boston, MA) "MGH"
 *
 * Terms and conditions for use, reproduction, distribution and contribution
 * are found in the 'FreeSurfer Software License Agreement' contained
 * in the file 'LICENSE' found in the FreeSurfer distribution, and here:
 *
 * https://surfer.nmr.mgh.harvard.edu/fswiki/FreeSurferSoftwareLicense
 *
 * Reporting: freesurfer@nmr.mgh.harvard.edu
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include <memory.h>

#include "filter.h"
#include "macros.h"
#include "error.h"
#include "diag.h"
#include "proto.h"
#include "utils.h"
#include "mri.h"
#include "region.h"
#include "version.h"

static char vcid[] = "$Id: mri_nlfilter.c,v 1.14 2011/03/02 00:04:23 nicks Exp $";

int main(int argc, char *argv[]) ;
static int get_option(int argc, char *argv[]) ;
static void usage_exit(void) ;
static void print_usage(void) ;
static void print_help(void) ;
static void print_version(void) ;

#define GAUSSIAN_SIGMA  2.0f
#define BLUR_SIGMA      0.5f
#define MAX_LEN         6
#define OFFSET_WSIZE    3
#define FILTER_WSIZE    3

char *Progname ;

static int no_offset = 0 ;
static int filter_type = FILTER_MINMAX ;
static float gaussian_sigma = GAUSSIAN_SIGMA ;
static float blur_sigma = BLUR_SIGMA ;
static int   offset_search_len = MAX_LEN ;
static int   offset_window_size = OFFSET_WSIZE ;
static int   filter_window_size = FILTER_WSIZE ;

static MRI *mri_gaussian ;

#define REGION_SIZE   16

#if 0
#undef DIAG_VERBOSE_ON
#define DIAG_VERBOSE_ON   1
#endif

static int region_size = REGION_SIZE ;
int
main(int argc, char *argv[]) {
  char   **av ;
  int    ac, nargs, width, height, depth, x, y, z, xborder, yborder, zborder,
  xrborder, yrborder, zrborder ;
  char   *in_fname, *out_fname ;
  MRI    *mri_smooth, *mri_grad, *mri_filter_src, *mri_filter_dst, *mri_dst,
  *mri_tmp, *mri_blur, *mri_src, *mri_filtered, *mri_direction,
  *mri_offset, *mri_up, *mri_polv, *mri_dir, *mri_clip, *mri_full ;
  MRI_REGION  region, clip_region ;

  /* rkt: check for and handle version tag */
  nargs = handle_version_option (argc, argv, "$Id: mri_nlfilter.c,v 1.14 2011/03/02 00:04:23 nicks Exp $", "$Name: release_5_3_0 $");
  if (nargs && argc - nargs == 1)
    exit (0);
  argc -= nargs;

  Progname = argv[0] ;
  ErrorInit(NULL, NULL, NULL) ;
  DiagInit(NULL, NULL, NULL) ;

  ac = argc ;
  av = argv ;
  for ( ; argc > 1 && ISOPTION(*argv[1]) ; argc--, argv++) {
    nargs = get_option(argc, argv) ;
    argc -= nargs ;
    argv += nargs ;
  }

  if (argc < 3)
    usage_exit() ;

  in_fname = argv[1] ;
  out_fname = argv[2] ;

  mri_full = mri_src = MRIread(in_fname) ;
  if (!mri_src)
    ErrorExit(ERROR_NOFILE, "%s: could not read '%s'", Progname, in_fname) ;
  if (!FZERO(blur_sigma))   /* allocate a blurring kernel */
  {
    mri_blur = MRIgaussian1d(blur_sigma, 0) ;
    if (!mri_blur)
      ErrorExit(ERROR_BADPARM,
                "%s: could not allocate blurring kernel with sigma=%2.3f",
                Progname, blur_sigma) ;
  } else
    mri_blur = NULL ;

  MRIboundingBox(mri_full, 0, &clip_region) ;
  REGIONexpand(&clip_region, &clip_region, (filter_window_size+1)/2) ;
  mri_src = MRIextractRegion(mri_full, NULL, &clip_region) ;
  width = mri_src->width ;
  height = mri_src->height ;
  depth = mri_src->depth ;
  mri_dst = MRIclone(mri_src, NULL) ;
  if (!mri_dst)
    ErrorExit(ERROR_NOFILE, "%s: could allocate space for destination image",
              Progname) ;

  for (z = 0 ; z < depth ; z += region_size) {
    for (y = 0 ; y < height ; y += region_size) {
      DiagHeartbeat((float)(z*height+y) / (float)(height*(depth-1))) ;
      for (x = 0 ; x < width ; x += region_size) {
        region.x = x ;
        region.y = y ;
        region.z = z ;
        region.dx = region.dy = region.dz = region_size ;
        if (region.x == 142)
          DiagBreak() ;
        REGIONexpand(&region, &region, (filter_window_size+1)/2) ;
        MRIclipRegion(mri_src, &region, &region) ;
        if (region.x == 142)
          DiagBreak() ;

        /* check for < 0 width regions */
        xborder = x-region.x ;
        yborder = y-region.y ;
        zborder = z-region.z ;
        xrborder = MAX(0, (region.dx-xborder) - region_size) ;
        yrborder = MAX(0, (region.dy-yborder) - region_size) ;
        zrborder = MAX(0, (region.dz-zborder) - region_size) ;
#if 0
        if (region.dx < 2*xborder || region.dy<2*yborder||region.dz<2*zborder)
          continue ;
#endif

        if (DIAG_VERBOSE_ON && (Gdiag & DIAG_SHOW))
          fprintf(stderr, "extracting region (%d, %d, %d) --> (%d, %d, %d)...",
                  region.x,region.y,region.z, region.x+region.dx-1,
                  region.y+region.dy-1,region.z+region.dz-1) ;
        mri_clip = MRIextractRegion(mri_src, NULL, &region) ;
        if (DIAG_VERBOSE_ON && (Gdiag & DIAG_SHOW))
          fprintf(stderr, "done.\nsmoothing region and up-sampling...") ;

        if (mri_blur)   /* smooth the input image to generate offset field */
          mri_smooth = MRIconvolveGaussian(mri_clip, NULL, mri_blur) ;
        else
          mri_smooth = MRIcopy(mri_clip, NULL) ;  /* no smoothing */
        if (!mri_smooth)
          ErrorExit(ERROR_BADPARM, "%s: image smoothing failed", Progname) ;

        /* now up-sample the smoothed image, and compute offset field in
           up-sampled domain */
        mri_up = MRIupsample2(mri_smooth, NULL) ;
        if (!mri_up)
          ErrorExit(ERROR_BADPARM, "%s: up sampling failed", Progname) ;
        MRIfree(&mri_smooth) ;
        if (DIAG_VERBOSE_ON && (Gdiag & DIAG_SHOW))
          fprintf(stderr, "done.\n") ;
        mri_smooth = mri_up ;
        mri_grad = MRIsobel(mri_smooth, NULL, NULL) ;
        mri_dir = MRIclone(mri_smooth, NULL) ;
        MRIfree(&mri_smooth) ;
        if (DIAG_VERBOSE_ON && (Gdiag & DIAG_SHOW))
          fprintf(stderr, "computing direction map...") ;
        mri_direction =
          MRIoffsetDirection(mri_grad, offset_window_size, NULL, mri_dir) ;

        if (DIAG_VERBOSE_ON && (Gdiag & DIAG_SHOW))
          fprintf(stderr, "computing offset magnitudes...") ;
        MRIfree(&mri_grad) ;
        mri_offset =
          MRIoffsetMagnitude(mri_direction, NULL, offset_search_len);
        MRIfree(&mri_direction) ;
        if (!mri_offset)
          ErrorExit(ERROR_NOMEMORY,
                    "%s: offset calculation failed", Progname) ;
        if (DIAG_VERBOSE_ON && (Gdiag & DIAG_SHOW))
          fprintf(stderr, "done.\nfiltering image...") ;
        mri_filter_src = MRIupsample2(mri_clip, NULL) ;
        MRIfree(&mri_clip) ;
        switch (filter_type) {
        case FILTER_CPOLV_MEDIAN:
          mri_polv = MRIplaneOfLeastVarianceNormal(mri_filter_src,NULL,5);
          mri_filter_dst =
            MRIpolvMedian(mri_filter_src, NULL, mri_polv,filter_window_size);
          MRIfree(&mri_polv) ;
          break ;
        case FILTER_GAUSSIAN:
          mri_filter_dst =
            MRIconvolveGaussian(mri_filter_src, NULL, mri_gaussian) ;
          if (!mri_filter_dst)
            ErrorExit(ERROR_NOMEMORY,
                      "%s: could not allocate temporary buffer space",Progname);
          break ;
        case FILTER_MEDIAN:
          mri_filter_dst = MRImedian(mri_filter_src,NULL,filter_window_size, NULL);
          if (!mri_filter_dst)
            ErrorExit(ERROR_NOMEMORY,
                      "%s: could not allocate temporary buffer space",Progname);
          break ;
        case FILTER_MEAN:
          mri_filter_dst = MRImean(mri_filter_src, NULL, filter_window_size) ;
          if (!mri_filter_dst)
            ErrorExit(ERROR_NOMEMORY,
                      "%s: could not allocate temporary buffer space",Progname);
          break ;
        case FILTER_MINMAX:
          mri_filter_dst =
            MRIminmax(mri_filter_src, NULL, mri_dir, filter_window_size) ;
          if (!mri_filter_dst)
            ErrorExit(ERROR_NOMEMORY,
                      "%s: could not allocate space for filtered image",
                      Progname) ;
          break ;
        default:
          mri_filter_dst = MRIcopy(mri_filter_src, NULL) ; /* no filtering */
          break ;
        }
        MRIfree(&mri_dir) ;
        MRIfree(&mri_filter_src) ;

        if (no_offset)
        {
          mri_filtered = MRIcopy(mri_filter_dst, NULL) ;
        }
        else
        {
          if (DIAG_VERBOSE_ON && (Gdiag & DIAG_SHOW))
            fprintf(stderr, "applying offset field...") ;
          if (Gdiag & DIAG_WRITE)
            MRIwrite(mri_filter_dst, "minmax.mgz") ;
          mri_filtered = MRIapplyOffset(mri_filter_dst, NULL, mri_offset) ;
          if (!mri_filtered)
            ErrorExit(ERROR_NOMEMORY,
                      "%s: could not allocate filtered image", Progname) ;
          if (DIAG_VERBOSE_ON && (Gdiag & DIAG_SHOW))
            fprintf(stderr, "done.\n") ;
          if (region.x == 142)
          DiagBreak() ;
          MRIfree(&mri_offset) ;
        }
        MRIfree(&mri_filter_dst) ;
        if (Gdiag & DIAG_WRITE)
          MRIwrite(mri_filtered, "upfilt.mgz") ;
        mri_tmp = MRIdownsample2(mri_filtered, NULL) ;
        MRIfree(&mri_filtered) ;
        if (Gdiag & DIAG_WRITE)
          MRIwrite(mri_tmp, "downfilt.mgz") ;
        region.x += xborder ;
        region.y += yborder ;
        region.z += zborder ;
#if 0
        region.dx -=2*xborder;
        region.dy-= 2*yborder;
        region.dz -= 2 * zborder;
#else
        region.dx -= xrborder + xborder;
        region.dy -= yrborder + yborder;
        region.dz -= zrborder + zborder;
#endif
        if (region.dx <= 0 || region.dy <= 0 || region.dz <= 0) {
          fprintf(stderr, "invalid region: (%d,%d,%d) --> (%d,%d,%d)\n",
                  region.x,region.y,region.z,region.dx,region.dy,region.dz);
        } else
          MRIextractIntoRegion(mri_tmp,mri_dst,xborder,yborder,zborder,&region);
        MRIfree(&mri_tmp);
      }
    }
  }
  MRIextractIntoRegion(mri_dst,mri_full, 0, 0, 0, &clip_region);
  MRIfree(&mri_dst) ;
  if (DIAG_VERBOSE_ON && (Gdiag & DIAG_SHOW))
    fprintf(stderr, "writing output image %s...", out_fname) ;
  MRIwrite(mri_full, out_fname) ;
  MRIfree(&mri_full) ;
  if (DIAG_VERBOSE_ON && (Gdiag & DIAG_SHOW))
    fprintf(stderr, "done.\n") ;
  exit(0) ;
  return(0) ;  /* for ansi */
}

/*----------------------------------------------------------------------
            Parameters:

           Description:
----------------------------------------------------------------------*/
static int
get_option(int argc, char *argv[]) {
  int  nargs = 0 ;
  char *option ;

  option = argv[1] + 1 ;            /* past '-' */
  if (!stricmp(option, "-help"))
    print_help() ;
  else if (!stricmp(option, "gaussian")) {
    if (sscanf(argv[2], "%f", &gaussian_sigma) < 1)
      ErrorExit(ERROR_BADPARM, "%s: could not scan sigma of Gaussian from %s",
                Progname, argv[2]) ;
    if (gaussian_sigma <= 0.0f)
      ErrorExit(ERROR_BADPARM, "%s: Gaussian sigma must be positive",Progname);
    mri_gaussian = MRIgaussian1d(gaussian_sigma, 0) ;
    filter_type = FILTER_GAUSSIAN ;
    nargs = 1 ;
  } else if (!stricmp(option, "cpolv"))
    filter_type = FILTER_CPOLV_MEDIAN ;
  else if (!stricmp(option, "minmax"))
    filter_type = FILTER_MINMAX ;
  else if (!stricmp(option, "median"))
    filter_type = FILTER_MEDIAN ;
  else if (!stricmp(option, "none"))
    filter_type = FILTER_NONE ;
  else if (!stricmp(option, "mean"))
    filter_type = FILTER_MEAN ;
  else if (!stricmp(option, "-version"))
    print_version() ;
  else if (!stricmp(option, "blur")) {
    if (sscanf(argv[2], "%f", &blur_sigma) < 1)
      ErrorExit(ERROR_BADPARM, "%s: could not scan blurring sigma from  '%s'",
                Progname, argv[2]) ;
    if (blur_sigma < 0.0f)
      ErrorExit(ERROR_BADPARM, "%s: blurring sigma must be positive",Progname);
    nargs = 1 ;
  } else switch (toupper(*option)) {
    case 'F':
      if (sscanf(argv[2], "%d", &filter_window_size) < 1)
        ErrorExit(ERROR_BADPARM, "%s: could not scan window size from '%s'",
                  Progname, argv[2]) ;
      if (filter_window_size < 3)
        ErrorExit(ERROR_BADPARM, "%s: offset window size must be > 3",Progname);
      nargs = 1 ;
      break ;
  case 'N':
    no_offset =1 ;
    printf("not using offset in filtering\n") ;
    break ;
    case 'W':
      if (sscanf(argv[2], "%d", &offset_window_size) < 1)
        ErrorExit(ERROR_BADPARM, "%s: could not scan window size from '%s'",
                  Progname, argv[2]) ;
      if (offset_window_size < 3)
        ErrorExit(ERROR_BADPARM, "%s: offset window size must be > 3",Progname);
      nargs = 1 ;
      break ;
    case 'B':
      if (sscanf(argv[2], "%f", &blur_sigma) < 1)
        ErrorExit(ERROR_BADPARM, "%s: could not scan blurring sigma from  '%s'",
                  Progname, argv[2]) ;
      if (blur_sigma < 0.0f)
        ErrorExit(ERROR_BADPARM, "%s: blurring sigma must be positive",Progname);
      nargs = 1 ;
      break ;
    case 'R':
      sscanf(argv[2], "%d", &region_size) ;
      nargs = 1 ;
      if (DIAG_VERBOSE_ON && (Gdiag & DIAG_SHOW))
        fprintf(stderr, "using region size = %d\n", region_size) ;
      break ;
    case '?':
    case 'U':
      print_usage() ;
      exit(1) ;
      break ;
    default:
      fprintf(stderr, "unknown option %s\n", argv[1]) ;
      exit(1) ;
      break ;
    }

  return(nargs) ;
}

static void
usage_exit(void) {
  print_usage() ;
  exit(1) ;
}

static void
print_usage(void) {
  fprintf(stderr,
          "usage: %s [options] <input image file> <output image file>\n",
          Progname) ;
}

static void
print_help(void) {
  print_usage() ;
  fprintf(stderr,
          "\nThis program will process the image contained in "
          "<input image file>\n"
          "using a nonlocal filter, and write the results to "
          "<output image file>.\n"
          "\nThe default filter is the median, but either Gaussian or mean "
          "filtering\n"
          "can be specified through command line options (see below).\n"
          "\nBy default the image is smoothed using a Gaussian kernel "
          "(sigma=%2.3f)\n"
          "before the offset field is calculated. This can be modified using\n"
          "the -blur command line option.\n"
          "\nThe input and output image formats are specified by the file name"
          " extensions.\n"
          "Supported formats are:\n\n"
          "   HIPS   (.hipl or .hips)\n"
          "   MATLAB (.mat)\n"
          "   TIFF   (.tif or .tiff).\n"
          "\nNote that 8-bit output images, which are generated if the input\n"
          "image is 8-bit, are scaled to be in the range 0-255.\n",
          BLUR_SIGMA) ;
  fprintf(stderr, "\nvalid options are:\n\n") ;
  fprintf(stderr,
          "  -blur <sigma>     - specify sigma of blurring "
          "kernel (default=%2.3f).\n", BLUR_SIGMA) ;
  fprintf(stderr,
          "  -gaussian <sigma> - filter with Gaussian instead of median.\n") ;
  fprintf(stderr,
          "  -mean             - filter with mean instead of median.\n") ;
  fprintf(stderr,
          "  -w <window size>  - specify window used for offset calculation "
          "(default=%d).\n", OFFSET_WSIZE) ;
  fprintf(stderr,
          "  --version         - display version #.\n") ;
  fprintf(stderr,
          "  --help            - display this help message.\n\n") ;
  exit(1) ;
}

static void
print_version(void) {
  fprintf(stderr, "%s\n", vcid) ;
  exit(1) ;
}

