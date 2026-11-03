#pragma once

#include "netvars.hpp"

class C_BaseEntity : public IClientEntity
{
public:
    NETVAR( vec3, m_vecOrigin, "DT_BaseEntity", "m_vecOrigin" )
    NETVAR( vec3, m_angRotation, "DT_BaseEntity", "m_angRotation" )
    NETVAR( i32, m_iTeamNum, "DT_BaseEntity", "m_iTeamNum" )
    NETVAR( i32, m_CollisionGroup, "DT_BaseEntity", "m_CollisionGroup" )
    NETVAR( float, m_flSimulationTime, "DT_BaseEntity", "m_flSimulationTime" )
    NETVAR( u8, m_ubInterpolationFrame, "DT_BaseEntity", "m_ubInterpolationFrame" )
    NETVAR( EHANDLE, m_hOwnerEntity, "DT_BaseEntity", "m_hOwnerEntity" )
    NETVAR( EHANDLE, m_hMoveParent, "DT_BaseEntity", "moveparent" )
    NETVAR( i16, m_nModelIndex, "DT_BaseEntity", "m_nModelIndex" )
    NETVAR( vec3, m_vecMins, "DT_BaseEntity", "m_vecMins" )
    NETVAR( vec3, m_vecMaxs, "DT_BaseEntity", "m_vecMaxs" )
    NETVAR( u16, m_usSolidFlags, "DT_BaseEntity", "m_usSolidFlags" )
    NETVAR( vec3, m_vecMinsPreScaled, "DT_BaseEntity", "m_vecMinsPreScaled" )
    NETVAR( vec3, m_vecMaxsPreScaled, "DT_BaseEntity", "m_vecMaxsPreScaled" )
    NETVAR( color_t, m_clrRender, "DT_BaseEntity", "m_clrRender" )
    NETVAR( f32, m_flGravity, "DT_BaseEntity", "m_flGravity" )
    NETVAR( i32, m_fEffects, "DT_BaseEntity", "m_fEffects" )

    NETVAR_OFFSET( float, m_flOldSimulationTime, "DT_BaseEntity", "m_flSimulationTime", 4 )
    NETVAR_OFFSET( vec3, m_vecOldOrigin, "DT_BaseEntity", "m_vecOrigin", -0x108 )

    CLASS_VAR( u8 *, m_pOriginalData, 1864 )
    u8 **m_pIntermediateData( )
    {
        return reinterpret_cast< u8 ** >( reinterpret_cast< u8 * >( this ) + 1144u );
    }

    DATAMAP_VAR( vec3, m_vecAbsVelocity )
    DATAMAP_VAR( vec3, m_vecAngVelocity )
    DATAMAP_VAR( i32, m_iEFlags )
    DATAMAP_VAR( u8, m_MoveType )

    datamap_t  *GetPredDescMap( );
    void        OnLatchInterpolatedVariables( i32 flags );
    const char *GetClassname( );
    vec3        WorldSpaceCenter( );
    void        EstimateAbsVelocity( );
    const vec3 &GetAbsVelocity( );
    SolidType_t GetSolid( );
    bool        ShouldCollide( i32 collision_group, i32 contents_mask );
};

class C_Precipitation : public C_BaseEntity
{
public:
    NETVAR( i32, m_nPrecipType, "DT_Precipitation", "m_nPrecipType" )
};

class C_BaseAnimating : public C_BaseEntity
{
public:
    NETVAR( i32, m_nSequence, "DT_BaseAnimating", "m_nSequence" )
    NETVAR( i32, m_nHitboxSet, "DT_BaseAnimating", "m_nHitboxSet" )
    NETVAR( float, m_flCycle, "DT_BaseAnimating", "m_flCycle" )
    NETVAR( float, m_flPlaybackRate, "DT_BaseAnimating", "m_flPlaybackRate" )
    NETVAR( float, m_flModelScale, "DT_BaseAnimating", "m_flModelScale" )
    NETVAR( i32, m_nSkin, "DT_BaseAnimating", "m_nSkin" )
    NETVAR_PTR( float, m_flPoseParameter, "DT_BaseAnimating", "m_flPoseParameter" )
    NETVAR_PTR( float, m_flEncodedController, "DT_BaseAnimating", "m_flEncodedController" )

