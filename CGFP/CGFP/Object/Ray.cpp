#include "Object/Ray.h"

#include "Character/Camera.h"
#include "Object/InteractableObject.h"

Ray Ray::FromCamera(const Camera& camera, float maxDistance) {
    Ray ray;
    ray.origin = camera.position;
    ray.direction = camera.GetForward();
    ray.maxDistance = maxDistance;
    return ray;
}

InteractableObject* Ray::Pick(const std::vector<InteractableObject*>& candidates, float* outDistance) const {
    InteractableObject* best = nullptr;
    float bestDistance = maxDistance;
    for (InteractableObject* candidate : candidates) {
        if (!candidate || candidate->IsHeld() || !candidate->IsInteractEnabled() || candidate->IsDestroyed()) continue;
        float distance = 0.0f;
        if (candidate->GetWorldCollider().IntersectsRay(origin, direction, bestDistance, distance) && distance <= bestDistance) {
            best = candidate;
            bestDistance = distance;
        }
    }
    if (best && outDistance) *outDistance = bestDistance;
    return best;
}

Object* Ray::PickObject(const std::vector<Object*>& candidates, float* outDistance) const {
    Object* best = nullptr;
    float bestDistance = maxDistance;
    for (Object* candidate : candidates) {
        if (!candidate || candidate->IsDestroyed()) continue;
        float distance = 0.0f;
        if (candidate->GetWorldCollider().IntersectsRay(origin, direction, bestDistance, distance) && distance <= bestDistance) {
            best = candidate;
            bestDistance = distance;
        }
    }
    if (best && outDistance) *outDistance = bestDistance;
    return best;
}
