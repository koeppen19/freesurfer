/*----------------------------------------------------------
  Name: mri_surf2surf.c
  $Id: mri_surf2surf.c,v 1.21.2.1 2005/03/03 23:52:43 greve Exp $
  Author: Douglas Greve
  Purpose: Resamples data from one surface onto another. If
  both the source and target subjects are the same, this is
  just a format conversion. The source or target subject may
  be ico.  Can handle data with multiple frames.
  -----------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "mri.h"
#include "icosahedron.h"
#include "fio.h"

#include "MRIio_old.h"
#include "error.h"
#include "diag.h"
#include "mrisurf.h"
#include "mri2.h"
#include "mri_identify.h"

#include "bfileio.h"
#include "registerio.h"
//  extern char *ResampleVtxMapFile;
#include "resample.h"
#include "selxavgio.h"
#include "prime.h"
#include "version.h"


double MRISareaTriangle(double x0, double y0, double z0, 
			double x1, double y1, double z1, 
			double x2, double y2, double z2);
int MRIStriangleAngles(double x0, double y0, double z0, 
		       double x1, double y1, double z1, 
		       double x2, double y2, double z2,
		       double *a0, double *a1, double *a2);
MRI *MRISdiffusionWeights(MRIS *surf);
MRI *MRISdiffusionSmooth(MRIS *Surf, MRI *Src, double GStd, MRI *Targ);
int MRISareNeighbors(MRIS *surf, int vtxno1, int vtxno2);

double MRISdiffusionEdgeWeight(MRIS *surf, int vtxno0, int vtxnonbr);
double MRISsumVertexFaceArea(MRIS *surf, int vtxno);
int MRIScommonNeighbors(MRIS *surf, int vtxno1, int vtxno2, 
			int *cvtxno1, int *cvtxno2);
int MRISdumpVertexNeighborhood(MRIS *surf, int vtxno);

static int  parse_commandline(int argc, char **argv);
static void check_options(void);
static void print_usage(void) ;
static void usage_exit(void);
static void print_help(void) ;
static void print_version(void) ;
static void argnerr(char *option, int n);
static void dump_options(FILE *fp);
static int  singledash(char *flag);
int GetNVtxsFromWFile(char *wfile);
int GetICOOrderFromValFile(char *filename, char *fmt);
int GetNVtxsFromValFile(char *filename, char *fmt);
int dump_surf(char *fname, MRIS *surf, MRI *mri);

int main(int argc, char *argv[]) ;

static char vcid[] = "$Id: mri_surf2surf.c,v 1.21.2.1 2005/03/03 23:52:43 greve Exp $";
char *Progname = NULL;

char *surfreg = NULL;
char *hemi    = NULL;

char *srcsubject = NULL;
char *srcvalfile = NULL;
char *srctypestring = NULL;
int   srctype = MRI_VOLUME_TYPE_UNKNOWN;
MRI  *SrcVals, *SrcHits, *SrcDist;
MRI_SURFACE *SrcSurfReg;
char *SrcHitFile = NULL;
char *SrcDistFile = NULL;
int nSrcVtxs = 0;
int SrcIcoOrder = -1;

char *trgsubject = NULL;
char *trgvalfile = NULL;
char *trgtypestring = NULL;
int   trgtype = MRI_VOLUME_TYPE_UNKNOWN;
MRI  *TrgVals, *TrgValsSmth, *TrgHits, *TrgDist;
MRI_SURFACE *TrgSurfReg;
char *TrgHitFile = NULL;
char *TrgDistFile = NULL;
int TrgIcoOrder;

MRI  *mritmp;
int  reshape = 1;
int  reshapefactor;

char *mapmethod = "nnfr";

int UseHash = 1;
int framesave = 0;
float IcoRadius = 100.0;
int nthstep, nnbrs, nthnbr, nbrvtx, frame;
int nSmoothSteps = 0;
double fwhm=0, gstd;

double fwhm_Input=0, gstd_Input=0;
int nSmoothSteps_Input = 0;
int usediff = 0;

int debug = 0;
char *SrcDumpFile  = NULL;
char *TrgDumpFile = NULL;

char *SUBJECTS_DIR = NULL;
char *FREESURFER_HOME = NULL;
SXADAT *sxa;
FILE *fp;

char tmpstr[2000];

int ReverseMapFlag = 0;
int cavtx = 0; /* command-line vertex -- for debugging */

MRI *sphdist;

