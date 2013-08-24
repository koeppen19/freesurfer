/**
 * @file  LayerPropertySurface.h
 * @brief The common properties available to MRI layers
 *
 * An interface implemented by a collection. Layers will get
 * a pointer to an object of this type so they can get access to
 * shared layer settings.
 */
/*
 * Original Author: Ruopeng Wang
 * CVS Revision Info:
 *    $Author: zkaufman $
 *    $Date: 2013/05/03 17:52:32 $
 *    $Revision: 1.4.2.7 $
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

#ifndef LayerPropertySurface_h
#define LayerPropertySurface_h

#include "vtkSmartPointer.h"
#include "LayerProperty.h"
#include <QColor>
#include <QVariantMap>

class vtkLookupTable;
class vtkRGBAColorTransferFunction;
class FSSurface;

class LayerPropertySurface  : public LayerProperty
{
  Q_OBJECT
public:
  LayerPropertySurface ( QObject* parent = NULL );
  ~LayerPropertySurface ();

  enum CURVATURE_MAP
  { CM_Off = 0, CM_Threshold, CM_Binary };

  enum SuraceRenderMode
  { SM_Surface = 0, SM_Wireframe, SM_SurfaceAndWireframe };

  enum MeshColorMap
  { MC_Surface = 0, MC_Curvature, MC_Overlay, MC_Solid };

  double GetOpacity() const;

  double* GetBinaryColor()
  {
    return m_dRGB;
  }

  double* GetThresholdHighColor()
  {
    return m_dRGBThresholdHigh;
  }
  double* GetThresholdLowColor()
  {
    return m_dRGBThresholdLow;
  }
  void SetThresholdColor( double* low, double* high );

  double GetThresholdMidPoint()
  {
    return m_dThresholdMidPoint;
  }

  double GetThresholdSlope()
  {
    return m_dThresholdSlope;
  }

  double* GetEdgeColor()
  {
    return m_dRGBEdge;
  }

  double* GetVectorColor()
  {
    return m_dRGBVector;
  }

  int GetEdgeThickness()
  {
    return m_nEdgeThickness;
  }

  int GetVectorPointSize()
  {
    return m_nVectorPointSize;
  }

  void SetSurfaceSource( FSSurface* surf );

  vtkRGBAColorTransferFunction* GetCurvatureLUT() const;

  void BuildCurvatureLUT( vtkRGBAColorTransferFunction* lut, int nMap );

  int GetCurvatureMap()
  {
    return m_nCurvatureMap;
  }

  int GetSurfaceRenderMode()
  {
    return m_nSurfaceRenderMode;
  }

  double* GetVertexColor()
  {
    return m_dRGBVertex;
  }

  bool GetShowVertices()
  {
    return m_bShowVertices;
  }

  int GetVertexPointSize()
  {
    return m_nVertexPointSize;
  }

  double* GetMeshColor()
  {
    return m_dRGBMesh;
  }

  int GetMeshColorMap()
  {
    return m_nMeshColorMap;
  }

  double* GetPosition()
  {
    return m_dPosition;
  }
  void SetPosition( double* p );

  QVariantMap GetFullSettings();

  void RestoreFullSettings(const QVariantMap& map);

public slots:
  void SetOpacity( double opacity );
  void SetCurvatureMap( int nMap );
  void SetSurfaceRenderMode( int nMode );
  void SetBinaryColor( double r, double g, double b );
  void SetBinaryColor( const QColor& c );
  void SetThresholdMidPoint( double dvalue );
  void SetThresholdSlope( double dvalue );
  void SetEdgeColor( double r, double g, double b );
  void SetEdgeColor( const QColor& c );
  void SetVectorColor( double r, double g, double b );
  void SetVectorColor( const QColor& c );
  void SetVertexColor( double r, double g, double b );
  void SetVertexColor( const QColor& c );
  void SetMeshColor( double r, double g, double b );
  void SetMeshColor( const QColor& c );
  void SetVertexPointSize( int nSize );
  void SetMeshColorMap( int nMap );
  void ShowVertices( bool bShow );
  void SetEdgeThickness( int nThickness );
  void SetVectorPointSize( int nSize );

Q_SIGNALS:
  void OpacityChanged( double opacity );
  void EdgeThicknessChanged( int nThickness );
  void VectorPointSizeChanged( int nSize );
  void RenderModeChanged( int nMode );
  void VertexRenderChanged();
  void MeshRenderChanged();
  void ColorMapChanged();
  void PositionChanged();
  void PositionChanged(double dx, double dy, double dz);

private:
  void SetColorMapChanged();

  double  m_dOpacity;
  double  m_dRGB[3];
  double  m_dRGBThresholdHigh[3];
  double  m_dRGBThresholdLow[3];
  double  m_dRGBEdge[3];
  double  m_dRGBVector[3];
  double  m_dRGBMesh[3];
  int     m_nEdgeThickness;
  int     m_nVectorPointSize;

  double  m_dThresholdMidPoint;
  double  m_dThresholdSlope;

  double  m_dPosition[3];

  int     m_nCurvatureMap;

  int     m_nSurfaceRenderMode;

  bool    m_bShowVertices;
  double  m_dRGBVertex[3];
  int     m_nVertexPointSize;

  int     m_nMeshColorMap;

  vtkSmartPointer<vtkRGBAColorTransferFunction> m_lutCurvature;

  FSSurface* m_surface;
};

#endif
