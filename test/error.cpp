#include <boost/preprocessor.hpp>
#include <fmt/format.h>
#include <delameta/debug.h>
#include <delameta/error.h>
#include <delameta/http/server.h>
#include <delameta/modbus/api.h>
#include <gtest/gtest.h>

using namespace Project;

TEST(Error, elision) {
    {
        delameta::Error e(-1, "Fatal error");
        delameta::http::Server::Error he = e;
        EXPECT_EQ(he.status, delameta::http::StatusInternalServerError);
        EXPECT_EQ(he.what, e.what + ": -1");
    } {
        delameta::http::Server::Error he(delameta::http::StatusOK, "OK");
        delameta::Error e = he;
        EXPECT_EQ(e.code, delameta::http::StatusOK);
        EXPECT_EQ(e.what, "OK");
    } {
        delameta::modbus::Error me(delameta::modbus::Error::InvalidCRC);
        delameta::Error e = me;
        EXPECT_EQ(e.code, delameta::modbus::Error::InvalidCRC);
        EXPECT_EQ(e.what, "Invalid CRC");
    } {
        delameta::Error e(-1, "Fatal error");
        delameta::modbus::Error me = std::move(e);
        EXPECT_EQ(me.code, e.code);
        EXPECT_EQ(me.what, e.what);
    } {
        delameta::modbus::Error me(delameta::modbus::Error::InvalidCRC);
        delameta::http::Server::Error he = me;
        EXPECT_EQ(he.status, delameta::http::StatusInternalServerError);
        EXPECT_EQ(he.what, me.what + ": " + std::to_string((int)delameta::modbus::Error::InvalidCRC));
    } {
        delameta::http::Server::Error he(delameta::http::StatusOK, "OK");
        delameta::modbus::Error me = delameta::Error(he);
        EXPECT_EQ(me.code, delameta::http::StatusOK);
        EXPECT_EQ(me.what, "OK");
    }
}