/*---------------------------------------------------------------------------*/
int main(int argc, char **argv)
{
  int f,tvtx,svtx,n;
  float *framepower = NULL;
  char fname[2000];
  int nTrg121,nSrc121,nSrcLost;
  int nTrgMulti,nSrcMulti;
  float MnTrgMultiHits,MnSrcMultiHits;
  int nargs;
  FACE *face;
  VERTEX *vtx0,*vtx1,*vtx2;
  double area, a0, a1, a2;

  /* rkt: check for and handle version tag */
  nargs = handle_version_option (argc, argv, "$Id: mri_surf2surf.c,v 1.21.2.1 2005/03/03 23:52:43 greve Exp $", "$Name:  $");
  if (nargs && argc - nargs == 1)
    exit (0);
  argc -= nargs;

  Progname = argv[0] ;
  argc --;
  argv++;
  ErrorInit(NULL, NULL, NULL) ;
  DiagInit(NULL, NULL, NULL) ;

  if(argc == 0) usage_exit();

  parse_commandline(argc, argv);
  check_options();
  dump_options(stdout);

  SUBJECTS_DIR = getenv("SUBJECTS_DIR");
  if(SUBJECTS_DIR==NULL){
    fprintf(stderr,"ERROR: SUBJECTS_DIR not defined in environment\n");
    exit(1);
  }
  FREESURFER_HOME = getenv("FREESURFER_HOME") ;
  if(FREESURFER_HOME==NULL){
    fprintf(stderr,"ERROR: FREESURFER_HOME not defined in environment\n");
    exit(1);
  }

  /* --------- Load the registration surface for source subject --------- */
  if(!strcmp(srcsubject,"ico")){ /* source is ico */
    if(SrcIcoOrder == -1)
      SrcIcoOrder = GetICOOrderFromValFile(srcvalfile,srctypestring);
    sprintf(fname,"%s/lib/bem/ic%d.tri",FREESURFER_HOME,SrcIcoOrder);
    SrcSurfReg = ReadIcoByOrder(SrcIcoOrder, IcoRadius);
    printf("Source Ico Order = %d\n",SrcIcoOrder);
  }
  else{
    sprintf(fname,"%s/%s/surf/%s.%s",SUBJECTS_DIR,srcsubject,hemi,surfreg);
    printf("Reading source surface reg %s\n",fname);
    SrcSurfReg = MRISread(fname) ;
    if(cavtx > 0) 
      printf("cavtx = %d, srcsurfreg: %g, %g, %g\n",cavtx,
	   SrcSurfReg->vertices[cavtx].x,
	   SrcSurfReg->vertices[cavtx].y,
	   SrcSurfReg->vertices[cavtx].z);
  }
  if (!SrcSurfReg)
    ErrorExit(ERROR_NOFILE, "%s: could not read surface %s", Progname, fname) ;

  MRIScomputeMetricProperties(SrcSurfReg);
  for(n = 0; n < SrcSurfReg->nfaces && 0; n++){
    face = &SrcSurfReg->faces[n];
    vtx0 = &SrcSurfReg->vertices[face->v[0]];
    vtx1 = &SrcSurfReg->vertices[face->v[1]];
    vtx2 = &SrcSurfReg->vertices[face->v[2]];
    area = MRISareaTriangle(vtx0->x,vtx0->y,vtx0->z,
			    vtx1->x,vtx1->y,vtx1->z,
			    vtx2->x,vtx2->y,vtx2->z);
    MRIStriangleAngles(vtx0->x,vtx0->y,vtx0->z,
		       vtx1->x,vtx1->y,vtx1->z,
		       vtx2->x,vtx2->y,vtx2->z,&a0,&a1,&a2);
    printf("n=%d, area = %f, %f %f %f   %f\n",n,face->area,a0,a1,a2,a0+a1+a2);
  }

  /* ------------------ load the source data ----------------------------*/
  printf("Loading source data\n");
  if(!strcmp(srctypestring,"curv")){ /* curvature file */
    if(fio_FileExistsReadable(srcvalfile)){
      memset(fname,0,strlen(fname));
      memcpy(fname,srcvalfile,strlen(srcvalfile));
    }
    else
      sprintf(fname,"%s/%s/surf/%s.%s",SUBJECTS_DIR,srcsubject,hemi,srcvalfile);
    printf("Reading curvature file %s\n",fname);
    if(MRISreadCurvatureFile(SrcSurfReg, fname) != 0){
      printf("ERROR: reading curvature file\n");
      exit(1);
    }
    SrcVals = MRIcopyMRIS(NULL, SrcSurfReg, 0, "curv");
  }
  else if(!strcmp(srctypestring,"paint") || !strcmp(srctypestring,"w")){
    MRISreadValues(SrcSurfReg,srcvalfile);
    SrcVals = MRIcopyMRIS(NULL, SrcSurfReg, 0, "val");
  }
  else { /* Use MRIreadType */
    SrcVals =  MRIreadType(srcvalfile,srctype);
    if(SrcVals == NULL){
      printf("ERROR: could not read %s as type %d\n",srcvalfile,srctype);
      exit(1);
    }
    if(SrcVals->height != 1 || SrcVals->depth != 1){
      reshapefactor = SrcVals->height * SrcVals->depth;
      printf("Reshaping %d\n",reshapefactor);
      mritmp = mri_reshape(SrcVals, reshapefactor*SrcVals->width, 
         1, 1, SrcVals->nframes);
      MRIfree(&SrcVals);
      SrcVals = mritmp;
      reshapefactor = 0; /* reset for output */
    }

    if(SrcVals->width != SrcSurfReg->nvertices){
      fprintf(stderr,"ERROR: dimesion inconsitency in source data\n");
      fprintf(stderr,"       Number of surface vertices = %d\n",
        SrcSurfReg->nvertices);
      fprintf(stderr,"       Number of value vertices = %d\n",SrcVals->width);
      exit(1);
    }
    if(is_sxa_volume(srcvalfile)){
      printf("INFO: Source volume detected as selxavg format\n");
      sxa = ld_sxadat_from_stem(srcvalfile);
      if(sxa == NULL) exit(1);
      framepower = sxa_framepower(sxa,&f);
      if(f != SrcVals->nframes){
	fprintf(stderr," number of frames is incorrect (%d,%d)\n",
		f,SrcVals->nframes);
	exit(1);
      }
      printf("INFO: Adjusting Frame Power\n");  fflush(stdout);
      mri_framepower(SrcVals,framepower);
    }
  }
  if(SrcVals == NULL){
    fprintf(stderr,"ERROR loading source values from %s\n",srcvalfile);
    exit(1);
  }

  if(SrcDumpFile != NULL){
    printf("Dumping input to %s\n",SrcDumpFile);
    dump_surf(SrcDumpFile,SrcSurfReg,SrcVals);
  }

  if(SrcDumpFile != NULL){
    printf("Dumping input to %s\n",SrcDumpFile);
    dump_surf(SrcDumpFile,SrcSurfReg,SrcVals);
  }

  /* Smooth input if desired */
  if(nSmoothSteps_Input > 0){
    printf("NN smoothing input with n = %d\n",nSmoothSteps_Input);
    MRISsmoothMRI(SrcSurfReg, SrcVals, nSmoothSteps_Input, SrcVals);
  }
  if(fwhm_Input > 0){
    printf("Gaussian smoothing input with fwhm = %g, std = %g\n",
	   fwhm_Input,gstd_Input);
    if(!usediff) MRISgaussianSmooth(SrcSurfReg, SrcVals, gstd_Input, SrcVals, 3.0);
    if(usediff){
      printf("Computing distance along the sphere \n");
      sphdist = MRISdistSphere(SrcSurfReg, gstd_Input*3);
      printf("Computing weights\n");
      MRISgaussianWeights(SrcSurfReg, sphdist, gstd_Input);
      MRIwrite(sphdist,"weights.mgh");
      printf("Applying weights\n");
      MRISspatialFilter(SrcVals, sphdist, SrcVals);      
      printf("Done filtering input\n");
    }
    //if(usediff)  MRISdiffusionSmooth(SrcSurfReg, SrcVals, gstd_Input, SrcVals);
  }

  if(strcmp(srcsubject,trgsubject)){
    /* ------- Source and Target Subjects are different -------------- */
    /* ------- Load the registration surface for target subject ------- */
    if(!strcmp(trgsubject,"ico")){
      sprintf(fname,"%s/lib/bem/ic%d.tri",FREESURFER_HOME,TrgIcoOrder);
      TrgSurfReg = ReadIcoByOrder(TrgIcoOrder, IcoRadius);
      reshapefactor = 6;
    }
    else{
      sprintf(fname,"%s/%s/surf/%s.%s",SUBJECTS_DIR,trgsubject,hemi,surfreg);
      printf("Reading target surface reg %s\n",fname);
      TrgSurfReg = MRISread(fname) ;
    }
    if (!TrgSurfReg)
      ErrorExit(ERROR_NOFILE, "%s: could not read surface %s", 
    Progname, fname) ;
    printf("Done\n");
    
    if(!strcmp(mapmethod,"nnfr")) ReverseMapFlag = 1;
    else                          ReverseMapFlag = 0;
    
    /*-------------------------------------------------------------*/
    /* Map the values from the surface to surface */
    printf("Mapping Source Volume onto Source Subject Surface\n");
    TrgVals = surf2surf_nnfr(SrcVals, SrcSurfReg,TrgSurfReg,
           &SrcHits,&SrcDist,&TrgHits,&TrgDist,
           ReverseMapFlag,UseHash);
    
    
    /* Compute some stats on the mapping number of srcvtx mapping to a 
       target vtx*/
    nTrg121 = 0;
    MnTrgMultiHits = 0.0;
    for(tvtx = 0; tvtx < TrgSurfReg->nvertices; tvtx++){
      n = MRIFseq_vox(TrgHits,tvtx,0,0,0);
      if(n == 1) nTrg121++;
      else MnTrgMultiHits += n;
    }
    nTrgMulti = TrgSurfReg->nvertices - nTrg121;
    if(nTrgMulti > 0) MnTrgMultiHits = (MnTrgMultiHits/nTrgMulti);
    else              MnTrgMultiHits = 0;
    printf("nTrg121 = %5d, nTrgMulti = %5d, MnTrgMultiHits = %g\n",
     nTrg121,nTrgMulti,MnTrgMultiHits);
    
    /* Compute some stats on the mapping number of trgvtxs mapped from a 
       source vtx*/
    nSrc121 = 0;
    nSrcLost = 0;
    MnSrcMultiHits = 0.0;
    for(svtx = 0; svtx < SrcSurfReg->nvertices; svtx++){
      n = MRIFseq_vox(SrcHits,svtx,0,0,0);
      if(n == 1)      nSrc121++;
      else if(n == 0) nSrcLost++;
      else MnSrcMultiHits += n;
    }
    nSrcMulti = SrcSurfReg->nvertices - nSrc121;
    if(nSrcMulti > 0) MnSrcMultiHits = (MnSrcMultiHits/nSrcMulti);
    else              MnSrcMultiHits = 0;
    
    printf("nSrc121 = %5d, nSrcLost = %5d, nSrcMulti = %5d, "
     "MnSrcMultiHits = %g\n", nSrc121,nSrcLost,nSrcMulti,
     MnSrcMultiHits);
    
    /* save the Source Hits into a .w file */
    if(SrcHitFile != NULL){
      printf("INFO: saving source hits to %s\n",SrcHitFile);
      MRIScopyMRI(SrcSurfReg, SrcHits, 0, "val");
      //for(vtx = 0; vtx < SrcSurfReg->nvertices; vtx++)
      //SrcSurfReg->vertices[vtx].val = MRIFseq_vox(SrcHits,vtx,0,0,0) ;
      MRISwriteValues(SrcSurfReg, SrcHitFile) ;
    }
    /* save the Source Distance into a .w file */
    if(SrcDistFile != NULL){
      printf("INFO: saving source distance to %s\n",SrcDistFile);
      MRIScopyMRI(SrcSurfReg, SrcDist, 0, "val");
      MRISwriteValues(SrcSurfReg, SrcDistFile) ;
      //for(vtx = 0; vtx < SrcSurfReg->nvertices; vtx++)
      //SrcSurfReg->vertices[vtx].val = MRIFseq_vox(SrcDist,vtx,0,0,0) ;
    }
    /* save the Target Hits into a .w file */
    if(TrgHitFile != NULL){
      printf("INFO: saving target hits to %s\n",TrgHitFile);
      MRIScopyMRI(TrgSurfReg, TrgHits, 0, "val");
      MRISwriteValues(TrgSurfReg, TrgHitFile) ;
      //for(vtx = 0; vtx < TrgSurfReg->nvertices; vtx++)
      //TrgSurfReg->vertices[vtx].val = MRIFseq_vox(TrgHits,vtx,0,0,0) ;
    }
    /* save the Target Hits into a .w file */
    if(TrgDistFile != NULL){
      printf("INFO: saving target distance to %s\n",TrgDistFile);
      MRIScopyMRI(TrgSurfReg, TrgDist, 0, "val");
      MRISwriteValues(TrgSurfReg, TrgDistFile) ;
      //for(vtx = 0; vtx < TrgSurfReg->nvertices; vtx++)
      //TrgSurfReg->vertices[vtx].val = MRIFseq_vox(TrgDist,vtx,0,0,0) ;
    }
  }
  else{
    /* --- Source and Target Subjects are the same --- */
    printf("INFO: trgsubject = srcsubject\n");
    TrgSurfReg = SrcSurfReg;
    TrgVals = SrcVals;
  }
       
  /* Smooth output if desired */
  if(nSmoothSteps > 0)
    MRISsmoothMRI(TrgSurfReg, TrgVals, nSmoothSteps, TrgVals);
  if(fwhm > 0){
    printf("Gaussian smoothing with fwhm = %g, std = %g\n",fwhm,gstd);
    MRISgaussianSmooth(TrgSurfReg, TrgVals, gstd, TrgVals, 3.0);
  }

  /* readjust frame power if necessary */
  if(is_sxa_volume(srcvalfile)){
    printf("INFO: Readjusting Frame Power\n");  fflush(stdout);
    for(f=0; f < TrgVals->nframes; f++) framepower[f] = 1.0/framepower[f];
    mri_framepower(TrgVals,framepower);
    sxa->nrows = 1;
    sxa->ncols = TrgVals->width;
  }

  if(TrgDumpFile != NULL){
    /* Dump before reshaping */
    printf("Dumping output to %s\n",TrgDumpFile);
    dump_surf(TrgDumpFile,TrgSurfReg,TrgVals);
  }

  /* ------------ save the target data -----------------------------*/
  printf("Saving target data\n");
  if(!strcmp(trgtypestring,"paint") || !strcmp(trgtypestring,"w")){
    MRIScopyMRI(TrgSurfReg, TrgVals, framesave, "val");
    MRISwriteValues(TrgSurfReg,trgvalfile);
  }
  else {
    if(reshape){
      if(reshapefactor == 0) 
	reshapefactor = GetClosestPrimeFactor(TrgVals->width,6);
      
      printf("Reshaping %d (nvertices = %d)\n",reshapefactor,TrgVals->width);
      mritmp = mri_reshape(TrgVals, TrgVals->width / reshapefactor, 
			   1, reshapefactor,TrgVals->nframes);
      if(mritmp == NULL){
	printf("ERROR: mri_reshape could not alloc\n");
	return(1);
      }
      MRIfree(&TrgVals);
      TrgVals = mritmp;
    }
    MRIwriteType(TrgVals,trgvalfile,trgtype);
    if(is_sxa_volume(srcvalfile)) sv_sxadat_by_stem(sxa,trgvalfile);
  }

  return(0);
}
/* --------------------------------------------- */
static int parse_commandline(int argc, char **argv)
{
  int  nargc , nargsused;
  char **pargv, *option ;

  if(argc < 1) usage_exit();

  nargc   = argc;
  pargv = argv;
  while(nargc > 0){

    option = pargv[0];
    if(debug) printf("%d %s\n",nargc,option);
    nargc -= 1;
    pargv += 1;

    nargsused = 0;

    if (!strcasecmp(option, "--help"))  print_help() ;

    else if (!strcasecmp(option, "--version")) print_version() ;

    else if (!strcasecmp(option, "--debug"))   debug = 1;
    else if (!strcasecmp(option, "--usehash")) UseHash = 1;
    else if (!strcasecmp(option, "--hash")) UseHash = 1;
    else if (!strcasecmp(option, "--dontusehash")) UseHash = 0;
    else if (!strcasecmp(option, "--nohash")) UseHash = 0;
    else if (!strcasecmp(option, "--noreshape")) reshape = 0;
    else if (!strcasecmp(option, "--reshape"))   reshape = 1;
    else if (!strcasecmp(option, "--usediff"))   usediff = 1;
    else if (!strcasecmp(option, "--nousediff")) usediff = 0;

    /* -------- source value inputs ------ */
    else if (!strcmp(option, "--srcsubject")){
      if(nargc < 1) argnerr(option,1);
      srcsubject = pargv[0];
      nargsused = 1;
    }
    else if (!strcmp(option, "--srcsurfval")){
      if(nargc < 1) argnerr(option,1);
      srcvalfile = pargv[0];
      nargsused = 1;
    }
    else if (!strcmp(option, "--srcdump")){
      if(nargc < 1) argnerr(option,1);
      SrcDumpFile = pargv[0];
      nargsused = 1;
    }
    else if (!strcmp(option, "--srcfmt") ||
       !strcmp(option, "--src_type")){
      if(nargc < 1) argnerr(option,1);
      srctypestring = pargv[0];
      srctype = string_to_type(srctypestring);
      nargsused = 1;
    }
    else if (!strcmp(option, "--srcicoorder")){
      if(nargc < 1) argnerr(option,1);
      sscanf(pargv[0],"%d",&SrcIcoOrder);
      nargsused = 1;
    }
    else if (!strcmp(option, "--nsmooth-in")){
      if(nargc < 1) argnerr(option,1);
      sscanf(pargv[0],"%d",&nSmoothSteps_Input);
      if(nSmoothSteps_Input < 1){
	fprintf(stderr,"ERROR: number of smooth steps (%d) must be >= 1\n",
		nSmoothSteps_Input);
      }
      nargsused = 1;
    }
    else if (!strcmp(option, "--nsmooth-out") ||
	     !strcmp(option, "--nsmooth")){
      if(nargc < 1) argnerr(option,1);
      sscanf(pargv[0],"%d",&nSmoothSteps);
      if(nSmoothSteps < 1){
	fprintf(stderr,"ERROR: number of smooth steps (%d) must be >= 1\n",
		nSmoothSteps);
      }
      nargsused = 1;
    }
    else if (!strcmp(option, "--fwhm-in")){
      if(nargc < 1) argnerr(option,1);
      sscanf(pargv[0],"%lf",&fwhm_Input);
      gstd_Input = fwhm_Input/sqrt(log(256.0));
      nargsused = 1;
    }
    else if (!strcmp(option, "--fwhm") ||
	     !strcmp(option, "--fwhm-out")){
      if(nargc < 1) argnerr(option,1);
      sscanf(pargv[0],"%lf",&fwhm);
      gstd = fwhm/sqrt(log(256.0));
      nargsused = 1;
    }

    /* -------- target value inputs ------ */
    else if (!strcmp(option, "--trgsubject")){
      if(nargc < 1) argnerr(option,1);
      trgsubject = pargv[0];
      nargsused = 1;
    }
    else if (!strcmp(option, "--trgicoorder")){
      if(nargc < 1) argnerr(option,1);
      sscanf(pargv[0],"%d",&TrgIcoOrder);
      nargsused = 1;
    }
    else if (!strcmp(option, "--trgsurfval")){
      if(nargc < 1) argnerr(option,1);
      trgvalfile = pargv[0];
      nargsused = 1;
    }
    else if (!strcmp(option, "--trgdump")){
      if(nargc < 1) argnerr(option,1);
      TrgDumpFile = pargv[0];
      nargsused = 1;
    }
    else if (!strcmp(option, "--trgfmt") ||
       !strcmp(option, "--trg_type")){
      if(nargc < 1) argnerr(option,1);
      trgtypestring = pargv[0];
      if(!strcmp(trgtypestring,"curv")){
  fprintf(stderr,"ERROR: Cannot select curv as target format\n");
  exit(1);
      }
      trgtype = string_to_type(trgtypestring);
      nargsused = 1;
    }

    else if (!strcmp(option, "--frame")){
      if(nargc < 1) argnerr(option,1);
      sscanf(pargv[0],"%d",&framesave);
      nargsused = 1;
    }
    else if (!strcmp(option, "--cavtx")){
      /* command-line vertex -- for debugging */
      if(nargc < 1) argnerr(option,1);
      sscanf(pargv[0],"%d",&cavtx);
      nargsused = 1;
    }
    else if (!strcmp(option, "--hemi")){
      if(nargc < 1) argnerr(option,1);
      hemi = pargv[0];
      nargsused = 1;
    }
    else if (!strcmp(option, "--surfreg")){
      if(nargc < 1) argnerr(option,1);
      surfreg = pargv[0];
      nargsused = 1;
    }
    else if (!strcmp(option, "--mapmethod")){
      if(nargc < 1) argnerr(option,1);
      mapmethod = pargv[0];
      if(strcmp(mapmethod,"nnfr") && strcmp(mapmethod,"nnf")){
	fprintf(stderr,"ERROR: mapmethod must be nnfr or nnf\n");
	exit(1);
      }
      nargsused = 1;
    }
    else if (!strcmp(option, "--srchits")){
      if(nargc < 1) argnerr(option,1);
      SrcHitFile = pargv[0];
      nargsused = 1;
    }
    else if (!strcmp(option, "--srcdist")){
      if(nargc < 1) argnerr(option,1);
      SrcDistFile = pargv[0];
      nargsused = 1;
    }
    else if (!strcmp(option, "--trghits")){
      if(nargc < 1) argnerr(option,1);
      TrgHitFile = pargv[0];
      nargsused = 1;
    }
    else if (!strcmp(option, "--trgdist")){
      if(nargc < 1) argnerr(option,1);
      TrgDistFile = pargv[0];
      nargsused = 1;
    }
    else if (!strcmp(option, "--vtxmap")){
      if(nargc < 1) argnerr(option,1);
      ResampleVtxMapFile = pargv[0];
      nargsused = 1;
    }
    else{
      fprintf(stderr,"ERROR: Option %s unknown\n",option);
      if(singledash(option))
  fprintf(stderr,"       Did you really mean -%s ?\n",option);
      exit(-1);
    }
    nargc -= nargsused;
    pargv += nargsused;
  }
  return(0);
}
/* ------------------------------------------------------ */
static void usage_exit(void)
{
  print_usage() ;
  exit(1) ;
}
/* --------------------------------------------- */
static void print_usage(void)
{
  fprintf(stdout, "USAGE: %s \n",Progname) ;
  fprintf(stdout, "\n");
  fprintf(stdout, "   --srcsubject source subject\n");
  fprintf(stdout, "   --srcsurfval path of file with input values \n");
  fprintf(stdout, "   --src_type   source format\n");
  fprintf(stdout, "   --srcicoorder when srcsubject=ico and src is .w\n");
  fprintf(stdout, "   --trgsubject target subject\n");
  fprintf(stdout, "   --trgicoorder when trgsubject=ico\n");
  fprintf(stdout, "   --trgsurfval path of file in which to store output values\n");
  fprintf(stdout, "   --trg_type   target format\n");
  fprintf(stdout, "   --hemi       hemisphere (lh or rh) \n");
  fprintf(stdout, "   --surfreg    surface registration (sphere.reg)  \n");
  fprintf(stdout, "   --mapmethod  nnfr or nnf\n");
  fprintf(stdout, "   --frame      save only nth frame (with --trg_type paint)\n");
  fprintf(stdout, "   --nsmooth-in N  : smooth the input\n");  
  fprintf(stdout, "   --nsmooth-out N : smooth the output\n");  
  fprintf(stdout, "   --noreshape  do not reshape output to multiple 'slices'\n");  

  fprintf(stdout, "\n");
  printf("%s\n", vcid) ;
  printf("\n");

}
/* --------------------------------------------- */
static void print_help(void)
{
  print_usage() ;
  printf(

"This program will resample one surface onto another. The source and \n"
"target subjects can be any subject in $SUBJECTS_DIR and/or the  \n"
"icosahedron (ico). The source and target file formats can be anything \n"
"supported by mri_convert. The source format can also be a curvature \n"
"file or a paint (.w) file. The user also has the option of smoothing \n"
"on the surface. \n"
"\n"
"OPTIONS\n"
"\n"
"  --srcsubject subjectname\n"
"\n"
"    Name of source subject as found in $SUBJECTS_DIR or ico for icosahedron.\n"
"    The input data must have been sampled onto this subject's surface (eg, \n"
"    using mri_vol2surf)\n"
"\n"
"  --srcsurfval sourcefile\n"
"\n"
"    Name of file where the data on the source surface is located.\n"
"\n"
"  --src_type typestring\n"
"\n"
"    Format type string. Can be either curv (for FreeSurfer curvature file), \n"
"    paint or w (for FreeSurfer paint files), or anything accepted by \n"
"    mri_convert. If no type string  is given, then the type is determined \n"
"    from the sourcefile (if possible). If curv is used, then the curvature\n"
"    file will be looked for in $SUBJECTS_DIR/srcsubject/surf/hemi.sourcefile.\n"
"\n"
"  --srcicoorder order\n"
"\n"
"    Icosahedron order of the source. Normally, this can be detected based\n"
"    on the number of verticies, but this will fail with a .w file as input.\n"
"    This is only needed when the source is a .w file.\n"
"\n"
"  --trgsubject subjectname\n"
"\n"
"    Name of target subject as found in $SUBJECTS_DIR or ico for icosahedron.\n"
"\n"
"  --trgicoorder order\n"
"\n"
"    Icosahedron order number. This specifies the size of the\n"
"    icosahedron according to the following table: \n"
"              Order  Number of Vertices\n"
"                0              12 \n"
"                1              42 \n"
"                2             162 \n"
"                3             642 \n"
"                4            2562 \n"
"                5           10242 \n"
"                6           40962 \n"
"                7          163842 \n"
"    In general, it is best to use the largest size available.\n"
"\n"
"  --trgsurfval targetfile\n"
"\n"
"    Name of file where the data on the target surface will be stored.\n"
"    BUG ALERT: for trg_type w or paint, use the full path.\n"
"\n"
"  --trg_type typestring\n"
"\n"
"    Format type string. Can be paint or w (for FreeSurfer paint files) or anything\n"
"    accepted by mri_convert. NOTE: output cannot be stored in curv format\n"
"    If no type string  is given, then the type is determined from the sourcefile\n"
"    (if possible). If using paint or w, see also --frame.\n"
"\n"
"  --hemi hemifield (lh or rh)\n"
"\n"
"  --surfreg registration_surface"
"\n"
"    If the source and target subjects are not the same, this surface is used \n"
"    to register the two surfaces. sphere.reg is used as the default. Don't change\n"
"    this unless you know what you are doing.\n"
"\n"
"  --mapmethod methodname\n"
"\n"
"    Method used to map from the vertices in one subject to those of another.\n"
"    Legal values are: nnfr (neighest-neighbor, forward and reverse) and nnf\n"
"    (neighest-neighbor, forward only). Default is nnfr. The mapping is done\n"
"    in the following way. For each vertex on the target surface, the closest\n"
"    vertex in the source surface is found, based on the distance in the \n"
"    registration space (this is the forward map). If nnf is chosen, then the\n"
"    the value at the target vertex is set to that of the closest source vertex.\n"
"    This, however, can leave some source vertices unrepresented in target (ie,\n"
"    'holes'). If nnfr is chosen, then each hole is assigned to the closest\n"
"    target vertex. If a target vertex has multiple source vertices, then the\n"
"    source values are averaged together. It does not seem to make much difference.\n"
"\n"
"  --nsmooth-in  niterations\n"
"  --nsmooth-out niterations  [note: same as --smooth]\n"
"\n"
"    Number of smoothing iterations. Each iteration consists of averaging each\n"
"    vertex with its neighbors. When only smoothing is desired, just set the \n"
"    the source and target subjects to the same subject. --smooth-in smooths\n"
"    the input surface values prior to any resampling. --smooth-out smooths\n"
"    after any resampling. \n"
"\n"
"  --frame framenumber\n"
"\n"
"    When using paint/w output format, this specifies which frame to output. This\n"
"    format can store only one frame. The frame number is zero-based (default is 0).\n"
"\n"
"  --noreshape"
"\n"
"    By default, mri_surf2surf will save the output as multiple\n"
"    'slices'; has no effect for paint/w output format. For ico, the output\n"
"    will appear to be a 'volume' with Nv/R colums, 1 row, R slices and Nf \n"
"    frames, where Nv is the number of vertices on the surface. For icosahedrons, \n"
"    R=6. For others, R will be the prime factor of Nv closest to 6. Reshaping \n"
"    is for logistical purposes (eg, in the analyze format the size of a dimension \n"
"    cannot exceed 2^15). Use this flag to prevent this behavior. This has no \n"
"    effect when the output type is paint.\n"
"\n"
"EXAMPLES:\n"
"\n"
"1. Resample a subject's thickness of the left cortical hemisphere on to a \n"
"   7th order icosahedron and save in analyze4d format:\n"
"\n"
"   mri_surf2surf --hemi lh --srcsubject bert \n"
"      --srcsurfval thickness --src_type curv \n"
"      --trgsubject ico --trgicoorder 7 \n"
"      --trgsurfval bert-thickness-lh.img --trg_type analyze4d \n"
"\n"
"2. Resample data on the icosahedron to the right hemisphere of subject bert.\n"
"   Save in paint so that it can be viewed as an overlay in tksurfer. The \n"
"   source data is stored in bfloat format (ie, icodata_000.bfloat, ...)\n"
"\n"
"   mri_surf2surf --hemi rh --srcsubject ico \n"
"      --srcsurfval icodata-rh --src_type bfloat \n"
"      --trgsubject bert \n"
"      --trgsurfval ./bert-ico-rh.w --trg_type paint \n"
"\n"
"BUG REPORTS: send bugs to analysis-bugs@nmr.mgh.harvard.edu. Make sure \n"
"    to include the version and full command-line and enough information to\n"
"    be able to recreate the problem. Not that anyone does.\n"
"\n"
"\n"
"BUGS:\n"
"\n"
"  When the output format is paint, the output file must be specified with\n"
"  a partial path (eg, ./data-lh.w) or else the output will be written into\n"
"  the subject's anatomical directory.\n"
"\n"
"\n"
"AUTHOR: Douglas N. Greve, Ph.D., MGH-NMR Center (greve@nmr.mgh.harvard.edu)\n"
"\n");

  exit(1) ;
}

