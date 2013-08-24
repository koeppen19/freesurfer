/**
 * @file  LayerVolumeTrack.h
 * @brief Layer class for tracks saved in a multi-frame volume.
 *
 */
/*
 * Original Author: Ruopeng Wang
 * CVS Revision Info:
 *    $Author: zkaufman $
 *    $Date: 2013/05/03 17:52:33 $
 *    $Revision: 1.3.2.9 $
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

#ifndef LAYERVOLUMETRACK_H
#define LAYERVOLUMETRACK_H

#include "LayerMRI.h"
#include "vtkSmartPointer.h"
#include <QList>
#include <QVariantMap>

class vtkActor;
class vtkProp;

class LayerVolumeTrack : public LayerMRI
{
  Q_OBJECT
public:
  LayerVolumeTrack( LayerMRI* ref, QObject* parent = NULL );
  virtual ~LayerVolumeTrack();

  bool LoadFromFile();

  void SetVisible(bool bVisible);

  virtual void Append3DProps( vtkRenderer* renderer, bool* bPlaneVisibility = NULL );

  virtual COLOR_TABLE* GetEmbeddedColorTable()
  {
    return m_ctabStripped;
  }

  virtual void UpdateOpacity();

  virtual bool HasProp(vtkProp *prop);

  QVariantMap GetLabelByProp(vtkProp* prop);

  double GetThreshold(int nLabel);

  void SetThreshold(int nLabel, double th);

  int GetFrameLabel(int nFrame);

public slots:
  void Highlight(int nLabel);
  void RestoreColors();

protected slots:
  void UpdateFrameActor(int n);
  void UpdateColorMap();
  void UpdateData();
  void RebuildActors();

protected:
  QList< vtkSmartPointer<vtkActor> >  m_actors;
  COLOR_TABLE* m_ctabStripped;
};

#endif // LAYERVOLUMETRACK_H
