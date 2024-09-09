#include<iostream>
#include<unordered_map>
#include "structures.hpp"


namespace agario {


    class BaseCollisionDetection {
    public:
        BaseCollisionDetection(Border border) : border(border) {}

        virtual void solve(std::vector<Query>& query_list, std::vector<Gallery>& gallery_list) = 0;

    protected:
        Border border;
    };


    class PrecisionCollisionDetection : public BaseCollisionDetection {
    public:
        PrecisionCollisionDetection(Border border, int precision) : BaseCollisionDetection(border), precision(precision) {}

        int get_row(float x) {
            return static_cast<int>((x - border.minx) / border.height * precision);
        }

        void solve(std::vector<std::pair<agario::pid, std::vector<Cell>>>& query_list, std::vector<std::pair<agario::pid, std::vector<Cell>>>& gallery_list) override {
            std::unordered_map<int, std::vector<std::pair<int, float>>> vec;
            for (int id = 0; id < gallery_list.size(); id++) {
                const auto& node = gallery_list[id];
                int row_id = get_row(node.position.x);
                vec[row_id].push_back(std::make_pair(id, node.position.y));
            }
            for (auto& val : vec) {
                std::sort(val.second.begin(), val.second.end(), [](const auto& a, const auto& b) {
                    return a.second < b.second;
                });
            }
            std::unordered_map<int, std::vector<std::pair<agario::pid, std::vector<Cell>>>> results;
            for (int id = 0; id < query_list.size(); id++) {
                const auto& query = query_list[id];
                results[id] = {};
                float left = query.position.y - query.radius;
                float right = query.position.y + query.radius;
                int top = get_row(query.position.x - query.radius);
                int bottom = get_row(query.position.x + query.radius);
                for (int i = top; i <= bottom; i++) {
                    if (vec.find(i) == vec.end()) continue;
                    int l = vec[i].size();
                    int start_pos = 0;
                    for (int j = 15; j >= 0; j--) {
                        if (start_pos + (1 << j) < l && vec[i][start_pos + (1 << j)].second < left) {
                            start_pos += (1 << j);
                        }
                    }
                    for (int j = start_pos; j < l; j++) {
                        if (vec[i][j].second > right) break;
                        if (query.touches_with_margin(gallery_list[vec[i][j].first])) {
                            results[id].push_back(gallery_list[vec[i][j].first]);
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
