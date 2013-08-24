/**
 * @file  mris_thickness.c
 * @brief program for computing thickness of the cerebral cortex from 
 *  previously generated surfaces
 *
 * See (Fischl and Dale, 2000, PNAS)
 */
/*
 * Original Author: Bruce Fischl
 * CVS Revision Info:
 *    $Author: nicks $
 *    $Date: 2011/03/02 00:04:34 $
 *    $Revision: 1.23 $
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
#include "timer.h"
#include "error.h"
#include "diag.h"
#include "proto.h"
#include "mrisurf.h"
#include "mri.h"
#include "macros.h"
#include "version.h"
#include "icosahedron.h"

static char vcid[] = "$Id: mris_thickness.c,v 1.23 2011/03/02 00:04:34 nicks Exp $";

int main(int argc, char *argv[]) ;

int  MRISmeasureDistanceBetweenSurfaces(MRI_SURFACE *mris, MRI_SURFACE *mris2, int signed_dist) ;
static int  get_option(int argc, char *argv[]) ;
static void usage_exit(void) ;
static void print_usage(void) ;
static void print_help(void) ;
static void print_version(void) ;

char *Progname ;
static char pial_name[100] = "pial" ;
static char white_name[100] = WHITE_MATTER_NAME ;
static int write_vertices = 0 ;

static int nbhd_size = 2 ;
static float max_thick = 5.0 ;
static char *osurf_fname = NULL ;
static char *sphere_name = "sphere" ;
static int signed_dist = 0 ;
static char sdir[STRLEN] = "" ;
static int fmin_thick = 0 ;
static float laplace_res = 0.5 ;
static int laplace_thick = 0 ;
static INTEGRATION_PARMS parms ;

static char *long_fname = NULL ;

#include "voxlist.h"
#include "mrinorm.h"
MRI *
MRISsolveLaplaceEquation(MRI_SURFACE *mris, MRI *mri, double res)
{
  MRI     *mri_white, *mri_pial, *mri_laplace, *mri_control, *mri_tmp = NULL ;
  int     x, y, z, ncontrol, nribbon, v, i, xm1, xp1, ym1, yp1, zm1, zp1 ; 
  VOXLIST *vl ;
  float   wval, pval, max_change, change, val, oval ;

  MRISrestoreVertexPositions(mris, PIAL_VERTICES) ;
  mri_pial = MRISfillInterior(mris, res, NULL) ;
  mri_white = MRIclone(mri_pial, NULL) ;
  MRISrestoreVertexPositions(mris, WHITE_VERTICES) ;
  MRISfillInterior(mris, res, mri_white) ;

  {
    char fname[STRLEN] ;
    sprintf(fname, "pi.%2.2f.mgz", res) ;
    MRIwrite(mri_pial, fname) ;
    sprintf(fname, "wi.%2.2f.mgz", res) ;
    MRIwrite(mri_white, fname) ;
  }
  mri_laplace = MRIcloneDifferentType(mri_white,MRI_FLOAT) ;
  mri_control = MRIcloneDifferentType(mri_white,MRI_UCHAR) ;
  ncontrol = nribbon = 0 ;
  for (x = 0 ; x < mri_white->width ; x++)
    for (y = 0 ; y < mri_white->height ; y++)
      for (z = 0 ; z < mri_white->depth ; z++)
      {
        wval = MRIgetVoxVal(mri_white, x, y, z, 0) ;
        pval = MRIgetVoxVal(mri_pial, x, y, z, 0) ;
        if (wval)
        {
          MRIsetVoxVal(mri_control, x, y, z, 0, CONTROL_MARKED) ;
          MRIsetVoxVal(mri_laplace, x, y, z, 0, 0.0) ;
          ncontrol++ ;
        }
        else if (FZERO(pval)) // outside pial surface
        {
          MRIsetVoxVal(mri_control, x, y, z, 0, CONTROL_MARKED) ;
          MRIsetVoxVal(mri_laplace, x, y, z, 0, 1.0) ;
          ncontrol++ ;
        }
        else 
          nribbon++ ;
      }

  vl = VLSTalloc(nribbon) ;
  vl->mri = mri_laplace ;
  nribbon = 0 ;
  for (x = 0 ; x < mri_white->width ; x++)
    for (y = 0 ; y < mri_white->height ; y++)
      for (z = 0 ; z < mri_white->depth ; z++)
      {
        wval = MRIgetVoxVal(mri_white, x, y, z, 0) ;
        pval = MRIgetVoxVal(mri_pial, x, y, z, 0) ;
        if (FZERO(MRIgetVoxVal(mri_control, x, y, z, 0)))
        {
          vl->xi[nribbon] = x ;
          vl->yi[nribbon] = y ;
          vl->zi[nribbon] = z ;

          nribbon++ ;
        }
      }

  i = 0 ;
  do
  {
    max_change = 0.0 ;
    mri_tmp = MRIcopy(mri_laplace, mri_tmp) ;
    for (v = 0 ; v < vl->nvox  ; v++)
    {
      x = vl->xi[v] ; y = vl->yi[v] ; z = vl->zi[v] ;
      xm1 = mri_laplace->xi[x-1] ; xp1 = mri_laplace->xi[x+1] ;
      ym1 = mri_laplace->yi[y-1] ; yp1 = mri_laplace->yi[y+1] ;
      zm1 = mri_laplace->zi[z-1] ; zp1 = mri_laplace->zi[z+1] ;
      oval = MRIgetVoxVal(mri_laplace, x, y, z, 0) ;
      val = 
        (MRIgetVoxVal(mri_laplace, xm1, y, z, 0) +
         MRIgetVoxVal(mri_laplace, xp1, y, z, 0) +
         MRIgetVoxVal(mri_laplace, x, ym1, z, 0) +
         MRIgetVoxVal(mri_laplace, x, yp1, z, 0) +
         MRIgetVoxVal(mri_laplace, x, y, zm1, 0) +
         MRIgetVoxVal(mri_laplace, x, y, zp1, 0)) *
        1.0/6.0;
      change = fabs(val-oval) ;
      if (change > max_change)
        max_change = change ;
      MRIsetVoxVal(mri_tmp, x, y, z, 0, val);
    }
    MRIcopy(mri_tmp, mri_laplace) ;
    i++ ;
    if (i%10 == 0)
      printf("iter %d complete, max change %f\n", i, max_change) ;
  } while (max_change > 1e-3) ;

  if (Gdiag & DIAG_WRITE && DIAG_VERBOSE_ON)
  {
    MRIwrite(mri_white, "w.mgz") ;
    MRIwrite(mri_pial, "p.mgz") ;
  }
  {
    char fname[STRLEN] ;
    sprintf(fname, "laplace.%2.2f.mgz", mri_laplace->xsize) ;
    MRIwrite(mri_laplace, fname) ;
  }
  MRIfree(&mri_white) ; MRIfree(&mri_pial) ; VLSTfree(&vl) ;
  return(mri_laplace) ;
}

int
MRISmeasureLaplaceStreamlines(MRI_SURFACE *mris, MRI *mri_laplace)
{
  int    vno, f, x, y, z, npoints, nmissing ;
  VERTEX *v ;
  double dx, dy, dz, xv, yv, zv ;
  MRI    *mri_grad, *mri_mag ;
  double  val, norm, dt = mri_laplace->xsize/10, dist ;

  mri_mag = MRIclone(mri_laplace, NULL) ;
  mri_grad = MRIsobel(mri_laplace, NULL, mri_mag) ;

  MRISrestoreVertexPositions(mris, PIAL_VERTICES) ;

  // normalize the gradient to be unit vectors
  for (z = 0 ; z < mri_grad->depth ; z++)   
    for (y = 0 ; y < mri_grad->height ; y++)
      for (x = 0 ; x < mri_grad->width ; x++)
      {
        norm = MRIgetVoxVal(mri_mag, x, y, z, 0) ;
        if (FZERO(norm))
          continue ;
        for (f = 0 ; f < mri_grad->nframes ; f++)
        {
          val = MRIgetVoxVal(mri_grad, x, y, z, f) ;
          MRIsetVoxVal(mri_grad, x, y, z, f, val/norm) ;
        }
      }

  for (nmissing = vno = 0 ; vno < mris->nvertices ; vno++)
  {
    v = &mris->vertices[vno] ;
    if (vno == Gdiag_no)
      DiagBreak() ;

    if (v->ripflag)
      continue ;
    // check to see if this location doesn't have the resolution to represent the pial surface
    MRISsurfaceRASToVoxel(mris, mri_laplace, v->pialx, v->pialy, v->pialz, &xv, &yv, &zv);
    MRIsampleVolumeFrame(mri_laplace, xv, yv, zv, 0, &val) ;
    if (val < 0.5)
      nmissing++ ;

    MRISsurfaceRASToVoxel(mris, mri_laplace, v->whitex, v->whitey, v->whitez, &xv, &yv, &zv);
    dist = 0.0 ;
    npoints = 0 ;
    do
    {
      MRIsampleVolumeFrame(mri_grad, xv, yv, zv, 0, &dx) ;
      MRIsampleVolumeFrame(mri_grad, xv, yv, zv, 1, &dy) ;
      MRIsampleVolumeFrame(mri_grad, xv, yv, zv, 2, &dz) ;
      norm = sqrt(dx*dx + dy*dy + dz*dz) ;
      npoints++ ;
      if (FZERO(norm) || dist > 10)
      {
        if (val < .9)
          DiagBreak() ;
        if (dist > 10)
          DiagBreak() ;
        break ;
      }
      dx /= norm ; dy /= norm ; dz /= norm ;
      xv += dx*dt ; yv += dy*dt ; zv += dz*dt ;
      dist += dt*mri_laplace->xsize ;
      MRIsampleVolumeFrame(mri_laplace, xv, yv, zv, 0, &val) ;
      if (vno == Gdiag_no)
        printf("v %d:   (%2.2f %2.2f %2.2f): dist=%2.2f, val=%2.2f\n",
               vno, xv, yv, zv, dist, val) ;
    } while (val < 1) ;
    if (vno == Gdiag_no)
    {
      LABEL *area ;
      int   i ;
      char  fname[STRLEN] ;
      double xs, ys, zs ;

      area = LabelAlloc(npoints, NULL, NULL) ;
      MRISsurfaceRASToVoxel(mris, mri_laplace, v->whitex, v->whitey, v->whitez, &xv, &yv, &zv);
      dist = 0 ; i = 0 ;
      do
      {
        MRIsampleVolumeFrame(mri_laplace, xv, yv, zv, 0, &val) ;
        MRISsurfaceRASFromVoxel(mris, mri_laplace, xv, yv, zv, &xs, &ys, &zs);
        area->lv[i].x = xs ; area->lv[i].y = ys ;  area->lv[i].z = zs ;
        area->lv[i].stat = val ;
        area->lv[i].vno = vno ;
        area->n_points++ ;
        MRIsampleVolumeFrame(mri_grad, xv, yv, zv, 0, &dx) ;
        MRIsampleVolumeFrame(mri_grad, xv, yv, zv, 1, &dy) ;
        MRIsampleVolumeFrame(mri_grad, xv, yv, zv, 2, &dz) ;
        norm = sqrt(dx*dx + dy*dy + dz*dz) ;
        if (FZERO(norm) || dist > 10)
        {
          if (val < .9)
            DiagBreak() ;
          if (dist > 10)
            DiagBreak() ;
          break ;
        }
        dx /= norm ; dy /= norm ; dz /= norm ;
        xv += dx*dt ; yv += dy*dt ; zv += dz*dt ;
        MRIsampleVolumeFrame(mri_laplace, xv, yv, zv, 0, &val) ;
        dist += dt ;
        i++ ;
      } while (val < 1) ;
      sprintf(fname, "vno%d.label", vno) ;
      LabelWrite(area, fname) ;
      LabelFree(&area) ;
    }

    v->curv = dist ;
  }

  //  printf("%d of %d pial surface nodes not resolved - %2.3f %%\n",
  //         nmissing, mris->nvertices, 100.0*nmissing/mris->nvertices) ;
  MRIfree(&mri_mag) ; MRIfree(&mri_grad) ;
  return(NO_ERROR) ;
}
int
main(int argc, char *argv[]) {
  char          **av, *out_fname, *sname, *cp, fname[STRLEN], *hemi ;
  int           ac, nargs, msec ;
  MRI_SURFACE   *mris ;
  struct timeb  then ;

  /* rkt: check for and handle version tag */
  nargs = handle_version_option (argc, argv, "$Id: mris_thickness.c,v 1.23 2011/03/02 00:04:34 nicks Exp $", "$Name: release_5_3_0 $");
  if (nargs && argc - nargs == 1)
    exit (0);
  argc -= nargs;

  TimerStart(&then) ;
  Progname = argv[0] ;
  ErrorInit(NULL, NULL, NULL) ;
  DiagInit(NULL, NULL, NULL) ;

  // for variational thickness estimation
  parms.dt = 0.1 ;
  parms.remove_neg = 0 ;
  parms.momentum = .5; parms.niterations = 1000 ;
  parms.l_nlarea = 0 ;
  parms.l_thick_min = 1 ;
  parms.l_thick_spring = 0 ;
  parms.l_ashburner_triangle = 1 ;
  parms.l_ashburner_lambda = .1 ;
  parms.l_tspring = .25;
  parms.l_thick_normal = 1;
  parms.integration_type = INTEGRATE_LM_SEARCH ;
  parms.tol = 1e-2 ;

  ac = argc ;
  av = argv ;
  for ( ; argc > 1 && ISOPTION(*argv[1]) ; argc--, argv++) {
    nargs = get_option(argc, argv) ;
    argc -= nargs ;
    argv += nargs ;
  }

  if (argc < 4)
    usage_exit() ;

  sname = argv[1] ;
  hemi = argv[2] ;
  out_fname = argv[3] ;
  if (!strlen(sdir)) {
    cp = getenv("SUBJECTS_DIR") ;
    if (!cp)
      ErrorExit(ERROR_BADPARM,
                "%s: SUBJECTS_DIR not defined in environment.\n", Progname) ;
    strcpy(sdir, cp) ;
  }


