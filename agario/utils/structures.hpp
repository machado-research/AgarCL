    // #include<iostream>



    // class Vector2 {
    //     public:
    //     // implement the Vector2 class here
    //     float x;
    //     float y;

    //     Vector2() : x(0.0f), y(0.0f) {}
    //     Vector2(float x, float y) : x(x), y(y) {}

    // };


    // class Border {


    // public:

    //     double minx;
    //     double miny;
    //     double maxx;
    //     double maxy;
    //     double width;
    //     double height;
    //     std::default_random_engine random_generator;

    //     Border(double minx, double miny, double maxx, double maxy, std::default_random_engine random_generator = std::default_random_engine())
    //         : minx(minx), miny(miny), maxx(maxx), maxy(maxy), width(maxx - minx), height(maxy - miny), random_generator(random_generator) {}

    //     std::string toString() const {
    //         return "[" + std::to_string(minx) + "," + std::to_string(miny) + "," + std::to_string(maxx) + "," + std::to_string(maxy) + "]";
    //     }

    //     bool contains(const Vector2& position) const {
    //         return position.x > minx && position.x < maxx && position.y > miny && position.y < maxy;
    //     }

    //     Vector2 sample() const {
    //         std::uniform_real_distribution<double> dist_x(minx, maxx);
    //         std::uniform_real_distribution<double> dist_y(miny, maxy);
    //         double x = dist_x(random_generator);
    //         double y = dist_y(random_generator);
    //         return Vector2(x, y);
    //     }

    //     Border getJoint(const Border& border) const {
    //         double new_minx = std::max(minx, border.minx);
    //         double new_maxx = std::min(maxx, border.maxx);
    //         double new_miny = std::max(miny, border.miny);
    //         double new_maxy = std::min(maxy, border.maxy);
    //         if (new_minx > new_maxx || new_miny > new_maxy) {
    //             return Border(0, 0, 0, 0);
    //         }
    //         return Border(new_minx, new_miny, new_maxx, new_maxy, random_generator);
    //     }
    // };
