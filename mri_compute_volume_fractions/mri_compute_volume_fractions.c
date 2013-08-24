/**
 * @file  mri_compute_volume_fraction.c
 * @brief compute the % of gm, wm and CSF in each voxel in a volume
 *
 * REPLACE_WITH_LONG_DESCRIPTION_OR_REFERENCE
 */
/*
 * Original Author: REPLACE_WITH_FULL_NAME_OF_CREATING_AUTHOR 
 * CVS Revision Info:
 *    $Author: nicks $
 *    $Date: 2011/03/02 00:04:14 $
 *    $Revision: 1.4 $
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

#include "mri.h"
#include "macros.h"
#include "error.h"
#include "diag.h"
#include "proto.h"
#include "mrimorph.h"
#include "mri_conform.h"
#include "utils.h"
#include "const.h"
#include "timer.h"
#include "version.h"
#include "mrisurf.h"
#include "registerio.h"
#include "cma.h"

#define WM_VAL         1
#define GM_VAL         2
#define CSF_VAL        3
#define SUBCORT_GM_VAL 4

int main(int argc, char *argv[]) ;
static int get_option(int argc, char *argv[]) ;

char *Progname ;
static void usage_exit(int code) ;

static int gm_val = GM_VAL ;
static int cortex_only = 0 ;
static char sdir[STRLEN] = "" ;
static double resolution = .5 ;
MRI *add_aseg_structures_outside_ribbon(MRI *mri_src, MRI *mri_aseg, MRI *mri_dst,
                                       int wm_val, int gm_val, int csf_val) ;
int MRIcomputePartialVolumeFractions(MRI *mri_src, MATRIX *m_vox2vox, 
                                     MRI *mri_seg, MRI *mri_wm, MRI *mri_gm, MRI *mri_csf,
                                     int wm_val, int gm_val, int csf_val) ;
int
main(int argc, char *argv[]) {
  char   **av, fname[STRLEN] ;
  int    ac, nargs ;
  char   *subject, *reg_fname, *in_fname, *out_stem, *cp ;
  int    msec, minutes, seconds, nvox, float2int ;
  struct timeb start ;
  MRI_SURFACE *mris_lh_white, *mris_rh_white, *mris_lh_pial, *mris_rh_pial ;
  MRI         *mri_aseg, *mri_seg, *mri_pial, *mri_tmp, *mri_ribbon, *mri_in, *mri_gm, *mri_wm, *mri_csf ;
  MATRIX      *m_regdat ;
  float       intensity, betplaneres, inplaneres ;

  /* rkt: check for and handle version tag */
  nargs = handle_version_option (argc, argv, "$Id: mri_compute_volume_fractions.c,v 1.4 2011/03/02 00:04:14 nicks Exp $", "$Name: release_5_3_0 $");
  if (nargs && argc - nargs == 1)
    exit (0);
  argc -= nargs;

  Progname = argv[0] ;
  ac = argc ;
  av = argv ;
  for ( ; argc > 1 && ISOPTION(*argv[1]) ; argc--, argv++) {
    nargs = get_option(argc, argv) ;
    argc -= nargs ;
    argv += nargs ;
  }
  printf("arg = %d\n", argc) ;

  if (argc < 4)
    usage_exit(1) ;
  if (!strlen(sdir)) {
    cp = getenv("SUBJECTS_DIR") ;
    if (!cp)
      ErrorExit(ERROR_BADPARM,
                "%s: SUBJECTS_DIR not defined in environment.\n", Progname) ;
    strcpy(sdir, cp) ;
  }
  reg_fname = argv[1] ; in_fname = argv[2] ;
  out_stem = argv[3] ; Progname = argv[0] ;
  ErrorInit(NULL, NULL, NULL) ;
  DiagInit(NULL, NULL, NULL) ;

  TimerStart(&start) ;

  printf("reading registration file %s\n", reg_fname) ;
  regio_read_register(reg_fname, &subject, &inplaneres,
                        &betplaneres, &intensity,  &m_regdat,
                        &float2int);

  m_regdat = regio_read_registermat(reg_fname) ;
  if (m_regdat == NULL)
    ErrorExit(ERROR_NOFILE, "%s: could not load registration file from %s", Progname,reg_fname) ;

  sprintf(fname, "%s/%s/surf/lh.white", sdir, subject) ;
  printf("reading surface %s\n", fname) ;
  mris_lh_white = MRISread(fname) ;
  if (mris_lh_white == NULL)
    ErrorExit(ERROR_NOFILE, "%s: could not load lh white surface from %s", Progname,fname) ;

  sprintf(fname, "%s/%s/surf/rh.white", sdir, subject) ;
  printf("reading surface %s\n", fname) ;
  mris_rh_white = MRISread(fname) ;
  if (mris_rh_white == NULL)
    ErrorExit(ERROR_NOFILE, "%s: could not load rh white surface from %s", Progname,fname) ;

  sprintf(fname, "%s/%s/surf/lh.pial", sdir, subject) ;
  printf("reading surface %s\n", fname) ;
  mris_lh_pial = MRISread(fname) ;
  if (mris_lh_pial == NULL)
    ErrorExit(ERROR_NOFILE, "%s: could not load lh pial surface from %s", Progname,fname) ;

  sprintf(fname, "%s/%s/surf/rh.pial", sdir, subject) ;
  printf("reading surface %s\n", fname) ;
  mris_rh_pial = MRISread(fname) ;
  if (mris_rh_pial == NULL)
    ErrorExit(ERROR_NOFILE, "%s: could not load rh pial surface from %s", Progname,fname) ;

  sprintf(fname, "%s/%s/mri/aseg.mgz", sdir, subject) ;
  printf("reading volume %s\n", fname) ;
  mri_aseg = MRIread(fname) ;
  if (mri_aseg == NULL)
    ErrorExit(ERROR_NOFILE, "%s: could not load aseg volume from %s", Progname,fname) ;

  printf("reading movable volume %s\n", in_fname) ;
  mri_in = MRIread(in_fname) ;
  if (mri_in == NULL)
    ErrorExit(ERROR_NOFILE, "%s: could not load input volume from %s", Progname,in_fname) ;
 
  nvox = (int)ceil(256/resolution); 
  mri_pial = MRIalloc(nvox, nvox, nvox, MRI_UCHAR) ;
  MRIsetResolution(mri_pial, resolution, resolution, resolution) ;

  mri_pial->xstart = -resolution*mri_pial->width/2.0 ;
  mri_pial->xend = resolution*mri_pial->width/2.0 ;
  mri_pial->ystart = -resolution*mri_pial->height/2.0 ;
  mri_pial->yend = resolution*mri_pial->height/2.0 ;
  mri_pial->zstart = -resolution*mri_pial->depth/2.0 ;
  mri_pial->zend = resolution*mri_pial->depth/2 ;
  mri_pial->c_r = mri_aseg->c_r ; mri_pial->c_a = mri_aseg->c_a ; mri_pial->c_s = mri_aseg->c_s ;
  MRIreInitCache(mri_pial) ; 

  printf("filling interior of lh pial surface...\n") ;
  MRISfillInterior(mris_lh_pial, resolution, mri_pial) ;
  mri_seg = MRIclone(mri_pial, NULL) ;
  mri_tmp = MRIclone(mri_pial, NULL) ;
  printf("filling interior of rh pial surface...\n") ;
  MRISfillInterior(mris_rh_pial, resolution, mri_tmp) ;
  MRIcopyLabel(mri_tmp, mri_pial, 1) ;
  MRIclear(mri_tmp) ;
  printf("filling interior of lh white matter surface...\n") ;
  if (cortex_only)
    gm_val = WM_VAL ;
  MRISfillWhiteMatterInterior(mris_lh_white, mri_aseg, mri_seg, resolution,
                              WM_VAL, gm_val, CSF_VAL);
  printf("filling interior of rh white matter surface...\n") ;
  MRISfillWhiteMatterInterior(mris_rh_white, mri_aseg, mri_tmp, resolution,
                              WM_VAL, gm_val, CSF_VAL);
  MRIcopyLabel(mri_tmp, mri_seg, WM_VAL) ;
  MRIcopyLabel(mri_tmp, mri_seg, gm_val) ;
  MRIcopyLabel(mri_tmp, mri_seg, CSF_VAL) ;
  MRIfree(&mri_tmp) ;
  if (cortex_only)
    MRIreplaceValuesOnly(mri_seg, mri_seg, GM_VAL, SUBCORT_GM_VAL) ;
  
  mri_ribbon = MRInot(mri_seg, NULL) ;
  MRIcopyLabel(mri_seg, mri_pial, CSF_VAL) ;
  MRIreplaceValuesOnly(mri_pial, mri_pial, CSF_VAL, 0) ;
  MRIand(mri_ribbon, mri_pial, mri_ribbon, 1) ;
  MRIbinarize(mri_ribbon, mri_ribbon, 1, 0, GM_VAL) ;
  MRIcopyLabel(mri_ribbon, mri_seg, GM_VAL) ;
  MRIreplaceValuesOnly(mri_seg, mri_seg, CSF_VAL, 0) ;
  if (cortex_only)
    add_aseg_structures_outside_ribbon(mri_seg, mri_aseg, mri_seg, WM_VAL, SUBCORT_GM_VAL, CSF_VAL) ;
  else
    add_aseg_structures_outside_ribbon(mri_seg, mri_aseg, mri_seg, WM_VAL, GM_VAL, CSF_VAL) ;


  {
    MATRIX *m_conformed_to_epi_vox2vox, *m_seg_to_conformed_vox2vox,
           *m_seg_to_epi_vox2vox ;

    m_conformed_to_epi_vox2vox = MRIvoxToVoxFromTkRegMtx(mri_in, mri_aseg, m_regdat);
    m_seg_to_conformed_vox2vox = MRIgetVoxelToVoxelXform(mri_seg, mri_aseg) ;

    m_seg_to_epi_vox2vox = MatrixMultiply(m_conformed_to_epi_vox2vox, m_seg_to_conformed_vox2vox, NULL) ;

    printf("seg to EPI vox2vox matrix:\n") ;
    MatrixPrint(Gstdout, m_seg_to_epi_vox2vox) ;
    mri_gm = MRIcloneDifferentType(mri_in, MRI_FLOAT) ;
    mri_wm = MRIcloneDifferentType(mri_in, MRI_FLOAT) ;
    mri_csf = MRIcloneDifferentType(mri_in, MRI_FLOAT) ;
    printf("computing partial volume fractions...\n") ;
    MRIcomputePartialVolumeFractions(mri_in, m_seg_to_epi_vox2vox, mri_seg, mri_wm, mri_gm, mri_csf,
                                     WM_VAL, GM_VAL, 0) ;
    MatrixFree(&m_regdat) ; MatrixFree(&m_conformed_to_epi_vox2vox) ; 
    MatrixFree(&m_seg_to_conformed_vox2vox);
  }
  
  sprintf(fname, "%s.wm.mgz", out_stem) ;
  printf("writing wm %% to %s\n", fname) ;
  MRIwrite(mri_wm, fname) ;

  sprintf(fname, "%s.gm.mgz", out_stem) ;
  printf("writing gm %% to %s\n", fname) ;
  MRIwrite(mri_gm, fname) ;
  
  sprintf(fname, "%s.csf.mgz", out_stem) ;
  printf("writing csf %% to %s\n", fname) ;
  MRIwrite(mri_csf, fname) ;

  msec = TimerStop(&start) ;
  seconds = nint((float)msec/1000.0f) ;
  minutes = seconds / 60 ;
  seconds = seconds % 60 ;
  printf("volume fraction calculation took %d minutes"
          " and %d seconds.\n", minutes, seconds) ;
  exit(0) ;
  return(0) ;
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
  if (!stricmp(option, "SDIR")) {
    strcpy(sdir, argv[2]) ;
    printf("using %s as SUBJECTS_DIR...\n", sdir) ;
    nargs = 1 ;

  } else if (!stricmp(option, "DEBUG_VOXEL")) {
    Gx = atoi(argv[2]) ;
    Gy = atoi(argv[3]) ;
    Gz = atoi(argv[4]) ;
    printf("debugging voxel (%d, %d, %d)\n", Gx, Gy, Gz) ;
    nargs = 3 ;
  } else if (!stricmp(option, "cortex")) {
    printf("limitting gm val to cortex\n") ;
    cortex_only = 1 ;
  } else switch (toupper(*option)) {
  case 'R':
    resolution = atof(argv[2]) ;
    printf("setting resolution = %2.3f\n", resolution) ;
    nargs = 1 ;
    break ;
  case '?':
  case 'U':
    usage_exit(0) ;
    break ;
  default:
    fprintf(stderr, "unknown option %s\n", argv[1]) ;
    exit(1) ;
    break ;
  }

  return(nargs) ;
}
/*----------------------------------------------------------------------
            Parameters:

           Description:
----------------------------------------------------------------------*/
static void
usage_exit(int code) {
  printf("usage: %s [options] <reg file> <input volume> <output stem>\n",
         Progname) ;
  printf("  -SDIR SUBJECTS_DIR \n");
  printf(
         "\t\n");
  exit(code) ;
}





