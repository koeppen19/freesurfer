/**
 * @file  mris_inflate.c
 * @brief binary for inflating a surface.
 *
 * "Cortical Surface-Based Analysis II: Inflation, Flattening, and a
 * Surface-Based Coordinate System", Fischl, B., Sereno, M.I., Dale, A.M.
 * (1999) NeuroImage, 9(2):195-207.
 * Program for "inflating" a surface by modeling it as a mesh of springs
 * and applying an area correction post-hoc to ensure area preservation.
 */
/*
 * Original Author: Bruce Fischl
 * CVS Revision Info:
 *    $Author: nicks $
 *    $Date: 2011/03/02 00:04:32 $
 *    $Revision: 1.43 $
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
#include <string.h>
#include <math.h>
#include <ctype.h>

#include "macros.h"
#include "error.h"
#include "diag.h"
#include "proto.h"
#include "tags.h"
#include "timer.h"
#include "mrisurf.h"
#include "mri.h"
#include "macros.h"
#include "version.h"

#ifdef FS_CUDA
#include "devicemanagement.h"
#endif // FS_CUDA

static char vcid[] =
  "$Id: mris_inflate.c,v 1.43 2011/03/02 00:04:32 nicks Exp $";

int main(int argc, char *argv[]) ;

static int  get_option(int argc, char *argv[]) ;
/*static void usage_exit(void) ;*/
static void print_usage(void) ;
static void print_help(void) ;
static void print_version(void) ;

char *Progname ;

static INTEGRATION_PARMS  parms ;
static int talairach_flag = 0 ;
static int nbrs = 2 ;
static int navgs = 0 ;
static int DEFAULT_ITERATIONS = 10;
static float DEFAULT_DIST = 0.1f;

#define BASE_DT_SCALE    1.0
static float base_dt_scale = BASE_DT_SCALE ;

static int SaveSulc = 1;
static char *sulc_name = "sulc" ;