/* --------------------------------------------- */
static void dump_options(FILE *fp)
{
  fprintf(fp,"srcsubject = %s\n",srcsubject);
  fprintf(fp,"srcval     = %s\n",srcvalfile);
  fprintf(fp,"srctype    = %s\n",srctypestring);
  fprintf(fp,"trgsubject = %s\n",trgsubject);
  fprintf(fp,"trgval     = %s\n",trgvalfile);
  fprintf(fp,"trgtype    = %s\n",trgtypestring);
  fprintf(fp,"surfreg    = %s\n",surfreg);
  fprintf(fp,"hemi       = %s\n",hemi);
  fprintf(fp,"frame      = %d\n",framesave);
  fprintf(fp,"fwhm-in    = %g\n",fwhm_Input);
  fprintf(fp,"fwhm-out   = %g\n",fwhm);
  return;
}
/* --------------------------------------------- */
static void print_version(void)
{
  fprintf(stdout, "%s\n", vcid) ;
  exit(1) ;
}
/* --------------------------------------------- */
static void argnerr(char *option, int n)
{
  if(n==1)
    fprintf(stdout,"ERROR: %s flag needs %d argument\n",option,n);
  else
    fprintf(stdout,"ERROR: %s flag needs %d arguments\n",option,n);
  exit(-1);
}
/* --------------------------------------------- */
static void check_options(void)
{
  if(srcsubject == NULL){
    fprintf(stdout,"ERROR: no source subject specified\n");
    exit(1);
  }
  if(srcvalfile == NULL){
    fprintf(stdout,"A source value path must be supplied\n");
    exit(1);
  }

  if(srctypestring == NULL){
    srctypestring = "bfloat";
    srctype = BFLOAT_FILE;
  }
  if( strcasecmp(srctypestring,"w") != 0 &&
      strcasecmp(srctypestring,"curv") != 0 &&
      strcasecmp(srctypestring,"paint") != 0 ){
    if(srctype == MRI_VOLUME_TYPE_UNKNOWN) {
  srctype = mri_identify(srcvalfile);
  if(srctype == MRI_VOLUME_TYPE_UNKNOWN){
    fprintf(stdout,"ERROR: could not determine type of %s\n",srcvalfile);
    exit(1);
  }
    }
  }

  if(trgsubject == NULL){
    fprintf(stdout,"ERROR: no target subject specified\n");
    exit(1);
  }
  if(trgvalfile == NULL){
    fprintf(stdout,"A target value path must be supplied\n");
    exit(1);
  }

  if(trgtypestring == NULL){
    trgtypestring = "bfloat";
    trgtype = BFLOAT_FILE;
  }
  if( strcasecmp(trgtypestring,"w") != 0 &&
      strcasecmp(trgtypestring,"curv") != 0 &&
      strcasecmp(trgtypestring,"paint") != 0 ){
    if(trgtype == MRI_VOLUME_TYPE_UNKNOWN) {
      trgtype = mri_identify(trgvalfile);
      if(trgtype == MRI_VOLUME_TYPE_UNKNOWN){
	fprintf(stdout,"ERROR: could not determine type of %s\n",trgvalfile);
	exit(1);
      }
    }
  }

  if(surfreg == NULL) surfreg = "sphere.reg";
  else printf("Registration surface changed to %s\n",surfreg);

  if(hemi == NULL){
    fprintf(stdout,"ERROR: no hemifield specified\n");
    exit(1);
  }

  if(fwhm != 0 && nSmoothSteps != 0){
    printf("ERROR: cannot specify --fwhm-out and --nsmooth-out\n");
    exit(1);
  }
  if(fwhm_Input != 0 && nSmoothSteps_Input != 0){
    printf("ERROR: cannot specify --fwhm-in and --nsmooth-in\n");
    exit(1);
  }
  return;
}

