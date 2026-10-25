#include <bits/stdc++.h>
using namespace std;

struct Score {
    string name;
    int points;
    bool operator<(const Score& other) const {
        return points < other.points;
    }
};

int change(Score value) {
    value.points += 100;
    return value.points;
}

int main() {
    int t;
    cin >> t;
    while (t--) {
        string word;
        cin >> word;
        string other = word;
        other[0] = 'x';
        Score score{word, (int)word.size()};
        auto clone = score;
        clone.points = 42;
        cout << change(score) << ' ' << score.points << ' ' << clone.points << '\n';
        cout << word << ' ' << other << ' ' << word.substr(1) << '\n';
        int sum = 0;
        for (char c : word)
            sum += c;
        cout << sum << ' ' << (word == other) << '\n';
        cout << (word != other) << ' ' << (word < other) << '\n';
        vector<Score> ranks{{word, 9}, {other, 1}, {word, 5}};
        sort(ranks.begin(), ranks.end());
        for (auto s : ranks)
            cout << s.points << ' ';
        cout << '\n';
    }
}