    CLASS_VAR( bone_accessor_t, m_BoneAccessor, 2120 )
    CLASS_VAR( bool, m_bSequenceLoops, 2245 )
    CLASS_VAR( bone_cache_t, m_CachedBoneData, 2944 )
    CLASS_VAR( float, m_flAnimTime, 160 )

    float FrameAdvance( float interval );
    void  InvalidateBoneCache( );
    bool  GetAttachment( i32 number, vec3 &origin );
};

class C_BaseAnimatingOverlay : public C_BaseAnimating
{
public:
    CLASS_VAR( anim_overlay_t, m_AnimOverlay, 3096 )
};

class C_BaseFlex : public C_BaseAnimatingOverlay
{
public:
};

class C_BaseCombatCharacter : public C_BaseFlex
{
public:
    NETVAR( EHANDLE, m_hActiveWeapon, "DT_BaseCombatCharacter", "m_hActiveWeapon" )
    NETVAR( float, m_flNextAttack, "DT_BaseCombatCharacter", "m_flNextAttack" )
    NETVAR_PTR( EHANDLE, m_hMyWeapons, "DT_BaseCombatCharacter", "m_hMyWeapons" )
};

class C_BasePlayer : public C_BaseCombatCharacter
{
public:
    NETVAR( vec3, m_vecViewOffset, "DT_BasePlayer", "m_vecViewOffset[0]" )
    NETVAR( vec3, m_vecPunchAngle, "DT_BasePlayer", "m_vecPunchAngle" )
    NETVAR( vec3, m_vecVelocity, "DT_BasePlayer", "m_vecVelocity[0]" )
    NETVAR( vec3, m_vecBaseVelocity, "DT_BasePlayer", "m_vecBaseVelocity" )
    NETVAR( i32, m_iHealth, "DT_BasePlayer", "m_iHealth" )
    NETVAR( i32, m_iObserverMode, "DT_BasePlayer", "m_iObserverMode" )
    NETVAR( EHANDLE, m_hObserverTarget, "DT_BasePlayer", "m_hObserverTarget" )
    NETVAR( u8, m_lifeState, "DT_BasePlayer", "m_lifeState" )
    NETVAR( i32, m_fFlags, "DT_BasePlayer", "m_fFlags" )
    NETVAR( EHANDLE, m_hGroundEntity, "DT_BasePlayer", "m_hGroundEntity" )
    NETVAR( i32, m_nTickBase, "DT_BasePlayer", "m_nTickBase" )
    NETVAR( i32, m_iAmmo, "DT_BasePlayer", "m_iAmmo" )
    NETVAR( float, m_flMaxspeed, "DT_BasePlayer", "m_flMaxspeed" )
    NETVAR( i32, m_iFOV, "DT_BasePlayer", "m_iFOV" )
    NETVAR( i32, m_iDefaultFOV, "DT_BasePlayer", "m_iDefaultFOV" )
    NETVAR( float, m_flFOVTime, "DT_BasePlayer", "m_flFOVTime" )
    NETVAR( EHANDLE, m_hViewModel, "DT_BasePlayer", "m_hViewModel[0]" )
    NETVAR( float, m_flDuckAmount, "DT_BasePlayer", "m_flDuckAmount" )
    NETVAR( float, m_flStepSize, "DT_BasePlayer", "m_flStepSize" )
    NETVAR( bool, m_bDucked, "DT_BasePlayer", "m_bDucked" )
    NETVAR( bool, m_bDucking, "DT_BasePlayer", "m_bDucking" )
    NETVAR( float, m_flDucktime, "DT_BasePlayer", "m_flDucktime" )
    NETVAR_OFFSET( vec3, m_vecPreviouslyPredictedOrigin, "DT_BaseEntity", "m_vecOrigin", 0x1454 )