/*---------------------------------------------------------------*/
static int singledash(char *flag)
{
  int len;
  len = strlen(flag);
  if(len < 2) return(0);

  if(flag[0] == '-' && flag[1] != '-') return(1);
  return(0);
}

/*---------------------------------------------------------------*/
int GetNVtxsFromWFile(char *wfile)
{
  FILE *fp;
  int i,ilat, num, nvertices;
  int *vtxnum;
  float *wval;

  fp = fopen(wfile,"r");
  if (fp==NULL) {
    fprintf(stdout,"ERROR: Progname: GetNVtxsFromWFile():\n");
    fprintf(stdout,"Could not open %s\n",wfile);
    fprintf(stdout,"(%s,%d,%s)\n",__FILE__, __LINE__,__DATE__);
    exit(1);
  }
  
  fread2(&ilat,fp);
  fread3(&num,fp);
  vtxnum = (int *)   calloc(sizeof(int),   num);
  wval   = (float *) calloc(sizeof(float), num);

  for (i=0;i<num;i++){
    fread3(&vtxnum[i],fp);
    wval[i] = freadFloat(fp) ;
  }
  fclose(fp);

  nvertices = vtxnum[num-1] + 1;

  free(vtxnum);
  free(wval);

  return(nvertices);
}
//MRI *MRIreadHeader(char *fname, int type);
/*---------------------------------------------------------------*/
int GetNVtxsFromValFile(char *filename, char *typestring)
{
  //int err,nrows, ncols, nslcs, nfrms, endian;
  int nVtxs=0;
  int type;
  MRI *mri;

  printf("GetNVtxs: %s %s\n",filename,typestring);

  if(!strcmp(typestring,"curv")){
    fprintf(stdout,"ERROR: cannot get nvertices from curv format\n");
    exit(1);
  }

  if(!strcmp(typestring,"paint") || !strcmp(typestring,"w")){
    nVtxs = GetNVtxsFromWFile(filename);
    return(nVtxs);
  }

  type = string_to_type(typestring);
  mri = MRIreadHeader(filename, type);
  if(mri == NULL) exit(1);
  
  nVtxs = mri->width*mri->height*mri->depth;

  MRIfree(&mri);

  return(nVtxs);
}
/*---------------------------------------------------------------*/
int GetICOOrderFromValFile(char *filename, char *fmt)
{
  int nIcoVtxs,IcoOrder;

  nIcoVtxs = GetNVtxsFromValFile(filename, fmt);

  IcoOrder = IcoOrderFromNVtxs(nIcoVtxs);
  if(IcoOrder < 0){
    fprintf(stdout,"ERROR: number of vertices = %d, does not mach ico\n",
      nIcoVtxs);
    exit(1);

  }
  
  return(IcoOrder);
}
/*---------------------------------------------------------------*/
int dump_surf(char *fname, MRIS *surf, MRI *mri)
{
  FILE *fp;
  float val;
  int vtxno,nnbrs;
  VERTEX *vtx;

  fp = fopen(fname,"w");
  if(fp == NULL){
    printf("ERROR: could not open %s\n",fname);
    exit(1);
  }
  for(vtxno = 0; vtxno < surf->nvertices; vtxno++){
    val = MRIFseq_vox(mri,vtxno,0,0,0); //first frame
    if(val == 0.0) continue;
    nnbrs = surf->vertices[vtxno].vnum;
    vtx = &surf->vertices[vtxno];
    fprintf(fp,"%5d  %2d  %8.4f %8.4f %8.4f   %g\n",
	    vtxno,nnbrs,vtx->x,vtx->y,vtx->z,val);
  }
  fclose(fp);
  return(0);
}



