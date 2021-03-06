/// \file
/// \brief A library containing classes to perform various types of search algorithms

#include <algorithm>
#include <cmath>
#include <iostream>
#include <memory>
#include <numeric>
#include <unordered_map>
#include <vector>

#include "global_search/heuristic_search.hpp"
#include "rigid2d/rigid2d.hpp"
#include "roadmap/collision.hpp"
#include "roadmap/prm.hpp"
#include "roadmap/grid.hpp"

namespace hsearch
{

  bool Key::operator<(const Key &rhs) const
  {
    if(rigid2d::almost_equal(k1, rhs.k1)) return k2 < rhs.k2;
    else return k1 < rhs.k1;
  }

  bool Key::operator>(const Key &rhs) const
  {
    if(rigid2d::almost_equal(k1, rhs.k1)) return k2 > rhs.k2;
    else return k1 > rhs.k1;
  }

  SearchNode::SearchNode(const prm::Node & n)
  {
    node_p = std::make_shared<prm::Node>(n);
  }

  void SearchNode::CalcKey(double km)
  {
    auto buf = std::min(g_val, rhs_val);
    key_val.k1 = buf + h_val + km;
    key_val.k2 = buf;
  }

  bool SearchNode::operator>(const SearchNode &rhs) const
  {
    if(rigid2d::almost_equal(key_val.k1, rhs.key_val.k1)) return key_val.k2 > rhs.key_val.k2;
    else return key_val.k1 > rhs.key_val.k1;
  }

  bool SearchNode::operator<(const SearchNode &rhs) const
  {
    if(rigid2d::almost_equal(key_val.k1, rhs.key_val.k1)) return key_val.k2 < rhs.key_val.k2;
    else return key_val.k1 < rhs.key_val.k1;
  }

  std::ostream & operator<<(std::ostream & os, const Key & k)
  {
    os << "Key: " << k.k1 << ", " << k.k2 << std::endl;
    return os;
  }

  std::ostream & operator<<(std::ostream & os, const SearchNode & n)
  {

    if(n.parent_p != nullptr) os << "Node ID: " << n.node_p->id << "\n\tPoint: " << n.node_p->point.x << ", " << n.node_p->point.y << "\n\t" << "G Val: " << n.g_val << "\n\t" << "RHS Val: " << n.rhs_val <<  "\n\t" << "Parent ID: " << n.parent_p->id << std::endl;
    else os << "Node ID: " << n.node_p->id << "\n\tPoint: " << n.node_p->point.x << ", " << n.node_p->point.y << "\n\t" << "G Val: " << n.g_val << "\n\t" << "RHS Val: " << n.rhs_val << std::endl;

    return os;
  }

  // =========================== HSearch =======================================

  HSearch::HSearch(std::vector<prm::Node>* node_list)
  {
    created_graph_p = node_list;
  }

  bool HSearch::ComputeShortestPath(const prm::Node & s_start, const prm::Node & s_goal)
  {

    goal_loc = s_goal.point;

    // Initialize the start node
    start = SearchNode(s_start);

    start.search_id = 0;

    start.state = Open;
    start.g_val = 0;
    start.h_val = h(start);
    start.CalcKey();

    start.parent_p = nullptr;

    open_list.push_back(start);
    std::make_heap(open_list.begin(), open_list.end(), std::greater<>{});

    while(open_list.size() != 0)
    {
      if(!std::is_heap(open_list.begin(), open_list.end(), std::greater<>{}))  std::make_heap(open_list.begin(), open_list.end(), std::greater<>{});

      // std::cout << "open_list size: " << open_list.size() << std::endl;
      // Get the node with the minimum total cost
      std::pop_heap(open_list.begin(), open_list.end(), std::greater<>{});
      auto cur_s = open_list.back();
      open_list.pop_back();

      // std::cout << "Current Node Cost: " << cur_s.f_val << std::endl;
      // std::cout << "End Node Cost: " << open_list.back().f_val << std::endl;
      // std::cout << "Current Node: " << cur_s.node_p->point << "\n";

      // check if cur_s is the goal
      if (cur_s.node_p->point == goal_loc)
      {
          assemble_path(cur_s);
          return true;
      }

      // Add current node to the closed list
      closed_list.push_back(cur_s);
      cur_s.state = Closed;

      // Expand the search to the neighbors of the current node
      for(auto node_id : cur_s.node_p->id_set)
      {

        // std::cout << "s' ID: " << node_id << "\n";

        auto result = std::find_if(closed_list.begin(), closed_list.end(), [node_id](SearchNode n) {return n.node_p->id == node_id;});

        // std::cout << "On closed list? " << result - closed_list.begin() << "\n";

        // Check if the node is not on the closed list and try to update/create it
        if(result == closed_list.end())
        {

          result = std::find_if(open_list.begin(), open_list.end(), [node_id](SearchNode n) {return n.node_p->id == node_id;});

          // std::cout << "On open list? " << result - open_list.begin() << "\n";

          SearchNode neighbor;

          // check if the node is not on the open list and create it
          if(result == open_list.end())
          {
            SearchNode buf(created_graph_p->at(node_id));
            buf.search_id = id_cnt;
            id_cnt++;
            neighbor = buf;
          }
          else // otherwise use the
          {
            neighbor = *result;
          }

          // calculate the cost and update the cost/parent if needed
          ComputeCost(cur_s, neighbor);

          if (neighbor.state == New) // add node to the heap
          {
            neighbor.state = Open;
            open_list.push_back(neighbor);
          }
          else // update the node already in the heap
          {
            open_list.at(std::distance(open_list.begin(),result)) = neighbor;
          }
          // std::cout << neighbor;
        }
        // Update the heap order
        push_heap(open_list.begin(), open_list.end(), std::greater<>{});
      }
    }
    return false;
  }

