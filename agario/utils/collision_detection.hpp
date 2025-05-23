#include<iostream>
#include<unordered_map>
#include "structures.hpp"



namespace agario {


    template<bool renderable>
    class PrecisionCollisionDetection {
    public:
        typedef Cell<renderable> Cell;
        std::pair<float,float> border;
        PrecisionCollisionDetection(std::pair<float,float> border, int precision = 10) : border(border), precision(precision) {}

        int get_row(float x) {
            return static_cast<int>(x / border.first * precision);
        }

        std::unordered_map<int, std::vector<std::pair<agario::pid, Cell>>> solve(std::vector<std::pair<agario::pid, Cell>>& query_list, std::vector<std::pair<agario::pid, Cell>>& gallery_list) {
            std::unordered_map<int, std::vector<std::pair<int, float>>> vec;
            for (int id = 0; id < gallery_list.size(); id++) {
                const auto& node = gallery_list[id].second;
                int row_id = get_row(node.x);
                vec[row_id].emplace_back(std::make_pair(id, node.y));
            }
            for (auto& val : vec) {
                std::sort(val.second.begin(), val.second.end(), [](const auto& a, const auto& b) {
                    return a.second < b.second;
                });
            }

            std::unordered_map<int, std::vector<std::pair<agario::pid, Cell>>> results;
            for (int id = 0; id < query_list.size(); id++) {
                const auto& query = query_list[id].second; //cell
                float left = query.x - query.radius();
                float right = query.x + query.radius();
                int top = get_row(left);
                int bottom = get_row(right);
                for (int i = top; i <= bottom; i++) {
                    if (vec.find(i) == vec.end()) continue;
                    int l = vec[i].size();
                    int start_pos = 0;
                    for (int j = 10; j >= 0; j--) {
                        if (start_pos + (1 << j) < l && vec[i][start_pos + (1 << j)].second < left) {
                            start_pos += (1 << j);
                        }
                    }
                    for (int j = start_pos; j < l; j++) {
                        if (query_list[id].first == gallery_list[vec[i][j].first].first) break;
                        if (query.collides_with(gallery_list[vec[i][j].first].second) && query.can_eat(gallery_list[vec[i][j].first].second)) {
                            results[id].emplace_back(std::move(gallery_list[vec[i][j].first]));
                        }
                    }

                }
            }
            return results;
        }

    private:
        int precision;
    };
    // query_list: the cells of a player.
    // gallery_list : the pellets
    // template<bool renderable>
    // class RebuildQuadTreeCollisionDetection {
    //     public:
    //         RebuildQuadTreeCollisionDetection(Border border, int node_capacity = 64, int tree_depth = 32)
    //             : node_capacity(node_capacity), tree_depth(tree_depth), border(border) {}

    //         std::unordered_map<int, std::vector<agario::Pellet<renderable>>> solve(Player<renderable> &player, const std::vector<agario::Pellet<renderable>>& gallery_list) {
    //             auto &query_list = player.cells;
    //             QuadNode<renderable> quadTree(border, tree_depth, node_capacity);
    //             for (const auto& node : gallery_list) {
    //                 quadTree.insert(node);
    //             }

    //             std::unordered_map<int, std::vector<agario::Pellet<renderable>>> results;
    //             for (size_t i = 0; i < query_list.size(); ++i) {
    //                 results[i] = {};
    //                 auto query = query_list[i];
    //                 Border query_border(
    //                     std::max(query.position.x - query.radius(), border.minx),
    //                     std::max(query.position.y - query.radius(), border.miny),
    //                     std::min(query.position.x + query.radius(), border.maxx),
    //                     std::min(query.position.y + query.radius(), border.maxy)
    //                 );

    //                 auto quadTree_results = quadTree.find(query_border);
    //                 for (const auto& result : quadTree_results) {
    //                     if (query.can_eat(result) && query.collides_with(result)) {
    //                         results[i].push_back(result);
    //                     }
    //                 }
    //             }
    //             return results;
    //         }

    //     private:
    //         int node_capacity;
    //         int tree_depth;
    //         Border border;
    // };

    // template<typename NodeType, bool renderable>
    // class RemoveQuadTreeCollisionDetection {
    // public:
    //     RemoveQuadTreeCollisionDetection(Border border, int node_capacity = 64, int tree_depth = 32)
    //         : node_capacity(node_capacity), tree_depth(tree_depth), border(border) {
    //         quadTree = new QuadNode<renderable>(border, tree_depth, node_capacity);
    //     }

    //     ~RemoveQuadTreeCollisionDetection() {
    //         delete quadTree;
    //     }

    //     std::unordered_map<int, std::vector<agario::Pellet<renderable>>> solve(Player<renderable> &player, const std::vector<agario::Pellet<renderable>>& changed_node_list) {
    //         auto& query_list = player.cells;

    //         for (const auto& node : changed_node_list) {
    //             if (node.quad_node != nullptr) {
    //                 node.quad_node->remove(node);
    //             }
    //             if (!node.is_remove) {
    //                 quadTree->insert(node);
    //             }
    //         }

    //         std::unordered_map<int, std::vector<agario::Pellet<renderable>>> results;
    //         for (size_t i = 0; i < query_list.size(); ++i) {
    //             results[i] = {};
    //             const auto& query = query_list[i];
    //             Border query_border(
    //                 std::max(query.position.x - query.radius(), border.minx),
    //                 std::max(query.position.y - query.radius(), border.miny),
    //                 std::min(query.position.x + query.radius(), border.maxx),
    //                 std::min(query.position.y + query.radius(), border.maxy)
    //             );

    //             auto quadTree_results = quadTree->find(query_border);
    //             for (const auto& result : quadTree_results) {
    //                 if (query.judge_cover(result)) {
    //                     results[i].push_back(result);
    //                 }
    //             }
    //         }
    //         return results;
    //     }

    // private:
    //     int node_capacity;
    //     int tree_depth;
    //     Border border;
    //     QuadNode<renderable>* quadTree;
    // };;;

}