/*---------------------------------------------------------------
  See also mrisTriangleArea() in mrisurf.c
  ---------------------------------------------------------------*/
double MRISareaTriangle(double x0, double y0, double z0, 
			double x1, double y1, double z1, 
			double x2, double y2, double z2)
{
  double xx, yy, zz, a;
      
  xx = (y1-y0)*(z2-z0) - (z1-z0)*(y2-y0);
  yy = (z1-z0)*(x2-x0) - (x1-x0)*(z2-z0);
  zz = (x1-x0)*(y2-y0) - (y1-y0)*(x2-x0);
      
  a = 0.5 * sqrt( xx*xx + yy*yy + zz*zz );
  return(a);
}
/*------------------------------------------------------------*/
int MRIStriangleAngles(double x0, double y0, double z0, 
		       double x1, double y1, double z1, 
		       double x2, double y2, double z2,
		       double *a0, double *a1, double *a2)
{
  double d0, d1, d2, d0s, d1s, d2s;
      
  /* dN is the distance of the segment opposite vertex N*/
  d0s = (x1-x2)*(x1-x2) + (y1-y2)*(y1-y2) + (z1-z2)*(z1-z2);
  d1s = (x0-x2)*(x0-x2) + (y0-y2)*(y0-y2) + (z0-z2)*(z0-z2);
  d2s = (x0-x1)*(x0-x1) + (y0-y1)*(y0-y1) + (z0-z1)*(z0-z1);
  d0 = sqrt(d0s);
  d1 = sqrt(d1s);
  d2 = sqrt(d2s);

  /* Law of cosines */
  *a0 = acos( -(d0s-d1s-d2s)/(2*d1*d2) );
  *a1 = acos( -(d1s-d0s-d2s)/(2*d0*d2) );
  *a2 = M_PI - (*a0 + *a1);

  return(0);
}
/*------------------------------------------------------------*/
MRI *MRISdiffusionWeights(MRIS *surf)
{
  MRI *w;
  int nnbrsmax, nnbrs, vtxno, vtxnonbr;
  double area, wtmp;

  /* count the maximum number of neighbors */
  nnbrsmax = surf->vertices[0].vnum;
  for(vtxno = 0; vtxno < surf->nvertices; vtxno++)
    if(nnbrsmax < surf->vertices[vtxno].vnum) 
      nnbrsmax = surf->vertices[vtxno].vnum;
  printf("nnbrsmax = %d\n",nnbrsmax);

  MRIScomputeMetricProperties(surf); /* for area */

  w = MRIallocSequence(surf->nvertices, 1, 1, MRI_FLOAT, nnbrsmax);
  for(vtxno = 0; vtxno < surf->nvertices; vtxno++){
    area = MRISsumVertexFaceArea(surf, vtxno);
    nnbrs = surf->vertices[vtxno].vnum;
    //printf("%d %6.4f   ",vtxno,area);
    for(nthnbr = 0; nthnbr < nnbrs; nthnbr++){
      vtxnonbr = surf->vertices[vtxno].v[nthnbr];
      wtmp = MRISdiffusionEdgeWeight(surf, vtxno, vtxnonbr);
      MRIFseq_vox(w,vtxno,0,0,nthnbr) = (float)wtmp/area;
      //printf("%6.4f ",wtmp);
    }    
    //printf("\n");
    //MRISdumpVertexNeighborhood(surf,vtxno);
  }

  return(w);
}
/*----------------------------------------------------------------------*/
MRI *MRISdiffusionSmooth(MRIS *Surf, MRI *Src, double GStd, MRI *Targ)
{
  MRI *w, *SrcTmp;
  double FWHM;
  float wtmp,val,val0,valnbr;
  int vtxno, nthnbr, nbrvtxno, Niters, nthiter;
  double dt=1;

  if(Surf->nvertices != Src->width){
    printf("ERROR: MRISgaussianSmooth: Surf/Src dimension mismatch\n");
    return(NULL);
  }

  if(Targ == NULL){
    Targ = MRIallocSequence(Src->width, Src->height, Src->depth, 
            MRI_FLOAT, Src->nframes);
    if(Targ==NULL){
      printf("ERROR: MRISgaussianSmooth: could not alloc\n");
      return(NULL);
    }
  }
  else{
    if(Src->width   != Targ->width  || 
       Src->height  != Targ->height || 
       Src->depth   != Targ->depth  ||
       Src->nframes != Targ->nframes){
      printf("ERROR: MRISgaussianSmooth: output dimension mismatch\n");
      return(NULL);
    }
    if(Targ->type != MRI_FLOAT){
      printf("ERROR: MRISgaussianSmooth: structure passed is not MRI_FLOAT\n");
      return(NULL);
    }
  }

  /* Make a copy in case it's done in place */
  SrcTmp = MRIcopy(Src,NULL);

  /* Compute the weights */
  w = MRISdiffusionWeights(Surf);

  FWHM = GStd*sqrt(log(256.0));
  Niters = (int)(((FWHM*FWHM)/(16*log(2)))/dt);
  printf("Niters = %d, dt=%g, GStd = %g, FWHM = %g\n",Niters,dt,GStd,FWHM);
  for(nthiter = 0; nthiter < Niters; nthiter ++){
    //printf("Step = %d\n",nthiter); fflush(stdout);

    for(vtxno = 0; vtxno < Surf->nvertices; vtxno++){
      nnbrs = Surf->vertices[vtxno].vnum;

      for(frame = 0; frame < Targ->nframes; frame ++){
	val0 = MRIFseq_vox(SrcTmp,vtxno,0,0,frame);
	val = val0;
	//printf("%2d %5d %7.4f   ",nthiter,vtxno,val0);
	for(nthnbr = 0; nthnbr < nnbrs; nthnbr++){
	  nbrvtxno = Surf->vertices[vtxno].v[nthnbr];
	  valnbr = MRIFseq_vox(SrcTmp,nbrvtxno,0,0,frame) ;
	  wtmp = dt*MRIFseq_vox(w,vtxno,0,0,nthnbr);
	  val += wtmp*(valnbr-val0);
	  //printf("%6.4f ",wtmp);
	}/* end loop over neighbor */
	//printf("   %7.4f\n",val);

	MRIFseq_vox(Targ,vtxno,0,0,frame) = val;
      }/* end loop over frame */
      
    } /* end loop over vertex */
    
    MRIcopy(Targ,SrcTmp);
  }/* end loop over smooth step */
  
  MRIfree(&SrcTmp);
  MRIfree(&w);
  
  return(Targ);
}
/*-------------------------------------------------------------
  MRISareNeighbors() - tests whether two vertices are neighbors.
  -------------------------------------------------------------*/