    DATAMAP_VAR( f32, m_surfaceFriction );
    DATAMAP_VAR( i32, m_nButtons )
    DATAMAP_VAR( i32, m_afButtonLast )
    DATAMAP_VAR( i32, m_afButtonPressed )
    DATAMAP_VAR( i32, m_afButtonReleased )
    DATAMAP_VAR( i32, m_nOldButtons )
    CLASS_VAR( handle_vector_t, m_hMyWearables, 6384 )
    CLASS_VAR( CUserCmd *, m_pCurrentCommand, 5672 ) // CPrediction::RunCommand sets it to the command it runs, clears it after RunPostThink

    vec3 Weapon_ShootPosition( );
    void ThirdPersonSwitch( bool thirdperson );
};

struct gesture_slot_t
{
    i32               m_iGestureSlot;
    i32               m_iActivity;
    bool              m_bAutoKill;
    bool              m_bActive;
    C_AnimationLayer *m_pAnimLayer;
};

static_assert( sizeof( gesture_slot_t ) == 24 );

struct gesture_slots_t
{
    gesture_slot_t *m_slots;
    i32             m_allocation_count;
    i32             m_grow_size;
    i32             m_count;
};

class CTFPlayerAnimState
{
public:
    void Update( float eye_yaw, float eye_pitch );
    void ClearStaleSwimming( );

    CLASS_VAR( bool, m_bForceAimYaw, 8 )
    CLASS_VAR( gesture_slots_t, m_aGestureSlots, 16 )
    CLASS_VAR( C_BaseEntity *, m_pEntity, 48 )
    CLASS_VAR( vec3, m_angRender, 56 )
    CLASS_VAR( i32, m_iAimYaw, 80 )
    CLASS_VAR( i32, m_iAimPitch, 84 )
    CLASS_VAR( float, m_flPoseTurnTime, 104 )
    CLASS_VAR( float, m_flEyeYaw, 140 )
    CLASS_VAR( float, m_flEyePitch, 144 )
    CLASS_VAR( float, m_flGoalFeetYaw, 148 )
    CLASS_VAR( float, m_flCurrentFeetYaw, 152 )
    CLASS_VAR( float, m_flLastAimTurnTime, 156 )
    CLASS_VAR( float, m_flJumpStartTime, 180 )
    CLASS_VAR( bool, m_bInSwim, 185 )
    CLASS_VAR( bool, m_bFirstSwimFrame, 186 )
    CLASS_VAR( i32, m_nSpecificMainSequence, 196 )
    CLASS_VAR( float, m_flHoldDeployedPoseUntilTime, 316 )
};

class C_TFWeaponBase;
class CTFPlayerShared;
class C_TFPlayer : public C_BasePlayer
{
public:
    vec3 GetClassEyeHeight( );

