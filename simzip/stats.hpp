#pragma once

#include <ostream>
#include <sstream>
#include <vector>

namespace simzip {

    // Keep track of count, sum and sum of squares with optional
    // simple histogramming.
    struct Stats {
        bool sample{false};
        size_t S0{0};
        double S1{0}, S2{0};
        std::vector<double> samples; 

        double mean() const {
            if (S0 == 0) return 0.0;
            return S1/S0;
        }

        double rms() const {
            if (S0 <= 1) return -1.0;
            const double d = S2 - S1*S1/S0;
            // round off errors can give small negative
            if (d < 0) return 0.0;
            return sqrt(d/(S0-1));
        }

        void operator()(double val) {
            S0 += 1;
            S1 += val;
            S2 += val*val;
            if (sample) samples.push_back(val);
        }


        std::string dump() const {
            std::stringstream ss;
            ss << mean() << " +/- " << rms()
               << " [" << S0 << " " << S1 << " " << S2 << "]";
            return ss.str();
        }
    };

    std::ostream& operator<<(std::ostream& o, const Stats& s) {
        o << s.dump();
        return o;
    }
}

