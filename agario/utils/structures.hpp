// #include<iostream>
// #include<random>
// #include"agario/core/Entities.hpp"
// #include"agario/core/types.hpp"

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

//     Border(double minx, double miny, double maxx, double maxy)
//         : minx(minx), miny(miny), maxx(maxx), maxy(maxy), width(maxx - minx), height(maxy - miny) {}

//     std::string toString() const {
//         return "[" + std::to_string(minx) + "," + std::to_string(miny) + "," + std::to_string(maxx) + "," + std::to_string(maxy) + "]";
//     }

//     bool contains(const Vector2& position) const {
//         return position.x > minx && position.x < maxx && position.y > miny && position.y < maxy;
//     }

//     Border getJoint(const Border& border) const {
//         double new_minx = std::max(minx, border.minx);
//         double new_maxx = std::min(maxx, border.maxx);
//         double new_miny = std::max(miny, border.miny);
//         double new_maxy = std::min(maxy, border.maxy);
//         if (new_minx > new_maxx || new_miny > new_maxy) {
//             return Border(0, 0, 0, 0);
//         }
//         return Border(new_minx, new_miny, new_maxx, new_maxy);
//     }
// };

// namespace agario{
//     <bool renderable, unsigned NumSides>
//     class QuadNode {
//     public:
//         Border border;
//         int max_depth;
//         double midx;
//         double midy;
//         int max_num;
//         std::vector<QuadNode<<bool renderable, unsigned NumSides>>> children; // Changed to value semantics
//         std::vector<<bool renderable, unsigned NumSides>> items;
//         // QuadNode() : border(0, 0, 0, 0), max_depth(32), midx(0), midy(0), max_num(64) {}
//         QuadNode(const Border& border, int max_depth = 32, int max_num = 64)
//             : border(border), max_depth(max_depth), max_num(max_num) {
//             midx = (border.minx + border.maxx) / 2;
//             midy = (border.miny + border.maxy) / 2;
//             children.clear();
//         }

//         int getQuad(const Vector2& position) const {
//             if (position.x < midx) {
//                 return (position.y < midy) ? 0 : 1;
//             } else {
//                 return (position.y < midy) ? 2 : 3;
//             }
//         }

//         void insert(agario::Pellet<renderable>& node) {
//             if (!children.empty()) {
//                 children[getQuad(node.position)].insert(node);
//             } else {
//                 items.push_back(node);
//                 node.quad_node = this; // Assuming agario::Pellet has a quad_node member
//                 if (items.size() > max_num && max_depth >= 1) {
//                     Border b0(border.minx, border.miny, midx, midy);
//                     Border b1(border.minx, midy, midx, border.maxy);
//                     Border b2(midx, border.miny, border.maxx, midy);
//                     Border b3(midx, midy, border.maxx, border.maxy);
//                     children.emplace_back(b0, max_depth - 1, max_num);
//                     children.emplace_back(b1, max_depth - 1, max_num);
//                     children.emplace_back(b2, max_depth - 1, max_num);
//                     children.emplace_back(b3, max_depth - 1, max_num);
//                     for (auto& item : items) {
//                         children[getQuad(item.position)].insert(item);
//                     }
//                     items.clear();
//                 }
//             }
//         }

//         std::vector<agario::Pellet<renderable>> find(const Border& search_border) {
//             std::vector<agario::Pellet<renderable>> ans(items);
//             if (!children.empty()) {
//                 for (auto& child : children) {
//                     Border tmpBorder = search_border.getJoint(child.border);
//                     if (tmpBorder.minx < tmpBorder.maxx && tmpBorder.miny < tmpBorder.maxy) {
//                         auto child_items = child.find(tmpBorder);
//                         ans.insert(ans.end(), child_items.begin(), child_items.end());
//                     }
//                 }
//             }
//             return ans;
//         }

//         void clear() {
//             if (children.empty()) return;
//             int remaining_num = max_num;
//             for (auto& child : children) {
//                 if (!child.children.empty()) return;
//                 remaining_num -= child.items.size();
//             }
//             if (remaining_num >= 0) {
//                 for (auto& child : children) {
//                     for (auto& item : child.items) {
//                         item.quad_node = this; // Assuming agario::Pellet has a quad_node member
//                         items.push_back(item);
//                     }
//                 }
//                 children.clear();
//             }
//         }

//         void remove(agario::Pellet<renderable>& node) {
//             auto it = std::remove_if(items.begin(), items.end(), [&](const agario::Pellet<renderable>& item) {
//                 return item.ball_id == node.ball_id; // Assuming agario::Pellet has a ball_id member
//             });
//             items.erase(it, items.end());
//             delete node.quad_node;  // Assuming agario::Pellet has a quad_node member
//         }
//     };
// } // namespace agario