  void HSearch::assemble_path(SearchNode goal)
  {
    // add the goal to the path
    final_path.push_back(goal.node_p->point);

    auto cur_node = goal;

    // follow the parent points back to the starting node and store each location
    while (cur_node.parent_p != nullptr)
    {
      final_path.push_back(cur_node.parent_p->point);

      auto next_id = cur_node.parent_p->id;

      // search for the parent on the closed list
      auto result = std::find_if(closed_list.begin(), closed_list.end(), [next_id](SearchNode n) {return n.node_p->id == next_id;});

      if(result != closed_list.end())
      {
        cur_node = *result;
      }
      else
      {
        // search for the parent on the open list
        result = std::find_if(open_list.begin(), open_list.end(), [next_id](SearchNode n) {return n.node_p->id == next_id;});
        cur_node = *result;
      }
    }
  }

  std::vector<rigid2d::Vector2D> HSearch::get_path()
  {
    return final_path;
  }

  std::vector<rigid2d::Vector2D> HSearch::get_expanded_nodes()
  {
    return expanded_nodes;
  }

  std::vector<double> HSearch::f(SearchNode s, SearchNode sp)
  {
    double buf_h = h(sp);
    double buf_g = g(s, sp);
    double buf_f = buf_g + buf_h;

    return {buf_f, buf_g, buf_h};
  }

  double HSearch::g(SearchNode s, SearchNode sp)
  {
    return (s.g_val + sp.node_p->point.distance(s.node_p->point));
  }

  double HSearch::h(SearchNode sp)
  {
    rigid2d::Vector2D pt1 = sp.node_p->point;

    return pt1.distance(goal_loc);
  }

  // =========================== A* ============================================

  void AStar::ComputeCost(SearchNode &s, SearchNode &sp)
  {
    auto cost = f(s, sp);
    // If the path from s to s' is cheaper than the existing one, update it.
    if(cost.at(0) < sp.key_val.k1)
    {
      sp.g_val = cost.at(1);
      sp.h_val = cost.at(2);

      sp.CalcKey(); // update the key values

      sp.parent_p = s.node_p;
    }
  }

  // =========================== Theta* ========================================

  ThetaStar::ThetaStar(std::vector<prm::Node> * node_list, grid::Map map, double buffer) : HSearch(node_list)
  {
    known_map = map;
    buffer_radius = buffer;
  }

