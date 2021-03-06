//
// Created by Morten on 18-03-2020.
//

#define BOOST_TEST_MODULE SyntacticNetwork

#include <boost/test/unit_test.hpp>
#include <aalwines/model/Network.h>
#include <aalwines/query/QueryBuilder.h>
#include <pdaaal/SolverAdapter.h>
#include <fstream>
#include <aalwines/utils/outcome.h>
#include <aalwines/model/NetworkPDAFactory.h>
#include <pdaaal/Reducer.h>
#include <aalwines/synthesis/RouteConstruction.h>
#include <aalwines/engine/Moped.h>
#include <aalwines/utils/stopwatch.h>

using namespace aalwines;

Network construct_synthetic_network(size_t nesting = 1){
    size_t router_size = 5 * nesting;
    std::string router_name = "Router";
    std::vector<std::string> router_names;
    std::vector<std::unique_ptr<Router> > _routers;
    std::vector<const Interface*> _all_interfaces;
    Network::routermap_t _mapping;

    for(size_t i = 0; i < router_size; i++) {
        router_names.push_back(router_name + std::to_string(i));
    }

    size_t network_node = 0;
    bool nested = nesting > 1;
    bool fall_through = false;
    size_t last_nesting_begin = nesting * 5 - 5;

    std::vector<std::vector<std::string>> links;

    for(size_t i = 0; i < router_size; i++, network_node++) {
        router_name = router_names[i];
        _routers.emplace_back(std::make_unique<Router>(i));
        Router &router = *_routers.back().get();
        router.add_name(router_name);
        router.get_interface(_all_interfaces, "i" + router_names[i]);
        auto res = _mapping.insert(router_name.c_str(), router_name.length());
        _mapping.get_data(res.second) = &router;
        switch (network_node) {
            case 1:
                router.get_interface(_all_interfaces, router_names[i - 1]);
                router.get_interface(_all_interfaces, router_names[i + 2]);
                links.push_back({router_names[i - 1], router_names[i + 2]} );
                break;
            case 2:
                router.get_interface(_all_interfaces, router_names[i + 1]);
                router.get_interface(_all_interfaces, router_names[i + 2]);
                links.push_back({router_names[i + 1], router_names[i + 2]});
                if(nested){
                    router.get_interface(_all_interfaces, router_names[i + 6]);
                    links.back().push_back({router_names[i + 6]});
                } else {
                    router.get_interface(_all_interfaces, router_names[i - 2]);
                    links.back().push_back({router_names[i - 2]});
                }
                break;
            case 3:
                router.get_interface(_all_interfaces, router_names[i - 2]);
                router.get_interface(_all_interfaces, router_names[i + 1]);
                router.get_interface(_all_interfaces, router_names[i - 1]);
                links.push_back({router_names[i - 2], router_names[i + 1], router_names[i - 1]});
                if(i != 3) {
                    router.get_interface(_all_interfaces, router_names[i - 6]);
                    links.back().push_back({router_names[i - 6]});
                }
                break;
            case 4:
                router.get_interface(_all_interfaces, router_names[i - 2]);
                router.get_interface(_all_interfaces, router_names[i - 1]);
                links.push_back({router_names[i - 2], router_names[i - 1]});
                break;
            case 5:
                network_node = 0;   //Fall through
                if(i == last_nesting_begin){
                    nested = false;
                }
                fall_through = true;
            case 0:
                router.get_interface(_all_interfaces, router_names[i + 1]);
                links.push_back({router_names[i + 1]});
                if(nested){
                    router.get_interface(_all_interfaces, router_names[i + 5]);
                    links.back().push_back({router_names[i + 5]});
                } else {
                    router.get_interface(_all_interfaces, router_names[i + 2]);
                    links.back().push_back({router_names[i + 2]});
                }
                if(fall_through){
                    router.get_interface(_all_interfaces, router_names[i - 5]);
                    links.back().push_back({router_names[i - 5]});
                    fall_through = false;
                }
                break;
            default:
                throw base_error("Something went wrong in the construction");
        }
    }
    for (size_t i = 0; i < router_size; ++i) {
        for (const auto &other : links[i]) {
            auto res1 = _mapping.exists(router_names[i].c_str(), router_names[i].length());
            assert(res1.first);
            auto res2 = _mapping.exists(other.c_str(), other.length());
            if(!res2.first) continue;
            _mapping.get_data(res1.second)->find_interface(other)->make_pairing(_mapping.get_data(res2.second)->find_interface(router_names[i]));
        }
    }
    Router::add_null_router(_routers, _all_interfaces, _mapping); //Last router
    return Network(std::move(_mapping), std::move(_routers), std::move(_all_interfaces));
}

