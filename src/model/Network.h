/* 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/* 
 * File:   Network.h
 * Author: Peter G. Jensen <root@petergjoel.dk>
 *
 * Created on July 17, 2019, 2:17 PM
 */

#ifndef NETWORK_H
#define NETWORK_H
#include "model/Router.h"
#include "model/RoutingTable.h"
#include "Query.h"
#include "filter.h"

#include <ptrie_map.h>
#include <vector>
#include <memory>
#include <functional>

namespace mpls2pda {
class Network {
public:
    Network(ptrie::map<Router*>&& mapping, std::vector<std::unique_ptr < Router>>&& routers);
    Network(const Network&) = default;
    Network(Network&&) = default;
    Network& operator=(const Network&) = default;
    Network& operator=(Network&&) = default;
    
    const Router* get_router(size_t id) const;
    std::unordered_set<Query::label_t> interfaces(filter_t& filter);
    std::unordered_set<Query::label_t> get_labels(uint64_t label, uint64_t mask);
    std::unordered_set<Query::label_t> ip_labels(filter_t& filter);
    
private:
    // NO TOUCHEE AFTER INIT!
    const ptrie::map<Router*> _mapping;
    const std::vector<std::unique_ptr<Router>>&& _routers;
    std::unordered_map<int64_t,std::vector<std::pair<const RoutingTable::entry_t*, const Router*>>> _label_map;
    
//    ptrie::map<size_t> _linkmap;
//    std::vector<std::pair<Interface*, Interface*>> _links;
};
}

#endif /* NETWORK_H */
