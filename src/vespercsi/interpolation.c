#include <vespercsi/interpolation.h>
#include <math.h>

double vespercsi_interpolate_linear(double y1, double y2, double mu){
    return (y1 * (1 - mu) + y2 * mu);
}

double vespercsi_interpolate_cosine(double y1, double y2, double mu){
    double mu2;
    mu2 = (1 - cos(mu * M_PI)) / 2;
    return (y1 * (1 - mu2) + y2 * mu2);
}

double vespercsi_interpolate_cubic(double y0, double y1, double y2, double y3, double mu){
    double a0, a1, a2, a3, mu2;

    mu2 = mu * mu;
    a0 = y3 - y2 - y0 + y1;
    a1 = y0 - y1 - a0;
    a2 = y2 - y0;
    a3 = y1;

    return (a0 * mu * mu2 + a1 * mu2 + a2 * mu + a3);
}