void performance_query(const std::string& query, Network& synthetic_network, Builder builder, std::ostream& trace_stream){
    //Adapt to existing query parser
    std::istringstream qstream(query);
    builder.do_parse(qstream);

    pdaaal::Moped moped;
    pdaaal::SolverAdapter solver;

    std::vector<Query::mode_t> modes{builder._result[0].approximation()};
    std::pair<size_t, size_t> reduction;
    std::vector<pdaaal::TypedPDA<Query::label_t>::tracestate_t> trace;
    builder._result[0].set_approximation(modes[0]);
    NetworkPDAFactory factory(builder._result[0], synthetic_network, false);
    auto pda = factory.compile();
    reduction = pdaaal::Reducer::reduce(pda, 0, pda.initial(), pda.terminal());

    std::stringstream results;
    stopwatch verification_time_post(false);

    trace_stream << std::endl << "Post*: " <<std::endl;

    verification_time_post.start();
    auto solver_result1 = solver.post_star<pdaaal::Trace_Type::Any>(pda);
    verification_time_post.stop();
    trace = solver.get_trace(pda, std::move(solver_result1.second));
    factory.write_json_trace(trace_stream, trace);

    results << std::endl << "post*-time: " << verification_time_post.duration() << std::endl;

    trace_stream << std::endl << "Moped: " << std::endl;
    moped.verify(pda, false);
    trace = moped.get_trace(pda);
    factory.write_json_trace(trace_stream, trace);

    results << std::endl << "moped-time: " << moped.verification_duration() << std::endl;

    BOOST_TEST_MESSAGE(results.str());
}

void build_query(const std::string& query, Network& synthetic_network, Builder builder){
    //Adapt to existing query parser
    std::istringstream qstream(query);
    builder.do_parse(qstream);
    size_t query_no = 0;

    pdaaal::SolverAdapter solver;
    for(auto& q : builder._result) {
        query_no++;
        std::vector<Query::mode_t> modes{q.approximation()};

        bool was_dual = q.approximation() == Query::DUAL;
        if (was_dual)
            modes = std::vector<Query::mode_t>{Query::OVER, Query::UNDER};
        std::pair<size_t, size_t> reduction;
        utils::outcome_t result = utils::MAYBE;
        std::vector<pdaaal::TypedPDA<Query::label_t>::tracestate_t> trace;
        std::stringstream proof;

        size_t tos = 0;
        bool no_ip_swap = false;
        bool get_trace = true;

        for (auto m : modes) {
            q.set_approximation(m);
            NetworkPDAFactory factory(q, synthetic_network, no_ip_swap);
            auto pda = factory.compile();
            reduction = pdaaal::Reducer::reduce(pda, tos, pda.initial(), pda.terminal());
            bool need_trace = was_dual || get_trace;

            auto solver_result1 = solver.post_star<pdaaal::Trace_Type::Any>(pda);
            bool engine_outcome = solver_result1.first;
            if (need_trace && engine_outcome) {
                trace = solver.get_trace(pda, std::move(solver_result1.second));
                if (factory.write_json_trace(proof, trace))
                    result = utils::YES;
            }
            if (q.number_of_failures() == 0)
                result = engine_outcome ? utils::YES : utils::NO;

            if (result == utils::MAYBE && m == Query::OVER && !engine_outcome)
                result = utils::NO;
            if (result != utils::MAYBE)
                break;
        }
        std::cout << "\t\"Q" << query_no << "\" : {\n\t\t\"result\":";
        BOOST_CHECK_EQUAL(result, utils::YES);
        switch (result) {
            case utils::MAYBE:
                std::cout << "null";
                break;
            case utils::NO:
                std::cout << "false";
                break;
            case utils::YES:
                std::cout << "true";
                break;
        }
        std::cout << ",\n";
        std::cout << "\t\t\"reduction\":[" << reduction.first << ", " << reduction.second << "]";
        if (get_trace && result == utils::YES) {
            std::cout << ",\n\t\t\"trace\":[\n";
            std::cout << proof.str();
            std::cout << "\n\t\t]";
        }
        std::cout << "\n\t}";
        if (query_no != builder._result.size())
            std::cout << ",";
        std::cout << "\n";
        std::cout << "\n}}" << std::endl;
    }
}

