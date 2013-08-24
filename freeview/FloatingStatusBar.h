/**
 * @file  FloatingStatusBar.h
 * @brief REPLACE_WITH_ONE_LINE_SHORT_DESCRIPTION
 *
 */
/*
 * Original Author: Ruopeng Wang
 * CVS Revision Info:
 *    $Author: zkaufman $
 *    $Date: 2013/05/03 17:52:30 $
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
 */
#ifndef FLOATINGSTATUSBAR_H
#define FLOATINGSTATUSBAR_H

#include <QWidget>
#include <QTimer>

namespace Ui
{
class FloatingStatusBar;
}

class FloatingStatusBar : public QWidget
{
  Q_OBJECT

public:
  explicit FloatingStatusBar(QWidget *parent = 0);
  ~FloatingStatusBar();

public slots:
  void SetProgress( int nProgress );
  void ShowProgress();
  void HideProgress();
  void Reposition();
  void StartTimer()
  {
    m_timer->start();
  }

  void StopTimer()
  {
    m_timer->stop();
  }

private slots:
  void OnProgressTimer();

private:
  Ui::FloatingStatusBar *ui;
  QTimer*       m_timer;

};

#endif // FLOATINGSTATUSBAR_H