  void ThetaStar::ComputeCost(SearchNode &s, SearchNode &sp)
  {
    std::vector<double> cost;

    bool collision = true;

    // Check for the start node
    if(s.parent_p != nullptr)
    {
      for(auto obstacle : known_map.obstacles)
      {
        collision = collision::line_shape_intersection(s.parent_p->point, sp.node_p->point, obstacle, buffer_radius);
        if(collision) break;
      }
    }

    if(!collision) // there is line of sight, so evaluate path 2
    {

      // find the parent node
      auto par_id = s.parent_p->id;
      auto result = std::find_if(closed_list.begin(), closed_list.end(), [par_id](SearchNode n) {return n.node_p->id == par_id;});

      if(result == closed_list.end())
      {
        result = std::find_if(open_list.begin(), open_list.end(), [par_id](SearchNode n) {return n.node_p->id == par_id;});
      }

      cost = f(*result, sp);

      // If the path from par(s) to s' is cheaper than the existing one, update it.
      if(cost.at(0) < sp.key_val.k1)
      {
        sp.g_val = cost.at(1);
        sp.h_val = cost.at(2);

        sp.CalcKey(); // update the key values

        sp.parent_p = s.parent_p;
      }
    }
    else // use path 1
    {
      cost = f(s, sp);

      // If the path from s to s' is cheaper than the existing one, update it.
      if(cost.at(0) < sp.key_val.k1)
      {
        sp.g_val = cost.at(1);
        sp.h_val = cost.at(2);

        sp.CalcKey(); // update the key values

        sp.parent_p = s.node_p;
      }
    }

  }

  // =========================== LPA* ==========================================

  LPAStar::LPAStar(std::vector<std::vector<prm::Node>>* grid_graph, grid::Grid* base_grid, rigid2d::Vector2D start_loc, rigid2d::Vector2D goal_loc) : HSearch()
  {
    // populate class attributes
    created_graph_p = grid_graph;
    known_grid_p = base_grid;

    this->goal_loc = known_grid_p->grid_to_world(goal_loc);

    // Use the provided grid_world to create a graph of the cell centers and initialize a grid of SearchNodes
    // Loop through each cell in the grid and create an unordered map of SearchNodes.
    auto grid_dims = base_grid->get_grid_dimensions();

    for(int i = 0 ; i < grid_dims.at(1); i++) // integer y-coord
    {
      for(int j = 0 ; j < grid_dims.at(0); j++) //integer x-coord
      {
        SearchNode s(created_graph_p->at(i).at(j));
        s.search_id = s.node_p->id;
        s.h_val = h(s);
        standby.insert({s.search_id, s});
      }
    }

    // Get start Node ID
    start_id = created_graph_p->at(start_loc.y).at(start_loc.x).id;

    // Get goal Node ID
    goal_id = created_graph_p->at(goal_loc.y).at(goal_loc.x).id;

    // Get the start node from the standby list
    start = standby.at(start_id);
    standby.erase(start_id);

    start.rhs_val = 0;
    start.CalcKey(km);
    start.state = Open;

    // Add start node to the open list
    open_list.push_back(start);
    std::make_heap(open_list.begin(), open_list.end(), std::greater<>{});
  }

  bool LPAStar::ComputeShortestPath()
  {
    bool result = false;

    expanded_nodes.clear();
    std::cout << "\n\nSTARTING NEW SEARCH =========================================== \n =============================================================== \n";
    std::cout << "Start ID: " << start_id << "\n";
    std::cout << "Goal ID: " << goal_id << "\n";

    while(open_list.size() != 0)
    {

      if(!std::is_heap(open_list.begin(), open_list.end(), std::greater<>{}))
      {
        std::make_heap(open_list.begin(), open_list.end(), std::greater<>{});
      }

      // Get the node at the top of the open list
      auto cur_s = open_list.at(0);

      bool cond1 = cur_s.key_val > get_goal_key();
      bool cond2 = goal_is_consistent();

      // std::cout << "CSP: Open List Size: " << open_list.size() << "\n";
      // std::cout << "Picked Node " << cur_s.search_id << " off the open list \n";

      // std::cout << cur_s;
      // std::cout << cur_s.key_val;
      // std::cout << get_goal_key();
      // std::cout << "K Condition:\t" << cond1 << std::endl;
      // std::cout << "G Condition:\t" << cond2 << std::endl;

      // Check the exit condition
      if(cond1 && cond2)
      {
        auto g_fin = locate_node(goal_id);
        assemble_path(*g_fin);
        result = true;
        break;
      }

      //Remove the node from the open list and put it on standby
      std::pop_heap(open_list.begin(), open_list.end(), std::greater<>{});
      open_list.back();
      open_list.pop_back();

      cur_s.state = Closed;
      standby.insert({cur_s.search_id, cur_s});

      auto k_old = cur_s.key_val;
      cur_s.h_val = h(cur_s);
      cur_s.CalcKey(km);

      if(k_old < cur_s.key_val)
      {
        cur_s.state = Open;
        open_list.push_back(cur_s);
        standby.erase(cur_s.search_id);
        
        push_heap(open_list.begin(), open_list.end(), std::greater<>{});
      }
      else if(cur_s.g_val > cur_s.rhs_val)
      {
        standby.at(cur_s.search_id).g_val = standby.at(cur_s.search_id).rhs_val;

        // loop through neighbors
        for(const auto & sp_id : cur_s.node_p->id_set)
        {
          UpdateVertex(sp_id);
        }
      }
      else
      {
        standby.at(cur_s.search_id).g_val = BIG_NUM;

        // loop through neighbors and self
        for(const auto & sp_id : cur_s.node_p->id_set)
        {
          UpdateVertex(sp_id);
        }

        UpdateVertex(cur_s.search_id);
      }
    }
    return result;
  }