BOOST_AUTO_TEST_CASE(NetworkInjectionAndTrace) {
    Network synthetic_network = construct_synthetic_network(2);

    std::vector<const Router*> path {synthetic_network.get_router(0),
                                     synthetic_network.get_router(5),
                                     synthetic_network.get_router(7),
                                     synthetic_network.get_router(9)};
    uint64_t i = 42;
    auto next_label = [&i](){return Query::label_t(Query::type_t::MPLS, 0, i++);};
    auto success = RouteConstruction::make_data_flow(
            synthetic_network.get_router(0)->find_interface("iRouter0"),
            synthetic_network.get_router(9)->find_interface("iRouter9"),
            next_label, path);

    Network synthetic_network2 = construct_synthetic_network();
    path = {synthetic_network2.get_router(0),
            synthetic_network2.get_router(2),
            synthetic_network2.get_router(3)};
    auto success1 = RouteConstruction::make_data_flow(
            synthetic_network2.get_router(0)->find_interface("iRouter0"),
            synthetic_network2.get_router(3)->find_interface("iRouter3"),
            next_label, path);

    BOOST_CHECK_EQUAL(success, success1);

    synthetic_network.inject_network(
            synthetic_network.get_router(5)->find_interface("Router7"),
            std::move(synthetic_network2),
            synthetic_network2.get_router(0)->find_interface("iRouter0"),
            synthetic_network2.get_router(3)->find_interface("iRouter3"),
            {Query::MPLS, 0, (uint64_t)47},
            {Query::MPLS, 0, (uint64_t)50});

    BOOST_TEST_MESSAGE("After: ");
    std::stringstream s_after;
    synthetic_network.print_simple(s_after);
    BOOST_TEST_MESSAGE(s_after.str());

    Builder builder(synthetic_network);
    {
        std::string query("<.*> [.#Router0] .* [Router2'#.] <.*> 0 OVER \n"
                          "<.*> [.#Router0] .* [Router5#.] <.*> 0 OVER \n"
                          "<.*> [.#Router0] .* [Router7#.] <.*> 0 OVER \n"
                          "<.*> [.#Router0] .* [Router9#.] <.*> 0 OVER \n"
                          "<.*> [.#Router0] .* [Router0'#.] <.*> 0 OVER \n"
        );
        build_query(query, synthetic_network, builder);
    }
}

