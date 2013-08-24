/**
 * @file  gcautils.hpp
 * @brief C++ GCA utilities
 *
 */
/*
 * Original Authors: Richard Edgar
 * CVS Revision Info:
 *    $Author: nicks $
 *    $Date: 2012/12/14 10:35:06 $
 *    $Revision: 1.1.2.1 $
 *
 * Copyright © 2011-2012 The General Hospital Corporation (Boston, MA) "MGH"
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

#ifndef GCA_UTILS_HPP
#define GCA_UTILS_HPP

#include "gca.h"

namespace Freesurfer
{

void GetGCAstats( const GCA* const src );

void GetGCAnodeStats( const GCA* const src );
void GetGCApriorStats( const GCA* const src );
}


#endif
