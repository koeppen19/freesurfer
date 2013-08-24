/**
 * @file  DialogRepositionSurface.h
 * @brief Dialog window to execute surface reposition.
 *
 */
/*
 * Original Author: Ruopeng Wang
 * CVS Revision Info:
 *    $Author: zkaufman $
 *    $Date: 2013/05/03 17:52:28 $
 *    $Revision: 1.5.2.7 $
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


#ifndef DIALOGREPOSITIONSURFACE_H
#define DIALOGREPOSITIONSURFACE_H

#include <QDialog>

namespace Ui {
    class DialogRepositionSurface;
}

class DialogRepositionSurface : public QDialog
{
    Q_OBJECT

  public:
    explicit DialogRepositionSurface(QWidget *parent = 0);
    ~DialogRepositionSurface();

    int GetVertex();
    int GetNeighborSize();

    double GetIntensity();
    double GetSigma();

    void GetCoordinate( double* pos );

    int GetFlags();

  public slots:
    void OnApply();
    void OnUndo();
    void OnSave();
    void OnSaveAs();
    void OnComboTarget(int n);
    void UpdateUI();

    void OnSurfaceVertexClicked();
    void UpdateVertex();
    void UpdateIntensity();
    void OnCoordinateTypeChanged();

  private:
    bool ValidateAll();

    Ui::DialogRepositionSurface *ui;
};

#endif // DIALOGREPOSITIONSURFACE_H
