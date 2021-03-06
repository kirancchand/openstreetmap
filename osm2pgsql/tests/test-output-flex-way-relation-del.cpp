/**
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * This file is part of osm2pgsql (https://osm2pgsql.org/).
 *
 * Copyright (C) 2006-2022 by the osm2pgsql developer community.
 * For a full list of authors see the git log.
 */

#include <catch.hpp>

#include "common-import.hpp"
#include "common-options.hpp"

static testing::db::import_t db;

static char const *const conf_file = "test_output_flex_way.lua";

static const char *const tdata[] = {
    "n10 v1 dV x10.0 y10.0",
    "n11 v1 dV x10.0 y10.1",
    "n12 v1 dV x10.1 y10.0",
    "n13 v1 dV x10.1 y10.1",
    "n14 v1 dV x10.2 y10.0",
    "n15 v1 dV x10.2 y10.1",
    "n16 v1 dV x10.3 y10.0",
    "n17 v1 dV x10.3 y10.1",
    "n18 v1 dV x10.4 y10.0",
    "n19 v1 dV x10.4 y10.1",
    "w11 v1 dV Tt1=yes Nn12,n13",
    "w12 v1 dV Tt2=yes Nn14,n15",
    "w13 v1 dV Ttboth=yes Nn16,n17",
    "w14 v1 dV Ttboth=yes Nn18,n19",
    "w15 v1 dV Tt=ag Nn17,n19",
    "r30 v1 dV Tt=ag Mw10@,w11@,w12@mark,w13@,w14@mark"};

TEST_CASE("test way: delete relation with way not in relation")
{
    options_t options = testing::opt_t().slim().flex(conf_file);

    testing::db::data_t data{tdata};

    data.add("w10 v1 dV Tt=ag Nn10,n11");
    data.add("r32 v1 dV Tt=ag Mw11@,w12@,w13@,w14@,w15@");

    REQUIRE_NOTHROW(db.run_import(options, data()));

    auto conn = db.db().connect();

    CHECK(1 == conn.get_count("osm2pgsql_test_t1"));
    CHECK(0 == conn.get_count("osm2pgsql_test_t1", "way_id = 10"));
    CHECK(1 == conn.get_count("osm2pgsql_test_t2"));
    CHECK(0 == conn.get_count("osm2pgsql_test_t2", "way_id = 10"));
    CHECK(2 == conn.get_count("osm2pgsql_test_tboth"));
    CHECK(0 == conn.get_count("osm2pgsql_test_tboth", "way_id = 10"));

    options.append = true;

    REQUIRE_NOTHROW(db.run_import(options, "r32 v2 dD\n"));

    CHECK(1 == conn.get_count("osm2pgsql_test_t1"));
    CHECK(0 == conn.get_count("osm2pgsql_test_t1", "way_id = 10"));
    CHECK(1 == conn.get_count("osm2pgsql_test_t2"));
    CHECK(0 == conn.get_count("osm2pgsql_test_t2", "way_id = 10"));
    CHECK(2 == conn.get_count("osm2pgsql_test_tboth"));
    CHECK(0 == conn.get_count("osm2pgsql_test_tboth", "way_id = 10"));
}

TEST_CASE("test way: delete relation with way in t1")
{
    options_t options = testing::opt_t().slim().flex(conf_file);

    testing::db::data_t data{tdata};

    data.add("w10 v1 dV Tt1=yes Nn10,n11");
    data.add("r32 v1 dV Tt=ag Mw11@,w12@,w13@,w14@,w15@");

    REQUIRE_NOTHROW(db.run_import(options, data()));

    auto conn = db.db().connect();

    CHECK(2 == conn.get_count("osm2pgsql_test_t1"));
    CHECK(1 == conn.get_count("osm2pgsql_test_t1", "way_id = 10"));
    CHECK(1 == conn.get_count("osm2pgsql_test_t2"));
    CHECK(0 == conn.get_count("osm2pgsql_test_t2", "way_id = 10"));
    CHECK(2 == conn.get_count("osm2pgsql_test_tboth"));
    CHECK(0 == conn.get_count("osm2pgsql_test_tboth", "way_id = 10"));

    options.append = true;

    REQUIRE_NOTHROW(db.run_import(options, "r32 v2 dD\n"));

    CHECK(2 == conn.get_count("osm2pgsql_test_t1"));
    CHECK(1 == conn.get_count("osm2pgsql_test_t1", "way_id = 10"));
    CHECK(1 == conn.get_count("osm2pgsql_test_t2"));
    CHECK(0 == conn.get_count("osm2pgsql_test_t2", "way_id = 10"));
    CHECK(2 == conn.get_count("osm2pgsql_test_tboth"));
    CHECK(0 == conn.get_count("osm2pgsql_test_tboth", "way_id = 10"));
}

