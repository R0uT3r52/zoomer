#pragma once

#include <algorithm>
#include <cmath>

#define EPS 1.0e-6F

#define MIN_SCALE 0.01F
#define SCROLL_SPEED 1.5F
#define DRAG_FRICTION 6.0F
#define SCALE_FRICTION 4.0F
#define VELOCITY_THRESHOLD 15.0F

struct Vec2 {
    float x;
    float y;

    Vec2(){
        x = 0.0f;
        y = 0.0f;
    };
    Vec2(float x, float y){
        this->x = x;
        this->y = y;
    };

    Vec2 operator+(const Vec2 &other) const {
        return Vec2{this->x + other.x, this->y + other.y};
    }

    Vec2& operator+=(const Vec2 &other) {
        this->x += other.x;
        this->y += other.y;
        return *this;
    }

    Vec2 operator-(const Vec2 &other) const {
        return Vec2{this->x - other.x, this->y - other.y};
    }

    Vec2& operator-=(const Vec2 &other) {
        this->x -= other.x;
        this->y -= other.y;
        return *this;
    }

    Vec2 operator*(const Vec2 &other) const {
        return Vec2{this->x * other.x, this->y * other.y};
    }

    Vec2 operator*(const float d) const {
        return Vec2{this->x * d, this->y * d};
    }

    Vec2& operator*=(const Vec2 &other) {
        this->x *= other.x;
        this->y *= other.y;
        return *this;
    }

    Vec2 operator/(const Vec2 &other) const {
        return Vec2{this->x / other.x, this->y / other.y};
    }

    Vec2 operator/(const float d) const {
        return Vec2{this->x / d, this->y / d};
    }

    Vec2& operator/=(const Vec2 &other) {
        this->x /= other.x;
        this->y /= other.y;
        return *this;
    }

    float length() const {
        return std::sqrt(this->x * this->x + this->y * this->y);
    };

    Vec2& normalize() {
        float lS = this->x * this->x + this->y * this->y;
        if (lS < EPS * EPS) {
            return *this;
        }

        float l = std::sqrt(lS);
        this->x = this->x / l;
        this->y = this->y / l;
        return *this;
    }
};

struct cursor {
    Vec2 Cur;
    Vec2 Prev;
    bool drag;

    cursor(){
        Cur = Vec2();
        Prev = Vec2();
        drag = false;
    };
};

struct camera {
    Vec2 Pos;
    Vec2 Vel;
    float Scale;
    float dScale;
    Vec2 scalePivot;

    camera(){
        Pos = Vec2();
        Vel = Vec2();
        Scale = 1.0f;
        dScale = 0.0f;
        scalePivot = Vec2();
    };

    void update(float dt, cursor curs, Vec2 windowSize) {
        if (std::fabs(dScale) > 0.5) {
            Vec2 point1 = (scalePivot - (windowSize / 2)) / Scale;
            Scale = std::max(Scale + dScale * dt, MIN_SCALE);
            Vec2 point2 = (scalePivot - (windowSize / 2)) / Scale;

            Pos += point1 - point2;
            dScale -= dScale * dt * SCALE_FRICTION;
        }

        if(!curs.drag && Vel.length() > VELOCITY_THRESHOLD) {
            Pos += Vel * dt;
            Vel -= Vel * dt * DRAG_FRICTION;
        }
    };
};

Vec2 world(camera cam, Vec2 v){ return v / cam.Scale; };