    NETVAR( i32, m_iClass, "DT_TFPlayer", "m_iClass" )
    NETVAR( i32, m_nPlayerCond, "DT_TFPlayer", "m_nPlayerCond" )
    NETVAR( i32, m_nPlayerState, "DT_TFPlayer", "m_nPlayerState" )
    NETVAR( i32, m_nPlayerCondEx, "DT_TFPlayer", "m_nPlayerCondEx" )
    NETVAR( i32, m_nPlayerCondEx2, "DT_TFPlayer", "m_nPlayerCondEx2" )
    NETVAR( i32, m_nPlayerCondEx3, "DT_TFPlayer", "m_nPlayerCondEx3" )
    NETVAR( i32, m_nPlayerCondEx4, "DT_TFPlayer", "m_nPlayerCondEx4" )
    NETVAR( i32, _condition_bits, "DT_TFPlayer", "_condition_bits" )
    NETVAR( i32, m_iAirDash, "DT_TFPlayer", "m_iAirDash" )
    NETVAR( i32, m_nAirDucked, "DT_TFPlayer", "m_nAirDucked" )
    NETVAR( f32, m_flDuckTimer, "DT_TFPlayer", "m_flDuckTimer" )
    NETVAR( f32, m_flChargeMeter, "DT_TFPlayer", "m_flChargeMeter" )
    NETVAR( f32, m_flItemChargeMeter, "DT_TFPlayer", "m_flItemChargeMeter" ) // [ LOADOUT_POSITION_PRIMARY ]
    NETVAR( i32, m_iStunFlags, "DT_TFPlayer", "m_iStunFlags" )
    NETVAR( i32, m_iMovementStunAmount, "DT_TFPlayer", "m_iMovementStunAmount" )
    NETVAR( float, m_flStealthNoAttackExpire, "DT_TFPlayer", "m_flStealthNoAttackExpire" )
    NETVAR( vec3, m_angEyeAngles, "DT_TFPlayer", "m_angEyeAngles[0]" )
    NETVAR( u8, m_nWaterLevel, "DT_TFPlayer", "m_nWaterLevel" )
    NETVAR( i32, m_iKills, "DT_TFPlayer", "m_iKills" )
    NETVAR( i32, m_iDeaths, "DT_TFPlayer", "m_iDeaths" )
    NETVAR( i32, m_iSpawnCounter, "DT_TFPlayer", "m_iSpawnCounter" )
    NETVAR( bool, m_bFeignDeathReady, "DT_TFPlayer", "m_bFeignDeathReady" )
    NETVAR( i32, m_iCritMult, "DT_TFPlayer", "m_iCritMult" )
    NETVAR_PTR( i32, m_nStreaks, "DT_TFPlayer", "m_nStreaks" )

    DATAMAP_VAR( bool, m_bScattergunJump )

    CLASS_VAR( CTFPlayerAnimState *, m_PlayerAnimState, 9048 )

    float            GetCritMult( );
    CTFPlayerShared *GetSharedPlayerData( );

    C_TFWeaponBase *Weapon_OwnsThisID( i32 weapon_id );

    bool InCond( const ETFCond cond );
    bool IsAlive( );
    bool IsOnGround( );
    bool IsDucked( );
    bool IsBurning( );
    bool IsCritBoosted( );
    bool IsMiniCritBoosted( );
    bool IsBonked( );
    bool IsDisguised( );
    bool IsCharging( );
    bool IsTaunting( );
    bool IsMarkedForDeath( );
    bool IsJarated( );
    bool IsStunned( );
    bool IsInKart( );
    bool IsGhost( );
    bool IsCloaked( );
    bool IsZoomed( );
    bool IsUbered( );
    bool IsQuickFixUbered( );
    bool IsRegenBuffed( );
    bool IsDefenseBuffed( );
    bool IsBannerBuffed( );
    bool IsBleeding( );
    bool IsMilked( );
    bool IsSlowed( );
    bool IsBulletResistant( );
    bool IsBlastResistant( );
    bool IsFireResistant( );
    bool IsBulletUbered( );
    bool IsBlastUbered( );
    bool IsFireUbered( );
    bool IsHighKD( );
    bool IsFeigned( );
    bool IsFrozen( );

    f32  TeamFortress_CalculateMaxSpeed( bool bIgnoreSpecialAbility = false ) const;
    bool IsPlayerOnSteamFriendsList( C_TFPlayer *player );
};

class CTFPlayerShared
{
public:
    NETVAR( float, m_flNextNoiseMakerTime, "DT_TFPlayerShared", "m_flNextNoiseMakerTime" )
    NETVAR( i32, m_nNumHealers, "DT_TFPlayerShared", "m_nNumHealers" )
};

class C_PlayerResource : public C_BaseEntity
{
public:
    NETVAR_PTR( i32, m_iHealth, "DT_PlayerResource", "m_iHealth" )
    NETVAR_PTR( i32, m_iPing, "DT_PlayerResource", "m_iPing" )
    NETVAR_PTR( i32, m_iTeam, "DT_PlayerResource", "m_iTeam" )
    NETVAR_PTR( bool, m_bAlive, "DT_PlayerResource", "m_bAlive" )
    NETVAR_PTR( bool, m_bValid, "DT_PlayerResource", "m_bValid" )
    NETVAR_PTR( bool, m_bConnected, "DT_PlayerResource", "m_bConnected" )
};

