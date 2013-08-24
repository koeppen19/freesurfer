/**
 * @file  Interactor3D.cpp
 * @brief Base Interactor class for 3D render view.
 *
 */
/*
 * Original Author: Ruopeng Wang
 * CVS Revision Info:
 *    $Author: zkaufman $
 *    $Date: 2013/05/03 17:52:31 $
 *    $Revision: 1.29.2.6 $
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

#include "Interactor3D.h"
#include "RenderView3D.h"
#include "MainWindow.h"
#include "LayerCollection.h"
#include "CursorFactory.h"
#include "LayerPropertyMRI.h"
#include "LayerMRI.h"
#include "CursorFactory.h"
#include "VolumeCropper.h"
#include "SurfaceROI.h"
#include <vtkRenderer.h>

Interactor3D::Interactor3D(QObject* parent) :
  Interactor(parent),
  m_nMousePosX( -1 ),
  m_nMousePosY( -1 ),
  m_bWindowLevel( false ),
  m_bMoveSlice( false ),
  m_surfaceROI( NULL )
{}

Interactor3D::~Interactor3D()
{}

bool Interactor3D::ProcessMouseDownEvent( QMouseEvent* event, RenderView* renderview )
{
  RenderView3D* view = ( RenderView3D* )renderview;

  m_nMousePosX = event->x();
  m_nMousePosY = event->y();

  view->CancelUpdateMouseRASPosition();

  if ( event->button() == Qt::MidButton && event->modifiers() & CONTROL_MODIFIER )
  {
    m_bWindowLevel = true;
  }
  else if ( view->GetHighlightedSlice() >= 0 )
  {
    m_bMoveSlice = true;
  }
  else if ( event->button() == Qt::LeftButton && event->modifiers() & CONTROL_MODIFIER )
  {
    m_surfaceROI = view->InitializeSurfaceROI(event->x(), event->y());
    if (m_surfaceROI)
      return false;   // intercept the event, do not pass down
  }
  else
  {
    return Interactor::ProcessMouseDownEvent( event, renderview ); // pass down the event
  }

  return false; // do not pass down the event
}

bool Interactor3D::ProcessMouseUpEvent( QMouseEvent* event, RenderView* renderview )
{
  RenderView3D* view = ( RenderView3D* )renderview;

  if (m_surfaceROI)
  {
    m_surfaceROI->Close();
    view->RequestRedraw();
  }
  else if ( event->x() == m_nMousePosX && event->y() == m_nMousePosY )
  {
    if ( event->button() == Qt::LeftButton )
    {
      view->UpdateCursorRASPosition( event->x(), event->y() );
      view->UpdateConnectivityDisplay();
    }
  }
  else
  {
    m_nMousePosX = event->x();
    m_nMousePosY = event->y();
  }

  m_bWindowLevel = false;
  m_bMoveSlice = false;

  m_surfaceROI = NULL;

  return Interactor::ProcessMouseUpEvent( event, renderview );
}

bool Interactor3D::ProcessMouseMoveEvent( QMouseEvent* event, RenderView* renderview )
{
  RenderView3D* view = ( RenderView3D* )renderview;

  MainWindow* mainwnd = MainWindow::GetMainWindow();
  if ( mainwnd->IsEmpty() )
  {
    return Interactor::ProcessMouseMoveEvent( event, renderview );
  }

  int posX = event->x();
  int posY = event->y();

  if ( m_bWindowLevel )
  {
    LayerMRI* layer = ( LayerMRI* )mainwnd->GetActiveLayer( "MRI" );
    if ( layer && layer->IsVisible() )
    {
      double scaleX = 0.002;
      double scaleY = 0.002;
      double w = ( posX - m_nMousePosX ) * scaleX;
      double l = ( posY - m_nMousePosY ) * scaleY;
      double scaleOverall = layer->GetProperty()->GetMaxValue() -
                            layer->GetProperty()->GetMinValue();
      w *= scaleOverall;
      l *= scaleOverall;
      w += layer->GetProperty()->GetWindow();
      l += layer->GetProperty()->GetLevel();
      if ( w < 0 )
      {
        w = 0;
      }
      layer->GetProperty()->SetWindowLevel( w, l );
    }
  }
  else if ( m_bMoveSlice )
  {
    view->MoveSliceToScreenCoord( posX, posY );
  }
  else if (m_surfaceROI)
  {
    view->AddSurfaceROIPoint(posX, posY );
  }
  else
  {
    if ( event->buttons() & Qt::MidButton ||
         event->buttons() & Qt::RightButton ||
         event->buttons() & Qt::LeftButton )
    {
      view->CancelUpdateMouseRASPosition();
    }
    else
    {
      view->UpdateMouseRASPosition( posX, posY );
    }

    return Interactor::ProcessMouseMoveEvent( event, view );
  }

  m_nMousePosX = posX;
  m_nMousePosY = posY;

  UpdateCursor( event, view );
  return false;
}

bool Interactor3D::ProcessKeyDownEvent( QKeyEvent* event, RenderView* renderview )
{
  RenderView3D* view = ( RenderView3D* )renderview;

  if ( MainWindow::GetMainWindow()->IsEmpty() )
  {
    return Interactor::ProcessKeyDownEvent( event, renderview );
  }

  int nKeyCode = event->key();
  if  ( nKeyCode == Qt::Key_Up )
  {
    view->MoveUp();
  }
  else if ( nKeyCode == Qt::Key_Down )
  {
    view->MoveDown();
  }
  else if ( nKeyCode == Qt::Key_Left )
  {
    view->MoveLeft();
  }
  else if ( nKeyCode == Qt::Key_Right )
  {
    view->MoveRight();
  }
  else if ( nKeyCode == Qt::Key_R ) //|| nKeyCode == 'F' || nKeyCode == 'S' || nKeyCode == 'W' )
  {
    // do nothing, just intercept these keycodes
  }
  else if ( nKeyCode == Qt::Key_Delete )
  {
    view->DeleteCurrentSelectRegion();
  }
  else
  {
    return Interactor::ProcessKeyDownEvent( event, view );
  }

  return false;
}

void Interactor3D::UpdateCursor( QEvent* event, QWidget* wnd )
{
  RenderView3D* view = ( RenderView3D* )wnd;
  if ( view->GetHighlightedSlice() >= 0 )
  {
    wnd->setCursor( CursorFactory::CursorGrab );
  }
  else
  {
    Interactor::UpdateCursor( event, wnd );
  }
}

