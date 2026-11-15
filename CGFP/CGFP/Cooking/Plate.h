#pragma once
#include "Cooking/Ingredient.h"

#include <vector>

// 접시: 재료 스택
class Plate : public InteractableObject {
public:
    std::vector<Ingredient*> stack;
};
