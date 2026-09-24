                                                                              
std::atomic<ULONGLONG> funResetEpoch{0};
std::atomic_bool homerunKicksOn{false},groundSlamOn{false},chainExplosionsOn{false},gravityPunchOn{false};
std::atomic_bool vehicleBoostOn{false},grappleUpgradesOn{false},grappleMomentumOn{false},chaosHitsOn{false},driveByOn{false},driveByReady{false};
std::atomic<float> grappleSpeed{45.f};
std::atomic<ULONGLONG> slamRequested{0},boostRequested{0};
std::atomic_bool grappleReleaseRequested{false};
struct FunMeleeResult{bool valid=false,launch=false,pull=false,explode=false;int originalType=-1;bool changedType=false;V position{};uintptr_t player=0,victim=0;};
FunMeleeResult BeforeFunMelee(uintptr_t caller,uintptr_t attacker,uintptr_t victim,uintptr_t info);
void AfterFunMelee(const FunMeleeResult& hit,uintptr_t info);
void NotifyFunExplosion(uintptr_t owner,V position);
void PumpFunMovement(void*who,float dt);
void PumpVehicleBoost(void*who,float dt);
void ReleaseUpgradedGrapple(void*who);
float CurrentGrappleSpeed();
void ResetFunFeatures();
