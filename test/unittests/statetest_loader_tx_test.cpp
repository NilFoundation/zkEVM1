// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2023 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <test/statetest/statetest.hpp>

using namespace evmone;

TEST(statetest_loader, tx_create_legacy)
{
    constexpr std::string_view input = R"({
        "input": "b0b1",
        "gas": "9091",
        "value": "0xe0e1",
        "sender": "a0a1",
        "to": "",
        "gasPrice": "0x7071"
    })";

    const auto tx = test::from_json<state::Transaction>(json::json::parse(input));
    EXPECT_EQ(tx.kind, state::Transaction::Kind::legacy);
    EXPECT_EQ(tx.data, (bytes{0xb0, 0xb1}));
    EXPECT_EQ(tx.gas_limit, 0x9091);
    EXPECT_EQ(tx.value, 0xe0e1);
    EXPECT_EQ(tx.sender, 0xa0a1_address);
    EXPECT_FALSE(tx.to.has_value());
    EXPECT_EQ(tx.max_gas_price, 0x7071);
    EXPECT_EQ(tx.max_priority_gas_price, 0x7071);
    EXPECT_TRUE(tx.access_list.empty());
}

TEST(statetest_loader, tx_eip1559)
{
    constexpr std::string_view input = R"({
        "input": "b0b1",
        "gas": "9091",
        "value": "0xe0e1",
        "sender": "a0a1",
        "to": "c0c1",
        "maxFeePerGas": "0x7071",
        "maxPriorityFeePerGas": "0x6061",
        "accessList": []
    })";

    const auto tx = test::from_json<state::Transaction>(json::json::parse(input));
    EXPECT_EQ(tx.kind, state::Transaction::Kind::eip1559);
    EXPECT_EQ(tx.data, (bytes{0xb0, 0xb1}));
    EXPECT_EQ(tx.gas_limit, 0x9091);
    EXPECT_EQ(tx.value, 0xe0e1);
    EXPECT_EQ(tx.sender, 0xa0a1_address);
    EXPECT_EQ(tx.to, 0xc0c1_address);
    EXPECT_EQ(tx.max_gas_price, 0x7071);
    EXPECT_EQ(tx.max_priority_gas_price, 0x6061);
    EXPECT_TRUE(tx.access_list.empty());
}

TEST(statetest_loader, tx_access_list)
{
    constexpr std::string_view input = R"({
        "input": "",
        "gas": "0",
        "value": "0",
        "sender": "",
        "to": "",
        "maxFeePerGas": "0",
        "maxPriorityFeePerGas": "0",
        "accessList": [
            {"address": "ac01", "storageKeys": []},
            {"address": "ac02", "storageKeys": ["fe", "00"]}
        ]
    })";

    const auto tx = test::from_json<state::Transaction>(json::json::parse(input));
    ASSERT_EQ(tx.access_list.size(), 2);
    EXPECT_EQ(tx.access_list[0].first, 0xac01_address);
    EXPECT_EQ(tx.access_list[0].second.size(), 0);
    EXPECT_EQ(tx.access_list[1].first, 0xac02_address);
    EXPECT_EQ(tx.access_list[1].second, (std::vector{0xfe_bytes32, 0x00_bytes32}));
}
