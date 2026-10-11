struct Point {
    int x;
    int y;
};

int square(int n) {
    return n * n;
}

int main() {
    int points[3];
    for (int i = 0; i < 3; ++i) {
        points.push_back({i, i * i});
    }
    int total = 0;
    for (const Point& p : points) {
        total += square(p.x) + p.y;
    }
    return 0;
}