BOOST_AUTO_TEST_CASE(NetworkInjectionAndTrace1) {
    Network synthetic_network = construct_synthetic_network(1);

    std::vector<const Router*> path {synthetic_network.get_router(0),
                                     synthetic_network.get_router(2),
                                     synthetic_network.get_router(4)};
    uint64_t i = 42;
    auto next_label = [&i](){return Query::label_t(Query::type_t::MPLS, 0, i++);};
    auto success = RouteConstruction::make_data_flow(
            synthetic_network.get_router(0)->find_interface("iRouter0"),
            synthetic_network.get_router(4)->find_interface("iRouter4"),
            next_label, path);

    Network synthetic_network2 = construct_synthetic_network();

    path = {synthetic_network2.get_router(0),
            synthetic_network2.get_router(2),
            synthetic_network2.get_router(3)};
    auto success1 = RouteConstruction::make_data_flow(
            synthetic_network2.get_router(0)->find_interface("iRouter0"),
            synthetic_network2.get_router(3)->find_interface("iRouter3"),
            next_label, path);

    BOOST_CHECK_EQUAL(success, success1);

    std::stringstream s_before;
    synthetic_network2.print_simple(s_before);
    BOOST_TEST_MESSAGE(s_before.str());

    synthetic_network.inject_network(
            synthetic_network.get_router(0)->find_interface("Router2"),
            std::move(synthetic_network2),
            synthetic_network2.get_router(0)->find_interface("iRouter0"),
            synthetic_network2.get_router(3)->find_interface("iRouter3"),
            {Query::MPLS, 0, (uint64_t)46},
            {Query::MPLS, 0, (uint64_t)49});

    BOOST_TEST_MESSAGE("After: ");
    std::stringstream s_after;
    synthetic_network.print_simple(s_after);
    BOOST_TEST_MESSAGE(s_after.str());

    Builder builder(synthetic_network);
    {
        std::string query("<.*> [.#Router0] .* [Router2'#.] <.*> 0 OVER \n"
                          "<.*> [.#Router0] .* [Router2#.] <.*> 0 OVER \n"
                          "<.*> [.#Router0] .* [Router0'#.] <.*> 0 OVER \n"
        );
        build_query(query, synthetic_network, builder);
    }
}

BOOST_AUTO_TEST_CASE(NetworkInjectionAndTrace2) {
    Network synthetic_network = construct_synthetic_network(5);

    std::vector<const Router*> path {synthetic_network.get_router(0),
                                     synthetic_network.get_router(5),
                                     synthetic_network.get_router(10),
                                     synthetic_network.get_router(15),
                                     synthetic_network.get_router(20),
                                     synthetic_network.get_router(22),
                                     synthetic_network.get_router(24)};
    uint64_t i = 42;
    auto next_label = [&i](){return Query::label_t(Query::type_t::MPLS, 0, i++);};
    auto success = RouteConstruction::make_data_flow(
            synthetic_network.get_router(0)->find_interface("iRouter0"),
            synthetic_network.get_router(24)->find_interface("iRouter24"),
            next_label, path);

    Network synthetic_network2 = construct_synthetic_network(2);

    path = {synthetic_network2.get_router(0),
            synthetic_network2.get_router(5),
            synthetic_network2.get_router(6),
            synthetic_network2.get_router(8),
            synthetic_network2.get_router(2),
            synthetic_network2.get_router(3)};
    auto success1 = RouteConstruction::make_data_flow(
            synthetic_network2.get_router(0)->find_interface("iRouter0"),
            synthetic_network2.get_router(3)->find_interface("iRouter3"),
            next_label, path);

    BOOST_CHECK_EQUAL(success, success1);

    synthetic_network.inject_network(
            synthetic_network.get_router(20)->find_interface("Router22"),
            std::move(synthetic_network2),
            synthetic_network2.get_router(0)->find_interface("iRouter0"),
            synthetic_network2.get_router(3)->find_interface("iRouter3"),
            {Query::MPLS, 0, (uint64_t)50},
            {Query::MPLS, 0, (uint64_t)56});

    BOOST_TEST_MESSAGE("After: ");
    std::stringstream s_after;
    synthetic_network.print_simple(s_after);
    BOOST_TEST_MESSAGE(s_after.str());

    Builder builder(synthetic_network);
    {
        std::string query("<.*> [.#Router0] .* [Router24#.] <.*> 0 OVER \n"
                          "<.*> [.#Router0] .* [Router2'#.] <.*> 0 OVER \n"
                          "<.*> [.#Router0] .* [Router0'#.] <.*> 0 OVER \n"
        );
        build_query(query, synthetic_network, builder);
    }
}

