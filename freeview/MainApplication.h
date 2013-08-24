/**
 * @file  MainApplication.h
 * @brief Application object.
 *
 */
/*
 * Original Author: Ruopeng Wang
 * CVS Revision Info:
 *    $Author: zkaufman $
 *    $Date: 2013/05/03 17:52:33 $
 *    $Revision: 1.2.2.7 $
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
 *
 */


#ifndef MAINAPPLICATION_H
#define MAINAPPLICATION_H

#include <QApplication>

class MainApplication : public QApplication
{
    Q_OBJECT
public:
    explicit MainApplication( int & argc, char ** argv );

signals:
    void GlobalProgress(int n);

public slots:
    void EmitProgress(int n)
    {
      emit GlobalProgress(n);
    }
};

#endif // MAINAPPLICATION_H
