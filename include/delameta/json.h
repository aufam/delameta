#ifndef PROJECT_DELAMETA_JSON_H
#define PROJECT_DELAMETA_JSON_H

#include "etl/json_serialize.h"
#include "etl/json_deserialize.h"
#include "delameta/stream.h"

namespace Project::delameta::json {
    using namespace etl::json;

    template <typename T>
    Stream serialize_as_stream(T&& value) {
        Stream s;

        if constexpr (etl::detail::is_variant_v<T>) {
            return std::visit([&](auto&& item) {
                return serialize_as_stream<etl::decay_t<decltype(item)>>(std::move(item));
            }, std::move(value));
        }

        else if constexpr (etl::is_optional_v<T> || etl::detail::is_std_optional_v<T>) {
            if (value) {
                return serialize_as_stream(std::move(*value));
            }
            s << "null";
        }

        else if constexpr (etl::is_ref_v<T>) {
            using RT = std::remove_const_t<etl::remove_extent_t<T>>;
            
            if constexpr (
                etl::is_linked_list_v<RT> || etl::is_vector_v<RT> || etl::is_array_v<RT> || 
                etl::detail::is_std_list_v<RT> || etl::detail::is_std_vector_v<RT> || etl::detail::is_std_array_v<RT>
            ) {
                s << [buffer=std::string(), ref=value, it=value->begin()](Stream& s) mutable -> std::string_view {
                    if (it == ref->end()) return "[]";

                    buffer.clear();
                    buffer.reserve(2 + size_max(*it));
                    if (it == ref->begin()) buffer += '[';
                    buffer += serialize(*it);

                    ++it;
                    s.again = it != ref->end();
                    if (!s.again) buffer += ']';
                    else buffer += ',';
                    return buffer;
                };
            }

            else if constexpr (etl::is_map_v<RT> || etl::is_unordered_map_v<RT>) {
                s << [buffer=std::string(), ref=value, it=value->begin()](Stream& s) mutable -> std::string_view {
                    if (it == ref->end()) return "{}";
                    
                    buffer.clear();
                    buffer.reserve(2 + size_max(it->x) + 1 + size_max(it->y));

                    if (it == ref->begin()) buffer += '{';
                    buffer += serialize(it->x);
                    buffer += ':';
                    buffer += serialize(it->y);

                    ++it;
                    s.again = it != ref->end();
                    if (!s.again) buffer += '}';
                    else buffer += ',';
                    return buffer;
                };
            }

            else if constexpr (etl::detail::is_std_map_v<RT> || etl::detail::is_std_unordered_map_v<RT>) {
                s << [buffer=std::string(), ref=value, it=value->begin()](Stream& s) mutable -> std::string_view {
                    if (it == ref->end()) return "{}";
                    
                    buffer.clear();
                    buffer.reserve(2 + size_max(it->first) + 1 + size_max(it->second));

                    if (it == ref->begin()) buffer += '{';
                    buffer += serialize(it->first);
                    buffer += ':';
                    buffer += serialize(it->second);

                    ++it;
                    s.again = it != ref->end();
                    if (!s.again) buffer += '}';
                    else buffer += ',';
                    return buffer;
                };
            }

            else {
                s << [buffer=std::string(), ref=value](Stream&) mutable -> std::string_view {
                    buffer = serialize(ref);
                    return buffer;
                };
            }
        }

        else {
            T* ptr = new T(std::move(value));
            s << serialize_as_stream(etl::ref_const(*ptr));
            s.at_destructor = [ptr]() mutable { delete ptr; };
        }

        return s;
    }

    template <typename T>
    Stream serialize_as_stream(T&) = delete;
}

#ifdef BOOST_PREPROCESSOR_HPP

#define JSON_HELPER_WRAP_SEQUENCE_X(...) ((__VA_ARGS__)) JSON_HELPER_WRAP_SEQUENCE_Y
#define JSON_HELPER_WRAP_SEQUENCE_Y(...) ((__VA_ARGS__)) JSON_HELPER_WRAP_SEQUENCE_X
#define JSON_HELPER_WRAP_SEQUENCE_X0
#define JSON_HELPER_WRAP_SEQUENCE_Y0

#define JSON_HELPER_CALC_MAX_SIZE_MEMBER(r, data, elem) \
    is_empty = false; n += size_max(BOOST_PP_STRINGIZE(BOOST_PP_TUPLE_ELEM(2, 1, elem))) + size_max(m.BOOST_PP_TUPLE_ELEM(2, 1, elem)) + 2;