  bool LPAStar::MapChange(std::vector<std::pair<rigid2d::Vector2D, signed char>> points)
  {
    // make the updates to the occupancy data to effect the edge cost calculation
    auto updates_made = known_grid_p->update_grid(points);

    // determine if any changes were made
    auto tot_chng = std::accumulate(updates_made.begin(), updates_made.end(), 0);

    // std::cout << "Sum: " << tot_chng << "====================================" << std::endl;

    bool changed = false;

    if(tot_chng > 0)
    {
      changed = true;

      expanded_nodes.clear();

      for(auto it = updates_made.begin(); it < updates_made.end(); it++)
      {
        int i = std::distance(updates_made.begin(), it);

        if(*it == 1)
        {
          const auto point = points.at(i);

          // std::cout << "Updating Nodes around Node: " << created_graph_p->at(point.first.y).at(point.first.x).id;
          // std::cout << "\tGrid Coords: " << known_grid_p->world_to_grid(created_graph_p->at(point.first.y).at(point.first.x).point) << std::endl;

          // Loop through all of the neighbors of the cell and find the new best connection given the map update
          for(const auto& v_id : created_graph_p->at(point.first.y).at(point.first.x).id_set)
          {
            UpdateVertex(v_id);
          }
          // std::cout << "MU: Open List Size: " << open_list.size() << "\n";
          // std::cout << "MU: Expd List Size: " << expanded_nodes.size() << "\n";
        }
      }
    }
    return changed;
  }

  void LPAStar::assemble_path(SearchNode goal)
  {

    final_path.clear();

    // add the goal to the path
    final_path.push_back(goal.node_p->point);

    auto cur_node = goal;
    std::cout << "Assembling Path...\n";

    // follow the parent points back to the starting node and store each location
    // Select the neighbor node with the minimum cost
    while (cur_node.parent_p != nullptr)
    {
      // std::cout << "Scanning neighbors =========================================\n";
      // std::cout << cur_node;

      cur_node.rhs_val = BIG_NUM;

      for(const auto n_id : cur_node.node_p->id_set)
      {
        // std::cout << "\n\tLooking at Neighbor" << n_id << "...";
        auto neighbor = *locate_node(n_id);
        ComputeCost(neighbor, cur_node);
      }

      // std::cout << "\nAfter Update: \n";
      // std::cout << cur_node;

      final_path.push_back(cur_node.parent_p->point);

      auto next_id = cur_node.parent_p->id;

      cur_node = *locate_node(next_id);
    }
  }

