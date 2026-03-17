#ifndef VESPERCSI_INTERPOLATION_H
#define VESPERCSI_INTERPOLATION_H

#include <complex.h>

double vespercsi_interpolate_linear(double y1, double y2, double mu);
double vespercsi_interpolate_cosine(double y1, double y2, double mu);
double vespercsi_interpolate_cubic(double y0, double y1, double y2, double y3, double mu);

#endif
