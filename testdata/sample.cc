#include <iostream>
#include <vector>

struct Point {
    int x;
    int y;
};

int square(int n) {
    return n * n;
}

int main() {
    std::vector<Point> points;
    for (int i = 0; i < 3; ++i) {
        points.push_back({i, i * i});
    }
    int total = 0;
    for (const Point& p : points) {
        total += square(p.x) + p.y;
    }
    std::cout << "total: " << total << std::endl;
    return 0;
}