class C_TFPlayerResource : public C_PlayerResource
{
public:
    NETVAR_PTR( i32, m_iMaxHealth, "DT_TFPlayerResource", "m_iMaxHealth" )
    NETVAR_PTR( i32, m_iDamage, "DT_TFPlayerResource", "m_iDamage" )
    NETVAR_PTR( i32, m_iPlayerClass, "DT_TFPlayerResource", "m_iPlayerClass" )
    NETVAR_PTR( i32, m_iMaxBuffedHealth, "DT_TFPlayerResource", "m_iMaxBuffedHealth" )
};

struct CViewVectors
{
    vec3 m_vView{ };
    vec3 m_vHullMin{ };
    vec3 m_vHullMax{ };
    vec3 m_vDuckHullMin{ };
    vec3 m_vDuckHullMax{ };
    vec3 m_vDuckView{ };
    vec3 m_vObsHullMin{ };
    vec3 m_vObsHullMax{ };
    vec3 m_vDeadViewHeight{ };
};

class C_TFGameRules
{
public:
    const CViewVectors *GetViewVectors( );

    NETVAR( bool, m_bTruceActive, "DT_TFGameRulesProxy", "m_bTruceActive" )
    NETVAR( bool, m_bPowerupMode, "DT_TFGameRulesProxy", "m_bPowerupMode" )
    NETVAR( bool, m_bPlayingMannVsMachine, "DT_TFGameRulesProxy", "m_bPlayingMannVsMachine" )
    NETVAR( i32, m_nForceUpgrades, "DT_TFGameRulesProxy", "m_nForceUpgrades" )
    NETVAR( i32, m_nMatchGroupType, "DT_TFGameRulesProxy", "m_nMatchGroupType" )
    NETVAR( bool, m_bInSetup, "DT_TeamplayRoundBasedRulesProxy", "m_bInSetup" )
    NETVAR( i32, m_iRoundState, "DT_TeamplayRoundBasedRulesProxy", "m_iRoundState" )

    bool GameModeUsesUpgrades( )
    {
        const i32 forced{ this->m_nForceUpgrades( ) };
        return forced == 2 || ( forced != 1 && this->m_bPlayingMannVsMachine( ) );
    }
};

class C_BaseObject : public C_BaseCombatCharacter
{
public:
    NETVAR( i32, m_iHealth, "DT_BaseObject", "m_iHealth" )
    NETVAR( i32, m_iMaxHealth, "DT_BaseObject", "m_iMaxHealth" )
    NETVAR( i32, m_iUpgradeLevel, "DT_BaseObject", "m_iUpgradeLevel" )
    NETVAR( i32, m_iObjectMode, "DT_BaseObject", "m_iObjectMode" )
    NETVAR( bool, m_bMiniBuilding, "DT_BaseObject", "m_bMiniBuilding" )
    NETVAR( bool, m_bHasSapper, "DT_BaseObject", "m_bHasSapper" )
    NETVAR( bool, m_bCarried, "DT_BaseObject", "m_bCarried" )
    NETVAR( bool, m_bBuilding, "DT_BaseObject", "m_bBuilding" )
    NETVAR( bool, m_bPlacing, "DT_BaseObject", "m_bPlacing" )
    NETVAR( bool, m_bDisabled, "DT_BaseObject", "m_bDisabled" )
    NETVAR( bool, m_bPlasmaDisable, "DT_BaseObject", "m_bPlasmaDisable" )
};

class C_ObjectSentrygun : public C_BaseObject
{
public:
    NETVAR( i32, m_iAmmoShells, "DT_ObjectSentrygun", "m_iAmmoShells" )
    NETVAR( i32, m_iAmmoRockets, "DT_ObjectSentrygun", "m_iAmmoRockets" )
    NETVAR( u32, m_nShieldLevel, "DT_ObjectSentrygun", "m_nShieldLevel" )
};

