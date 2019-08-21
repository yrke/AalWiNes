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
 *  Copyright Peter G. Jensen
 */

/* 
 * File:   Query.cpp
 * Author: Peter G. Jensen <root@petergjoel.dk>
 * 
 * Created on July 16, 2019, 5:38 PM
 */

#include "Query.h"


namespace mpls2pda {

    Query::Query(pdaaal::NFA<label_t>&& pre, pdaaal::NFA<label_t>&& path, pdaaal::NFA<label_t>&& post, int lf, mode_t mode)
    : _prestack(std::move(pre)), _poststack(std::move(post)), _path(std::move(path)), _link_failures(lf), _mode(mode)
    {
        
    }
    
    void Query::print_dot(std::ostream& out)
    {
        out << "// PRE\n";
        _prestack.to_dot(out);
        out << "// POST\n";
        _poststack.to_dot(out);
        out << "// PATH\n";
        _path.to_dot(out);
    }
    
    const Query::label_t Query::label_t::unused_mpls(MPLS, 0, std::numeric_limits<uint64_t>::max());
    const Query::label_t Query::label_t::unused_ip4(IP4, 0, 0);
    const Query::label_t Query::label_t::unused_ip6(IP6, 0, 0);

    const Query::label_t Query::label_t::any_mpls(ANYMPLS, std::numeric_limits<uint8_t>::max(), std::numeric_limits<uint64_t>::max());
    const Query::label_t Query::label_t::any_ip(ANYIP, std::numeric_limits<uint8_t>::max(), std::numeric_limits<uint64_t>::max());
    const Query::label_t Query::label_t::any_ip4(IP4, std::numeric_limits<uint8_t>::max(), std::numeric_limits<uint64_t>::max());
    const Query::label_t Query::label_t::any_ip6(IP6, std::numeric_limits<uint8_t>::max(), std::numeric_limits<uint64_t>::max());
}