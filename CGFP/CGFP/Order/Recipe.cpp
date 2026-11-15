#include "Order/Recipe.h"

#include "Cooking/Plate.h"

bool Recipe::Matches(const Plate& plate) const {
    const std::vector<Ingredient*>& stack = plate.GetStack();
    if (stack.size() != layers.size()) return false;
    for (size_t i = 0; i < layers.size(); ++i) {
        if (stack[i]->GetType() != layers[i]) return false;
        if (stack[i]->IsCookable() && stack[i]->GetCookState() != CookState::Cooked) return false;
    }
    return true;
}
