#include "transform.h"

// TODO: Add a ton of tests
// In particular, we should show that all transforms can execute all six transform_* methods, and all six detransform_* methods
// We should show that transform(t,detransform(t,x)) == detransform(t,transform(t,x)) == x
// We should show that for linear transforms, transform(t,x) + transform(t,y) == transform(t,x+y)
// We should show that invertible transforms swap the effects of their transform_* and detransform_* methods
// We should show that for composable transforms, transform(mul(a,b),x) == transform(a, transform(b,x))
// We should show that scale-preserving transforms preserve the distances between pairs of points
// We should show that handedness-preserving transforms preserve the sign of the volume of tetrahedrons
// We should show that all properties can survive a coordinate system transform