TEST_CASE("test way: delete relation with way in t2 (multi)")
{
    options_t options = testing::opt_t().slim().flex(conf_file);

    testing::db::data_t data{tdata};

    data.add("w10 v1 dV Tt2=yes Nn10,n11");
    data.add("r31 v1 dV Tt=ag Mw10@mark");
    data.add("r32 v1 dV Tt=ag Mw10@mark,w11@,w12@,w13@,w14@,w15@");

    REQUIRE_NOTHROW(db.run_import(options, data()));

    auto conn = db.db().connect();

    CHECK(1 == conn.get_count("osm2pgsql_test_t1"));
    CHECK(0 == conn.get_count("osm2pgsql_test_t1", "way_id = 10"));
    CHECK(2 == conn.get_count("osm2pgsql_test_t2"));
    CHECK(1 == conn.get_count("osm2pgsql_test_t2",
                              "way_id = 10 AND rel_ids = '{31,32}'"));
    CHECK(2 == conn.get_count("osm2pgsql_test_tboth"));
    CHECK(0 == conn.get_count("osm2pgsql_test_tboth", "way_id = 10"));

    options.append = true;

    SECTION("delete")
    {
        REQUIRE_NOTHROW(db.run_import(options, "r32 v2 dD\n"));
    }

    SECTION("remove way from relation")
    {
        REQUIRE_NOTHROW(db.run_import(
            options, "r32 v2 dV Tt=ag Mw11@,w12@,w13@,w14@,w15@\n"));
    }

    CHECK(1 == conn.get_count("osm2pgsql_test_t1"));
    CHECK(0 == conn.get_count("osm2pgsql_test_t1", "way_id = 10"));
    CHECK(2 == conn.get_count("osm2pgsql_test_t2"));
    CHECK(1 == conn.get_count("osm2pgsql_test_t2",
                              "way_id = 10 AND rel_ids = '{31}'"));
    CHECK(2 == conn.get_count("osm2pgsql_test_tboth"));
    CHECK(0 == conn.get_count("osm2pgsql_test_tboth", "way_id = 10"));
}

TEST_CASE("test way: delete relation with way in t2 (single)")
{
    options_t options = testing::opt_t().slim().flex(conf_file);

    testing::db::data_t data{tdata};

    data.add("w10 v1 dV Tt2=yes Nn10,n11");
    data.add("r32 v1 dV Tt=ag Mw10@mark,w11@,w12@,w13@,w14@,w15@");

    REQUIRE_NOTHROW(db.run_import(options, data()));

    auto conn = db.db().connect();

    CHECK(1 == conn.get_count("osm2pgsql_test_t1"));
    CHECK(0 == conn.get_count("osm2pgsql_test_t1", "way_id = 10"));
    CHECK(2 == conn.get_count("osm2pgsql_test_t2"));
    CHECK(1 == conn.get_count("osm2pgsql_test_t2",
                              "way_id = 10 AND rel_ids = '{32}'"));
    CHECK(2 == conn.get_count("osm2pgsql_test_tboth"));
    CHECK(0 == conn.get_count("osm2pgsql_test_tboth", "way_id = 10"));

    options.append = true;

    SECTION("delete")
    {
        REQUIRE_NOTHROW(db.run_import(options, "r32 v2 dD\n"));
    }

    SECTION("remove way from relation")
    {
        REQUIRE_NOTHROW(db.run_import(
            options, "r32 v2 dV Tt=ag Mw11@,w12@,w13@,w14@,w15@\n"));
    }

    CHECK(1 == conn.get_count("osm2pgsql_test_t1"));
    CHECK(0 == conn.get_count("osm2pgsql_test_t1", "way_id = 10"));
    CHECK(1 == conn.get_count("osm2pgsql_test_t2"));
    CHECK(0 == conn.get_count("osm2pgsql_test_t2",
                              "way_id = 10 AND rel_ids IS NULL"));
    CHECK(2 == conn.get_count("osm2pgsql_test_tboth"));
    CHECK(0 == conn.get_count("osm2pgsql_test_tboth", "way_id = 10"));
}