BOOST_AUTO_TEST_CASE(NetworkConcatenationAndTrace) {
    Network synthetic_network = construct_synthetic_network(1);

    std::vector<const Router*> path {synthetic_network.get_router(0),
                                     synthetic_network.get_router(1),
                                     synthetic_network.get_router(3),
                                     synthetic_network.get_router(2),
                                     synthetic_network.get_router(4),
                                     synthetic_network.get_router(3)};
    uint64_t i = 42;
    auto next_label = [&i](){return Query::label_t(Query::type_t::MPLS, 0, i++);};
    auto success = RouteConstruction::make_data_flow(
            synthetic_network.get_router(0)->find_interface("iRouter0"),
            synthetic_network.get_router(3)->find_interface("iRouter3"),
            next_label, path);

    Network synthetic_network2 = construct_synthetic_network(2);

    path = {synthetic_network2.get_router(0),
            synthetic_network2.get_router(5),
            synthetic_network2.get_router(6),
            synthetic_network2.get_router(8),
            synthetic_network2.get_router(2),
            synthetic_network2.get_router(3)};
    i--;
    auto success1 = RouteConstruction::make_data_flow(
            synthetic_network2.get_router(0)->find_interface("iRouter0"),
            synthetic_network2.get_router(3)->find_interface("iRouter3"),
            next_label, path);

    BOOST_CHECK_EQUAL(success, success1);

    synthetic_network.concat_network(
            synthetic_network.get_router(3)->find_interface("iRouter3"), std::move(synthetic_network2),
            synthetic_network2.get_router(0)->find_interface("iRouter0"), {Query::MPLS, 0, (uint64_t)48});

    BOOST_TEST_MESSAGE("After: ");
    std::stringstream s_after;
    synthetic_network.print_simple(s_after);
    BOOST_TEST_MESSAGE(s_after.str());

    Builder builder(synthetic_network);
    {
        std::string query("<.*> [.#Router0] .* [Router3'#.] <.*> 0 OVER \n"
                          "<.*> [.#Router0] .* [Router2'#.] <.*> 0 OVER \n"
                          "<.*> [.#Router0] .* [Router0'#.] <.*> 0 OVER \n"
        );
        build_query(query, synthetic_network, builder);
    }
}

BOOST_AUTO_TEST_CASE(NetworkConcatenationAndTrace1) {
    Network synthetic_network = construct_synthetic_network(5);

    std::vector<const Router*> path {synthetic_network.get_router(0),
                                     synthetic_network.get_router(5),
                                     synthetic_network.get_router(10),
                                     synthetic_network.get_router(15),
                                     synthetic_network.get_router(20),
                                     synthetic_network.get_router(22),
                                     synthetic_network.get_router(24)};
    uint64_t i = 42;
    auto next_label = [&i](){return Query::label_t(Query::type_t::MPLS, 0, i++);};
    auto success = RouteConstruction::make_data_flow(
            synthetic_network.get_router(0)->find_interface("iRouter0"),
            synthetic_network.get_router(24)->find_interface("iRouter24"),
            next_label, path);

    Network synthetic_network2 = construct_synthetic_network(2);

    path = {synthetic_network2.get_router(0),
            synthetic_network2.get_router(5),
            synthetic_network2.get_router(6),
            synthetic_network2.get_router(8),
            synthetic_network2.get_router(2),
            synthetic_network2.get_router(3)};
    i--;
    auto success1 = RouteConstruction::make_data_flow(
            synthetic_network2.get_router(0)->find_interface("iRouter0"),
            synthetic_network2.get_router(3)->find_interface("iRouter3"),
            next_label, path);

    BOOST_CHECK_EQUAL(success, success1);

    synthetic_network.concat_network(
            synthetic_network.get_router(24)->find_interface("iRouter24"), std::move(synthetic_network2),
            synthetic_network2.get_router(0)->find_interface("iRouter0"), {Query::MPLS, 0, (uint64_t)48});

    Builder builder(synthetic_network);
    {
        std::string query("<.*> [.#Router0] .* [Router3'#.] <.*> 0 OVER \n"
                          "<.*> [.#Router0] .* [Router2'#.] <.*> 0 OVER \n"
                          "<.*> [.#Router0] .* [Router0'#.] <.*> 0 OVER \n"
        );
        build_query(query, synthetic_network, builder);
    }
}