class C_CaptureFlag : public C_BaseEntity
{
public:
    NETVAR( i32, m_nFlagStatus, "DT_CaptureFlag", "m_nFlagStatus" )
};

class C_ObjectTeleporter : public C_BaseObject
{
public:
    NETVAR( i32, m_iState, "DT_ObjectTeleporter", "m_iState" )
    NETVAR( float, m_flRechargeTime, "DT_ObjectTeleporter", "m_flRechargeTime" )
};

class C_TFGrenadePipebombProjectile : public C_BaseAnimating
{
public:
    NETVAR( vec3, m_vInitialVelocity, "DT_TFWeaponBaseGrenadeProj", "m_vInitialVelocity" )
    NETVAR( EHANDLE, m_hThrower, "DT_BaseGrenade", "m_hThrower" )
    NETVAR( bool, m_bTouched, "DT_TFProjectile_Pipebomb", "m_bTouched" )
    NETVAR( i32, m_iType, "DT_TFProjectile_Pipebomb", "m_iType" )
    NETVAR_OFFSET( float, m_flCreationTime, "DT_TFProjectile_Pipebomb", "m_iType", 4 )
    NETVAR( EHANDLE, m_hLauncher, "DT_TFProjectile_Pipebomb", "m_hLauncher" )
    NETVAR( bool, m_bCritical, "DT_TFWeaponBaseGrenadeProj", "m_bCritical" )
    NETVAR( i32, m_iDeflected, "DT_TFWeaponBaseGrenadeProj", "m_iDeflected" )
};

class C_TFProjectile_Rocket : public C_BaseAnimating
{
public:
    NETVAR( vec3, m_vInitialVelocity, "DT_TFBaseRocket", "m_vInitialVelocity" )
    NETVAR( i32, m_iDeflected, "DT_TFBaseRocket", "m_iDeflected" )
    NETVAR( bool, m_bCritical, "DT_TFProjectile_Rocket", "m_bCritical" )
    NETVAR( EHANDLE, m_hLauncher, "DT_TFBaseRocket", "m_hLauncher" )
};

class C_TFProjectile_Arrow : public C_BaseAnimating
{
public:
    NETVAR( bool, m_bCritical, "DT_TFProjectile_Arrow", "m_bCritical" )
    NETVAR( EHANDLE, m_hLauncher, "DT_TFBaseRocket", "m_hLauncher" )
};

class C_TFProjectile_Flare : public C_BaseAnimating
{
public:
    NETVAR( vec3, m_vInitialVelocity, "DT_TFBaseRocket", "m_vInitialVelocity" )
    NETVAR( bool, m_bCritical, "DT_TFProjectile_Flare", "m_bCritical" )
    NETVAR( EHANDLE, m_hLauncher, "DT_TFBaseRocket", "m_hLauncher" )
    NETVAR_OFFSET( float, m_flSpawnTime, "DT_TFBaseRocket", "m_hLauncher", -8 )
};

class C_EconEntity : public C_BaseAnimating
{
public:
    NETVAR( u16, m_iItemDefinitionIndex, "DT_EconEntity", "m_iItemDefinitionIndex" )
    NETVAR( u32, m_iAccountID, "DT_EconEntity", "m_iAccountID" )
    NETVAR( CAttributeList, m_AttributeList, "DT_EconEntity", "m_AttributeList" )
    CLASS_VAR( material_overrides_t, m_MaterialOverrides, 3624 )
    CLASS_VAR( EHANDLE, m_hViewmodelAttachment, 3664 )
    NETVAR_OFFSET( item_material_overrides_t, m_ItemMaterialOverrides, "DT_EconEntity", "m_iItemDefinitionIndex", -0x28 )
    NETVAR_OFFSET( u32, m_nWeaponSkinGeneration, "DT_EconEntity", "m_iTeamNumber", -0x10 )
};

class C_TFWearable : public C_EconEntity
{
public:
};

