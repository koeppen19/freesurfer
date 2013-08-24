/**
 * @file  DialogSaveVolume.h
 * @brief Dialog to save volume
 *
 */
/*
 * Original Author: Ruopeng Wang
 * CVS Revision Info:
 *    $Author: zkaufman $
 *    $Date: 2013/05/03 17:52:28 $
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
#ifndef DIALOGSAVEVOLUME_H
#define DIALOGSAVEVOLUME_H

#include <QDialog>

namespace Ui
{
class DialogSaveVolume;
}

class DialogSaveVolume : public QDialog
{
  Q_OBJECT

public:
  explicit DialogSaveVolume(QWidget *parent = 0, const QString& filepath = "");
  ~DialogSaveVolume();

  QString GetFileName();

  bool GetResample();
  bool GetCrop();

protected slots:
  void OnOK();
  void OnOpen();

private:
  Ui::DialogSaveVolume *ui;
};

#endif // DIALOGSAVEVOLUME_H