TEST_CASE("test way: delete relation with way in t1 + t2 (multi)")
{
    options_t options = testing::opt_t().slim().flex(conf_file);

    testing::db::data_t data{tdata};

    data.add("w10 v1 dV Tt1=yes,t2=yes Nn10,n11");
    data.add("r31 v1 dV Tt=ag Mw10@mark");
    data.add("r32 v1 dV Tt=ag Mw10@mark,w11@,w12@,w13@,w14@,w15@");

    REQUIRE_NOTHROW(db.run_import(options, data()));

    auto conn = db.db().connect();

    CHECK(2 == conn.get_count("osm2pgsql_test_t1"));
    CHECK(1 == conn.get_count("osm2pgsql_test_t1", "way_id = 10"));
    CHECK(2 == conn.get_count("osm2pgsql_test_t2"));
    CHECK(1 == conn.get_count("osm2pgsql_test_t2",
                              "way_id = 10 AND rel_ids = '{31,32}'"));
    CHECK(2 == conn.get_count("osm2pgsql_test_tboth"));
    CHECK(0 == conn.get_count("osm2pgsql_test_tboth", "way_id = 10"));

    options.append = true;

    SECTION("delete")
    {
        REQUIRE_NOTHROW(db.run_import(options, "r32 v2 dD\n"));
    }

    SECTION("remove way from relation")
    {
        REQUIRE_NOTHROW(db.run_import(
            options, "r32 v2 dV Tt=ag Mw11@,w12@,w13@,w14@,w15@\n"));
    }

    CHECK(2 == conn.get_count("osm2pgsql_test_t1"));
    CHECK(1 == conn.get_count("osm2pgsql_test_t1", "way_id = 10"));
    CHECK(2 == conn.get_count("osm2pgsql_test_t2"));
    CHECK(1 == conn.get_count("osm2pgsql_test_t2",
                              "way_id = 10 AND rel_ids = '{31}'"));
    CHECK(2 == conn.get_count("osm2pgsql_test_tboth"));
    CHECK(0 == conn.get_count("osm2pgsql_test_tboth", "way_id = 10"));
}

TEST_CASE("test way: delete relation with way in t1 + t2 (single)")
{
    options_t options = testing::opt_t().slim().flex(conf_file);

    testing::db::data_t data{tdata};

    data.add("w10 v1 dV Tt1=yes,t2=yes Nn10,n11");
    data.add("r32 v1 dV Tt=ag Mw10@mark,w11@,w12@,w13@,w14@,w15@");

    REQUIRE_NOTHROW(db.run_import(options, data()));

    auto conn = db.db().connect();

    CHECK(2 == conn.get_count("osm2pgsql_test_t1"));
    CHECK(1 == conn.get_count("osm2pgsql_test_t1", "way_id = 10"));
    CHECK(2 == conn.get_count("osm2pgsql_test_t2"));
    CHECK(1 == conn.get_count("osm2pgsql_test_t2",
                              "way_id = 10 AND rel_ids = '{32}'"));
    CHECK(2 == conn.get_count("osm2pgsql_test_tboth"));
    CHECK(0 == conn.get_count("osm2pgsql_test_tboth", "way_id = 10"));

    options.append = true;

    SECTION("delete")
    {
        REQUIRE_NOTHROW(db.run_import(options, "r32 v2 dD\n"));
    }

    SECTION("remove way from relation")
    {
        REQUIRE_NOTHROW(db.run_import(
            options, "r32 v2 dV Tt=ag Mw11@,w12@,w13@,w14@,w15@\n"));
    }

    CHECK(2 == conn.get_count("osm2pgsql_test_t1"));
    CHECK(1 == conn.get_count("osm2pgsql_test_t1", "way_id = 10"));
    CHECK(1 == conn.get_count("osm2pgsql_test_t2"));
    CHECK(0 == conn.get_count("osm2pgsql_test_t2", "way_id = 10"));
    CHECK(2 == conn.get_count("osm2pgsql_test_tboth"));
    CHECK(0 == conn.get_count("osm2pgsql_test_tboth", "way_id = 10"));
}

