#include<iostream>
#include<unordered_map>
// #include "structures.hpp"



namespace agario {
    template<bool renderable>
    class PrecisionCollisionDetection {
    public:
        typedef Cell<renderable> Cell;
        std::pair<float,float> border;
        PrecisionCollisionDetection(std::pair<float,float> border, int precision = 50) : border(border), precision(precision) {}

        int get_column(float x) {
            return static_cast<int>(x / border.second * precision);
        }

        std::unordered_map<int, std::vector<std::pair<agario::pid, Cell>>> solve(std::vector<std::pair<agario::pid, Cell>>& query_list, std::vector<std::pair<agario::pid, Cell>>& gallery_list) {
            std::unordered_map<int, std::vector<std::pair<int, float>>> vec;
            for (int id = 0; id < gallery_list.size(); id++) {
                const auto& node = gallery_list[id].second;
                int column_id = get_column(node.x);
                vec[column_id].emplace_back(std::make_pair(id, node.y));
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
                int top = get_column(left);
                int bottom = get_column(right);
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
                        if (vec[i][j].second > right || query_list[id].first == gallery_list[vec[i][j].first].first) break;
                        if (query.touches(gallery_list[vec[i][j].first].second) && query.can_eat(gallery_list[vec[i][j].first].second)) {
                            results[id].emplace_back(std::move(gallery_list[vec[i][j].first]));
                            // in this case, query can eat gallery_list[vec[i][j].first]cell
                            //get tje player of gallery_list
                            // eat_others(player, gallery_list[vec[i][j].first].second);
                        }
                    }

                }
            }
            return results;
        }

    private:
        int precision;
    };
}