int
main(int argc, char *argv[])
{
  char         **av, *in_fname, *out_fname, fname[STRLEN], *cp, path[STRLEN] ;
  int          ac, nargs ;
  MRI_SURFACE  *mris ;
  int           msec ;
  struct timeb  then ;
  float         radius ;

  char cmdline[CMD_LINE_LEN] ;

  make_cmd_version_string
  (argc, argv,
   "$Id: mris_inflate.c,v 1.43 2011/03/02 00:04:32 nicks Exp $",
   "$Name: release_5_3_0 $", cmdline);

#ifdef FS_CUDA
  // Force CUDA initialisation
  AcquireCUDADevice();
#endif // FS_CUDA

  /* rkt: check for and handle version tag */
  nargs = handle_version_option
          (argc, argv,
           "$Id: mris_inflate.c,v 1.43 2011/03/02 00:04:32 nicks Exp $",
           "$Name: release_5_3_0 $");
  if (nargs && argc - nargs == 1)
  {
    exit (0);
  }
  argc -= nargs;

  TimerStart(&then) ;
  //Gdiag |= DIAG_SHOW ;
  Progname = argv[0] ;
  ErrorInit(NULL, NULL, NULL) ;
  DiagInit(NULL, NULL, NULL) ;

  parms.base_name[0] = 0 ;
  parms.projection = NO_PROJECTION ;
  parms.desired_rms_height = 0.015 ;
  parms.tol = 1e-4 ;
  parms.epsilon = EPSILON ;
  parms.dt = 0.9 ;
  parms.base_dt = BASE_DT_SCALE*parms.dt ;
  parms.n_averages = 16 ;
  parms.l_angle = 0.0 ;
  parms.l_dist = DEFAULT_DIST ;
  parms.l_area = 0.0 ;
  parms.l_spring = 0.0 ;
  parms.l_spring_norm = 1.0 ;
  parms.l_curv = 0.0 ;
  parms.niterations = DEFAULT_ITERATIONS ;   /* per # of averages */
  parms.write_iterations = 0 /*WRITE_ITERATIONS */;
  parms.a = parms.b = parms.c = 0.0f ;  /* ellipsoid parameters */
  parms.integration_type = INTEGRATE_MOMENTUM ;
  parms.momentum = 0.9 ;
  parms.dt_increase = 1.0 /* DT_INCREASE */;
  parms.dt_decrease = 1.0 /* DT_DECREASE*/ ;
  parms.error_ratio = 50.0 /*ERROR_RATIO */;
  parms.scale = 0 ;
  /*  parms.integration_type = INTEGRATE_LINE_MINIMIZE ;*/

  ac = argc ;
  av = argv ;
  for ( ; argc > 1 && ISOPTION(*argv[1]) ; argc--, argv++)
  {
    nargs = get_option(argc, argv) ;
    argc -= nargs ;
    argv += nargs ;
  }

  if (argc < 3)
  {
    print_help() ;
  }

  in_fname = argv[1] ;
  out_fname = argv[2] ;

  if (parms.base_name[0] == 0)
  {
    FileNameOnly(out_fname, fname) ;
    cp = strchr(fname, '.') ;
    if (cp)
    {
      strcpy(parms.base_name, cp+1) ;
    }
    else
    {
      strcpy(parms.base_name, "inflated") ;
    }
  }

  if (!SaveSulc)
  {
    printf("Not saving sulc\n");
  }

  mris = MRISread(in_fname) ;
  if (!mris)
    ErrorExit(ERROR_NOFILE, "%s: could not read surface file %s",
              Progname, in_fname) ;

  MRISaddCommandLine(mris, cmdline) ;

  if (talairach_flag)
  {
    MRIStalairachTransform(mris, mris) ;
  }

  radius = MRISaverageRadius(mris) ;
#if 0
#define AVERAGE_RADIUS 50
  parms.desired_rms_height *= AVERAGE_RADIUS / radius ;
  fprintf(stderr, "average radius = %2.1f mm, set rms target to %2.3f\n",
          radius, parms.desired_rms_height) ;
#endif
  fprintf(stderr, "avg radius = %2.1f mm, total surface area = %2.0f mm^2\n",
          radius, mris->total_area) ;
  MRISsetNeighborhoodSize(mris, nbrs) ;
  MRISstoreMetricProperties(mris) ;  /* use current surface as reference */
  MRISaverageVertexPositions(mris, navgs) ;
  MRISscaleBrainArea(mris) ;  /* current properties will be stored again */
  if (FZERO(parms.l_sphere))
  {
#if 1
    MRISinflateBrain(mris, &parms) ;
#else
    parms.n_averages = 32 ;
    parms.niterations = 30 ;
    parms.l_dist = 1.0 ;
    MRISinflateBrain(mris, &parms) ;
    MRISscaleBrainArea(mris) ;
    parms.n_averages = 0 ;
    parms.niterations = 70 ;
    parms.l_dist = DEFAULT_DIST ;
    MRISinflateBrain(mris, &parms) ;
#endif
  }
  else
  {
    MRISinflateToSphere(mris, &parms) ;
  }

  fprintf(stderr, "writing inflated surface to %s\n", out_fname) ;
  MRIScenter(mris, mris) ;
  MRISscaleBrainArea(mris) ;
  MRISwrite(mris, out_fname) ;
  FileNamePath(out_fname, path) ;
  MRISzeroMeanCurvature(mris) ;  /* make sulc zero mean */

  if (SaveSulc)
  {
    sprintf(fname, "%s/%s.%s", path,
            mris->hemisphere == LEFT_HEMISPHERE ? "lh" : "rh", sulc_name) ;
    fprintf(stderr, "writing sulcal depths to %s\n", fname) ;
    MRISwriteCurvature(mris, fname) ;
  }
  else
  {
    printf("Not saving sulc\n");
  }

  msec = TimerStop(&then) ;
  fprintf(stderr, "inflation took %2.1f minutes\n", (float)msec/(60*1000.0f));

  exit(0) ;
  return(0) ;  /* for ansi */
}


/*----------------------------------------------------------------------
  Parameters:

  Description:
  ----------------------------------------------------------------------*/
