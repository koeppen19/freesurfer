//
// Scuba-impl.h
//
// Warning: Do not edit the following four lines.  CVS maintains them.
// Revision Author: $Author: kteich $
// Revision Date  : $Date: 2005/03/25 18:24:45 $
// Revision       : $Revision: 1.6.2.1 $

// This file is necessary for creating the instantiations for template
// classes. Note that we actually include the .cpp files here.  Then
// one line for each template class or function using a specific
// type. See
// http://www.parashift.com/c++-faq-lite/containers-and-templates.html#faq-34.14



#include "Volume3.cpp"
#include "Point2.cpp"
#include "Point3.cpp"
#include "Array2.cpp"
#include "Path.cpp"
#include "ShortestPathFinder.h"
#include "VolumeCollection.h"

using namespace std;

template class Volume3<bool>;
template class Point3<int>;
template ostream& operator << ( ostream&, Point3<int> );
template class Point3<float>;
template ostream& operator << ( ostream&, Point3<float> );
template class Point2<int>;
template ostream& operator << ( ostream&, Point2<int> );
template class Point2<float>;
template ostream& operator << ( ostream&, Point2<float> );
template class Array2<float>;
template class Array2<int>;
template class Array2<bool>;
template class Array2<listElement>;
template class Path<float>;
DeclareIDTracker(Path<float>); //ugh
template class Volume3<Point3<int> >;
template class Array2<Point3<float> >;
template class Array2<VolumeLocation*>;
