/**
 * @file  Cursor3D.h
 * @brief Cursor for 3D view.
 *
 */
/*
 * Original Author: Ruopeng Wang
 * CVS Revision Info:
 *    $Author: rpwang $
 *    $Date: 2013/11/05 20:25:28 $
 *    $Revision: 1.13 $
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

#ifndef Cursor3D_h
#define Cursor3D_h

#include "RenderView.h"
#include "vtkSmartPointer.h"
#include <QColor>
#include <QObject>

class vtkRenderer;
class vtkActor;
class RenderView3D;

class Cursor3D : public QObject
{
  Q_OBJECT
public:
  Cursor3D( RenderView3D* view );
  virtual ~Cursor3D();

  void SetPosition( double* pos );

  double* GetPosition();
  void GetPosition( double* pos );

  void GetColor( double* rgb );
  void SetColor( double r, double g, double b );

  QColor GetColor();

  void Update();

  void AppendActor( vtkRenderer* renderer );

  void Show( bool bShow = true );

  bool IsShown();

public slots:
  void SetColor( const QColor& color );
  void RebuildActor(double scale = 1);

Q_SIGNALS:
  void Updated();

private:

  vtkSmartPointer<vtkActor> m_actorCursor;

  RenderView3D* m_view;

  double  m_dPosition[3];
};

#endif