#if 0
  sprintf(fname, "%s/%s/surf/%s.%s", sdir, sname, hemi, GRAY_MATTER_NAME) ;
#else
  sprintf(fname, "%s/%s/surf/%s.%s", sdir, sname, hemi, pial_name) ;
#endif
  if (!FileExists(fname))
    sprintf(fname, "%s/%s/surf/%s.gray", sdir, sname, hemi) ;

  fprintf(stderr, "reading gray matter surface %s...\n", fname) ;
  mris = MRISread(fname) ;
  if (!mris)
    ErrorExit(ERROR_NOFILE, "%s: could not read surface file %s",
              Progname, fname) ;

  MRISresetNeighborhoodSize(mris, nbhd_size) ;
  if (osurf_fname) {
    MRI_SURFACE *mris2 ;
    mris2 = MRISread(osurf_fname) ;
    if (mris2 == NULL)
      ErrorExit(ERROR_NOFILE, "%s: could not read 2nd surface from %s", Progname, osurf_fname) ;
    MRISmeasureDistanceBetweenSurfaces(mris, mris2, signed_dist) ;
    fprintf(stderr, "writing surface distance to curvature file %s...\n", out_fname) ;
    MRISwriteCurvature(mris, out_fname) ;
    exit(0) ;
  }

  if (MRISreadOriginalProperties(mris, white_name) != NO_ERROR)
    ErrorExit(Gerror, "%s: could not read white matter surface", Progname) ;
  fprintf(stderr, "measuring gray matter thickness...\n") ;

  if (laplace_thick)
  {
    MRI *mri_laplace ;

    {
      MRI *mri_dist_white, *mri_dist_pial ;
      int nwmissing, npmissing, nmissing, vno ;
      double xv, yv, zv, white_val, pial_val ;
      VERTEX *v ;
      FILE   *fp ;

      MRISsaveVertexPositions(mris, PIAL_VERTICES) ;
      mri_dist_pial = MRIScomputeDistanceToSurface(mris, NULL, laplace_res) ;

      MRISrestoreVertexPositions(mris, ORIGINAL_VERTICES) ;  // actually white
      MRISsaveVertexPositions(mris, WHITE_VERTICES) ;
      mri_dist_white = MRIScomputeDistanceToSurface(mris, NULL, laplace_res) ;
      for (nmissing = nwmissing = npmissing = vno = 0 ; vno < mris->nvertices ; vno++)
      {
        v = &mris->vertices[vno] ;
        MRISsurfaceRASToVoxel(mris, mri_dist_pial, v->pialx, v->pialy, v->pialz, &xv, &yv, &zv);
        MRIsampleVolumeFrameType(mri_dist_pial, xv, yv, zv, 0, SAMPLE_NEAREST, &pial_val) ;
        MRISsurfaceRASToVoxel(mris, mri_dist_white, v->whitex, v->whitey, v->whitez, &xv, &yv, &zv);
        MRIsampleVolumeFrameType(mri_dist_white, xv, yv, zv, 0, SAMPLE_NEAREST, &white_val) ;
        if (fabs(white_val) > laplace_res)
          nwmissing++ ;
        if (fabs(pial_val) > laplace_res)
          npmissing++ ;
        if ((fabs(pial_val) > laplace_res) || (fabs(white_val) > laplace_res))
          nmissing++ ;
      }
      printf("%d of %d pial surface nodes not resolved - %2.3f %%\n",
             npmissing, mris->nvertices, 100.0*npmissing/mris->nvertices) ;
      printf("%d of %d gray/white surface nodes not resolved - %2.3f %%\n",
             nwmissing, mris->nvertices, 100.0*nwmissing/mris->nvertices) ;
      MRIfree(&mri_dist_white) ; MRIfree(&mri_dist_pial) ;
      MRISrestoreVertexPositions(mris, PIAL_VERTICES) ;
      fp = fopen("laplace_missing.txt", "a") ;
      fprintf(fp, "%f %d %d %f %d %f %d %f\n", 
              laplace_res,  mris->nvertices,
              nwmissing, 100.0*nwmissing/mris->nvertices,
              npmissing, 100.0*npmissing/mris->nvertices,
              nmissing, 100.0*nmissing/mris->nvertices) ;
      fclose(fp) ;
    }
    MRISsaveVertexPositions(mris, PIAL_VERTICES) ;
    MRISrestoreVertexPositions(mris, ORIGINAL_VERTICES) ;
    MRISsaveVertexPositions(mris, WHITE_VERTICES) ;
    mri_laplace = MRISsolveLaplaceEquation(mris, NULL, laplace_res) ;
    MRISmeasureLaplaceStreamlines(mris, mri_laplace) ;
  }
  else if (fmin_thick)
  {
    char              *cp, surf_fname[STRLEN], fname[STRLEN] ; ;
    
    if (parms.base_name[0] == 0) {
      
      FileNameOnly(out_fname, fname) ;
      cp = strchr(fname, '.') ;
      if (cp)
        strcpy(parms.base_name, cp+1) ;
      else
        strcpy(parms.base_name, "sphere") ;
      cp = strrchr(parms.base_name, '.') ;
      if (cp)
        *cp = 0 ;
    }

    MRISsaveVertexPositions(mris, PIAL_VERTICES) ;
    if (MRISreadVertexPositions(mris, sphere_name) != NO_ERROR)
      ErrorExit(ERROR_NOFILE, "%s: could not read surface file %s", Progname, sphere_name) ;
    MRISsaveVertexPositions(mris, CANONICAL_VERTICES) ;
    MRISrestoreVertexPositions(mris, ORIGINAL_VERTICES) ;
    MRISsaveVertexPositions(mris, WHITE_VERTICES) ;
    MRISrestoreVertexPositions(mris, CANONICAL_VERTICES) ;
    MRIScomputeMetricProperties(mris) ;

    // read in icosahedron data (highly tessellated one)
    cp = getenv("FREESURFER_HOME");
    if (cp == NULL)
      ErrorExit(ERROR_BADPARM, "%s: FREESURFER_HOME not defined in environment", cp) ;
    sprintf(surf_fname,"%s/lib/bem/ic7.tri",cp);

    if (Gdiag & DIAG_WRITE && DIAG_VERBOSE_ON)
    {
      char tmp[STRLEN] ;
      FileNameRemoveExtension(out_fname, tmp) ;
      
      sprintf(fname, "%s.correspondence.init", tmp) ;
      printf("writing initial correspondences to %s\n", fname) ;
      MRISrestoreVertexPositions(mris, PIAL_VERTICES) ;
      MRIScomputeMetricProperties(mris) ;
      MRISwrite(mris, fname) ;

      MRISrestoreVertexPositions(mris, CANONICAL_VERTICES) ;
      MRIScomputeMetricProperties(mris) ;
    }
    if (Gdiag & DIAG_WRITE)
    {
      char tmp[STRLEN] ;
      int  vno ;
      VERTEX *v ;
      FileNameRemoveExtension(out_fname, tmp) ;
      

      MRISrestoreVertexPositions(mris, CANONICAL_VERTICES) ;
      MRISsaveVertexPositions(mris, TMP_VERTICES) ;
      MRISrestoreVertexPositions(mris, PIAL_VERTICES) ;
      MRISsaveVertexPositions(mris, CANONICAL_VERTICES) ;
      MRISfindClosestPialVerticesCanonicalCoords(mris, mris->nsize) ;
      for (vno = 0 ; vno < mris->nvertices ; vno++)
      {
        v = &mris->vertices[vno] ;
        if (v->ripflag)
        {
          v->nx = v->ny = v->nz = 0 ;
          continue ;
        }
        v->nx = v->x - v->whitex ; 
        v->ny = v->y - v->whitey ; 
        v->nz = v->z - v->whitez ; 
      }
      sprintf(fname, "%s.normals.init.mgz", tmp) ;
      printf("writing initial surface normals to %s\n", fname) ;
      MRISwriteNormals(mris, fname) ;
      MRISrestoreVertexPositions(mris, TMP_VERTICES) ;
      MRISsaveVertexPositions(mris, CANONICAL_VERTICES) ;  // restore spherical canonical coords
    }
    
    //    MRISripZeroThicknessRegions(mris) ;
    MRISstoreRipFlags(mris) ;
#if 0
    {
      static INTEGRATION_PARMS nparms ;

      nparms.dt = 0.9 ;
      nparms.momentum = .5; 
      nparms.niterations = 1000 ;
      nparms.l_nlarea = 0 ;
      nparms.l_thick_min = 0 ;
      nparms.l_thick_parallel = 0;
      nparms.l_thick_normal = 1;
      nparms.l_tspring = 0;
      nparms.tol = 4e-1 ;
      MRISminimizeThicknessFunctional(mris, &nparms, max_thick) ;
      printf("------- normal vector field computed - minimizing full functional -----------\n") ;
      parms.start_t = nparms.start_t ;
    }
#endif
    MRISminimizeThicknessFunctional(mris, &parms, max_thick) ;

    if (Gdiag & DIAG_WRITE)
    {
      char tmp[STRLEN] ;
      int  vno ;
      VERTEX *v ;
      FileNameRemoveExtension(out_fname, tmp) ;
      
      sprintf(fname, "%s.normals.mgz", tmp) ;
      printf("writing final surface normals to %s\n", fname) ;
      MRISsaveVertexPositions(mris, TMP2_VERTICES) ;
      MRISrestoreVertexPositions(mris, TMP_VERTICES) ;
      for (vno = 0 ; vno < mris->nvertices ; vno++)
      {
        v = &mris->vertices[vno] ;
        if (v->ripflag)
        {
          v->nx = v->ny = v->nz = 0 ;
          continue ;
        }
        v->nx = v->tx - v->whitex ; 
        v->ny = v->ty - v->whitey ; 
        v->nz = v->tz - v->whitez ; 
      }
      MRISwriteNormals(mris, fname) ;
      MRISrestoreVertexPositions(mris, TMP2_VERTICES) ;
    }
    if (long_fname)
    {
      char line[STRLEN], subject[STRLEN], fname[STRLEN], base_name[STRLEN], *cp,
        tmp[STRLEN], out_fname_only[STRLEN] ;
      int  ntimepoints, vno ;
      FILE *fp ;
      VERTEX *v ;
      MHT   *mht ;

      MRIScopyCurvatureToImagValues(mris) ; // save base thickness 
      // to lookup closest face
      mht = MHTfillTableAtResolution(mris, NULL, CANONICAL_VERTICES, 1.0); 

      fp = fopen(long_fname, "r") ;
      if (fp == NULL)
        ErrorExit(ERROR_NOFILE, "%s: could not open timepoint file %s\n", Progname,long_fname) ;
      strcpy(tmp, long_fname) ;
      cp = strrchr(tmp, '/') ;
      if (cp == NULL)
        ErrorExit(ERROR_BADPARM, "could not read trailing / from %s", tmp) ;
      *cp = 0 ;
      cp = strrchr(tmp, '/') ;
      if (cp == NULL)
        cp = tmp-1 ;
      strcpy(base_name, cp+1) ;
      ntimepoints = 0 ;
      do
      {
        if (fgetl(line, STRLEN-1, fp) == NULL)
          break ;
        sscanf(line, "%s", subject) ;
        printf("processing longitudinal subject %s\n", subject) ;
        sprintf(fname, "%s/%s.long.%s/surf/%s.%s", sdir, subject, base_name, hemi, 
                white_name) ;
        if (MRISreadWhiteCoordinates(mris, fname) != NO_ERROR)
          ErrorExit(ERROR_NOFILE, "%s: could not read surface file %s", Progname, fname) ;
        sprintf(fname, "%s/%s.long.%s/surf/%s.%s", sdir, subject, base_name, hemi, 
                pial_name) ;
        if (MRISreadPialCoordinates(mris, fname) != NO_ERROR)
          ErrorExit(ERROR_NOFILE, "%s: could not read surface file %s", Progname, fname) ;
        for (vno = 0 ; vno < mris->nvertices ; vno++)
        {
          float   xw, yw, zw, xp, yp, zp, thick ;
          
          v = &mris->vertices[vno] ;
          if (vno == Gdiag_no)
            DiagBreak() ;
          if (v->ripflag)
          {
            v->tx = v->whitex ; v->ty = v->whitey ; v->tz = v->whitez ;
            continue ;
          }
          MRISvertexCoord2XYZ_float(v, WHITE_VERTICES, &xw, &yw, &zw) ;
          MRISsampleFaceCoordsCanonical(mht, mris, v->x, v->y, v->z, 
                                        PIAL_VERTICES, &xp, &yp, &zp);
          thick = sqrt(SQR(xp-xw) + SQR(yp-yw) + SQR(zp-zw)) ;
          v->curv = thick ; v->tx = xp ; v->ty = yp ; v->tz = zp ;
        }
        FileNameOnly(out_fname, out_fname_only) ;
        sprintf(fname, "%s/%s.long.%s/surf/%s", sdir, subject, base_name, out_fname_only);
        printf("writing thickness estimate to %s\n", fname) ;
        MRISwriteCurvature(mris, fname) ;
      } while (strlen(line) > 0) ;
      MHTfree(&mht) ;
      MRIScopyImagValuesToCurvature(mris) ; // restore base  thickness 
    }
  }
  else if (write_vertices) {
    MRISfindClosestOrigVertices(mris, nbhd_size) ;
  } else {
    MRISmeasureCorticalThickness(mris, nbhd_size, max_thick) ;
  }

