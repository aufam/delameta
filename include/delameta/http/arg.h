#ifndef PROJECT_DELAMETA_HTTP_ARG_H
#define PROJECT_DELAMETA_HTTP_ARG_H

namespace Project::delameta::http {
    struct Arg { const char* name; };

    template <typename T>
    struct ArgDefaultVal { const char* name; T default_value; };

    template <typename F>
    struct ArgDefaultFn { const char* name; F default_fn; };

    struct ArgJsonItem { const char* key; };

    template <typename F>
    struct ArgJsonItemDefaultVal { const char* key; F default_value; };

    template <typename F>
    struct ArgJsonItemDefaultFn { const char* key; F default_fn; };

    template <typename F>
    struct ArgDepends { F depends; };

    struct ArgFormItem { const char* key; };

    struct ArgRequest {};
    struct ArgResponse {};
    struct ArgMethod {};
    struct ArgURL {};
    struct ArgHeaders {};
    struct ArgQueries {};
    struct ArgPath {};
    struct ArgFullPath {};
    struct ArgFragment {};
    struct ArgVersion {};
    struct ArgBody {};
    struct ArgJson {};
    struct ArgText {};
}

#endif