TEST_CASE("test way: delete relation with way in tboth (multi)")
{
    options_t options = testing::opt_t().slim().flex(conf_file);

    testing::db::data_t data{tdata};

    data.add("w10 v1 dV Ttboth=yes Nn10,n11");
    data.add("r31 v1 dV Tt=ag Mw10@mark");
    data.add("r32 v1 dV Tt=ag Mw10@mark,w11@,w12@,w13@,w14@,w15@");

    REQUIRE_NOTHROW(db.run_import(options, data()));

    auto conn = db.db().connect();

    CHECK(1 == conn.get_count("osm2pgsql_test_t1"));
    CHECK(0 == conn.get_count("osm2pgsql_test_t1", "way_id = 10"));
    CHECK(1 == conn.get_count("osm2pgsql_test_t2"));
    CHECK(0 == conn.get_count("osm2pgsql_test_t2", "way_id = 10"));
    CHECK(3 == conn.get_count("osm2pgsql_test_tboth"));
    CHECK(1 == conn.get_count("osm2pgsql_test_tboth",
                              "way_id = 10 AND rel_ids = '{31,32}'"));

    options.append = true;

    SECTION("delete")
    {
        REQUIRE_NOTHROW(db.run_import(options, "r32 v2 dD\n"));
    }

    SECTION("remove way from relation")
    {
        REQUIRE_NOTHROW(db.run_import(
            options, "r32 v2 dV Tt=ag Mw11@,w12@,w13@,w14@,w15@\n"));
    }

    CHECK(1 == conn.get_count("osm2pgsql_test_t1"));
    CHECK(0 == conn.get_count("osm2pgsql_test_t1", "way_id = 10"));
    CHECK(1 == conn.get_count("osm2pgsql_test_t2"));
    CHECK(0 == conn.get_count("osm2pgsql_test_t2", "way_id = 10"));
    CHECK(3 == conn.get_count("osm2pgsql_test_tboth"));
    CHECK(1 == conn.get_count("osm2pgsql_test_tboth",
                              "way_id = 10 AND rel_ids = '{31}'"));
}

TEST_CASE("test way: delete relation with way in tboth (single)")
{
    options_t options = testing::opt_t().slim().flex(conf_file);

    testing::db::data_t data{tdata};

    data.add("w10 v1 dV Ttboth=yes Nn10,n11");
    data.add("r32 v1 dV Tt=ag Mw10@mark,w11@,w12@,w13@,w14@,w15@");

    REQUIRE_NOTHROW(db.run_import(options, data()));

    auto conn = db.db().connect();

    CHECK(1 == conn.get_count("osm2pgsql_test_t1"));
    CHECK(0 == conn.get_count("osm2pgsql_test_t1", "way_id = 10"));
    CHECK(1 == conn.get_count("osm2pgsql_test_t2"));
    CHECK(0 == conn.get_count("osm2pgsql_test_t2", "way_id = 10"));
    CHECK(3 == conn.get_count("osm2pgsql_test_tboth"));
    CHECK(1 == conn.get_count("osm2pgsql_test_tboth",
                              "way_id = 10 AND rel_ids = '{32}'"));

    options.append = true;

    SECTION("delete")
    {
        REQUIRE_NOTHROW(db.run_import(options, "r32 v2 dD\n"));
    }

    SECTION("remove way from relation")
    {
        REQUIRE_NOTHROW(db.run_import(
            options, "r32 v2 dV Tt=ag Mw11@,w12@,w13@,w14@,w15@\n"));
    }

    CHECK(1 == conn.get_count("osm2pgsql_test_t1"));
    CHECK(0 == conn.get_count("osm2pgsql_test_t1", "way_id = 10"));
    CHECK(1 == conn.get_count("osm2pgsql_test_t2"));
    CHECK(0 == conn.get_count("osm2pgsql_test_t2", "way_id = 10"));
    CHECK(3 == conn.get_count("osm2pgsql_test_tboth"));
    CHECK(1 == conn.get_count("osm2pgsql_test_tboth",
                              "way_id = 10 AND rel_ids IS NULL"));
}