class C_TFRagdoll : public C_BaseEntity
{
public:
    NETVAR( EHANDLE, m_hPlayer, "DT_TFRagdoll", "m_hPlayer" )
    NETVAR( i32, m_iTeam, "DT_TFRagdoll", "m_iTeam" )
    NETVAR( bool, m_bGib, "DT_TFRagdoll", "m_bGib" )
    NETVAR( bool, m_bBurning, "DT_TFRagdoll", "m_bBurning" )
    NETVAR( bool, m_bElectrocuted, "DT_TFRagdoll", "m_bElectrocuted" )
    NETVAR( bool, m_bBecomeAsh, "DT_TFRagdoll", "m_bBecomeAsh" )
    NETVAR( bool, m_bGoldRagdoll, "DT_TFRagdoll", "m_bGoldRagdoll" )
    NETVAR( bool, m_bIceRagdoll, "DT_TFRagdoll", "m_bIceRagdoll" )
    NETVAR( i32, m_iDamageCustom, "DT_TFRagdoll", "m_iDamageCustom" )
};

class C_BaseViewModel : public C_BaseAnimating
{
public:
    NETVAR( EHANDLE, m_hWeapon, "DT_BaseViewModel", "m_hWeapon" )
};

class C_BaseCombatWeapon : public C_EconEntity
{
public:
    NETVAR( i32, m_iClip1, "DT_BaseCombatWeapon", "m_iClip1" )
    NETVAR( i32, m_iPrimaryAmmoType, "DT_BaseCombatWeapon", "m_iPrimaryAmmoType" )
    NETVAR( float, m_flNextPrimaryAttack, "DT_BaseCombatWeapon", "m_flNextPrimaryAttack" )
    NETVAR( float, m_flNextSecondaryAttack, "DT_BaseCombatWeapon", "m_flNextSecondaryAttack" )
    NETVAR( float, m_flTimeWeaponIdle, "DT_BaseCombatWeapon", "m_flTimeWeaponIdle" )

    DATAMAP_VAR( bool, m_bInReload )
    DATAMAP_VAR( bool, m_bFiringWholeClip )
};

class C_TFWeaponBase : public C_BaseCombatWeapon
{
public:
    NETVAR( float, m_flLastFireTime, "DT_TFWeaponBase", "m_flLastFireTime" )
    NETVAR( float, m_flEnergy, "DT_TFWeaponBase", "m_flEnergy" )
    NETVAR( float, m_flObservedCritChance, "DT_TFWeaponBase", "m_flObservedCritChance" )
    NETVAR( float, m_flLastCritCheckTime, "DT_TFWeaponBase", "m_flLastCritCheckTime" )
    NETVAR( bool, m_bLowered, "DT_TFWeaponBase", "m_bLowered" )
    NETVAR( i32, m_iReloadMode, "DT_TFWeaponBase", "m_iReloadMode" )
    NETVAR( float, m_flReloadPriorNextFire, "DT_TFWeaponBase", "m_flReloadPriorNextFire" )
    NETVAR( bool, m_bReloadedThroughAnimEvent, "DT_TFWeaponBase", "m_bReloadedThroughAnimEvent" )
    NETVAR( i32, m_iWorldModelIndex, "DT_TFWeaponBase", "m_iWorldModelIndex" )
    NETVAR_OFFSET( float, m_flSmackTime, "DT_TFWeaponBase", "m_nInspectStage", 0x1C )

    CLASS_VAR( i32, m_iCachedModelIndex, 4188 )

    i32            GetWeaponID( );
    i32            GetDamageType( );
    const char    *GetWeaponName( );
    float          ApplyFireDelay( float delay );
    CTFWeaponInfo *GetWeaponInfo( );
    i32            GetSlot( );
    bool           AreRandomCritsEnabled( );
    f32            GetProjectileBlastRadius( );
    bool           HasPrimaryAmmoForShot( C_BasePlayer *owner );

    CLASS_VAR( i32, m_iCurrentSeed, 4032 )
    CLASS_VAR( bool, m_bCurrentAttackIsCrit, 3994 )