int MRISareNeighbors(MRIS *surf, int vtxno1, int vtxno2)
{
  int nnbrs, nthnbr, nbrvtxno;
  nnbrs = surf->vertices[vtxno1].vnum;
  for(nthnbr = 0; nthnbr < nnbrs; nthnbr++){
    nbrvtxno = surf->vertices[vtxno1].v[nthnbr];
    if(nbrvtxno == vtxno2) return(1);
  }
  return(0);
}
/*-------------------------------------------------------------
  MRIScommonNeighbors() - returns the vertex numbers of the two 
  vertices that are common neighbors of the the two vertices 
  listed.
  -------------------------------------------------------------*/
int MRIScommonNeighbors(MRIS *surf, int vtxno1, int vtxno2, 
			int *cvtxno1, int *cvtxno2)
{
  int nnbrs, nthnbr, nbrvtxno;
  
  *cvtxno1 = -1;
  nnbrs = surf->vertices[vtxno1].vnum;
  for(nthnbr = 0; nthnbr < nnbrs; nthnbr++) {
    nbrvtxno = surf->vertices[vtxno1].v[nthnbr];
    if(nbrvtxno == vtxno2) continue;
    if(MRISareNeighbors(surf,nbrvtxno,vtxno2)){
      if(*cvtxno1 == -1) *cvtxno1 = nbrvtxno;
      else{
	*cvtxno2 = nbrvtxno;
	return(0);
      }
    }
  }
  return(0);
}
/*-------------------------------------------------------------------------
  MRISdiffusionEdgeWeight() - computes the unnormalized weight of an
  edge needed for diffusion-based smoothing on the surface. The actual
  weight must be divided by the area of all the traingles surrounding
  the center vertex. See Chung 2003. 
  ----------------------------------------------------------------------*/