#if 0
  sprintf(fname, "%s/%s/surf/%s", sdir, sname, out_fname) ;
  fprintf(stderr, "writing output surface to %s...\n", fname) ;
#endif
  fprintf(stderr, "writing %s to curvature file %s...\n",
          write_vertices ? "vertex correspondence" :
          "thickness", out_fname) ;
  MRISwriteCurvature(mris, out_fname) ;
  msec = TimerStop(&then) ;
  fprintf(stderr,"thickness measurement took %2.1f minutes\n", (float)msec/(60*1000.0f));
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
    print_usage() ;
  else if (!stricmp(option, "-version"))
    print_version() ;
  else if (!stricmp(option, "long"))
  {
    long_fname = argv[2] ;
    nargs = 1 ;
    printf("computing longitudinal thickness from time points found in %s\n",
           long_fname) ;
  }
  else if (!stricmp(option, "pial")) {
    strcpy(pial_name, argv[2]) ;
    fprintf(stderr,  "reading pial surface from file named %s\n", pial_name) ;
    nargs = 1 ;
  } else if (!stricmp(option, "optimal")) {
    parms.integration_type = INTEGRATE_LM_SEARCH ;
    fprintf(stderr,  "using line search minimization\n") ;
  } else if (!stricmp(option, "momentum")) {
    parms.integration_type = INTEGRATE_MOMENTUM ;
    fprintf(stderr,  "using gradient descent with momentum minimization\n") ;
  } else if (!stricmp(option, "ic")) {
    parms.ico_order = atoi(argv[2]) ;
    fprintf(stderr,  "using %dth order icosahedron\n", parms.ico_order) ;
    nargs = 1 ;
  } else if (!stricmp(option, "SDIR")) {
    strcpy(sdir, argv[2]) ;
    printf("using %s as SUBJECTS_DIR...\n", sdir) ;
    nargs = 1 ;
  } else if (!stricmp(option, "THICK_PARALLEL")) {
    parms.l_thick_parallel = atof(argv[2]) ;
    printf("using parallel thickness coefficient %2.3f\n", parms.l_thick_parallel);
    nargs = 1 ;
  } else if (!stricmp(option, "THICK_MIN")) {
    parms.l_thick_min = atof(argv[2]) ;
    printf("using min length thickness coefficient %2.3f\n", parms.l_thick_min);
    nargs = 1 ;
  } else if (!stricmp(option, "spring")) {
    parms.l_tspring = atof(argv[2]) ;
    printf("using spring coefficient %2.3f\n", parms.l_tspring);
    nargs = 1 ;
  } else if (!stricmp(option, "neg")) {
    parms.remove_neg = 0 ;
    printf("allowing negative  vertices to occur during integration\n") ;
  } else if (!stricmp(option, "nlarea")) {
    parms.l_nlarea = atof(argv[2]) ;
    printf("using nonlinear area coefficient %2.3f\n", parms.l_nlarea);
    nargs = 1 ;
  } else if (!stricmp(option, "triangle")) {
    parms.l_ashburner_triangle = atof(argv[2]) ;
    parms.l_ashburner_lambda = atof(argv[3]) ;
    printf("using Ashburner, 1999, triangle regularization with l=%2.2f and lambda=%2.1f\n", 
           parms.l_ashburner_triangle, parms.l_ashburner_lambda);
    nargs = 2 ;
  } else if (!stricmp(option, "THICK_NORMAL")) {
    parms.l_thick_normal = atof(argv[2]) ;
    printf("using normal thickness coefficient %2.3f\n", parms.l_thick_normal);
    nargs = 1 ;
  } else if (!stricmp(option, "DT")) {
    parms.dt = atof(argv[2]) ;
    printf("using time step %2.3f\n", parms.dt);
    nargs = 1 ;
  } else if (!stricmp(option, "tol")) {
    parms.tol = atof(argv[2]) ;
    printf("using tol %e\n", parms.tol);
    nargs = 1 ;
  } else if (!stricmp(option, "white")) {
    strcpy(white_name, argv[2]) ;
    fprintf(stderr,  "reading white matter surface from file named %s\n", white_name) ;
    nargs = 1 ;
  } else if (!stricmp(option, "max")) {
    max_thick = atof(argv[2]) ;
    fprintf(stderr,  "limiting maximum cortical thickness to %2.2f mm.\n",
            max_thick) ;
    nargs = 1 ;
  } else if (!stricmp(option, "osurf")) {
    osurf_fname = argv[2] ;
    signed_dist = 0 ;
    fprintf(stderr,  "measuring distance between input surface and %s\n", osurf_fname) ;
    nargs = 1 ;
  } else if (!stricmp(option, "new") || !stricmp(option, "fmin") || !stricmp(option, "variational")) {
    fmin_thick = 1 ;
    fprintf(stderr,  "using variational thickness measurement\n") ;
  } else if (!stricmp(option, "laplace") || !stricmp(option, "laplacian")) {
    laplace_thick = 1 ;
    laplace_res = atof(argv[2]) ;
    fprintf(stderr,  "using Laplacian thickness measurement with PDE resolution = %2.3fmm\n",laplace_res) ;
    nargs = 1 ;
  } else if (!stricmp(option, "nsurf")) {
    osurf_fname = argv[2] ;
    signed_dist = 1 ;
    fprintf(stderr,  "measuring signed distance between input surface and %s\n", osurf_fname) ;
    nargs = 1 ;
  } else if (!stricmp(option, "vno")) {
    Gdiag_no = atoi(argv[2]) ;
    nargs = 1 ;
    fprintf(stderr,  "debugging vertex %d\n", Gdiag_no) ;
  } else switch (toupper(*option)) {
  case 'W':
    parms.write_iterations = atoi(argv[2]) ;
    nargs = 1 ;
    Gdiag |= DIAG_WRITE ;
    printf("setting write iterations to %d\n", parms.write_iterations) ;
    break ;
    case 'V':
      write_vertices = 1 ;
      printf("writing vertex correspondences instead of thickness\n") ;
      nargs =  0 ;
      break ;
    case 'N':
      nbhd_size = atoi(argv[2]) ;
      fprintf(stderr, "using neighborhood size=%d\n", nbhd_size) ;
      nargs = 1 ;
      break ;
    case 'M':
      parms.momentum = atof(argv[2]) ;
      fprintf(stderr, "using momentum %2.3f\n", parms.momentum) ;
      nargs = 1 ;
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
          "usage: %s [options] <subject name> <hemi> <thickness file>\n",
          Progname) ;
  print_help() ;
}

static void
print_help(void) {
  fprintf(stderr,
          "\nThis program measures the thickness of the cortical surface and\n"
          "writes the resulting scalar field into a 'curvature' file "
          "<thickness file>.\n") ;
  fprintf(stderr, "\nvalid options are:\n\n") ;
  fprintf(stderr, "-max <max>\t use <max> to threshold thickness (default=5mm)\n") ;
  exit(1) ;
}

static void
print_version(void) {
  fprintf(stderr, "%s\n", vcid) ;
  exit(1) ;
}

