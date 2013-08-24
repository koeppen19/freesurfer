/**
 * @file  DialogTransformVolume.cpp
 * @brief REPLACE_WITH_ONE_LINE_SHORT_DESCRIPTION
 *
 */
/*
 * Original Author: Ruopeng Wang
 * CVS Revision Info:
 *    $Author: zkaufman $
 *    $Date: 2013/05/03 17:52:28 $
 *    $Revision: 1.11.2.10 $
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
#include "DialogTransformVolume.h"
#include "ui_DialogTransformVolume.h"
#include "MainWindow.h"
#include "LayerCollection.h"
#include "LayerMRI.h"
#include "LayerPropertyMRI.h"
#include "LayerLandmarks.h"
#include "RenderView.h"
#include "vtkMatrix4x4.h"
#include "vtkTransform.h"
#include "vtkMath.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QPixmap>
#include <QDebug>
#include <QButtonGroup>

extern "C"
{
#include "mri.h"
}

DialogTransformVolume::DialogTransformVolume(QWidget *parent) :
  QDialog(parent),
  UIUpdateHelper(),
  ui(new Ui::DialogTransformVolume)
{
  ui->setupUi(this);
  ui->groupBoxLandmarks->hide();
  ui->pushButtonApply->hide();
  ui->pushButtonSaveAndReload->hide();

  QButtonGroup* bg = new QButtonGroup(this);
  bg->addButton(ui->radioButtonRotateLandmarks);
  bg->addButton(ui->radioButtonRotateManual);
  bg->setExclusive(true);

  m_checkRotate[0] = ui->checkBoxRotateX;
  m_checkRotate[1] = ui->checkBoxRotateY;
  m_checkRotate[2] = ui->checkBoxRotateZ;
  m_comboRotate[0] = ui->comboBoxRotateX;
  m_comboRotate[1] = ui->comboBoxRotateY;
  m_comboRotate[2] = ui->comboBoxRotateZ;
  m_sliderRotate[0] = ui->horizontalSliderRotateX;
  m_sliderRotate[1] = ui->horizontalSliderRotateY;
  m_sliderRotate[2] = ui->horizontalSliderRotateZ;
  m_textAngle[0] = ui->lineEditRotateX;
  m_textAngle[1] = ui->lineEditRotateY;
  m_textAngle[2] = ui->lineEditRotateZ;
  m_scrollTranslate[0] = ui->scrollBarTranslateX;
  m_scrollTranslate[1] = ui->scrollBarTranslateY;
  m_scrollTranslate[2] = ui->scrollBarTranslateZ;
  m_textTranslate[0] = ui->lineEditTranslateX;
  m_textTranslate[1] = ui->lineEditTranslateY;
  m_textTranslate[2] = ui->lineEditTranslateZ;
  m_scrollScale[0] = ui->scrollBarScaleX;
  m_scrollScale[1] = ui->scrollBarScaleY;
  m_scrollScale[2] = ui->scrollBarScaleZ;
  m_textScale[0] = ui->lineEditScaleX;
  m_textScale[1] = ui->lineEditScaleY;
  m_textScale[2] = ui->lineEditScaleZ;
  m_btnPickLandmark << ui->pushButtonLandmarkPick1
                    << ui->pushButtonLandmarkPick2
                    << ui->pushButtonLandmarkPick3
                    << ui->pushButtonLandmarkPick4;
  m_colorPickerLandmark << ui->colorPickerLandmark1
                        << ui->colorPickerLandmark2
                        << ui->colorPickerLandmark3
                        << ui->colorPickerLandmark4;
  m_comboLandmark << ui->comboBoxAxis11
                  << ui->comboBoxAxis12
                  << ui->comboBoxAxis21
                  << ui->comboBoxAxis22;

  for (int i = 0; i < 3; i++)
  {
    m_checkRotate[i]->hide();
    m_comboRotate[i]->hide();
  }

  connect(MainWindow::GetMainWindow()->GetLayerCollection("MRI"), SIGNAL(ActiveLayerChanged(Layer*)),
          this, SLOT(OnActiveLayerChanged()));
  connect(ui->pushButtonSaveVolumeAs, SIGNAL(clicked()),
          MainWindow::GetMainWindow(), SLOT(SaveVolumeAs()));
  connect(ui->pushButtonSaveAndReload, SIGNAL(clicked()),
          MainWindow::GetMainWindow(), SLOT(SaveVolumeAsAndReload()));

  LayerLandmarks* landmarks = (LayerLandmarks*)MainWindow::GetMainWindow()
                              ->GetSupplementLayer("Landmarks");
  landmarks->SetLandmarkColor(0, Qt::red);
  landmarks->SetLandmarkColor(1, Qt::green);
  landmarks->SetLandmarkColor(2, Qt::blue);
  landmarks->SetLandmarkColor(3, Qt::yellow);
  for (int i = 0; i < m_colorPickerLandmark.size(); i++)
    m_colorPickerLandmark[i]->setCurrentColor(landmarks->GetLandmark(i).color);
  UpdateLandmarkColors();
}

DialogTransformVolume::~DialogTransformVolume()
{
  delete ui;
}

void DialogTransformVolume::showEvent(QShowEvent *e)
{
  OnRadioButtonLandmark(ui->radioButtonRotateLandmarks->isChecked());
  QDialog::showEvent(e);
}

void DialogTransformVolume::closeEvent(QCloseEvent *e)
{
  LayerLandmarks* landmarks = (LayerLandmarks*)MainWindow::GetMainWindow()
                              ->GetSupplementLayer("Landmarks");
  if (landmarks)
    landmarks->SetVisible(false);
  QDialog::closeEvent(e);
}

// scope: 0 => translate related, 1 => scale related, 2 => both
void DialogTransformVolume::UpdateUI( int scope )
{
  LayerMRI* layer = (LayerMRI* )MainWindow::GetMainWindow()->GetActiveLayer( "MRI" );
  if ( layer )
  {
    QList<QWidget*> allwidgets = this->findChildren<QWidget*>();
    for ( int i = 0; i < allwidgets.size(); i++ )
    {
      allwidgets[i]->blockSignals( true );
    }
    if ( scope == 0 || scope == 2 )
    {
      double* vs = layer->GetWorldVoxelSize();
      double* ws = layer->GetWorldSize();
      double pos[3];
      layer->GetTranslate( pos );
      for ( int i = 0; i < 3; i++ )
      {
        int range = (int)( ws[i] / vs[i] + 0.5 ) * 2;
        int npos = (int)(pos[i] / vs[i]) + range/2;
        m_scrollTranslate[i]->setRange(0, range);
        m_scrollTranslate[i]->setValue(npos);
        ChangeLineEditNumber(m_textTranslate[i], pos[i]);
      }
    }
    if ( scope == 1 || scope == 2 )
    {
      double scale[3];
      layer->GetScale( scale );
      for ( int i = 0; i < 3; i++ )
      {
        if ( scale[i] >= 1 )
        {
          m_scrollScale[i]->setValue( 50 + (int)( (scale[i]-1.0)*50 ) );
        }
        else
        {
          m_scrollScale[i]->setValue( 50 - (int)( (1.0-scale[i])*100 ) );
        }

        ChangeLineEditNumber(m_textScale[i], scale[i]);
      }
    }

    ui->pushButtonRestore->setEnabled( layer->IsTransformed() );
    ui->pushButtonSaveReg->setEnabled( layer->IsTransformed() );
    ui->pushButtonSaveVolumeAs->setEnabled( layer->IsTransformed() );
    double angle[3];
    layer->GetRotate(angle);
    ui->groupBoxAxis->setDisabled(angle[0] != 0 || angle[1] != 0 || angle[2] != 0);
    for (int i = 0; i < 3; i++)
    {
      double val = angle[i];
      while (val > 180)
        val -= 360;
      while (angle[i] < -180)
        val += 360;
      m_sliderRotate[i]->setValue((int)val);
      ChangeLineEditNumber(m_textAngle[i], angle[i]);
    }

    for ( int i = 0; i < allwidgets.size(); i++ )
    {
      allwidgets[i]->blockSignals( false );
    }
  }
}

bool DialogTransformVolume::GetRotation( int nIndex_in,
    int& plane_out,
    double& angle_out )
{
  if ( nIndex_in < 0 ||
       nIndex_in > 2 ||
       !m_checkRotate[ nIndex_in ]->isChecked() )
  {
    return false;
  }

  plane_out = m_comboRotate[ nIndex_in ]->currentIndex();
  bool bOK;
  double dVal = m_textAngle[ nIndex_in ]->text().toDouble(&bOK);
  if ( bOK)
  {
    angle_out = dVal;
  }
  return bOK;
}

void DialogTransformVolume::OnApply()
{
  if (ui->radioButtonRotateManual->isChecked())
  {
    int plane;
    double angle;
    if ( !m_checkRotate[0]->isChecked() &&
         !m_checkRotate[1]->isChecked() &&
         !m_checkRotate[2]->isChecked() )
    {
      QMessageBox::warning( this, "Error",
                            "Must at least select one rotation.");
      return;
    }
    else if ( ( m_checkRotate[0]->isChecked() && !GetRotation( 0, plane, angle ) ) ||
              ( m_checkRotate[1]->isChecked() && !GetRotation( 1, plane, angle ) ) ||
              ( m_checkRotate[2]->isChecked() && !GetRotation( 2, plane, angle ) ) )
    {
      QMessageBox::warning( this, "Error",
                            "Please enter correct rotation angle.");
      return;
    }
  }
  else
  {
    if (ui->comboBoxAxis11->currentIndex() == ui->comboBoxAxis12->currentIndex() ||
        ui->comboBoxAxis21->currentIndex() == ui->comboBoxAxis22->currentIndex() ||
        ui->comboBoxAxisTarget1->currentIndex() == ui->comboBoxAxisTarget2->currentIndex() ||
        (ui->comboBoxAxis11->currentIndex() == ui->comboBoxAxis21->currentIndex() &&
         ui->comboBoxAxis12->currentIndex() == ui->comboBoxAxis22->currentIndex()) ||
        (ui->comboBoxAxis11->currentIndex() == ui->comboBoxAxis22->currentIndex() &&
         ui->comboBoxAxis12->currentIndex() == ui->comboBoxAxis21->currentIndex()))
    {
      QMessageBox::warning(this, "Error", "Something is wrong on the mapping settings. Please correct it.");
      return;
    }
    for (int i = 0; i < this->m_btnPickLandmark.size(); i++)
      m_btnPickLandmark[i]->setChecked(false);
  }

  DoRotate();

  UpdateUI();
}

void DialogTransformVolume::OnSaveReg()
{
  LayerMRI* layer_mri = ( LayerMRI* )MainWindow::GetMainWindow()->GetActiveLayer( "MRI" );
  if ( !layer_mri)
  {
    return;
  }

  QString filename = QFileDialog::getSaveFileName(this, "Save Registration",
                     QFileInfo( layer_mri->GetFileName() ).absolutePath(),
                     "LTA files (*.lta);;All files (*)");
  if ( !filename.isEmpty() )
  {
    layer_mri->SaveRegistration( filename );
  }
}

void DialogTransformVolume::OnRestore()
{
  LayerMRI* layer = ( LayerMRI* )MainWindow::GetMainWindow()->GetActiveLayer( "MRI" );
  if ( layer )
  {
    layer->Restore();
    UpdateUI();
  }
  LayerLandmarks* landmarks = (LayerLandmarks*)MainWindow::GetMainWindow()->GetSupplementLayer("Landmarks");
  landmarks->Restore();
}

void DialogTransformVolume::DoRotate()
{
  LayerMRI* layer = ( LayerMRI* )MainWindow::GetMainWindow()->GetActiveLayer( "MRI" );
  if ( layer )
  {
    std::vector<RotationElement> rotations;
    RotationElement re;
    re.SampleMethod = SAMPLE_TRILINEAR;
    if ( ui->radioButtonNearestNeighbor->isChecked() )
    {
      re.SampleMethod = SAMPLE_NEAREST;
    }
    else if (ui->radioButtonCubic->isChecked())
    {
      re.SampleMethod = SAMPLE_CUBIC_BSPLINE;
    }
    if (ui->radioButtonRotateManual->isChecked())
    {
      if ( ui->radioButtonAroundCursor->isChecked() )
      {
        MainWindow::GetMainWindow()->GetLayerCollection( "MRI" )->
        GetSlicePosition( re.Point );
        layer->RemapPositionToRealRAS( re.Point, re.Point );
      }
      else
      {
        // use center of the volume to rotate
        layer->GetRASCenter( re.Point );
      }
  //    else if ( m_radioSinc->GetValue() )
  //      re.SampleMethod = SAMPLE_SINC;

      for ( int i = 0; i < 3; i++ )
      {
        if ( GetRotation( i, re.Plane, re.Angle ) )
        {
          rotations.push_back( re );
        }
      }
      MainWindow::GetMainWindow()->RotateVolume( rotations, false );
    }
    else
    {
      layer->GetRASCenter( re.Point );
      LayerLandmarks* landmarks = (LayerLandmarks*)MainWindow::GetMainWindow()->GetSupplementLayer("Landmarks");
      double* p[4];
      for (int i = 0; i < 4; i++)
        p[i] = landmarks->GetLandmark(i).pos;

      // first figure out landmark vectors
      double v[3][3], ax[3][3];
      int n0 = ui->comboBoxAxis11->currentIndex();
      int n1 = ui->comboBoxAxis12->currentIndex();
      for (int i = 0; i < 3; i++)
        v[0][i] = p[n1][i] - p[n0][i];
      vtkMath::Normalize(v[0]);

      n0 = ui->comboBoxAxis21->currentIndex();
      n1 = ui->comboBoxAxis22->currentIndex();
      for (int i = 0; i < 3; i++)
        v[1][i] = p[n1][i] - p[n0][i];
      vtkMath::Normalize(v[1]);
      vtkMath::Cross(v[0], v[1], v[2]);
      vtkMath::Normalize(v[2]);
      vtkMath::Cross(v[2], v[0], v[1]);

      int n[3];
      n[0] = ui->comboBoxAxisTarget1->currentIndex();
      n[1] = ui->comboBoxAxisTarget2->currentIndex();
      if (n[0] == 0)
        n[2] = (n[1] == 1 ? 2 : 1);
      else if (n[0] == 1)
        n[2] = (n[1] == 0 ? 2 : 0);
      else
        n[2] = (n[1] == 0 ? 1 : 0);

      for (int i = 0; i < 3; i++)
      {
        for (int j = 0; j < 3; j++)
          ax[n[i]][j] = v[i][j];
      }

      double m[16];
      memset(m, 0, sizeof(double)*16);
      for (int i = 0; i < 16; i++)
      {
        if (i/4 < 3 && i%4 < 3)
          m[i] = ax[i/4][i%4];
      }
      m[15] = 1;

      vtkSmartPointer<vtkTransform> tf = vtkSmartPointer<vtkTransform>::New();
      tf->Identity();
      double pt[3];
      layer->RASToTarget( re.Point, pt );
      tf->Translate(pt[0], pt[1], pt[2]);
      tf->Concatenate(m);
      tf->Translate(-pt[0], -pt[1], -pt[2]);
      vtkMatrix4x4::DeepCopy(m, tf->GetMatrix());
      MainWindow::GetMainWindow()->TransformVolume(m, re.SampleMethod);
    }
  }
}

void DialogTransformVolume::OnActiveLayerChanged()
{
  if ( isVisible() )
  {
    UpdateUI();
  }
  LayerMRI* layer = (LayerMRI* )MainWindow::GetMainWindow()->GetActiveLayer( "MRI" );
  LayerLandmarks* landmarks = (LayerLandmarks*)MainWindow::GetMainWindow()->GetSupplementLayer( "Landmarks" );
  landmarks->SetMRIRef(layer);
  OnSampleMethodChanged();
}

void DialogTransformVolume::OnSampleMethodChanged()
{
  LayerMRI* layer = (LayerMRI* )MainWindow::GetMainWindow()->GetActiveLayer( "MRI" );
  if (layer)
  {
    int nMethod = SAMPLE_NEAREST;
    if (ui->radioButtonTrilinear->isChecked())
      nMethod = SAMPLE_TRILINEAR;
    else if (ui->radioButtonCubic->isChecked())
      nMethod = SAMPLE_CUBIC_BSPLINE;
    layer->GetProperty()->SetResliceInterpolation(nMethod);
  }
}

void DialogTransformVolume::OnLineEditRotateX(const QString& text)
{
  RespondTextRotate( 0 );
}

void DialogTransformVolume::OnLineEditRotateY(const QString& text)
{
  RespondTextRotate( 1 );
}

void DialogTransformVolume::OnLineEditRotateZ(const QString& text)
{
  RespondTextRotate( 2 );
}

void DialogTransformVolume::OnSliderRotateX(int nVal)
{
  RespondSliderRotate( 0 );
}

void DialogTransformVolume::OnSliderRotateY(int nVal)
{
  RespondSliderRotate( 1 );
}

void DialogTransformVolume::OnSliderRotateZ(int nVal)
{
  RespondSliderRotate( 2 );
}

void DialogTransformVolume::OnLineEditTranslateX(const QString& text)
{
  RespondTextTranslate( 0 );
}

void DialogTransformVolume::OnLineEditTranslateY(const QString& text)
{
  RespondTextTranslate( 1 );
}

void DialogTransformVolume::OnLineEditTranslateZ(const QString& text)
{
  RespondTextTranslate( 2 );
}

void DialogTransformVolume::OnScrollBarTranslateX(int nVal)
{
  RespondScrollTranslate( 0 );
}

void DialogTransformVolume::OnScrollBarTranslateY(int nVal)
{
  RespondScrollTranslate( 1 );
}

void DialogTransformVolume::OnScrollBarTranslateZ(int nVal)
{
  RespondScrollTranslate( 2 );
}

void DialogTransformVolume::RespondTextTranslate( int n )
{
  if ( isVisible() )
  {
    bool bOK;
    double dvalue =m_textTranslate[n]->text().toDouble(&bOK);
    if ( bOK )
    {
      LayerMRI* layer = ( LayerMRI* )MainWindow::GetMainWindow()->GetActiveLayer( "MRI" );
      if ( layer )
      {
        double pos[3];
        layer->GetTranslate( pos );
        pos[n] = dvalue;
        layer->SetTranslate( pos );
        MainWindow::GetMainWindow()->RequestRedraw();

        double* vs = layer->GetWorldVoxelSize();
        int range = m_scrollTranslate[n]->maximum();
        m_scrollTranslate[n]->blockSignals(true);
        m_scrollTranslate[n]->setValue(range/2 + (int)( pos[n] / vs[n] ) );
        m_scrollTranslate[n]->blockSignals(false);
        UpdateUI( 1 );
      }
    }
  }
}

void DialogTransformVolume::RespondScrollTranslate( int n )
{
  if ( isVisible() )
  {
    LayerMRI* layer = ( LayerMRI* )MainWindow::GetMainWindow()->GetActiveLayer( "MRI" );
    if ( layer )
    {
      double pos[3];
      layer->GetTranslate( pos );
      int range = m_scrollTranslate[n]->maximum();
      int npos = m_scrollTranslate[n]->value();
      double* vs = layer->GetWorldVoxelSize();
      pos[n] = ( npos - range/2 ) * vs[n];
      layer->SetTranslate( pos );
      MainWindow::GetMainWindow()->RequestRedraw();
      ChangeLineEditNumber(m_textTranslate[n], pos[n] );
      UpdateUI( 1 );
    }
  }
}

void DialogTransformVolume::RespondTextRotate( int n )
{
  if ( isVisible() )
  {
    bool bOK;
    double dvalue = m_textAngle[n]->text().toDouble(&bOK);
    if ( bOK )
    {
      LayerMRI* layer = ( LayerMRI* )MainWindow::GetMainWindow()->GetActiveLayer( "MRI" );
      if ( layer )
      {
        double angle[3];
        layer->GetRotate( angle );
        angle[n] = dvalue;
        layer->SetRotate( angle, ui->radioButtonAroundCenter->isChecked() );
        MainWindow::GetMainWindow()->RequestRedraw();

        while (dvalue > 180)
          dvalue -= 360;
        while (dvalue < -180)
          dvalue += 360;

        m_sliderRotate[n]->blockSignals(true);
        m_sliderRotate[n]->setValue( (int)dvalue );
        m_sliderRotate[n]->blockSignals(false);
        UpdateUI( 1 );
      }
    }
  }
}

void DialogTransformVolume::RespondSliderRotate( int n )
{
  if ( isVisible() )
  {
    LayerMRI* layer = ( LayerMRI* )MainWindow::GetMainWindow()->GetActiveLayer( "MRI" );
    if ( layer )
    {
      double angle[3];
      layer->GetRotate( angle );
      angle[n] = m_sliderRotate[n]->value();
      layer->SetRotate( angle, ui->radioButtonAroundCenter->isChecked() );
      MainWindow::GetMainWindow()->RequestRedraw();
      ChangeLineEditNumber(m_textAngle[n], angle[n] );
      UpdateUI( 1 );
    }
  }
}

void DialogTransformVolume::OnLineEditScaleX(const QString& text)
{
  RespondTextScale( 0 );
}

void DialogTransformVolume::OnLineEditScaleY(const QString& text)
{
  RespondTextScale( 1 );
}

void DialogTransformVolume::OnLineEditScaleZ(const QString& text)
{
  RespondTextScale( 2 );
}

void DialogTransformVolume::OnScrollBarScaleX(int nVal)
{
  RespondScrollScale( 0 );
}

void DialogTransformVolume::OnScrollBarScaleY(int nVal)
{
  RespondScrollScale( 1 );
}

void DialogTransformVolume::OnScrollBarScaleZ(int nVal)
{
  RespondScrollScale( 2 );
}

void DialogTransformVolume::RespondTextScale( int n )
{
  if ( isVisible() )
  {
    bool bOK;
    double dvalue = m_textScale[n]->text().toDouble(&bOK);
    if ( bOK && dvalue > 0 )
    {
      LayerMRI* layer = ( LayerMRI* )MainWindow::GetMainWindow()->GetActiveLayer( "MRI" );
      if ( layer )
      {
        double scale[3];
        layer->GetScale( scale );
        scale[n] = dvalue;
        layer->SetScale( scale );
        MainWindow::GetMainWindow()->RequestRedraw();

        m_scrollScale[n]->blockSignals(true);
        if ( dvalue >= 1 )
        {
          m_scrollScale[n]->setValue( 50 + (int)( (dvalue-1.0)*50 ) );
        }
        else
        {
          m_scrollScale[n]->setValue( 50 - (int)( (1.0-dvalue)*100 ) );
        }
        m_scrollScale[n]->blockSignals(false);
        UpdateUI( 0 );
      }
    }
  }
}

void DialogTransformVolume::RespondScrollScale( int n )
{
  LayerMRI* layer = ( LayerMRI* )MainWindow::GetMainWindow()->GetActiveLayer( "MRI" );
  if ( layer )
  {
    double scale[3];
    layer->GetScale( scale );
    int npos = m_scrollScale[n]->value();
    if ( npos >= 50 )
    {
      scale[n] = ( npos - 50 ) / 50.0 + 1.0;
    }
    else
    {
      scale[n] = ( npos - 50 ) / 100.0 + 1.0;
    }
    layer->SetScale( scale );
    MainWindow::GetMainWindow()->RequestRedraw();

    ChangeLineEditNumber( m_textScale[n], scale[n] );
    UpdateUI( 0 );
  }
}

void DialogTransformVolume::OnButtonLandmarkPick()
{
  for (int i = 0; i < m_btnPickLandmark.size(); i++)
    m_btnPickLandmark[i]->blockSignals(true);

  for (int i = 0; i < m_btnPickLandmark.size(); i++)
  {
    if (qobject_cast<QPushButton*>(sender()) == m_btnPickLandmark[i])
    {
      for (int j = 0; j < m_btnPickLandmark.size(); j++)
      {
        if (i != j)
          m_btnPickLandmark[j]->setChecked(false);
      }
    }
  }

  int n = -1;
  for (int i = 0; i < m_btnPickLandmark.size(); i++)
  {
    if (m_btnPickLandmark[i]->isChecked())
    {
      n = i;
      break;
    }
  }
  if (n >= 0)
  {
    // switch to landmark mode
    MainWindow::GetMainWindow()->SetMode(RenderView::IM_Navigate);
  }

  for (int i = 0; i < m_btnPickLandmark.size(); i++)
    m_btnPickLandmark[i]->blockSignals(false);

  emit CurrentLandmarkChanged(n);
}

void DialogTransformVolume::UpdateLandmarkColors()
{
  LayerLandmarks* landmarks = (LayerLandmarks*)MainWindow::GetMainWindow()
                              ->GetSupplementLayer("Landmarks");
  for (int i = 0; i < m_colorPickerLandmark.size(); i++)
  {
    if (qobject_cast<QtColorPicker*>(sender()) == m_colorPickerLandmark[i])
    {
      landmarks->SetLandmarkColor(i, m_colorPickerLandmark[i]->currentColor());
      break;
    }
  }
  QList<QColor> colors;
  for (int i = 0; i < m_colorPickerLandmark.size(); i++)
    colors << landmarks->GetLandmark(i).color;
  foreach (QComboBox* cbox, m_comboLandmark)
  {
    for (int i = 0; i < m_colorPickerLandmark.size(); i++)
      cbox->setItemIcon(i, MakeIcon(colors[i], 12));
  }
}

QIcon DialogTransformVolume::MakeIcon(const QColor& color, int size)
{
  QPixmap pix( size, size );
  pix.fill( color );
  return QIcon(pix);
}

void DialogTransformVolume::OnRadioButtonLandmark(bool bChecked)
{
  if (!bChecked)
  {
    for (int i = 0; i < m_btnPickLandmark.size(); i++)
      this->m_btnPickLandmark[i]->setChecked(false);
  }
  LayerLandmarks* landmarks = (LayerLandmarks*)MainWindow::GetMainWindow()
                              ->GetSupplementLayer("Landmarks");
  if (landmarks)
    landmarks->SetVisible(bChecked);
}

void DialogTransformVolume::OnButtonCenterToCursor()
{
  if ( isVisible() )
  {
    LayerMRI* layer = ( LayerMRI* )MainWindow::GetMainWindow()->GetActiveLayer( "MRI" );
    if ( layer )
    {
      double pos[3];
      layer->GetSlicePosition(pos);
      qDebug() << pos[0] << pos[1] << pos[2];
      layer->SetTranslateByCenterPosition( pos );
      MainWindow::GetMainWindow()->RequestRedraw();

      double* vs = layer->GetWorldVoxelSize();
      for (int n = 0; n < 3; n++)
      {
        int range = m_scrollTranslate[n]->maximum();
        m_scrollTranslate[n]->blockSignals(true);
        m_scrollTranslate[n]->setValue(range/2 + (int)( pos[n] / vs[n] ) );
        m_scrollTranslate[n]->blockSignals(false);
      }
      UpdateUI( 0 );
    }
  }
}
