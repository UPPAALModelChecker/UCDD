// -*- mode: C++; c-file-style: "stroustrup"; c-basic-offset: 4; indent-tabs-mode: nil; -*-

#include "base/random.h"

#define BOOST_MATH_DISABLE_DEPRECATED_03_WARNING 1
#include <boost/math/distributions/chi_squared.hpp>

#include <iostream>
#include <iomanip>
#include <limits>

using namespace std;

static double fracInRange(const int* values, int length, double from, double till)
{
    double count = 0;
    for (int i = 0; i < length; ++i)
        if (from <= values[i] && values[i] < till)
            count++;
    return count / length;
}

using boost::math::chi_squared;
using boost::math::complement;
using boost::math::quantile;

static bool frequency_analysis(int n, int values[], int range, double alpha)
{
    const double sqrt_n = sqrt(n);
    double sum = 0, sumsq = 0;
    double minv = numeric_limits<double>::infinity(), maxv = -numeric_limits<double>::infinity();
    double chi2 = 0;  // Pearsons chi square test

    cout << "_________________________________________" << endl;
    cout << "Frequency analysis (vs. expected values):" << endl;
    cout << "Boundary values: " << values[0] << ", ..., " << values[range - 1] << endl;

    for (int i = 0; i < range; ++i) {
        if (values[i] < 0)
            cout << i << endl;
        if (values[i] == 0) {
            cout << "value " << i << " was never generated." << endl;
            exit(EXIT_FAILURE);
        }
        sum += values[i];
        sumsq += values[i] * values[i];
        if (values[i] < minv)
            minv = values[i];
        if (values[i] > maxv)
            maxv = values[i];
        chi2 += 1.0 * (values[i] - n) * (values[i] - n) / n;
    }
    double mean = sum / range;
    double var = sumsq / range - sum / range * sum / range;
    double Sd = sqrt(var);
    double skewness = 0, kurtosis = 0;
    for (int i = 0; i < range; ++i) {
        double g = (double)values[i] - mean;
        double g2 = g * g;
        skewness += g2 * g;
        kurtosis += g2 * g2;
    }
    skewness /= range * var * Sd;
    kurtosis = kurtosis / range / var / var - 3;
    cout << "Range:    " << minv << " .. " << maxv << endl;
    cout << "Mean:     " << fixed << setprecision(4) << mean << " (" << n << ")" << endl;
    cout << "Sd^2:     " << var << " (" << n << ")" << endl;
    cout << "Sd:       " << Sd << " (" << sqrt_n << ")" << endl;
    cout << "Sd/mean:  " << fixed << setprecision(4) << Sd / mean << " (" << sqrt_n / mean << ")"
         << endl;
    cout << "  -3*Sd:  " << fracInRange(values, range, 0, mean - 3 * Sd) * 100 << "% (0.1%)"
         << endl;
    cout << "-3-2*Sd:  " << fracInRange(values, range, mean - 3 * Sd, mean - 2 * Sd) * 100
         << "% (2.1%)" << endl;
    cout << "-2-1*Sd:  " << fracInRange(values, range, mean - 2 * Sd, mean - 1 * Sd) * 100
         << "% (13.6%)" << endl;
    cout << "-1-0*Sd:  " << fracInRange(values, range, mean - Sd, mean) * 100 << "% (34.1%)"
         << endl;
    cout << "+0-1*Sd:  " << fracInRange(values, range, mean, mean + 1 * Sd) * 100 << "% (34.1%)"
         << endl;
    cout << "+1-2*Sd:  " << fracInRange(values, range, mean + 1 * Sd, mean + 2 * Sd) * 100
         << "% (13.6%)" << endl;
    cout << "+2-3*Sd:  " << fracInRange(values, range, mean + 2 * Sd, mean + 3 * Sd) * 100
         << "% (2.1%)" << endl;
    cout << "+3- *Sd:  "
         << fracInRange(values, range, mean + 3 * Sd, numeric_limits<double>::infinity()) * 100
         << "% (0.1%)" << endl;
    cout << "Skew:     " << skewness << " (0.0)" << endl;
    cout << "Kurtosis: " << kurtosis << " (0.0)" << endl;

    boost::math::chi_squared dist(range - 1);
    const double lower_Sd = sqrt((range - 1) * var / quantile(complement(dist, alpha / 2)));
    const double upper_Sd = sqrt((range - 1) * var / quantile(dist, alpha / 2));

    cout << "________________________________________" << endl;
    cout << "Chi-square test for normal distribution:" << endl;
    cout << "Chi^2:   " << chi2 << " (" << range << ")" << endl;
    cout << "Sd lower: " << lower_Sd << endl;
    cout << "Sd upper: " << upper_Sd << endl;
    if (lower_Sd <= sqrt_n &&
        sqrt_n <= upper_Sd) {  // check whether the interval covers the expected
        cout << "Test passed with " << alpha << " significance (probability of failure)." << endl;
        return true;
    } else {
        cout << "Sample was not good enough for " << alpha
             << " significance (probability of failure)." << endl;
        return false;
    }
}

/** returns true if passed, otherwise false. */
static bool floating_point_test(int n, int range, int offset, double alpha)
{
    int values[range];
    for (int i = 0; i < range; ++i)
        values[i] = 0;
    RandomGenerator rand;

    rand.seed((uint32_t)time(nullptr));
    cout << "_________________________________________" << endl;
    cout << "Random FLOATING POINT number test" << endl;
    cout << "Generating " << (n * range) << " random numbers from a range of " << range << "... ";
    cout.flush();
    for (int i = 0; i < n * range; ++i) {
        int r = (int)floor(rand.uni(offset, offset + range)) - offset;
        if (r >= 0 && r < range)
            values[r]++;
        else {
            cout << "range check failed" << endl;
            exit(EXIT_FAILURE);
        }
    }
    cout << endl;
    return frequency_analysis(n, values, range, alpha);
}

/** returns true if passed, otherwise false. */
static bool integer_test(int n, int range, int offset, double alpha)
{
    int values[range];
    for (int i = 0; i < range; ++i)
        values[i] = 0;
    RandomGenerator rand;

    rand.seed((uint32_t)time(nullptr));

    cout << "_________________________________________" << endl;
    cout << "Random INTEGER test" << endl;
    cout << "Generating " << (n * range) << " random numbers from a range of " << range << "... ";
    cout.flush();
    for (int i = 0; i < n * range; ++i) {
        int r = rand.uni(offset, offset + range - 1) - offset;
        if (r >= 0 && r < range)
            values[r]++;
        else {
            cout << "range check failed" << endl;
            exit(EXIT_FAILURE);
        }
    }
    cout << endl;
    return frequency_analysis(n, values, range, alpha);
}

int main(const int, const char*[])
{
    const int n = 30000;
    const int range = 2000;
    const int offset = 5000;
    const double alpha = 0.01;  // level of significance (probability of failure)
    if (integer_test(n, range, offset, alpha)) {
        if (floating_point_test(n, range, offset, alpha)) {
            return 0;
        } else {
            return 1;
        }
    } else {
        return 1;
    }
}