double MRISdiffusionEdgeWeight(MRIS *surf, int vtxno0, int vtxnonbr)
{
  int cvtxno1, cvtxno2;
  VERTEX *v0, *vnbr, *cv1, *cv2;
  double a0, a1, btmp, ctmp, w;

  MRIScommonNeighbors(surf, vtxno0, vtxnonbr, &cvtxno1, &cvtxno2);

  v0   = &surf->vertices[vtxno0];
  vnbr = &surf->vertices[vtxnonbr];
  cv1  = &surf->vertices[cvtxno1];
  cv2  = &surf->vertices[cvtxno2];

  MRIStriangleAngles(cv1->x,cv1->y,cv1->z, v0->x,v0->y,v0->z,
		     vnbr->x,vnbr->y,vnbr->z, &a0,&btmp,&ctmp);
  MRIStriangleAngles(cv2->x,cv2->y,cv2->z, v0->x,v0->y,v0->z,
		     vnbr->x,vnbr->y,vnbr->z, &a1,&btmp,&ctmp);
  w = 1/tan(a0) + 1/tan(a1);
  return(w);
}
/*-------------------------------------------------------------------------
  MRISvertexSumFaceArea() - sum the area of the faces that the given
  vertex is part of. Make sure to run MRIScomputeMetricProperties()
  prior to calling this function.
  ----------------------------------------------------------------------*/