  void LPAStar::UpdateVertex(int u_id)
  {
    // First get a pointer to the node in one of the lists
    auto u = locate_node(u_id);

    expanded_nodes.push_back(u->node_p->point);

    // std::cout << "Evaluating Node " << u_id << "================= \n";
    // std::cout << *u;
    // std::cout << "Old Key: " << u->key_val;

    // Scan the predecessors of u and set the min cost to the rhs val

    if(u_id != start_id)
    {
      u->rhs_val = BIG_NUM; //Ensures the following for loop with set the rhs to min given the most current info

      for(const auto sp_id : u->node_p->id_set)
      {
        // std::cout << "\n\tLooking at Neighbor" << sp_id << "...";
        auto sp = locate_node(sp_id);

        ComputeCost(*sp, *u);
      }

      // If rhs is INF set parent to null?

      u->h_val = h(*u);
      u->CalcKey(km);
    }

    // std::cout << "\nAfter Update: \n";
    // std::cout << *u;
    // std::cout << "New Key: " << u->key_val;

    // Check for consistency,
    if(is_consistent(*u))
    {
      // make sure the node is not on the open list, if it is remove it and place it in standby
      auto it_vec = std::find_if(open_list.begin(), open_list.end(), [u_id](SearchNode n) {return n.search_id == u_id;});
      if(it_vec != open_list.end())
      {
        u->state = Closed;
        standby.insert({u->search_id, *u});
        open_list.erase(it_vec); // this line deletes the node the pointer u, is pointing to. u is now no longer useable after this line
        push_heap(open_list.begin(), open_list.end(), std::greater<>{});
      }
    }
    else // the node is not consistent, and should be placed on the open list, if not already there
    {
      auto it_map = standby.find(u_id);
      if(it_map != standby.end())
      {
        u->state = Open;
        open_list.push_back(*u);
        standby.erase(u->search_id); // this line deletes the node the pointer u, is pointing to. u is now no longer useable after this line
      }
    }

    push_heap(open_list.begin(), open_list.end(), std::greater<>{});
  }

  void LPAStar::ComputeCost(SearchNode &sp, SearchNode &u)
  {
    double buf = sp.g_val + edge_cost(sp, u);

    // std::cout << "G: " << sp.g_val << " Cost: " << edge_cost(sp, u) << " ...";

    if(buf < u.rhs_val)
    {
      // std::cout << "Updated RHS";
      u.rhs_val = buf;
      u.parent_p = sp.node_p;
    }
  }

  double LPAStar::edge_cost(SearchNode &sp, SearchNode &u)
  {

    // convert the world coords stored in each node to grid coords

    auto sp_grid_pt = known_grid_p->world_to_grid(sp.node_p->point);
    auto u_grid_pt = known_grid_p->world_to_grid(u.node_p->point);

    // retrieve the occupancy data

    auto sp_occ = known_grid_p->get_grid().at(sp_grid_pt.y).at(sp_grid_pt.x);
    auto u_occ = known_grid_p->get_grid().at(u_grid_pt.y).at(u_grid_pt.x);

    // std::cout << "Edge from " << sp_grid_pt.x << ", " << sp_grid_pt.y << " to "  << u_grid_pt.x << ", " << u_grid_pt.y << ": " << int(sp_occ) << ", " << int(u_occ) << std::endl;

    // caculate the cost
    if(sp_occ == 0 && u_occ == 0) return sp.node_p->point.distance(u.node_p->point);
    else return BIG_NUM;
  }

  bool LPAStar::goal_is_consistent()
  {
    return is_consistent( *locate_node(goal_id) );
  }

  bool LPAStar::is_consistent(SearchNode u) const
  {
    // if(u.g_val == BIG_NUM && u.rhs_val == BIG_NUM) return true;
    return rigid2d::almost_equal(u.g_val, u.rhs_val);
  }

  SearchNode* LPAStar::locate_node(int u_id)
  {
    auto it_map = standby.find(u_id);

    if(it_map == standby.end())
    {
      auto it_vec = std::find_if(open_list.begin(), open_list.end(), [u_id](SearchNode n) {return n.search_id == u_id;});
      return &(*it_vec);
    }
    else
    {
      return &(it_map->second);
    }
  }

  Key LPAStar::get_goal_key()
  {
    SearchNode* g = locate_node(goal_id);

    g->h_val = h(*g);
    g->CalcKey(km);

    return g->key_val;
  }

  // =========================== D* Lite =======================================

  DStarLite::DStarLite(std::vector<std::vector<prm::Node>>* grid_graph, grid::Grid* base_grid, rigid2d::Vector2D start_loc, rigid2d::Vector2D goal_loc) : LPAStar(grid_graph, base_grid, goal_loc, start_loc)
  { }

  void DStarLite::UpdateRobotLoc(rigid2d::Vector2D robot_loc)
  {
    // update the robot location
    auto old_goal = *locate_node(goal_id);

    goal_id = created_graph_p->at(robot_loc.y).at(robot_loc.x).id;
    goal_loc = known_grid_p->grid_to_world(robot_loc);

    // update the stored km value
    km += h(old_goal); // calculate the heuristic between the old start position and the new one.
  }

}
