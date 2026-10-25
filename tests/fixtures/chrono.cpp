#include <bits/stdc++.h>
#include <thread>
using namespace std;
using namespace std::chrono;
using namespace std::chrono_literals;

int main() {
    int cases;
    cin >> cases;
    while (cases--) {
        int x, y;
        cin >> x >> y;
        milliseconds span(1000LL * x + 500 + y);
        seconds whole(x + y);
        cout << span.count() << ' ' << duration_cast<seconds>(span).count() << ' '
             << floor<seconds>(span).count() << ' ' << ceil<seconds>(span).count() << ' '
             << round<seconds>(span).count() << '\n';
        auto combined = span + whole;
        auto difference = whole - span;
        cout << combined << ' ' << difference << ' ' << (whole < span) << ' ' << (span >= whole)
             << ' ' << (span == whole) << '\n';
        cout << (span * 3).count() << ' ' << (4 * span).count() << ' ' << (span / 3).count() << ' '
             << (span % 7).count() << ' ' << (span / 2s) << ' ' << (span % 2s) << '\n';
        auto copy = span;
        auto& held = span;
        span += 2s;
        held -= 500ms;
        held *= 3;
        held /= 2;
        held %= 1700;
        cout << held << ' ' << copy << ' ' << (span++).count() << ' ' << (++span).count() << ' '
             << (span--).count() << ' ' << (--span).count() << '\n';
        cout << +span << ' ' << -span << ' ' << chrono::abs(span) << '\n';
        duration<long long, ratio<2, 3>> fractional(7LL * x + y);
        duration<long long, ratio<5, 7>> other(3LL * y - x);
        cout << fractional << ' ' << duration_cast<milliseconds>(fractional).count() << ' '
             << (fractional + other) << ' ' << (fractional < other) << '\n';
        vector<milliseconds> ordered{span, copy, 0ms, -1ms, 10000ms};
        sort(ordered.begin(), ordered.end());
        for (auto value : ordered)
            cout << value.count() << ' ';
        cout << '\n';

        time_point<system_clock, milliseconds> point(span);
        auto moved = point + whole;
        auto backwards = whole + point - 700ms;
        cout << point.time_since_epoch().count() << ' ' << moved.time_since_epoch().count() << ' '
             << (moved - backwards) << ' ' << (moved > point) << '\n';
        auto original = point;
        auto& retained = point;
        point += 3s;
        retained -= 750ms;
        point = backwards;
        cout << retained.time_since_epoch() << ' ' << original.time_since_epoch() << '\n';
        cout << time_point_cast<seconds>(point).time_since_epoch().count() << ' '
             << floor<seconds>(point).time_since_epoch().count() << ' '
             << ceil<seconds>(point).time_since_epoch().count() << ' '
             << round<seconds>(point).time_since_epoch().count() << '\n';
        auto stamp = system_clock::from_time_t(123456789LL + x);
        cout << duration_cast<seconds>(stamp.time_since_epoch()).count() << ' '
             << system_clock::to_time_t(stamp) << '\n';
    }
    cout << 17ns << ' ' << 1'234'567ns << ' ' << 23us << ' ' << 2ms << ' ' << 3s << ' ' << 4min
         << ' ' << 5h << ' ' << days{6} << '\n';
    cout << round<seconds>(-1500ms).count() << ' ' << round<seconds>(2500ms).count() << ' '
         << round<seconds>(-2500ms).count() << ' ' << round<seconds>(3500ms).count() << '\n';
    cout << seconds::zero().count() << ' ' << seconds::min().count() << ' '
         << seconds::max().count() << '\n';
    auto steadyStart = steady_clock::now();
    auto wallStart = system_clock::now();
    this_thread::sleep_for(20ms);
    auto elapsed = steady_clock::now() - steadyStart;
    cout << (elapsed >= 10ms) << ' ' << (system_clock::now() - wallStart >= 10ms) << ' '
         << steady_clock::is_steady << ' ' << system_clock::is_steady << '\n';
    auto epochSeconds = duration_cast<seconds>(wallStart.time_since_epoch()).count();
    cout << (epochSeconds > 1577836800LL && epochSeconds < 4102444800LL) << '\n';
    this_thread::sleep_for(-1ms);
}