double MRISsumVertexFaceArea(MRIS *surf, int vtxno)
{
  int n, nfvtx;
  FACE *face;
  double area;

  area = 0;
  nfvtx = 0;
  for(n = 0; n < surf->nfaces; n++){
    face = &surf->faces[n];
    if(face->v[0] == vtxno || face->v[1] == vtxno || face->v[2] == vtxno){
      area += face->area;
      nfvtx ++;
    }
  }

  if(surf->vertices[vtxno].vnum != nfvtx){
    printf("ERROR: MRISsumVertexFaceArea: number of adjacent faces (%d) "
	   "does not equal number of neighbors (%d)\n",
	   nfvtx,surf->vertices[vtxno].vnum);
    exit(1);
  }

  return(area);
}
/*-------------------------------------------------------------------------
  MRISdumpVertexNeighborhood() 
  ----------------------------------------------------------------------*/
int MRISdumpVertexNeighborhood(MRIS *surf, int vtxno)
{
  int  n, nnbrs, nthnbr, nbrvtxno, nnbrnbrs, nthnbrnbr, nbrnbrvtxno;
  VERTEX *v0, *v;
  FACE *face;

  v0 = &surf->vertices[vtxno];
  nnbrs = surf->vertices[vtxno].vnum;

  printf("  seed vtx %d vc = [%6.3f %6.3f %6.3f], nnbrs = %d\n",vtxno,
	 v0->x,v0->y,v0->z,nnbrs);

  for(nthnbr = 0; nthnbr < nnbrs; nthnbr++) {
    nbrvtxno = surf->vertices[vtxno].v[nthnbr];
    v = &surf->vertices[nbrvtxno];
    printf("   nbr vtx %5d v%d = [%6.3f %6.3f %6.3f]    ",
	   nbrvtxno,nthnbr,v->x,v->y,v->z);

    nnbrnbrs = surf->vertices[nbrvtxno].vnum;
    for(nthnbrnbr = 0; nthnbrnbr < nnbrnbrs; nthnbrnbr++) {
      nbrnbrvtxno = surf->vertices[nbrvtxno].v[nthnbrnbr];
      if(MRISareNeighbors(surf,vtxno,nbrnbrvtxno))
      	printf("%5d ",nbrnbrvtxno);
    }
    printf("\n");
  }

  printf("   faces");  
  for(n = 0; n < surf->nfaces; n++){
    face = &surf->faces[n];
    if(face->v[0] == vtxno || face->v[1] == vtxno || face->v[2] == vtxno){
      printf("  %7.4f",face->area);

    }
  }
  printf("\n");

  return(0);
}

/*--------------------------------------------------------------------------*/