BOOST_AUTO_TEST_CASE(SyntheticNetworkPerformance) {
    Network synthetic_network = construct_synthetic_network(1);

    std::vector<const Router*> path {synthetic_network.get_router(0),
                                     synthetic_network.get_router(2),
                                     synthetic_network.get_router(4)};
    uint64_t i = 42;
    auto next_label = [&i](){return Query::label_t(Query::type_t::MPLS, 0, i++);};
    auto success = RouteConstruction::make_data_flow(
            synthetic_network.get_router(0)->find_interface("iRouter0"),
            synthetic_network.get_router(4)->find_interface("iRouter4"),
            next_label, path);

    BOOST_CHECK_EQUAL(success, true);

    std::stringstream trace;
    Builder builder(synthetic_network);
    {
        std::string query("<.*> [.#Router0] .* [Router4#.] <.*> 0 OVER \n");
        performance_query(query, synthetic_network, builder, trace);
    }
}

BOOST_AUTO_TEST_CASE(SyntheticNetworkPerformance1) {
    Network synthetic_network = construct_synthetic_network(5);

    std::vector<const Router*> path {synthetic_network.get_router(0),
                                     synthetic_network.get_router(5),
                                     synthetic_network.get_router(10),
                                     synthetic_network.get_router(15),
                                     synthetic_network.get_router(20),
                                     synthetic_network.get_router(22),
                                     synthetic_network.get_router(24)};
    uint64_t i = 42;
    auto next_label = [&i](){return Query::label_t(Query::type_t::MPLS, 0, i++);};
    auto success = RouteConstruction::make_data_flow(
            synthetic_network.get_router(0)->find_interface("iRouter0"),
            synthetic_network.get_router(24)->find_interface("iRouter24"),
            next_label, path);

    BOOST_CHECK_EQUAL(success, true);

    std::stringstream trace;
    Builder builder(synthetic_network);
    {
        std::string query("<.*> [.#Router0] .* [Router24#.] <.*> 0 OVER \n");
        performance_query(query, synthetic_network, builder, trace);
    }
}

BOOST_AUTO_TEST_CASE(SyntheticNetworkPerformanceInjection) {
    Network synthetic_network = construct_synthetic_network(5);

    std::vector<const Router*> path {synthetic_network.get_router(0),
                                     synthetic_network.get_router(5),
                                     synthetic_network.get_router(10),
                                     synthetic_network.get_router(15),
                                     synthetic_network.get_router(20),
                                     synthetic_network.get_router(22),
                                     synthetic_network.get_router(24)};
    uint64_t i = 42;
    auto next_label = [&i](){return Query::label_t(Query::type_t::MPLS, 0, i++);};
    auto success = RouteConstruction::make_data_flow(
            synthetic_network.get_router(0)->find_interface("iRouter0"),
            synthetic_network.get_router(24)->find_interface("iRouter24"),
            next_label, path);

    Network synthetic_network2 = construct_synthetic_network(2);

    path = {synthetic_network2.get_router(0),
            synthetic_network2.get_router(5),
            synthetic_network2.get_router(6),
            synthetic_network2.get_router(8),
            synthetic_network2.get_router(2),
            synthetic_network2.get_router(3)};
    auto success1 = RouteConstruction::make_data_flow(
            synthetic_network2.get_router(0)->find_interface("iRouter0"),
            synthetic_network2.get_router(3)->find_interface("iRouter3"),
            next_label, path);

    BOOST_CHECK_EQUAL(success, success1);

    synthetic_network.inject_network(
            synthetic_network.get_router(20)->find_interface("Router22"),
            std::move(synthetic_network2),
            synthetic_network2.get_router(0)->find_interface("iRouter0"),
            synthetic_network2.get_router(3)->find_interface("iRouter3"),
            {Query::MPLS, 0, (uint64_t)50},
            {Query::MPLS, 0, (uint64_t)56});

    std::stringstream trace;
    Builder builder(synthetic_network);
    {
        std::string query("<.*> [.#Router0] .* [Router24#.] <.*> 0 OVER \n");
        performance_query(query, synthetic_network, builder, trace);
    }
    BOOST_TEST_MESSAGE(trace.str());
}