    DATAMAP_VAR( float, m_flCritTokenBucket )
    DATAMAP_VAR( i32, m_nCritChecks )
    DATAMAP_VAR( i32, m_nCritSeedRequests )
    DATAMAP_VAR( i32, m_iWeaponMode )
    DATAMAP_VAR( float, m_flCritTime )
    DATAMAP_VAR( float, m_flLastRapidFireCritCheckTime )
    DATAMAP_VAR( i32, m_iLastCritCheckFrame )
};

class C_TFWeaponBaseGun : public C_TFWeaponBase
{
public:
    f32 GetProjectileDamage( );
    f32 CalcRangedDamage( C_BaseEntity *target_ent, bool force_crit = false );
};

class C_TFPipebombLauncher : public C_TFWeaponBaseGun
{
public:
    NETVAR( float, m_flChargeBeginTime, "DT_WeaponPipebombLauncher", "m_flChargeBeginTime" )
    NETVAR( i32, m_iPipebombCount, "DT_WeaponPipebombLauncher", "m_iPipebombCount" )
};

class C_TFCompoundBow : public C_TFPipebombLauncher
{
public:
    NETVAR( bool, m_bNoFire, "DT_WeaponCompoundBow", "m_bNoFire" )
};

class C_TFGrenadeLauncher : public C_TFWeaponBaseGun
{
public:
    NETVAR( float, m_flDetonateTime, "DT_WeaponGrenadeLauncher", "m_flDetonateTime" )
};

class C_TFSniperRifle : public C_TFWeaponBaseGun
{
public:
    NETVAR( float, m_flChargedDamage, "DT_TFSniperRifle", "m_flChargedDamage" )
    DATAMAP_VAR( float, m_flUnzoomTime )
    DATAMAP_VAR( float, m_flRezoomTime )

    bool IsReadyForHeadshot( );
};

class C_TFRevolver : public C_TFWeaponBaseGun
{
public:
    DATAMAP_VAR( f32, m_flLastAccuracyCheck )

    bool CanHeadshot( );
    bool IsReadyForHeadshot( );
    bool CanHeadshotTarget( C_TFPlayer *target );
};

class C_SniperDot : public C_BaseEntity
{
public:
    NETVAR( float, m_flChargeStartTime, "DT_SniperDot", "m_flChargeStartTime" )
};

class C_TFParticleCannon : public C_TFWeaponBaseGun
{
public:
    NETVAR( float, m_flChargeBeginTime, "DT_ParticleCannon", "m_flChargeBeginTime" )
};

class C_TFFlameThrower : public C_TFWeaponBaseGun
{
public:
};

class C_TFWeaponFlameBall : public C_TFFlameThrower
{
public:
    NETVAR( float, m_flRechargeScale, "DT_WeaponFlameBall", "m_flRechargeScale" )
};

class C_WeaponMedigun : public C_TFWeaponBaseGun
{
public:
    NETVAR( i32, m_nChargeResistType, "DT_WeaponMedigun", "m_nChargeResistType" )
    NETVAR( float, m_flChargeLevel, "DT_WeaponMedigun", "m_flChargeLevel" )
    NETVAR( EHANDLE, m_hHealingTarget, "DT_WeaponMedigun", "m_hHealingTarget" )
    NETVAR( bool, m_bChargeRelease, "DT_WeaponMedigun", "m_bChargeRelease" )

    i32 GetMedigunType( ) const;
};

class C_TFMinigun : public C_TFWeaponBaseGun
{
public:
    NETVAR( i32, m_iWeaponState, "DT_WeaponMinigun", "m_iWeaponState" )
    NETVAR( bool, m_bCritShot, "DT_WeaponMinigun", "m_bCritShot" )
    DATAMAP_VAR( float, m_flStartedFiringAt )
    DATAMAP_VAR( float, m_flStartedWindUpAt )
    CLASS_VAR( float, m_flBarrelCurrentVelocity, 4284 )
    CLASS_VAR( float, m_flBarrelTargetVelocity, 4288 )
    CLASS_VAR( float, m_flBarrelAngle, 4296 )

    void StartMuzzleEffect( );
    void StartBrassEffect( );
};
