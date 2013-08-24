/**
 * @file  rforest.h
 * @brief types and prototypes for random forest classifier
 *
 * Base on Leo Breiman's random forest classification algorithm
 */
/*
 * Original Author: Bruce Fischl
 * CVS Revision Info:
 *    $Author: rpwang $
 *    $Date: 2012/04/06 19:15:36 $
 *    $Revision: 1.4.2.2 $
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


#ifndef RFOREST_H
#define RFOREST_H


typedef struct
{
  int    ntrees ;
  int    nfeatures ;
  int    nclasses ;
  int    max_depth ;
  int    nsteps ;
  double training_fraction ;
  double feature_fraction ;
} RFOREST_PARMS, RF_PARMS ;

typedef struct node
{
  struct node *left, *right ;  // the children. NULL is this is a leaf
  int    *class_counts ;       // the number of each class at this node ;
  int    feature ;
  double thresh ;
  int    depth ;
  int    total_counts ;       // sum of all the class_counts
  int    *training_set ;      // list of training set indices (total_counts of them)
} NODE ;

typedef struct
{
  int  *feature_list ;  // list of indices into feature vector
  int  nfeatures ;
  NODE root ;
  int  depth ;
  int  nleaves ;
  int  max_leaves ;
  NODE **leaves ;
} TREE ;

typedef struct
{
  int    nfeatures ;
  int    nclasses ;
  char   **class_names ;
  int    ntrees ;
  TREE   *trees ;
  int    max_depth ;
  double *feature_min ;    // min this feature attains over all training samples
  double *feature_max ;    // max this feature attains over all training samples
  int    *training_classes ;
  double **training_data ;
  int    ntraining ;
  int    nsteps ;           // # of steps to take in threshold search
  double feature_fraction ;  // the fraction of features that each tree contains
  double training_fraction ; // the fraction of training data that each tree contains
} RANDOM_FOREST, RF ;

RANDOM_FOREST *RFalloc(int ntrees, int nfeatures, int nclasses, int max_depth,
                       char **class_names, int nsteps) ;
RANDOM_FOREST *RFread(char *fname) ;
RANDOM_FOREST *RFreadFrom(FILE *fp) ;
int  RFtrain(RANDOM_FOREST *rf, double feature_fraction, double training_fraction, int *training_classes, double **training_data,int ntraining);
int  RFwrite(RANDOM_FOREST *rf, char *fname) ;
int  RFwriteInto(RANDOM_FOREST *rf, FILE *fp) ;
int  RFclassify(RANDOM_FOREST *rf, double *feature, double *p_pval, int true_class) ;
int  RFcomputeOutOfBagCorrect(RANDOM_FOREST *rf, int *training_classes, double **training_data,int ntraining);
int  RFtrainTree(RANDOM_FOREST *rf, int tno, int *training_classes, double **training_data, int ntraining);
int  RFsetNumberOfClasses(RANDOM_FOREST *rf, int nlabels) ;


#endif
