#ifndef PROJECT_DELAMETA_JSON_H
#define PROJECT_DELAMETA_JSON_H

#include "etl/json_serialize.h"
#include "etl/json_deserialize.h"

#ifdef BOOST_PREPROCESSOR_HPP

#define JSON_HELPER_WRAP_SEQUENCE_X(...) ((__VA_ARGS__)) JSON_HELPER_WRAP_SEQUENCE_Y
#define JSON_HELPER_WRAP_SEQUENCE_Y(...) ((__VA_ARGS__)) JSON_HELPER_WRAP_SEQUENCE_X
#define JSON_HELPER_WRAP_SEQUENCE_X0
#define JSON_HELPER_WRAP_SEQUENCE_Y0

#define JSON_HELPER_CALC_MAX_SIZE_MEMBER(r, data, elem) \
    n += size_max(BOOST_PP_STRINGIZE(BOOST_PP_TUPLE_ELEM(2, 1, elem))) + size_max(m.BOOST_PP_TUPLE_ELEM(2, 1, elem)) + 2;

#define JSON_HELPER_APPEND_MEMBER(r, data, elem) \
    detail::json_append(res, BOOST_PP_STRINGIZE(BOOST_PP_TUPLE_ELEM(2, 1, elem)), m.BOOST_PP_TUPLE_ELEM(2, 1, elem));

#define JSON_HELPER_DESERIALIZE_MEMBER(r, data, elem) \
    { auto res = \
      detail::json_deserialize(j, BOOST_PP_STRINGIZE(BOOST_PP_TUPLE_ELEM(2, 1, elem)), m.BOOST_PP_TUPLE_ELEM(2, 1, elem)); \
      if (res.is_err()) { return res; } \
    }

#define JSON_HELPER_DEFINE_MEMBER(r, data, elem) \
    BOOST_PP_TUPLE_ELEM(2, 0, elem) BOOST_PP_TUPLE_ELEM(2, 1, elem);

#define JSON_TRAITS_I(name, seq) \
    namespace Project::etl::json { \
        template <> inline \
        size_t size_max(const name& m) { \
            size_t n = 2; \
            BOOST_PP_SEQ_FOR_EACH(JSON_HELPER_CALC_MAX_SIZE_MEMBER, ~, seq) \
            return n; \
        } \
        template <> inline \
        std::string serialize(const name& m) { \
            std::string res; \
            res.reserve(size_max(m)); \
            res += '{'; \
            BOOST_PP_SEQ_FOR_EACH(JSON_HELPER_APPEND_MEMBER, ~, seq) \
            res.back() = '}'; \
            return res; \
        } \
        template <> inline \
        constexpr etl::Result<void, const char*> deserialize(const etl::Json& j, name& m) { \
            if (j.error_message()) return etl::Err(j.error_message().data()); \
            if (!j.is_dictionary()) return etl::Err("JSON is not a map"); \
            BOOST_PP_SEQ_FOR_EACH(JSON_HELPER_DESERIALIZE_MEMBER, ~, seq) \
            return etl::Ok(); \
        } \
    }

#define JSON_TRAITS(name, items) \
    JSON_TRAITS_I(BOOST_PP_TUPLE_ELEM(1, 0, name), BOOST_PP_CAT(JSON_HELPER_WRAP_SEQUENCE_X items, 0))

#define JSON_DECLARE_I(name, seq) \
    struct name { BOOST_PP_SEQ_FOR_EACH(JSON_HELPER_DEFINE_MEMBER, ~, seq) }; \
    JSON_TRAITS_I(name, seq)

#define JSON_DECLARE(name, items) \
    JSON_DECLARE_I(BOOST_PP_TUPLE_ELEM(1, 0, name), BOOST_PP_CAT(JSON_HELPER_WRAP_SEQUENCE_X items, 0))

#endif
#endif