#pragma once
#include "Object/Object.h"

// 상호작용 없는 배경: 벽, 바닥, 카운터, 테이블 외형.
// 기본으로 플레이어 이동을 막는다(바닥처럼 막지 않을 것은 solid = false).
class StaticObject : public Object {
public:
    StaticObject() { solid = true; }
};