int
MRIcomputePartialVolumeFractions(MRI *mri_src, MATRIX *m_vox2vox, 
                                 MRI *mri_seg, MRI *mri_wm, MRI *mri_gm, MRI *mri_csf,
                                 int wm_val, int gm_val, int csf_val)
{
  int    x, y, z, xs, ys, zs, label ;
  VECTOR *v1, *v2 ;
  MRI    *mri_counts ;
  float  val, count ;
  MATRIX *m_inv ;

  m_inv = MatrixInverse(m_vox2vox, NULL) ;
  if (m_inv == NULL)
  {
    MatrixPrint(stdout, m_vox2vox) ;
    ErrorExit(ERROR_BADPARM, "MRIcomputePartialVolumeFractions: non-invertible vox2vox matrix");
  }
  mri_counts = MRIcloneDifferentType(mri_src, MRI_INT) ;

  v1 = VectorAlloc(4, MATRIX_REAL) ;
  v2 = VectorAlloc(4, MATRIX_REAL) ;
  VECTOR_ELT(v1, 4) = 1.0 ; VECTOR_ELT(v2, 4) = 1.0 ;
  for (x = 0 ; x < mri_seg->width ; x++)
  {
    V3_X(v1) = x ;
    for (y = 0 ; y < mri_seg->height ; y++)
    {
      V3_Y(v1) = y ;
      for (z = 0 ; z < mri_seg->depth ; z++)
      {
        if (x == Gx && y == Gy && z == Gz)
          DiagBreak() ;
        V3_Z(v1) = z ;
        MatrixMultiply(m_vox2vox, v1, v2) ;
        xs = nint(V3_X(v2)) ; ys = nint(V3_Y(v2)) ; zs = nint(V3_Z(v2)) ;
        if (xs >= 0 && ys >= 0 && zs >= 0 &&
            xs < mri_src->width && ys < mri_src->height && zs < mri_src->depth)
        {
          val = MRIgetVoxVal(mri_counts, xs, ys, zs, 0) ;
          MRIsetVoxVal(mri_counts, xs, ys, zs, 0, val+1) ;

          label = MRIgetVoxVal(mri_seg, x, y, z, 0) ;
          if (label == csf_val)
          {
            val = MRIgetVoxVal(mri_csf, xs, ys, zs, 0) ;
            MRIsetVoxVal(mri_csf, xs, ys, zs, 0, val+1) ;
          }
          else if (label == wm_val)
          {
            val = MRIgetVoxVal(mri_wm, xs, ys, zs, 0) ;
            MRIsetVoxVal(mri_wm, xs, ys, zs, 0, val+1) ;
          }
          else if (label == gm_val)
          {
            val = MRIgetVoxVal(mri_gm, xs, ys, zs, 0) ;
            MRIsetVoxVal(mri_gm, xs, ys, zs, 0, val+1) ;
          }
          else
            DiagBreak() ;
        }
      }
    }
  }

  for (x = 0 ; x < mri_src->width ; x++)
    for (y = 0 ; y < mri_src->height ; y++)
      for (z = 0 ; z < mri_src->depth ; z++)
      {
        count = MRIgetVoxVal(mri_counts, x, y, z, 0) ;
        if (count >= 1)
        {
          if (x == Gx && y == Gy && z == Gz)
            DiagBreak() ;
          val = MRIgetVoxVal(mri_wm, x, y, z, 0) ;
          MRIsetVoxVal(mri_wm, x, y, z, 0, val/count) ;
          val = MRIgetVoxVal(mri_gm, x, y, z, 0) ;
          MRIsetVoxVal(mri_gm, x, y, z, 0, val/count) ;
          val = MRIgetVoxVal(mri_csf, x, y, z, 0) ;
          MRIsetVoxVal(mri_csf, x, y, z, 0, val/count) ;
        }
        else  // sample in other direction
        {
          V3_X(v1) = x ; V3_Y(v1) = y ; V3_Z(v1) = z ;
          MatrixMultiply(m_inv, v1, v2) ;
          MatrixMultiply(m_inv, v1, v2) ;
          xs = nint(V3_X(v2)) ; ys = nint(V3_Y(v2)) ; zs = nint(V3_Z(v2)) ;
          if (xs >= 0 && ys >= 0 && zs >= 0 &&
              xs < mri_seg->width && ys < mri_seg->height && zs < mri_seg->depth)
          {
            label = MRIgetVoxVal(mri_seg, xs, ys, zs, 0) ;
            if (label == csf_val)
              MRIsetVoxVal(mri_csf, x, y, z, 0, 1) ;
            else if (label == wm_val)
              MRIsetVoxVal(mri_wm, x, y, z, 0, 1) ;
            else if (label == gm_val)
              MRIsetVoxVal(mri_gm, x, y, z, 0, 1) ;
            else
              DiagBreak() ;
          }
        }
      }
  VectorFree(&v1) ; VectorFree(&v2) ; MatrixFree(&m_inv) ;
  MRIfree(&mri_counts) ;

  return(NO_ERROR) ;
}
MRI *
add_aseg_structures_outside_ribbon(MRI *mri_src, MRI *mri_aseg, MRI *mri_dst,
                                   int wm_val, int gm_val, int csf_val)
{
  VECTOR *v1, *v2 ;
  MATRIX *m_vox2vox ;
  int    x, y, z, xa, ya, za, label ;

  if (mri_dst == NULL)
    mri_dst = MRIcopy(mri_src, NULL) ;
  v1 = VectorAlloc(4, MATRIX_REAL) ;
  v2 = VectorAlloc(4, MATRIX_REAL) ;
  VECTOR_ELT(v1, 4) = 1.0 ; VECTOR_ELT(v2, 4) = 1.0 ;
  m_vox2vox = MRIgetVoxelToVoxelXform(mri_src, mri_aseg) ;


  for (x = 0 ; x < mri_dst->width ; x++)
  {
    V3_X(v1) = x ;
    for (y = 0 ; y < mri_dst->height ; y++)
    {
      V3_Y(v1) = y ;
      for (z = 0 ; z < mri_dst->depth ; z++)
      {
        if (x == Gx && y == Gy && z == Gz)
          DiagBreak() ;
        label = nint(MRIgetVoxVal(mri_dst, x, y, z, 0)) ;
        if (label != 0)  // already labeled, skip it
          continue ;
        V3_Z(v1) = z ;
        MatrixMultiply(m_vox2vox, v1, v2) ;
        xa = (int)(nint(V3_X(v2))) ;
        ya = (int)(nint(V3_Y(v2))) ;
        za = (int)(nint(V3_Z(v2))) ;
        if (xa < 0 || ya < 0 || za < 0 ||
            xa >= mri_aseg->width || ya >= mri_aseg->height || za >= mri_aseg->depth)
          continue ;
        if (xa == Gx && ya == Gy && za == Gz)
          DiagBreak() ;
        label = nint(MRIgetVoxVal(mri_aseg, xa, ya, za, 0)) ;
        switch (label)
        {
        case Left_Cerebellum_White_Matter:
        case Right_Cerebellum_White_Matter:
        case Brain_Stem:
          MRIsetVoxVal(mri_dst, x, y, z, 0, wm_val) ;
          break ;
        case Left_Cerebellum_Cortex:
        case Right_Cerebellum_Cortex:
          MRIsetVoxVal(mri_dst, x, y, z, 0, gm_val) ;
          break ;
        case Left_Pallidum:
        case Right_Pallidum:
        case Left_Thalamus_Proper:
        case Right_Thalamus_Proper:
        case Right_Putamen:
        case Left_Putamen:
        case Left_Accumbens_area:
        case Right_Accumbens_area:  // remove them from cortex
          MRIsetVoxVal(mri_dst, x, y, z, 0, gm_val) ;
          break ;
        default:
          break ;
        }
      }
    }
  }

  VectorFree(&v1) ; VectorFree(&v2) ; MatrixFree(&m_vox2vox) ;
  return(mri_dst) ;
}
