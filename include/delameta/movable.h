#ifndef PROJECT_DELAMETA_MOVABLE_H
#define PROJECT_DELAMETA_MOVABLE_H

namespace Project::delameta {

    class Movable {
    public:
        Movable() = default;
        virtual ~Movable() = default;

        Movable(const Movable&) = delete;
        Movable& operator=(const Movable&) = delete;

        Movable(Movable&&) noexcept = default;
        Movable& operator=(Movable&&) noexcept = default;
    };
}

#endif