#define JSON_HELPER_APPEND_MEMBER(r, data, elem) \
    key = BOOST_PP_STRINGIZE(BOOST_PP_TUPLE_ELEM(2, 1, elem)); \
    detail::json_append(res, key, m.BOOST_PP_TUPLE_ELEM(2, 1, elem));

#define JSON_HELPER_CONVERT_MEMBER_TO_STREAM_RULE(r, data, elem) \
    std::pair{BOOST_PP_STRINGIZE(BOOST_PP_TUPLE_ELEM(2, 1, elem)), +[](etl::Ref<const T> ref) -> std::string { \
        return serialize(ref->BOOST_PP_TUPLE_ELEM(2, 1, elem)); \
    }},

#define JSON_HELPER_DESERIALIZE_MEMBER(r, data, elem) \
    { auto res = \
      detail::json_deserialize(j, BOOST_PP_STRINGIZE(BOOST_PP_TUPLE_ELEM(2, 1, elem)), m.BOOST_PP_TUPLE_ELEM(2, 1, elem)); \
      if (res.is_err()) { return res; } \
    }

#define JSON_HELPER_DEFINE_MEMBER(r, data, elem) \
    BOOST_PP_TUPLE_ELEM(2, 0, elem) BOOST_PP_TUPLE_ELEM(2, 1, elem);

#define JSON_TRAITS_I(name, seq) \
    template <> inline \
    size_t Project::etl::json::size_max(const name& m) { \
        size_t n = 1; \
        bool is_empty = true; \
        BOOST_PP_SEQ_FOR_EACH(JSON_HELPER_CALC_MAX_SIZE_MEMBER, ~, seq) \
        return n + is_empty; \
    } \
    template <> inline \
    std::string Project::etl::json::serialize(const name& m) { \
        std::string res; \
        const char* key; \
        res.reserve(size_max(m)); \
        res += '{'; \
        BOOST_PP_SEQ_FOR_EACH(JSON_HELPER_APPEND_MEMBER, ~, seq) \
        res.back() = '}'; \
        return res; \
    } \
    template <> inline \
    constexpr Project::etl::Result<void, const char*> Project::etl::json::deserialize(const etl::Json& j, name& m) { \
        if (j.error_message()) return etl::Err(j.error_message().data()); \
        if (!j.is_dictionary()) return etl::Err("JSON is not a map"); \
        BOOST_PP_SEQ_FOR_EACH(JSON_HELPER_DESERIALIZE_MEMBER, ~, seq) \
        return etl::Ok(); \
    } \
    template <> inline \
    Project::delameta::Stream Project::delameta::json::serialize_as_stream(etl::Ref<const name>&& ref) { \
        using T = name; \
        delameta::Stream s; \
        static constexpr std::pair<const char*, std::string (*)(etl::Ref<const T>)> map[] = { \
            BOOST_PP_SEQ_FOR_EACH(JSON_HELPER_CONVERT_MEMBER_TO_STREAM_RULE, ~, seq) \
        };\
        s << [buffer=std::string(), ref, it=std::begin(map)](delameta::Stream& s) mutable -> std::string_view { \
            if (it == std::end(map)) return "{}"; \
            buffer.clear(); \
            auto value = it->second(ref); \
            buffer.reserve(2 + size_max(it->first) + 1 + value.size()); \
            if (it == std::begin(map)) buffer += '{'; \
            buffer += serialize(it->first); \
            buffer += ':'; \
            buffer += std::move(value); \
            ++it; \
            s.again = it != std::end(map); \
            if (!s.again) buffer += '}'; \
            else buffer += ','; \
            return buffer; \
        }; \
        return s; \
    } \

#define JSON_TRAITS(name, items) \
    JSON_TRAITS_I(BOOST_PP_TUPLE_ELEM(1, 0, name), BOOST_PP_CAT(JSON_HELPER_WRAP_SEQUENCE_X items, 0))

#define JSON_DECLARE_I(name, seq) \
    struct name { BOOST_PP_SEQ_FOR_EACH(JSON_HELPER_DEFINE_MEMBER, ~, seq) }; \
    JSON_TRAITS_I(name, seq)

#define JSON_DECLARE(name, items) \
    JSON_DECLARE_I(BOOST_PP_TUPLE_ELEM(1, 0, name), BOOST_PP_CAT(JSON_HELPER_WRAP_SEQUENCE_X items, 0))

#endif
#endif