static int
get_option(int argc, char *argv[])
{
  int  nargs = 0 ;
  char *option ;

  option = argv[1] + 1 ;            /* past '-' */
  if (!stricmp(option, "-help")||!stricmp(option, "-usage"))
  {
    print_help() ;
  }
  else if (!stricmp(option, "-version"))
  {
    print_version() ;
  }
  else if (!stricmp(option, "name"))
  {
    if (argc < 2)
    {
      print_usage() ;
    }
    strcpy(parms.base_name, argv[2]) ;
    nargs = 1 ;
    fprintf(stderr, "base name = %s\n", parms.base_name) ;
  }
  else if (!stricmp(option, "sulc"))
  {
    if (argc < 2)
    {
      print_usage() ;
    }
    sulc_name = argv[2] ;
    nargs = 1 ;
    fprintf(stderr, "sulc name = %s\n", sulc_name) ;
  }
  else if (!stricmp(option, "angle"))
  {
    if (argc < 2)
    {
      print_usage() ;
    }
    sscanf(argv[2], "%f", &parms.l_angle) ;
    nargs = 1 ;
    fprintf(stderr, "l_angle = %2.3f\n", parms.l_angle) ;
  }
  else if (!stricmp(option, "sphere"))
  {
    if (argc < 2)
    {
      print_usage() ;
    }
    parms.l_sphere = atof(argv[2]) ;
    nargs = 1 ;
    parms.a = 128.0f ;
    fprintf(stderr, "l_sphere = %2.3f\n", parms.l_sphere) ;
  }
  else if (!stricmp(option, "area"))
  {
    if (argc < 2)
    {
      print_usage() ;
    }
    sscanf(argv[2], "%f", &parms.l_area) ;
    nargs = 1 ;
    fprintf(stderr, "l_area = %2.3f\n", parms.l_area) ;
  }
  else if (!stricmp(option, "dist"))
  {
    if (argc < 2)
    {
      print_usage() ;
    }
    sscanf(argv[2], "%f", &parms.l_dist) ;
    nargs = 1 ;
    fprintf(stderr, "l_dist = %2.3f\n", parms.l_dist) ;
  }
  else if (!stricmp(option, "area"))
  {
    if (argc < 2)
    {
      print_usage() ;
    }
    sscanf(argv[2], "%f", &parms.l_parea) ;
    nargs = 1 ;
    fprintf(stderr, "l_parea = %2.3f\n", parms.l_parea) ;
  }
  else if (!stricmp(option, "curv"))
  {
    if (argc < 2)
    {
      print_usage() ;
    }
    parms.l_curv = atof(argv[2]) ;
    nargs = 1 ;
    fprintf(stderr, "l_curv = %2.3f\n", parms.l_curv) ;
  }
  else if (!stricmp(option, "save-sulc"))
  {
    SaveSulc=1;
  }
  else if (!stricmp(option, "no-save-sulc"))
  {
    SaveSulc=0;
  }
  else if (!stricmp(option, "spring"))
  {
    if (argc < 2)
    {
      print_usage() ;
    }
    parms.l_spring = atof(argv[2]) ;
    nargs = 1 ;
    fprintf(stderr, "l_spring = %2.3f\n", parms.l_spring) ;
  }
  else if (!stricmp(option, "nspring"))
  {
    if (argc < 2)
    {
      print_usage() ;
    }
    parms.l_nspring = atof(argv[2]) ;
    nargs = 1 ;
    fprintf(stderr, "l_nspring = %2.3f\n", parms.l_nspring) ;
  }
  else if (!stricmp(option, "tspring"))
  {
    if (argc < 2)
    {
      print_usage() ;
    }
    parms.l_tspring = atof(argv[2]) ;
    nargs = 1 ;
    fprintf(stderr, "l_tspring = %2.3f\n", parms.l_tspring) ;
  }
  else if (!stricmp(option, "spring_norm"))
  {
    if (argc < 2)
    {
      print_usage() ;
    }
    parms.l_spring_norm = atof(argv[2]) ;
    nargs = 1 ;
    fprintf(stderr, "l_spring_norm = %2.3f\n", parms.l_spring_norm) ;
  }
  else if (!stricmp(option, "tol"))
  {
    if (argc < 2)
    {
      print_usage() ;
    }
    parms.tol = atof(argv[2]) ;
    nargs = 1 ;
    fprintf(stderr, "tol = %2.2e\n", parms.tol) ;
  }
  else if (!stricmp(option, "hvariable"))
  {
    parms.flags |= IPFLAG_HVARIABLE ;
    fprintf(stderr, "variable Hdesired to drive integration\n") ;
  }
  else if (!stricmp(option, "lm"))
  {
    parms.integration_type = INTEGRATE_LINE_MINIMIZE ;
    fprintf(stderr, "integrating with line minimization\n") ;
  }
  else if (!stricmp(option, "dt"))
  {
    if (argc < 2)
    {
      print_usage() ;
    }
    parms.integration_type = INTEGRATE_MOMENTUM ;
    parms.dt = atof(argv[2]) ;
    parms.base_dt = base_dt_scale*parms.dt ;
    nargs = 1 ;
    fprintf(stderr, "momentum with dt = %2.2f\n", parms.dt) ;
  }
  else if (!stricmp(option, "avgs"))
  {
    if (argc < 2)
    {
      print_usage() ;
    }
    navgs = atoi(argv[2]) ;
    fprintf(stderr, "smoothing surface for %d iterations before inflating\n",
            navgs) ;
    nargs = 1 ;
  }
  else if (!stricmp(option, "nbrs"))
  {
    if (argc < 2)
    {
      print_usage() ;
    }
    nbrs = atoi(argv[2]) ;
    fprintf(stderr, "setting neighborhood size to %d\n", nbrs) ;
    nargs = 1 ;
  }
  else if (!stricmp(option, "error_ratio"))
  {
    if (argc < 2)
    {
      print_usage() ;
    }
    parms.error_ratio = atof(argv[2]) ;
    nargs = 1 ;
    fprintf(stderr, "error_ratio=%2.3f\n", parms.error_ratio) ;
  }
  else if (!stricmp(option, "scale"))
  {
    if (argc < 2)
    {
      print_usage() ;
    }
    parms.scale = atof(argv[2]) ;
    nargs = 1 ;
    parms.desired_rms_height = -1.0 ;
    fprintf(stderr, "scaling brain area during integration\n");
  }
  else if (!stricmp(option, "dt_inc"))
  {
    if (argc < 2)
    {
      print_usage() ;
    }
    parms.dt_increase = atof(argv[2]) ;
    nargs = 1 ;
    fprintf(stderr, "dt_increase=%2.3f\n", parms.dt_increase) ;
  }
  else if (!stricmp(option, "dt_dec"))
  {
    if (argc < 2)
    {
      print_usage() ;
    }
    parms.dt_decrease = atof(argv[2]) ;
    nargs = 1 ;
    fprintf(stderr, "dt_decrease=%2.3f\n", parms.dt_decrease) ;
  }
  else if (!stricmp(option, "seed"))
  {
    setRandomSeed(atol(argv[2])) ;
    fprintf(stderr,"setting seed for random number generator to %d\n",
            atoi(argv[2])) ;
    nargs = 1 ;
  }
  else switch (toupper(*option))
    {
    case 'T':
      talairach_flag = 1 ;
      fprintf(stderr,
              "applying talairach transform to brain before inflation\n");
      break ;
    case 'S':
      if (argc < 2)
      {
        print_usage() ;
      }
      parms.l_spring = atof(argv[2]) ;
      nargs = 1 ;
      fprintf(stderr, "l_spring = %2.3f\n", parms.l_spring) ;
      break ;
    case 'M':
      if (argc < 2)
      {
        print_usage() ;
      }
      parms.integration_type = INTEGRATE_MOMENTUM ;
      parms.momentum = atof(argv[2]) ;
      nargs = 1 ;
      fprintf(stderr, "momentum = %2.2f\n", parms.momentum) ;
      break ;
    case 'F':
      if (argc < 2)
      {
        print_usage() ;
      }
      parms.desired_rms_height = atof(argv[2]) ;
      fprintf(stderr, "desired rms height = %2.9f\n",
              parms.desired_rms_height) ;
      nargs = 1 ;
      break ;
    case 'B':
      if (argc < 2)
      {
        print_usage() ;
      }
      base_dt_scale = atof(argv[2]) ;
      parms.base_dt = base_dt_scale*parms.dt ;
      nargs = 1;
      break ;
    case 'V':
      if (argc < 2)
      {
        print_usage() ;
      }
      Gdiag_no = atoi(argv[2]) ;
      nargs = 1 ;
      break ;
    case 'E':
      if (argc < 2)
      {
        print_usage() ;
      }
      parms.epsilon = atof(argv[2]) ;
      fprintf(stderr, "using epsilon=%2.4f\n", parms.epsilon) ;
      nargs = 1 ;
      break ;
    case 'W':
      if (argc < 2)
      {
        print_usage() ;
      }
      sscanf(argv[2], "%d", &parms.write_iterations) ;
      nargs = 1 ;
      fprintf(stderr, "write iterations = %d\n", parms.write_iterations) ;
      Gdiag |= DIAG_WRITE ;
      break ;
    case 'A':
      if (argc < 2)
      {
        print_usage() ;
      }
      sscanf(argv[2], "%d", &parms.n_averages) ;
      nargs = 1 ;
      fprintf(stderr, "n_averages = %d\n", parms.n_averages) ;
      break ;
    case 'N':
      if (argc < 2)
      {
        print_usage() ;
      }
      sscanf(argv[2], "%d", &parms.niterations) ;
      nargs = 1 ;
      fprintf(stderr, "niterations = %d\n", parms.niterations) ;
      break ;
    case 'H':
#if 0
      if (argc < 2)
      {
        print_usage() ;
      }
      parms.Hdesired = atof(argv[2]) ;
      fprintf(stderr, "inflating to desired curvature %2.4f\n",
              parms.Hdesired);
      nargs = 1 ;
      break ;
#endif
    case '?':
    case 'U':
    default:
      print_help() ;
      break ;
    }

  return(nargs) ;
}

#if 0
static void
usage_exit(void)
{
  print_usage() ;
  exit(1) ;
}
#endif

#include "mris_inflate.help.xml.h"
static void
print_usage(void)
{
  outputHelpXml(mris_inflate_help_xml,
                mris_inflate_help_xml_len);
}

static void
print_help(void)
{
  print_usage() ;
  exit(1) ;
}

static void
print_version(void)
{
  fprintf(stderr, "%s\n", vcid) ;
  exit(1) ;
}

