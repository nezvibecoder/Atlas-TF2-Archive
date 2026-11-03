#pragma once

#include "base.hpp"

class bf_write
{
public:
    void StartWriting( void *data, i32 bytes, i32 start_bit = 0, i32 max_bits = -1 );

    void WriteUBitLong( u32 data, i32 numbits );
    bool WriteBits( const void *in, i32 bits );
    void WriteWord( i32 value );
    void WriteLong( i32 value );
    bool WriteString( const char *value );

    i32 GetNumBitsWritten( ) const
    {
        return m_iCurBit;
    }

    i32 GetNumBitsLeft( ) const
    {
        return m_nDataBits - m_iCurBit;
    }

    u8 *GetData( ) const
    {
        return reinterpret_cast< u8 * >( m_pData );
    }

    bool IsOverflowed( ) const
    {
        return m_bOverflow;
    }

    void SetOverflowFlag( )
    {
        m_bOverflow = true;
    }

    unsigned long *m_pData{ };
    i32            m_nDataBytes{ };
    i32            m_nDataBits{ -1 };
    i32            m_iCurBit{ };
    bool           m_bOverflow{ };
    bool           m_bAssertOnOverflow{ };
    const char    *m_pDebugName{ };
};

class bf_read
{
public:
    u32   ReadUBitLong( i32 numbits );
    u32   ReadOneBit( );
    u32   ReadVarInt32( );
    usize ReadString( char *out, usize capacity, bool line = false );
    void  Seek( i32 bit );

    inline long ReadLong( )
    {
        return ReadUBitLong( 32 );
    }

    inline int ReadByte( )
    {
        return ReadUBitLong( 8 );
    }

    unsigned long *m_pData{ };
    i32            m_nDataBytes{ };
    i32            m_nDataBits{ -1 };
    i32            m_iCurBit{ };
    bool           m_bOverflow{ };
    bool           m_bAssertOnOverflow{ };
    const char    *m_pDebugName{ };
};

static_assert( sizeof( bf_write ) == 0x20 );
static_assert( sizeof( bf_read ) == 0x20 );

typedef unsigned int CRC32_t;

inline constexpr usize MULTIPLAYER_BACKUP{ 90 };
inline constexpr usize MAX_VIEWMODELS{ 2 };

class CSaveRestoreData;
class CGlobalVarsBase
{
public:
    float             realtime{ };
    int               framecount{ };
    float             absoluteframetime{ };
    float             curtime{ };
    float             frametime{ };
    int               maxClients{ };
    int               tickcount{ };
    float             interval_per_tick{ };
    float             interpolation_amount{ };
    int               simTicksThisFrame{ };
    int               network_protocol{ };
    CSaveRestoreData *pSaveData{ };
    bool              m_bClient{ };
    int               nTimestampNetworkingBase{ };
    int               nTimestampRandomizeWindow{ };
};

enum StereoEye_t
{
    STEREO_EYE_MONO  = 0,
    STEREO_EYE_LEFT  = 1,
    STEREO_EYE_RIGHT = 2,
    STEREO_EYE_MAX   = 3,
};

class CViewSetup
{
public:
    // shared by 2D & 3D views

    // left side of view window
    int         x;
    int         m_nUnscaledX;
    // top side of view window
    int         y;
    int         m_nUnscaledY;
    // width of view window
    int         width;
    int         m_nUnscaledWidth;
    // height of view window
    int         height;
    // which eye are we rendering?
    StereoEye_t m_eStereoEye;
    int         m_nUnscaledHeight;

    // the rest are only used by 3D views

    // Orthographic projection?
    bool  m_bOrtho;
    // View-space rectangle for ortho projection.
    float m_OrthoLeft;
    float m_OrthoTop;
    float m_OrthoRight;
    float m_OrthoBottom;

    // horizontal FOV in degrees
    float fov;
    // horizontal FOV in degrees for in-view model
    float fovViewmodel;

    // 3D origin of camera
    vec3 origin;

    // heading of camera (pitch, yaw, roll)
    vec3  angles;
    // local Z coordinate of near plane of camera
    float zNear;
    // local Z coordinate of far plane of camera
    float zFar;

    // local Z coordinate of near plane of camera ( when rendering view model )
    float zNearViewmodel;
    // local Z coordinate of far plane of camera ( when rendering view model )
    float zFarViewmodel;

    // set to true if this is to draw into a subrect of the larger screen
    // this really is a hack, but no more than the rest of the way this class is used
    bool m_bRenderToSubrectOfLargerScreen;

    // The aspect ratio to use for computing the perspective projection matrix
    // (0.0f means use the viewport)
    float m_flAspectRatio;

    // Controls for off-center projection (needed for poster rendering)
    bool  m_bOffCenter;
    float m_flOffCenterTop;
    float m_flOffCenterBottom;
    float m_flOffCenterLeft;
    float m_flOffCenterRight;

    // Control that the SFM needs to tell the engine not to do certain post-processing steps
    bool m_bDoBloomAndToneMapping;

    // Cached mode for certain full-scene per-frame varying state such as sun entity coverage
    bool m_bCacheFullSceneState;

    // If using VR, the headset calibration will feed you a projection matrix per-eye.
    // This does NOT override the Z range - that will be set up as normal (i.e. the values in this matrix will be ignored).
    bool    m_bViewToProjectionOverride;
    VMatrix m_ViewToProjection; // NECESSARY
};

enum UserMessageType
{
    Geiger = 0,
    Train,
    HudText,
    SayText,
    SayText2,
    TextMsg,
    ResetHUD,
    GameTitle,
    ItemPickup,
    ShowMenu,
    Shake,
    Fade,
    VGUIMenu,
    Rumble,
    CloseCaption,
    SendAudio,
    VoiceMask,
    RequestState,
    Damage,
    HintText,
    KeyHintText,
    HudMsg,
    AmmoDenied,
    AchievementEvent,
    UpdateRadar,
    VoiceSubtitle,
    HudNotify,
    HudNotifyCustom,
    PlayerStatsUpdate,
    MapStatsUpdate,
    PlayerIgnited,
    PlayerIgnitedInv,
    HudArenaNotify,
    UpdateAchievement,
    TrainingMsg,
    TrainingObjective,
    DamageDodged,
    PlayerJarated,
    PlayerExtinguished,
    PlayerJaratedFade,
    PlayerShieldBlocked,
    BreakModel,
    CheapBreakModel,
    BreakModel_Pumpkin,
    BreakModelRocketDud,
    CallVoteFailed,
    VoteStart,
    VotePass,
    VoteFailed,
    VoteSetup,
    PlayerBonusPoints,
    RDTeamPointsChanged,
    SpawnFlyingBird,
    PlayerGodRayEffect,
    PlayerTeleportHomeEffect,
    MVMStatsReset,
    MVMPlayerEvent,
    MVMResetPlayerStats,
    MVMWaveFailed,
    MVMAnnouncement,
    MVMPlayerUpgradedEvent,
    MVMVictory,
    MVMWaveChange,
    MVMLocalPlayerUpgradesClear,
    MVMLocalPlayerUpgradesValue,
    MVMResetPlayerWaveSpendingStats,
    MVMLocalPlayerWaveSpendingValue,
    MVMResetPlayerUpgradeSpending,
    MVMServerKickTimeUpdate,
    PlayerLoadoutUpdated,
    PlayerTauntSoundLoopStart,
    PlayerTauntSoundLoopEnd,
    ForcePlayerViewAngles,
    BonusDucks,
    EOTLDuckEvent,
    PlayerPickupWeapon,
    QuestObjectiveCompleted,
    SPHapWeapEvent,
    HapDmg,
    HapPunch,
    HapSetDrag,
    HapSetConst,
    HapMeleeContact
};

// TODO: Perhaps we could dynamically dump these at runtime instead?
enum struct ETFClassID : int
{
    CAI_BaseNPC,
    CBaseAnimating,
    CBaseAnimatingOverlay,
    CBaseAttributableItem,
    CBaseCombatCharacter,
    CBaseCombatWeapon,
    CBaseDoor,
    CBaseEntity,
    CBaseFlex,
    CBaseGrenade,
    CBaseObject,
    CBaseObjectUpgrade,
    CBaseParticleEntity,
    CBasePlayer,
    CBaseProjectile,
    CBasePropDoor,
    CBaseTeamObjectiveResource,
    CBaseTempEntity,
    CBaseViewModel,
    CBeam,
    CBoneFollower,
    CBonusDuckPickup,
    CBonusPack,
    CBonusRoundLogic,
    CBreakableProp,
    CBreakableSurface,
    CCaptureFlag,
    CCaptureFlagReturnIcon,
    CCaptureZone,
    CColorCorrection,
    CColorCorrectionVolume,
    CCurrencyPack,
    CDynamicLight,
    CDynamicProp,
    CEconEntity,
    CEconWearable,
    CEmbers,
    CEntityDissolve,
    CEntityFlame,
    CEntityParticleTrail,
    CEnvDetailController,
    CEnvParticleScript,
    CEnvProjectedTexture,
    CEnvQuadraticBeam,
    CEnvScreenEffect,
    CEnvScreenOverlay,
    CEnvTonemapController,
    CEnvWind,
    CEyeballBoss,
    CFireSmoke,
    CFireTrail,
    CFish,
    CFogController,
    CFunc_Dust,
    CFunc_LOD,
    CFuncAreaPortalWindow,
    CFuncConveyor,
    CFuncForceField,
    CFuncLadder,
    CFuncMonitor,
    CFuncOccluder,
    CFuncPasstimeGoal,
    CFuncReflectiveGlass,
    CFuncRespawnRoom,
    CFuncRespawnRoomVisualizer,
    CFuncRotating,
    CFuncSmokeVolume,
    CFuncTrackTrain,
    CGameRulesProxy,
    CHalloweenGiftPickup,
    CHalloweenPickup,
    CHalloweenSoulPack,
    CHandleTest,
    CHeadlessHatman,
    CHightower_TeleportVortex,
    CInfoLadderDismount,
    CInfoLightingRelative,
    CInfoOverlayAccessor,
    CLaserDot,
    CLightGlow,
    CMannVsMachineStats,
    CMaterialModifyControl,
    CMerasmus,
    CMerasmusDancer,
    CMonsterResource,
    CObjectCartDispenser,
    CObjectDispenser,
    CObjectSapper,
    CObjectSentrygun,
    CObjectTeleporter,
    CParticleFire,
    CParticlePerformanceMonitor,
    CParticleSystem,
    CPasstimeBall,
    CPasstimeGun,
    CPhysBox,
    CPhysBoxMultiplayer,
    CPhysicsProp,
    CPhysicsPropMultiplayer,
    CPhysMagnet,
    CPlasma,
    CPlayerDestructionDispenser,
    CPlayerResource,
    CPointCamera,
    CPointCommentaryNode,
    CPointWorldText,
    CPoseController,
    CPrecipitation,
    CPropVehicleDriveable,
    CRagdollManager,
    CRagdollProp,
    CRagdollPropAttached,
    CRobotDispenser,
    CRopeKeyframe,
    CSceneEntity,
    CShadowControl,
    CSlideshowDisplay,
    CSmokeStack,
    CSniperDot,
    CSpotlightEnd,
    CSprite,
    CSpriteOriented,
    CSpriteTrail,
    CSteamJet,
    CSun,
    CTeam,
    CTeamplayRoundBasedRulesProxy,
    CTeamRoundTimer,
    CTeamTrainWatcher,
    CTEArmorRicochet,
    CTEBaseBeam,
    CTEBeamEntPoint,
    CTEBeamEnts,
    CTEBeamFollow,
    CTEBeamLaser,
    CTEBeamPoints,
    CTEBeamRing,
    CTEBeamRingPoint,
    CTEBeamSpline,
    CTEBloodSprite,
    CTEBloodStream,
    CTEBreakModel,
    CTEBSPDecal,
    CTEBubbles,
    CTEBubbleTrail,
    CTEClientProjectile,
    CTEDecal,
    CTEDust,
    CTEDynamicLight,
    CTEEffectDispatch,
    CTEEnergySplash,
    CTEExplosion,
    CTEFireBullets,
    CTEFizz,
    CTEFootprintDecal,
    CTEGaussExplosion,
    CTEGlowSprite,
    CTEImpact,
    CTEKillPlayerAttachments,
    CTELargeFunnel,
    CTeleportVortex,
    CTEMetalSparks,
    CTEMuzzleFlash,
    CTEParticleSystem,
    CTEPhysicsProp,
    CTEPlayerAnimEvent,
    CTEPlayerDecal,
    CTEProjectedDecal,
    CTEShatterSurface,
    CTEShowLine,
    CTesla,
    CTESmoke,
    CTESparks,
    CTESprite,
    CTESpriteSpray,
    CTest_ProxyToggle_Networkable,
    CTestTraceline,
    CTETFBlood,
    CTETFExplosion,
    CTETFParticleEffect,
    CTEWorldDecal,
    CTFAmmoPack,
    CTFBall_Ornament,
    CTFBaseBoss,
    CTFBaseProjectile,
    CTFBaseRocket,
    CTFBat,
    CTFBat_Fish,
    CTFBat_Giftwrap,
    CTFBat_Wood,
    CTFBonesaw,
    CTFBotHintEngineerNest,
    CTFBottle,
    CTFBreakableMelee,
    CTFBreakableSign,
    CTFBuffItem,
    CTFCannon,
    CTFChargedSMG,
    CTFCleaver,
    CTFClub,
    CTFCompoundBow,
    CTFCrossbow,
    CTFDRGPomson,
    CTFDroppedWeapon,
    CTFFireAxe,
    CTFFists,
    CTFFlameManager,
    CTFFlameRocket,
    CTFFlameThrower,
    CTFFlareGun,
    CTFFlareGun_Revenge,
    CTFGameRulesProxy,
    CTFGasManager,
    CTFGenericBomb,
    CTFGlow,
    CTFGrapplingHook,
    CTFGrenadeLauncher,
    CTFGrenadePipebombProjectile,
    CTFHalloweenMinigame,
    CTFHalloweenMinigame_FallingPlatforms,
    CTFHellZap,
    CTFItem,
    CTFJar,
    CTFJarGas,
    CTFJarMilk,
    CTFKatana,
    CTFKnife,
    CTFLaserPointer,
    CTFLunchBox,
    CTFLunchBox_Drink,
    CTFMechanicalArm,
    CTFMedigunShield,
    CTFMiniGame,
    CTFMinigameLogic,
    CTFMinigun,
    CTFObjectiveResource,
    CTFParachute,
    CTFParachute_Primary,
    CTFParachute_Secondary,
    CTFParticleCannon,
    CTFPasstimeLogic,
    CTFPEPBrawlerBlaster,
    CTFPipebombLauncher,
    CTFPistol,
    CTFPistol_Scout,
    CTFPistol_ScoutPrimary,
    CTFPistol_ScoutSecondary,
    CTFPlayer,
    CTFPlayerDestructionLogic,
    CTFPlayerResource,
    CTFPointManager,
    CTFPowerupBottle,
    CTFProjectile_Arrow,
    CTFProjectile_BallOfFire,
    CTFProjectile_Cleaver,
    CTFProjectile_EnergyBall,
    CTFProjectile_EnergyRing,
    CTFProjectile_Flare,
    CTFProjectile_GrapplingHook,
    CTFProjectile_HealingBolt,
    CTFProjectile_Jar,
    CTFProjectile_JarGas,
    CTFProjectile_JarMilk,
    CTFProjectile_MechanicalArmOrb,
    CTFProjectile_Rocket,
    CTFProjectile_SentryRocket,
    CTFProjectile_SpellBats,
    CTFProjectile_SpellFireball,
    CTFProjectile_SpellKartBats,
    CTFProjectile_SpellKartOrb,
    CTFProjectile_SpellLightningOrb,
    CTFProjectile_SpellMeteorShower,
    CTFProjectile_SpellMirv,
    CTFProjectile_SpellPumpkin,
    CTFProjectile_SpellSpawnBoss,
    CTFProjectile_SpellSpawnHorde,
    CTFProjectile_SpellSpawnZombie,
    CTFProjectile_SpellTransposeTeleport,
    CTFProjectile_Throwable,
    CTFProjectile_ThrowableBreadMonster,
    CTFProjectile_ThrowableBrick,
    CTFProjectile_ThrowableRepel,
    CTFPumpkinBomb,
    CTFRagdoll,
    CTFRaygun,
    CTFReviveMarker,
    CTFRevolver,
    CTFRobotArm,
    CTFRobotDestruction_Robot,
    CTFRobotDestruction_RobotGroup,
    CTFRobotDestruction_RobotSpawn,
    CTFRobotDestructionLogic,
    CTFRocketLauncher,
    CTFRocketLauncher_AirStrike,
    CTFRocketLauncher_DirectHit,
    CTFRocketLauncher_Mortar,
    CTFRocketPack,
    CTFScatterGun,
    CTFShotgun,
    CTFShotgun_HWG,
    CTFShotgun_Pyro,
    CTFShotgun_Revenge,
    CTFShotgun_Soldier,
    CTFShotgunBuildingRescue,
    CTFShovel,
    CTFSlap,
    CTFSMG,
    CTFSniperRifle,
    CTFSniperRifleClassic,
    CTFSniperRifleDecap,
    CTFSodaPopper,
    CTFSpellBook,
    CTFStickBomb,
    CTFStunBall,
    CTFSword,
    CTFSyringeGun,
    CTFTankBoss,
    CTFTauntProp,
    CTFTeam,
    CTFThrowable,
    CTFViewModel,
    CTFWeaponBase,
    CTFWeaponBaseGrenadeProj,
    CTFWeaponBaseGun,
    CTFWeaponBaseMelee,
    CTFWeaponBaseMerasmusGrenade,
    CTFWeaponBuilder,
    CTFWeaponFlameBall,
    CTFWeaponInvis,
    CTFWeaponPDA,
    CTFWeaponPDA_Engineer_Build,
    CTFWeaponPDA_Engineer_Destroy,
    CTFWeaponPDA_Spy,
    CTFWeaponPDAExpansion_Dispenser,
    CTFWeaponPDAExpansion_Teleporter,
    CTFWeaponSapper,
    CTFWearable,
    CTFWearableCampaignItem,
    CTFWearableDemoShield,
    CTFWearableItem,
    CTFWearableLevelableItem,
    CTFWearableRazorback,
    CTFWearableRobotArm,
    CTFWearableVM,
    CTFWrench,
    CVGuiScreen,
    CVoteController,
    CWaterBullet,
    CWaterLODControl,
    CWeaponIFMBase,
    CWeaponIFMBaseCamera,
    CWeaponIFMSteadyCam,
    CWeaponMedigun,
    CWorld,
    CZombie,
    DustTrail,
    MovieExplosion,
    NextBotCombatCharacter,
    ParticleSmokeGrenade,
    RocketTrail,
    SmokeTrail,
    SporeExplosion,
    SporeTrail,
};

//-----------------------------------------------------------------------------
// Purpose: The engine reports to the client DLL what stage it's entering so the DLL can latch events
//  and make sure that certain operations only happen during the right stages.
// The value for each stage goes up as you move through the frame so you can check ranges of values
//  and if new stages get added in-between, the range is still valid.
//-----------------------------------------------------------------------------
enum ClientFrameStage_t
{
    FRAME_UNDEFINED = -1, // (haven't run any frames yet)
    FRAME_START,

    // A network packet is being recieved
    FRAME_NET_UPDATE_START,
    // Data has been received and we're going to start calling PostDataUpdate
    FRAME_NET_UPDATE_POSTDATAUPDATE_START,
    // Data has been received and we've called PostDataUpdate on all data recipients
    FRAME_NET_UPDATE_POSTDATAUPDATE_END,
    // We've received all packets, we can now do interpolation, prediction, etc..
    FRAME_NET_UPDATE_END,

    // We're about to start rendering the scene
    FRAME_RENDER_START,
    // We've finished rendering the scene.
    FRAME_RENDER_END
};

enum SoundChannel
{
    CHAN_REPLACE    = -1,
    CHAN_AUTO       = 0,
    CHAN_WEAPON     = 1,
    CHAN_VOICE      = 2,
    CHAN_ITEM       = 3,
    CHAN_BODY       = 4,
    CHAN_STREAM     = 5,
    CHAN_STATIC     = 6,
    CHAN_VOICE_BASE = 7,
    CHAN_USER_BASE  = CHAN_VOICE_BASE + 128
};

inline constexpr i32 PITCH_NORM{ 100 };

class CSfxTable
{
public:
    virtual const char *getname( ) = 0;
};

struct StartSoundParams_t
{
    bool       staticsound;
    int        userdata;
    int        soundsource;
    int        entchannel;
    CSfxTable *pSfx;
    vec3       origin;
    vec3       direction;
    bool       bUpdatePositions;
    float      fvol;
    int        soundlevel;
    int        flags;
    int        pitch;
    int        specialdsp;
    bool       fromserver;
    float      delay;
    int        speakerentity;
    bool       suppressrecording;
    int        initialStreamPosition;
};

enum ETFCond
{
    TF_COND_INVALID                          = -1,
    TF_COND_AIMING                           = 0, // Sniper aiming, Heavy minigun.
    TF_COND_ZOOMED                           = 1,
    TF_COND_DISGUISING                       = 2,
    TF_COND_DISGUISED                        = 3,
    TF_COND_STEALTHED                        = 4, // Spy specific
    TF_COND_INVULNERABLE                     = 5,
    TF_COND_TELEPORTED                       = 6,
    TF_COND_TAUNTING                         = 7,
    TF_COND_INVULNERABLE_WEARINGOFF          = 8,
    TF_COND_STEALTHED_BLINK                  = 9,
    TF_COND_SELECTED_TO_TELEPORT             = 10,
    TF_COND_CRITBOOSTED                      = 11, // DO NOT RE-USE THIS -- THIS IS FOR KRITZKRIEG AND REVENGE CRITS ONLY
    TF_COND_TMPDAMAGEBONUS                   = 12,
    TF_COND_FEIGN_DEATH                      = 13,
    TF_COND_PHASE                            = 14,
    TF_COND_STUNNED                          = 15, // Any type of stun. Check iStunFlags for more info.
    TF_COND_OFFENSEBUFF                      = 16,
    TF_COND_SHIELD_CHARGE                    = 17,
    TF_COND_DEMO_BUFF                        = 18,
    TF_COND_ENERGY_BUFF                      = 19,
    TF_COND_RADIUSHEAL                       = 20,
    TF_COND_HEALTH_BUFF                      = 21,
    TF_COND_BURNING                          = 22,
    TF_COND_HEALTH_OVERHEALED                = 23,
    TF_COND_URINE                            = 24,
    TF_COND_BLEEDING                         = 25,
    TF_COND_DEFENSEBUFF                      = 26, // 35% defense! No crit damage.
    TF_COND_MAD_MILK                         = 27,
    TF_COND_MEGAHEAL                         = 28,
    TF_COND_REGENONDAMAGEBUFF                = 29,
    TF_COND_MARKEDFORDEATH                   = 30,
    TF_COND_NOHEALINGDAMAGEBUFF              = 31,
    TF_COND_SPEED_BOOST                      = 32, // = 32
    TF_COND_CRITBOOSTED_PUMPKIN              = 33, // Brandon hates bits
    TF_COND_CRITBOOSTED_USER_BUFF            = 34,
    TF_COND_CRITBOOSTED_DEMO_CHARGE          = 35,
    TF_COND_SODAPOPPER_HYPE                  = 36,
    TF_COND_CRITBOOSTED_FIRST_BLOOD          = 37, // arena mode first blood
    TF_COND_CRITBOOSTED_BONUS_TIME           = 38,
    TF_COND_CRITBOOSTED_CTF_CAPTURE          = 39,
    TF_COND_CRITBOOSTED_ON_KILL              = 40, // =40. KGB, etc.
    TF_COND_CANNOT_SWITCH_FROM_MELEE         = 41,
    TF_COND_DEFENSEBUFF_NO_CRIT_BLOCK        = 42, // 35% defense! Still damaged by crits.
    TF_COND_REPROGRAMMED                     = 43, // Bots only
    TF_COND_CRITBOOSTED_RAGE_BUFF            = 44,
    TF_COND_DEFENSEBUFF_HIGH                 = 45, // 75% defense! Still damaged by crits.
    TF_COND_SNIPERCHARGE_RAGE_BUFF           = 46, // Sniper Rage - Charge time speed up
    TF_COND_DISGUISE_WEARINGOFF              = 47, // Applied for half-second post-disguise
    TF_COND_MARKEDFORDEATH_SILENT            = 48, // Sans sound
    TF_COND_DISGUISED_AS_DISPENSER           = 49,
    TF_COND_SAPPED                           = 50, // =50. Bots only
    TF_COND_INVULNERABLE_HIDE_UNLESS_DAMAGED = 51,
    TF_COND_INVULNERABLE_USER_BUFF           = 52,
    TF_COND_HALLOWEEN_BOMB_HEAD              = 53,
    TF_COND_HALLOWEEN_THRILLER               = 54,
    TF_COND_RADIUSHEAL_ON_DAMAGE             = 55,
    TF_COND_CRITBOOSTED_CARD_EFFECT          = 56,
    TF_COND_INVULNERABLE_CARD_EFFECT         = 57,
    TF_COND_MEDIGUN_UBER_BULLET_RESIST       = 58,
    TF_COND_MEDIGUN_UBER_BLAST_RESIST        = 59,
    TF_COND_MEDIGUN_UBER_FIRE_RESIST         = 60, // =60
    TF_COND_MEDIGUN_SMALL_BULLET_RESIST      = 61,
    TF_COND_MEDIGUN_SMALL_BLAST_RESIST       = 62,
    TF_COND_MEDIGUN_SMALL_FIRE_RESIST        = 63,
    TF_COND_STEALTHED_USER_BUFF              = 64, // Any class can have this
    TF_COND_MEDIGUN_DEBUFF                   = 65,
    TF_COND_STEALTHED_USER_BUFF_FADING       = 66,
    TF_COND_BULLET_IMMUNE                    = 67,
    TF_COND_BLAST_IMMUNE                     = 68,
    TF_COND_FIRE_IMMUNE                      = 69,
    TF_COND_PREVENT_DEATH                    = 70, // =70
    TF_COND_MVM_BOT_STUN_RADIOWAVE           = 71, // Bots only
    TF_COND_HALLOWEEN_SPEED_BOOST            = 72,
    TF_COND_HALLOWEEN_QUICK_HEAL             = 73,
    TF_COND_HALLOWEEN_GIANT                  = 74,
    TF_COND_HALLOWEEN_TINY                   = 75,
    TF_COND_HALLOWEEN_IN_HELL                = 76,
    TF_COND_HALLOWEEN_GHOST_MODE             = 77, // =77
    TF_COND_MINICRITBOOSTED_ON_KILL          = 78,
    TF_COND_OBSCURED_SMOKE                   = 79,
    TF_COND_PARACHUTE_ACTIVE                 = 80, // actively being used (not retracted)
    TF_COND_BLASTJUMPING                     = 81,
    TF_COND_HALLOWEEN_KART                   = 82,
    TF_COND_HALLOWEEN_KART_DASH              = 83,
    TF_COND_BALLOON_HEAD                     = 84, // =84 larger head, lower-gravity-feeling jumps
    TF_COND_MELEE_ONLY                       = 85, // =85 melee only
    TF_COND_SWIMMING_CURSE                   = 86, // player movement become swimming movement
    TF_COND_FREEZE_INPUT                     = 87, // freezes player input
    TF_COND_HALLOWEEN_KART_CAGE              = 88, // attach cage model to player while in kart
    TF_COND_DONOTUSE_0                       = 89,
    TF_COND_RUNE_STRENGTH                    = 90,
    TF_COND_RUNE_HASTE                       = 91,
    TF_COND_RUNE_REGEN                       = 92,
    TF_COND_RUNE_RESIST                      = 93,
    TF_COND_RUNE_VAMPIRE                     = 94,
    TF_COND_RUNE_REFLECT                     = 95,
    TF_COND_RUNE_PRECISION                   = 96,
    TF_COND_RUNE_AGILITY                     = 97,
    TF_COND_GRAPPLINGHOOK                    = 98,
    TF_COND_GRAPPLINGHOOK_SAFEFALL           = 99,
    TF_COND_GRAPPLINGHOOK_LATCHED            = 100,
    TF_COND_GRAPPLINGHOOK_BLEEDING           = 101,
    TF_COND_AFTERBURN_IMMUNE                 = 102,
    TF_COND_RUNE_KNOCKOUT                    = 103,
    TF_COND_RUNE_IMBALANCE                   = 104,
    TF_COND_CRITBOOSTED_RUNE_TEMP            = 105,
    TF_COND_PASSTIME_INTERCEPTION            = 106,
    TF_COND_SWIMMING_NO_EFFECTS              = 107, // =107_DNOC_FT
    TF_COND_PURGATORY                        = 108,
    TF_COND_RUNE_KING                        = 109,
    TF_COND_RUNE_PLAGUE                      = 110,
    TF_COND_RUNE_SUPERNOVA                   = 111,
    TF_COND_PLAGUE                           = 112,
    TF_COND_KING_BUFFED                      = 113,
    TF_COND_TEAM_GLOWS                       = 114, // used to show team glows to living players
    TF_COND_KNOCKED_INTO_AIR                 = 115,
    TF_COND_COMPETITIVE_WINNER               = 116,
    TF_COND_COMPETITIVE_LOSER                = 117,
    TF_COND_HEALING_DEBUFF                   = 118,
    TF_COND_PASSTIME_PENALTY_DEBUFF          = 119, // when carrying the ball without any teammates nearby
    TF_COND_GRAPPLED_TO_PLAYER               = 120,
    TF_COND_GRAPPLED_BY_PLAYER               = 121,
    TF_COND_PARACHUTE_DEPLOYED               = 122, // activated at least once while player's been airborne, but not does mean it's active now (see TF_COND_PARACHUTE_ACTIVE)
    TF_COND_GAS                              = 123,
    TF_COND_BURNING_PYRO                     = 124,
    TF_COND_ROCKETPACK                       = 125,
    // Players who lose their footing have lessened friction and don't re-stick to the ground unless they're below a
    // tf_movement_lost_footing_restick speed
    TF_COND_LOST_FOOTING                     = 126,
    // When in the air, slide up/along surfaces with momentum as if caught up in a... blast of air of some sort.
    // Reduces air control as well.  See tf_movement_aircurrent convars.  Removed upon touching ground.
    TF_COND_AIR_CURRENT                      = 127,
    TF_COND_HALLOWEEN_HELL_HEAL              = 128,
    TF_COND_POWERUPMODE_DOMINANT             = 129,
    TF_COND_IMMUNE_TO_PUSHBACK               = 130,
    //
    // ADD NEW ITEMS HERE TO AVOID BREAKING DEMOS
    //

    // ******** Keep this block last! ********
    // Keep experimental conditions below and graduate out of it before shipping

    TF_COND_LAST
};

enum ETFConds
{
    TFCond_Slowed                   = ( 1 << 0 ),  // Toggled when a player is slowed down.
    TFCond_Zoomed                   = ( 1 << 1 ),  // Toggled when a player is zoomed.
    TFCond_Disguising               = ( 1 << 2 ),  // Toggled when a Spy is disguising.
    TFCond_Disguised                = ( 1 << 3 ),  // Toggled when a Spy is disguised.
    TFCond_Cloaked                  = ( 1 << 4 ),  // Toggled when a Spy is invisible.
    TFCond_Ubercharged              = ( 1 << 5 ),  // Toggled when a player is UberCharged.
    TFCond_TeleportedGlow           = ( 1 << 6 ),  // Toggled when someone leaves a teleporter and has glow beneath their feet.
    TFCond_Taunting                 = ( 1 << 7 ),  // Toggled when a player is taunting.
    TFCond_UberchargeFading         = ( 1 << 8 ),  // Toggled when the UberCharge is fading.
    TFCond_CloakFlicker             = ( 1 << 9 ),  // Toggled when a Spy is visible during cloak.
    TFCond_Teleporting              = ( 1 << 10 ), // Only activates for a brief second when the player is being teleported; not very useful.
    TFCond_Kritzkrieged             = ( 1 << 11 ), // Toggled when a player is being crit buffed by the KritzKrieg.
    TFCond_TmpDamageBonus           = ( 1 << 12 ), // Unknown what this is for. Name taken from the AlliedModders SDK.
    TFCond_DeadRingered             = ( 1 << 13 ), // Toggled when a player is taking reduced damage from the Deadringer.
    TFCond_Bonked                   = ( 1 << 14 ), // Toggled when a player is under the effects of The Bonk! Atomic Punch.
    TFCond_Stunned                  = ( 1 << 15 ), // Toggled when a player's speed is reduced from airblast or a Sandman ball.
    TFCond_Buffed                   = ( 1 << 16 ), // Toggled when a player is within range of an activated Buff Banner.
    TFCond_Charging                 = ( 1 << 17 ), // Toggled when a Demoman charges with the shield.
    TFCond_DemoBuff                 = ( 1 << 18 ), // Toggled when a Demoman has heads from the Eyelander.
    TFCond_CritCola                 = ( 1 << 19 ), // Toggled when the player is under the effect of The Crit-a-Cola.
    TFCond_InHealRadius             = ( 1 << 20 ), // Unused condition, name taken from AlliedModders SDK.
    TFCond_Healing                  = ( 1 << 21 ), // Toggled when someone is being healed by a medic or a dispenser.
    TFCond_OnFire                   = ( 1 << 22 ), // Toggled when a player is on fire.
    TFCond_Overhealed               = ( 1 << 23 ), // Toggled when a player has >100% health.
    TFCond_Jarated                  = ( 1 << 24 ), // Toggled when a player is hit with a Sniper's Jarate.
    TFCond_Bleeding                 = ( 1 << 25 ), // Toggled when a player is taking bleeding damage.
    TFCond_DefenseBuffed            = ( 1 << 26 ), // Toggled when a player is within range of an activated Battalion's Backup.
    TFCond_Milked                   = ( 1 << 27 ), // Player was hit with a jar of Mad Milk.
    TFCond_MegaHeal                 = ( 1 << 28 ), // Player is under the effect of Quick-Fix charge.
    TFCond_RegenBuffed              = ( 1 << 29 ), // Toggled when a player is within a Concheror's range.
    TFCond_MarkedForDeath           = ( 1 << 30 ), // Player is marked for death by a Fan O'War hit. Effects are similar to TFCond_Jarated.
    TFCond_NoHealingDamageBuff      = ( 1 << 31 ), // Unknown what this is used for.
    TFCondEx_SpeedBuffAlly          = ( 1 << 0 ),  // Toggled when a player gets hit with the disciplinary action.
    TFCondEx_HalloweenCritCandy     = ( 1 << 1 ),  // Only for Scream Fortress event maps that drop crit candy.
    TFCondEx_CritCanteen            = ( 1 << 2 ),  // Player is getting a crit boost from a MVM canteen.
    TFCondEx_CritDemoCharge         = ( 1 << 3 ),  // From demo's shield
    TFCondEx_CritHype               = ( 1 << 4 ),  // Soda Popper crits.
    TFCondEx_CritOnFirstBlood       = ( 1 << 5 ),  // Arena first blood crit buff.
    TFCondEx_CritOnWin              = ( 1 << 6 ),  // End of round crits.
    TFCondEx_CritOnFlagCapture      = ( 1 << 7 ),  // CTF intelligence capture crits.
    TFCondEx_CritOnKill             = ( 1 << 8 ),  // Unknown what this is for.
    TFCondEx_RestrictToMelee        = ( 1 << 9 ),  // Unknown what this is for.
    TFCondEx_DefenseBuffNoCritBlock = ( 1 << 10 ), // MvM Buff.
    TFCondEx_Reprogrammed           = ( 1 << 11 ), // MvM Bot has been reprogrammed.
    TFCondEx_PyroCrits              = ( 1 << 12 ), // Player is getting crits from the Mmmph charge.
    TFCondEx_PyroHeal               = ( 1 << 13 ), // Player is being healed from the Mmmph charge.
    TFCondEx_FocusBuff              = ( 1 << 14 ), // Player is getting a focus buff.
    TFCondEx_DisguisedRemoved       = ( 1 << 15 ), // Disguised remove from a bot.
    TFCondEx_MarkedForDeathSilent   = ( 1 << 16 ), // Player is under the effects of the Escape Plan/Equalizer or GRU.
    TFCondEx_DisguisedAsDispenser   = ( 1 << 17 ), // Bot is disguised as dispenser.
    TFCondEx_InKart                 = ( 1 << 18 ), // MvM bot is being sapped.
    TFCondEx_UberchargedHidden      = ( 1 << 19 ), // MvM Related
    TFCondEx_UberchargedCanteen     = ( 1 << 20 ), // Player is receiving UberCharge from a canteen.
    TFCondEx_HalloweenBombHead      = ( 1 << 21 ), // Player has a bomb on their head from Merasmus.
    TFCondEx_HalloweenThriller      = ( 1 << 22 ), // Players are forced to dance from Merasmus.
    TFCondEx_BulletCharge           = ( 1 << 26 ), // Player is receiving 75% reduced damage from bullets.
    TFCondEx_ExplosiveCharge        = ( 1 << 27 ), // Player is receiving 75% reduced damage from explosives.
    TFCondEx_FireCharge             = ( 1 << 28 ), // Player is receiving 75% reduced damage from fire.
    TFCondEx_BulletResistance       = ( 1 << 29 ), // Player is receiving 10% reduced damage from bullets.
    TFCondEx_ExplosiveResistance    = ( 1 << 30 ), // Player is receiving 10% reduced damage from explosives.
    TFCondEx_FireResistance         = ( 1 << 31 ), // Player is receiving 10% reduced damage from fire.
    TFCondEx2_Stealthed             = ( 1 << 0 ),
    TFCondEx2_MedigunDebuff         = ( 1 << 1 ),
    TFCondEx2_StealthedUserBuffFade = ( 1 << 2 ),
    TFCondEx2_BulletImmune          = ( 1 << 3 ),
    TFCondEx2_BlastImmune           = ( 1 << 4 ),
    TFCondEx2_FireImmune            = ( 1 << 5 ),
    TFCondEx2_PreventDeath          = ( 1 << 6 ),
    TFCondEx2_MVMBotRadiowave       = ( 1 << 7 ),
    TFCondEx2_HalloweenSpeedBoost   = ( 1 << 8 ),  // Wheel has granted player speed boost.
    TFCondEx2_HalloweenQuickHeal    = ( 1 << 9 ),  // Wheel has granted player quick heal.
    TFCondEx2_HalloweenGiant        = ( 1 << 10 ), // Wheel has granted player giant mode.
    TFCondEx2_HalloweenTiny         = ( 1 << 11 ), // Wheel has granted player tiny mode.
    TFCondEx2_HalloweenInHell       = ( 1 << 12 ), // Wheel has granted player in hell mode.
    TFCondEx2_HalloweenGhostMode    = ( 1 << 13 ), // Wheel has granted player ghost mode.
    TFCondEx2_Parachute             = ( 1 << 16 ), // Player has deployed the BASE Jumper.
    TFCondEx2_BlastJumping          = ( 1 << 17 ), // Player has sticky or rocket jumped.
    TFCond_MiniCrits                = ( TFCond_Buffed | TFCond_CritCola ),
    TFCond_IgnoreStates             = ( TFCond_Ubercharged | TFCond_Bonked ),
    TFCondEx_IgnoreStates           = ( TFCondEx_PyroHeal )
};

enum ETFStunFlags : i32
{
    TF_STUN_NONE                  = 0,
    TF_STUN_MOVEMENT              = ( 1 << 0 ),
    TF_STUN_CONTROLS              = ( 1 << 1 ),
    TF_STUN_MOVEMENT_FORWARD_ONLY = ( 1 << 2 ),
    TF_STUN_SPECIAL_SOUND         = ( 1 << 3 ),
    TF_STUN_DODGE_COOLDOWN        = ( 1 << 4 ),
    TF_STUN_NO_EFFECTS            = ( 1 << 5 ),
    TF_STUN_LOSER_STATE           = ( 1 << 6 ),
    TF_STUN_BY_TRIGGER            = ( 1 << 7 ),
    TF_STUN_BOTH                  = ( TF_STUN_MOVEMENT | TF_STUN_CONTROLS ),
    TF_STUN_SOUND                 = ( 1 << 8 )
};

enum ETFMatchGroup : i32
{
    k_eTFMatchGroup_Invalid = -1,

    k_eTFMatchGroup_MvM_Practice = 0,
    k_eTFMatchGroup_MvM_MannUp   = 1,

    k_eTFMatchGroup_First       = 0,
    k_eTFMatchGroup_MvM_Default = 0,
    k_eTFMatchGroup_MvM_First   = 0,
    k_eTFMatchGroup_MvM_Last    = 1,

    k_eTFMatchGroup_Ladder_6v6   = 2,
    k_eTFMatchGroup_Ladder_9v9   = 3,
    k_eTFMatchGroup_Ladder_12v12 = 4,

    k_eTFMatchGroup_Ladder_Default = 2,
    k_eTFMatchGroup_Ladder_First   = 2,
    k_eTFMatchGroup_Ladder_Last    = 4,

    k_eTFMatchGroup_Casual_6v6   = 5,
    k_eTFMatchGroup_Casual_9v9   = 6,
    k_eTFMatchGroup_Casual_12v12 = 7,

    k_eTFMatchGroup_Casual_Default = 7,
    k_eTFMatchGroup_Casual_First   = 5,
    k_eTFMatchGroup_Casual_Last    = 7,

    k_eTFMatchGroup_Event_Placeholder = 8,

    k_eTFMatchGroup_Event_Default = 8,
    k_eTFMatchGroup_Event_First   = 8,
    k_eTFMatchGroup_Event_Last    = 8
};

inline constexpr vec3 VEC_HULL_MIN{ -24.0f, -24.0f, 0.0f };
inline constexpr vec3 VEC_HULL_MAX{ 24.0f, 24.0f, 82.0f };
inline constexpr vec3 VEC_DUCK_HULL_MIN{ -24.0f, -24.0f, 0.0f };
inline constexpr vec3 VEC_DUCK_HULL_MAX{ 24.0f, 24.0f, 62.0f };

inline constexpr f32 TF_ROCKET_RADIUS_FOR_RJS{ 110.0f * 1.1f };
inline constexpr f32 TF_ROCKET_RADIUS{ 146.0f };
inline constexpr f32 SENTRY_MAX_RANGE{ 1100.0f };
inline constexpr f32 TF_WEAPON_GRENADE_DETONATE_TIME{ 2.0f };
inline constexpr f32 TF_WEAPON_PIPEBOMB_TIMER_DMG_REDUCTION{ 0.6f };
inline constexpr f32 TF_DAMAGE_CRIT_MULTIPLIER{ 3.0f };
inline constexpr f32 TF_DAMAGE_MINICRIT_MULTIPLIER{ 1.35f };
inline constexpr f32 TF_DAMAGE_CRIT_DURATION_RAPID{ 2.0f };
inline constexpr f32 TF_BURNING_DMG{ 4.0f };
inline constexpr f32 TF_BURNING_FREQUENCY{ 0.5f };
inline constexpr f32 FLARE_GRAVITY{ 0.3f };
inline constexpr f32 TF_WEAPON_SNIPERRIFLE_CHARGE_PER_SEC{ 50.0f };
inline constexpr f32 TF_WEAPON_SNIPERRIFLE_DAMAGE_MIN{ 50.0f };
inline constexpr f32 TF_WEAPON_SNIPERRIFLE_DAMAGE_MAX{ 150.0f };
inline constexpr f32 TF_WEAPON_SNIPERRIFLE_NO_CRIT_AFTER_ZOOM_TIME{ 0.2f };
inline constexpr f32 TF_PLAYER_MAX_SAFE_FALL_SPEED{ 650.0f };

inline constexpr f32 JUMP_SPEED{ 289.0f };          // @ CTFGameMovement::CheckJumpButton
inline constexpr f32 FAN_KNOCKBACK_SPEED{ 300.0f }; // @ CTFScatterGun::FireBullet

inline constexpr vec2 g_vecFixedWpnSpreadPellets[ 10 ]{
    { 0.0f, 0.0f },
    { 1.0f, 0.0f },
    { -1.0f, 0.0f },
    { 0.0f, -1.0f },
    { 0.0f, 1.0f },
    { 0.85f, -0.85f },
    { 0.85f, 0.85f },
    { -0.85f, -0.85f },
    { -0.85f, 0.85f },
    { 0.0f, 0.0f }
};

//
// NOTE: Inserting to most or all of the enums in this file will BREAK DEMOS -
// please add to the end instead.
//
enum ETFWeaponType
{
    TF_WEAPON_NONE = 0,
    TF_WEAPON_BAT,
    TF_WEAPON_BAT_WOOD,
    TF_WEAPON_BOTTLE,
    TF_WEAPON_FIREAXE,
    TF_WEAPON_CLUB,
    TF_WEAPON_CROWBAR,
    TF_WEAPON_KNIFE,
    TF_WEAPON_FISTS,
    TF_WEAPON_SHOVEL,
    TF_WEAPON_WRENCH,
    TF_WEAPON_BONESAW,
    TF_WEAPON_SHOTGUN_PRIMARY,
    TF_WEAPON_SHOTGUN_SOLDIER,
    TF_WEAPON_SHOTGUN_HWG,
    TF_WEAPON_SHOTGUN_PYRO,
    TF_WEAPON_SCATTERGUN,
    TF_WEAPON_SNIPERRIFLE,
    TF_WEAPON_MINIGUN,
    TF_WEAPON_SMG,
    TF_WEAPON_SYRINGEGUN_MEDIC,
    TF_WEAPON_TRANQ,
    TF_WEAPON_ROCKETLAUNCHER,
    TF_WEAPON_GRENADELAUNCHER,
    TF_WEAPON_PIPEBOMBLAUNCHER,
    TF_WEAPON_FLAMETHROWER,
    TF_WEAPON_GRENADE_NORMAL,
    TF_WEAPON_GRENADE_CONCUSSION,
    TF_WEAPON_GRENADE_NAIL,
    TF_WEAPON_GRENADE_MIRV,
    TF_WEAPON_GRENADE_MIRV_DEMOMAN,
    TF_WEAPON_GRENADE_NAPALM,
    TF_WEAPON_GRENADE_GAS,
    TF_WEAPON_GRENADE_EMP,
    TF_WEAPON_GRENADE_CALTROP,
    TF_WEAPON_GRENADE_PIPEBOMB,
    TF_WEAPON_GRENADE_SMOKE_BOMB,
    TF_WEAPON_GRENADE_HEAL,
    TF_WEAPON_GRENADE_STUNBALL,
    TF_WEAPON_GRENADE_JAR,
    TF_WEAPON_GRENADE_JAR_MILK,
    TF_WEAPON_PISTOL,
    TF_WEAPON_PISTOL_SCOUT,
    TF_WEAPON_REVOLVER,
    TF_WEAPON_NAILGUN,
    TF_WEAPON_PDA,
    TF_WEAPON_PDA_ENGINEER_BUILD,
    TF_WEAPON_PDA_ENGINEER_DESTROY,
    TF_WEAPON_PDA_SPY,
    TF_WEAPON_BUILDER,
    TF_WEAPON_MEDIGUN,
    TF_WEAPON_GRENADE_MIRVBOMB,
    TF_WEAPON_FLAMETHROWER_ROCKET,
    TF_WEAPON_GRENADE_DEMOMAN,
    TF_WEAPON_SENTRY_BULLET,
    TF_WEAPON_SENTRY_ROCKET,
    TF_WEAPON_DISPENSER,
    TF_WEAPON_INVIS,
    TF_WEAPON_FLAREGUN,
    TF_WEAPON_LUNCHBOX,
    TF_WEAPON_JAR,
    TF_WEAPON_COMPOUND_BOW,
    TF_WEAPON_BUFF_ITEM,
    TF_WEAPON_PUMPKIN_BOMB,
    TF_WEAPON_SWORD,
    TF_WEAPON_ROCKETLAUNCHER_DIRECTHIT,
    TF_WEAPON_LIFELINE,
    TF_WEAPON_LASER_POINTER,
    TF_WEAPON_DISPENSER_GUN,
    TF_WEAPON_SENTRY_REVENGE,
    TF_WEAPON_JAR_MILK,
    TF_WEAPON_HANDGUN_SCOUT_PRIMARY,
    TF_WEAPON_BAT_FISH,
    TF_WEAPON_CROSSBOW,
    TF_WEAPON_STICKBOMB,
    TF_WEAPON_HANDGUN_SCOUT_SECONDARY,
    TF_WEAPON_SODA_POPPER,
    TF_WEAPON_SNIPERRIFLE_DECAP,
    TF_WEAPON_RAYGUN,
    TF_WEAPON_PARTICLE_CANNON,
    TF_WEAPON_MECHANICAL_ARM,
    TF_WEAPON_DRG_POMSON,
    TF_WEAPON_BAT_GIFTWRAP,
    TF_WEAPON_GRENADE_ORNAMENT_BALL,
    TF_WEAPON_FLAREGUN_REVENGE,
    TF_WEAPON_PEP_BRAWLER_BLASTER,
    TF_WEAPON_CLEAVER,
    TF_WEAPON_GRENADE_CLEAVER,
    TF_WEAPON_STICKY_BALL_LAUNCHER,
    TF_WEAPON_GRENADE_STICKY_BALL,
    TF_WEAPON_SHOTGUN_BUILDING_RESCUE,
    TF_WEAPON_CANNON,
    TF_WEAPON_THROWABLE,
    TF_WEAPON_GRENADE_THROWABLE,
    TF_WEAPON_PDA_SPY_BUILD,
    TF_WEAPON_GRENADE_WATERBALLOON,
    TF_WEAPON_HARVESTER_SAW,
    TF_WEAPON_SPELLBOOK,
    TF_WEAPON_SPELLBOOK_PROJECTILE,
    TF_WEAPON_SNIPERRIFLE_CLASSIC,
    TF_WEAPON_PARACHUTE,
    TF_WEAPON_GRAPPLINGHOOK,
    TF_WEAPON_PASSTIME_GUN,
    TF_WEAPON_CHARGED_SMG,
    TF_WEAPON_BREAKABLE_SIGN,
    TF_WEAPON_ROCKETPACK,
    TF_WEAPON_SLAP,
    TF_WEAPON_JAR_GAS,
    TF_WEAPON_GRENADE_JAR_GAS,
    TF_WEAPON_FLAME_BALL,
    //
    // ADD NEW WEAPONS HERE TO AVOID BREAKING DEMOS
    //
    TF_WEAPON_COUNT
};

enum MinigunState_t
{
    AC_STATE_IDLE = 0,
    AC_STATE_STARTFIRING,
    AC_STATE_FIRING,
    AC_STATE_SPINNING,
    AC_STATE_DRYFIRE
};

enum ETFReloadMode
{
    TF_RELOAD_START = 0,
    TF_RELOADING,
    TF_RELOADING_CONTINUE,
    TF_RELOAD_FINISH
};

enum PlayerAnimEvent_t
{
    PLAYERANIMEVENT_ATTACK_PRIMARY,
    PLAYERANIMEVENT_ATTACK_SECONDARY,
    PLAYERANIMEVENT_ATTACK_GRENADE,
    PLAYERANIMEVENT_RELOAD,
    PLAYERANIMEVENT_RELOAD_LOOP,
    PLAYERANIMEVENT_RELOAD_END,
    PLAYERANIMEVENT_JUMP,
    PLAYERANIMEVENT_SWIM,
    PLAYERANIMEVENT_DIE,
    PLAYERANIMEVENT_FLINCH_CHEST,
    PLAYERANIMEVENT_FLINCH_HEAD,
    PLAYERANIMEVENT_FLINCH_LEFTARM,
    PLAYERANIMEVENT_FLINCH_RIGHTARM,
    PLAYERANIMEVENT_FLINCH_LEFTLEG,
    PLAYERANIMEVENT_FLINCH_RIGHTLEG,
    PLAYERANIMEVENT_DOUBLEJUMP,
    PLAYERANIMEVENT_CANCEL,
    PLAYERANIMEVENT_SPAWN,
    PLAYERANIMEVENT_SNAP_YAW,
    PLAYERANIMEVENT_CUSTOM,
    PLAYERANIMEVENT_CUSTOM_GESTURE,
    PLAYERANIMEVENT_CUSTOM_SEQUENCE,
    PLAYERANIMEVENT_CUSTOM_GESTURE_SEQUENCE,
    PLAYERANIMEVENT_ATTACK_PRE,
    PLAYERANIMEVENT_ATTACK_POST,
    PLAYERANIMEVENT_GRENADE1_DRAW,
    PLAYERANIMEVENT_GRENADE2_DRAW,
    PLAYERANIMEVENT_GRENADE1_THROW,
    PLAYERANIMEVENT_GRENADE2_THROW,
    PLAYERANIMEVENT_VOICE_COMMAND_GESTURE,
    PLAYERANIMEVENT_DOUBLEJUMP_CROUCH,
    PLAYERANIMEVENT_STUN_BEGIN,
    PLAYERANIMEVENT_STUN_MIDDLE,
    PLAYERANIMEVENT_STUN_END,
    PLAYERANIMEVENT_PASSTIME_THROW_BEGIN,
    PLAYERANIMEVENT_PASSTIME_THROW_MIDDLE,
    PLAYERANIMEVENT_PASSTIME_THROW_END,
    PLAYERANIMEVENT_CYOAPDA_BEGIN,
    PLAYERANIMEVENT_CYOAPDA_MIDDLE,
    PLAYERANIMEVENT_CYOAPDA_END,
    PLAYERANIMEVENT_ATTACK_PRIMARY_SUPER,
    PLAYERANIMEVENT_COUNT
};

enum loadout_positions_t
{
    LOADOUT_POSITION_INVALID = -1,
    LOADOUT_POSITION_PRIMARY = 0,
    LOADOUT_POSITION_SECONDARY,
    LOADOUT_POSITION_MELEE,
    LOADOUT_POSITION_UTILITY,
    LOADOUT_POSITION_BUILDING,
    LOADOUT_POSITION_PDA,
    LOADOUT_POSITION_PDA2,
    LOADOUT_POSITION_HEAD,
    LOADOUT_POSITION_MISC,
    LOADOUT_POSITION_ACTION,
    LOADOUT_POSITION_MISC2,
    LOADOUT_POSITION_TAUNT,
    LOADOUT_POSITION_TAUNT2,
    LOADOUT_POSITION_TAUNT3,
    LOADOUT_POSITION_TAUNT4,
    LOADOUT_POSITION_TAUNT5,
    LOADOUT_POSITION_TAUNT6,
    LOADOUT_POSITION_TAUNT7,
    LOADOUT_POSITION_TAUNT8,
    CLASS_LOADOUT_POSITION_COUNT
};

enum AnimeventType
{
    AE_TYPE_SERVER         = 1 << 0,
    AE_TYPE_SCRIPTED       = 1 << 1,
    AE_TYPE_SHARED         = 1 << 2,
    AE_TYPE_WEAPON         = 1 << 3,
    AE_TYPE_CLIENT         = 1 << 4,
    AE_TYPE_FACEPOSER      = 1 << 5,
    AE_TYPE_NEWEVENTSYSTEM = 1 << 10
};

enum Animevent
{
    AE_INVALID = -1,
    AE_EMPTY,
    AE_NPC_LEFTFOOT,
    AE_NPC_RIGHTFOOT,
    AE_NPC_BODYDROP_LIGHT,
    AE_NPC_BODYDROP_HEAVY,
    AE_NPC_SWISHSOUND,
    AE_NPC_180TURN,
    AE_NPC_ITEM_PICKUP,
    AE_NPC_WEAPON_DROP,
    AE_NPC_WEAPON_SET_SEQUENCE_NAME,
    AE_NPC_WEAPON_SET_SEQUENCE_NUMBER,
    AE_NPC_WEAPON_SET_ACTIVITY,
    AE_NPC_HOLSTER,
    AE_NPC_DRAW,
    AE_NPC_WEAPON_FIRE,
    AE_CL_PLAYSOUND,
    AE_SV_PLAYSOUND,
    AE_CL_STOPSOUND,
    AE_START_SCRIPTED_EFFECT,
    AE_STOP_SCRIPTED_EFFECT,
    AE_CLIENT_EFFECT_ATTACH,
    AE_MUZZLEFLASH,
    AE_NPC_MUZZLEFLASH,
    AE_THUMPER_THUMP,
    AE_AMMOCRATE_PICKUP_AMMO,
    AE_NPC_RAGDOLL,
    AE_NPC_ADDGESTURE,
    AE_NPC_RESTARTGESTURE,
    AE_NPC_ATTACK_BROADCAST,
    AE_NPC_HURT_INTERACTION_PARTNER,
    AE_NPC_SET_INTERACTION_CANTDIE,
    AE_SV_DUSTTRAIL,
    AE_CL_CREATE_PARTICLE_EFFECT,
    AE_RAGDOLL,
    AE_CL_ENABLE_BODYGROUP,
    AE_CL_DISABLE_BODYGROUP,
    AE_CL_BODYGROUP_SET_VALUE,
    AE_CL_BODYGROUP_SET_VALUE_CMODEL_WPN,
    AE_WPN_PRIMARYATTACK,
    AE_WPN_INCREMENTAMMO,
    AE_WPN_HIDE,
    AE_WPN_UNHIDE,
    AE_WPN_PLAYWPNSOUND,
    AE_RD_ROBOT_POP_PANELS_OFF,
    AE_TAUNT_ENABLE_MOVE,
    AE_TAUNT_DISABLE_MOVE,
    AE_CL_REMOVE_PARTICLE_EFFECT,
    AE_TAUNT_ADD_ATTRIBUTE,
    AE_SV_EXCLUDE_PLAYER_SOUND,
    AE_CL_EXCLUDE_PLAYER_SOUND,
    LAST_SHARED_ANIMEVENT
};

enum ETFWeapons
{
    Scout_m_Scattergun                                = 13,
    Scout_m_ScattergunR                               = 200,
    Scout_m_ForceANature                              = 45,
    Scout_m_TheShortstop                              = 220,
    Scout_m_TheSodaPopper                             = 448,
    Scout_m_FestiveScattergun                         = 669,
    Scout_m_BabyFacesBlaster                          = 772,
    Scout_m_SilverBotkillerScattergunMkI              = 799,
    Scout_m_GoldBotkillerScattergunMkI                = 808,
    Scout_m_RustBotkillerScattergunMkI                = 888,
    Scout_m_BloodBotkillerScattergunMkI               = 897,
    Scout_m_CarbonadoBotkillerScattergunMkI           = 906,
    Scout_m_DiamondBotkillerScattergunMkI             = 915,
    Scout_m_SilverBotkillerScattergunMkII             = 964,
    Scout_m_GoldBotkillerScattergunMkII               = 973,
    Scout_m_FestiveForceANature                       = 1078,
    Scout_m_TheBackScatter                            = 1103,
    Scout_m_NightTerror                               = 15002,
    Scout_m_TartanTorpedo                             = 15015,
    Scout_m_CountryCrusher                            = 15021,
    Scout_m_BackcountryBlaster                        = 15029,
    Scout_m_SpruceDeuce                               = 15036,
    Scout_m_CurrentEvent                              = 15053,
    Scout_m_MacabreWeb                                = 15065,
    Scout_m_Nutcracker                                = 15069,
    Scout_m_BlueMew                                   = 15106,
    Scout_m_FlowerPower                               = 15107,
    Scout_m_ShottoHell                                = 15108,
    Scout_m_CoffinNail                                = 15131,
    Scout_m_KillerBee                                 = 15151,
    Scout_m_Corsair                                   = 15157,
    Scout_s_ScoutsPistol                              = 23,
    Scout_s_PistolR                                   = 209,
    Scout_s_BonkAtomicPunch                           = 46,
    Scout_s_VintageLugermorph                         = 160,
    Scout_s_CritaCola                                 = 163,
    Scout_s_MadMilk                                   = 222,
    Scout_s_Lugermorph                                = 294,
    Scout_s_TheWinger                                 = 449,
    Scout_s_PrettyBoysPocketPistol                    = 773,
    Scout_s_TheFlyingGuillotine                       = 812,
    Scout_s_TheFlyingGuillotineG                      = 833,
    Scout_s_MutatedMilk                               = 1121,
    Scout_s_FestiveBonk                               = 1145,
    Scout_s_RedRockRoscoe                             = 15013,
    Scout_s_HomemadeHeater                            = 15018,
    Scout_s_HickoryHolepuncher                        = 15035,
    Scout_s_LocalHero                                 = 15041,
    Scout_s_BlackDahlia                               = 15046,
    Scout_s_SandstoneSpecial                          = 15056,
    Scout_s_MacabreWeb                                = 15060,
    Scout_s_Nutcracker                                = 15061,
    Scout_s_BlueMew                                   = 15100,
    Scout_s_BrainCandy                                = 15101,
    Scout_s_ShottoHell                                = 15102,
    Scout_s_DressedToKill                             = 15126,
    Scout_s_Blitzkrieg                                = 15148,
    Scout_s_TheCAPPER                                 = 30666,
    Scout_t_Bat                                       = 0,
    Scout_t_BatR                                      = 190,
    Scout_t_TheSandman                                = 44,
    Scout_t_TheHolyMackerel                           = 221,
    Scout_t_TheCandyCane                              = 317,
    Scout_t_TheBostonBasher                           = 325,
    Scout_t_SunonaStick                               = 349,
    Scout_t_TheFanOWar                                = 355,
    Scout_t_TheAtomizer                               = 450,
    Scout_t_ThreeRuneBlade                            = 452,
    Scout_t_TheConscientiousObjector                  = 474,
    Scout_t_UnarmedCombat                             = 572,
    Scout_t_TheWrapAssassin                           = 648,
    Scout_t_FestiveBat                                = 660,
    Scout_t_TheFreedomStaff                           = 880,
    Scout_t_TheBatOuttaHell                           = 939,
    Scout_t_TheMemoryMaker                            = 954,
    Scout_t_FestiveHolyMackerel                       = 999,
    Scout_t_TheHamShank                               = 1013,
    Scout_t_TheNecroSmasher                           = 1123,
    Scout_t_TheCrossingGuard                          = 1127,
    Scout_t_Batsaber                                  = 30667,
    Scout_t_PrinnyMachete                             = 30758,
    Soldier_m_RocketLauncher                          = 18,
    Soldier_m_RocketLauncherR                         = 205,
    Soldier_m_TheDirectHit                            = 127,
    Soldier_m_TheBlackBox                             = 228,
    Soldier_m_RocketJumper                            = 237,
    Soldier_m_TheLibertyLauncher                      = 414,
    Soldier_m_TheCowMangler5000                       = 441,
    Soldier_m_TheOriginal                             = 513,
    Soldier_m_FestiveRocketLauncher                   = 658,
    Soldier_m_TheBeggarsBazooka                       = 730,
    Soldier_m_SilverBotkillerRocketLauncherMkI        = 800,
    Soldier_m_GoldBotkillerRocketLauncherMkI          = 809,
    Soldier_m_RustBotkillerRocketLauncherMkI          = 889,
    Soldier_m_BloodBotkillerRocketLauncherMkI         = 898,
    Soldier_m_CarbonadoBotkillerRocketLauncherMkI     = 907,
    Soldier_m_DiamondBotkillerRocketLauncherMkI       = 916,
    Soldier_m_SilverBotkillerRocketLauncherMkII       = 965,
    Soldier_m_GoldBotkillerRocketLauncherMkII         = 974,
    Soldier_m_FestiveBlackBox                         = 1085,
    Soldier_m_TheAirStrike                            = 1104,
    Soldier_m_WoodlandWarrior                         = 15006,
    Soldier_m_SandCannon                              = 15014,
    Soldier_m_AmericanPastoral                        = 15028,
    Soldier_m_SmalltownBringdown                      = 15043,
    Soldier_m_ShellShocker                            = 15052,
    Soldier_m_AquaMarine                              = 15057,
    Soldier_m_Autumn                                  = 15081,
    Soldier_m_BlueMew                                 = 15104,
    Soldier_m_BrainCandy                              = 15105,
    Soldier_m_CoffinNail                              = 15129,
    Soldier_m_HighRollers                             = 15130,
    Soldier_m_Warhawk                                 = 15150,
    Soldier_s_SoldiersShotgun                         = 10,
    Soldier_s_ShotgunR                                = 199,
    Soldier_s_TheBuffBanner                           = 129,
    Soldier_s_Gunboats                                = 133,
    Soldier_s_TheBattalionsBackup                     = 226,
    Soldier_s_TheConcheror                            = 354,
    Soldier_s_TheReserveShooter                       = 415,
    Soldier_s_TheRighteousBison                       = 442,
    Soldier_s_TheMantreads                            = 444,
    Soldier_s_FestiveBuffBanner                       = 1001,
    Soldier_s_TheBASEJumper                           = 1101,
    Soldier_s_FestiveShotgun                          = 1141,
    Soldier_s_PanicAttack                             = 1153,
    Soldier_s_BackwoodsBoomstick                      = 15003,
    Soldier_s_RusticRuiner                            = 15016,
    Soldier_s_CivicDuty                               = 15044,
    Soldier_s_LightningRod                            = 15047,
    Soldier_s_Autumn                                  = 15085,
    Soldier_s_FlowerPower                             = 15109,
    Soldier_s_CoffinNail                              = 15132,
    Soldier_s_DressedtoKill                           = 15133,
    Soldier_s_RedBear                                 = 15152,
    Soldier_t_Shovel                                  = 6,
    Soldier_t_ShovelR                                 = 196,
    Soldier_t_TheEqualizer                            = 128,
    Soldier_t_ThePainTrain                            = 154,
    Soldier_t_TheHalfZatoichi                         = 357,
    Soldier_t_TheMarketGardener                       = 416,
    Soldier_t_TheDisciplinaryAction                   = 447,
    Soldier_t_TheConscientiousObjector                = 474,
    Soldier_t_TheEscapePlan                           = 775,
    Soldier_t_TheFreedomStaff                         = 880,
    Soldier_t_TheBatOuttaHell                         = 939,
    Soldier_t_TheMemoryMaker                          = 954,
    Soldier_t_TheHamShank                             = 1013,
    Soldier_t_TheNecroSmasher                         = 1123,
    Soldier_t_TheCrossingGuard                        = 1127,
    Soldier_t_PrinnyMachete                           = 30758,
    Pyro_m_FlameThrower                               = 21,
    Pyro_m_FlameThrowerR                              = 208,
    Pyro_m_TheBackburner                              = 40,
    Pyro_m_TheDegreaser                               = 215,
    Pyro_m_ThePhlogistinator                          = 594,
    Pyro_m_FestiveFlameThrower                        = 659,
    Pyro_m_TheRainblower                              = 741,
    Pyro_m_SilverBotkillerFlameThrowerMkI             = 798,
    Pyro_m_GoldBotkillerFlameThrowerMkI               = 807,
    Pyro_m_RustBotkillerFlameThrowerMkI               = 887,
    Pyro_m_BloodBotkillerFlameThrowerMkI              = 896,
    Pyro_m_CarbonadoBotkillerFlameThrowerMkI          = 905,
    Pyro_m_DiamondBotkillerFlameThrowerMkI            = 914,
    Pyro_m_SilverBotkillerFlameThrowerMkII            = 963,
    Pyro_m_GoldBotkillerFlameThrowerMkII              = 972,
    Pyro_m_FestiveBackburner                          = 1146,
    Pyro_m_DragonsFury                                = 1178,
    Pyro_m_ForestFire                                 = 15005,
    Pyro_m_BarnBurner                                 = 15017,
    Pyro_m_BovineBlazemaker                           = 15030,
    Pyro_m_EarthSkyandFire                            = 15034,
    Pyro_m_FlashFryer                                 = 15049,
    Pyro_m_TurbineTorcher                             = 15054,
    Pyro_m_Autumn                                     = 15066,
    Pyro_m_PumpkinPatch                               = 15067,
    Pyro_m_Nutcracker                                 = 15068,
    Pyro_m_Balloonicorn                               = 15089,
    Pyro_m_Rainbow                                    = 15090,
    Pyro_m_CoffinNail                                 = 15115,
    Pyro_m_Warhawk                                    = 15141,
    Pyro_m_NostromoNapalmer                           = 30474,
    Pyro_s_PyrosShotgun                               = 12,
    Pyro_s_ShotgunR                                   = 199,
    Pyro_s_TheFlareGun                                = 39,
    Pyro_s_TheDetonator                               = 351,
    Pyro_s_TheReserveShooter                          = 415,
    Pyro_s_TheManmelter                               = 595,
    Pyro_s_TheScorchShot                              = 740,
    Pyro_s_FestiveFlareGun                            = 1081,
    Pyro_s_FestiveShotgun                             = 1141,
    Pyro_s_PanicAttack                                = 1153,
    Pyro_s_ThermalThruster                            = 1179,
    Pyro_s_GasPasser                                  = 1180,
    Pyro_s_BackwoodsBoomstick                         = 15003,
    Pyro_s_RusticRuiner                               = 15016,
    Pyro_s_CivicDuty                                  = 15044,
    Pyro_s_LightningRod                               = 15047,
    Pyro_s_Autumn                                     = 15085,
    Pyro_s_FlowerPower                                = 15109,
    Pyro_s_CoffinNail                                 = 15132,
    Pyro_s_DressedtoKill                              = 15133,
    Pyro_s_RedBear                                    = 15152,
    Pyro_t_FireAxe                                    = 2,
    Pyro_t_FireAxeR                                   = 192,
    Pyro_t_TheAxtinguisher                            = 38,
    Pyro_t_Homewrecker                                = 153,
    Pyro_t_ThePowerjack                               = 214,
    Pyro_t_TheBackScratcher                           = 326,
    Pyro_t_SharpenedVolcanoFragment                   = 348,
    Pyro_t_ThePostalPummeler                          = 457,
    Pyro_t_TheMaul                                    = 466,
    Pyro_t_TheConscientiousObjector                   = 474,
    Pyro_t_TheThirdDegree                             = 593,
    Pyro_t_TheLollichop                               = 739,
    Pyro_t_NeonAnnihilator                            = 813,
    Pyro_t_NeonAnnihilatorG                           = 834,
    Pyro_t_TheFreedomStaff                            = 880,
    Pyro_t_TheBatOuttaHell                            = 939,
    Pyro_t_TheMemoryMaker                             = 954,
    Pyro_t_TheFestiveAxtinguisher                     = 1000,
    Pyro_t_TheHamShank                                = 1013,
    Pyro_t_TheNecroSmasher                            = 1123,
    Pyro_t_TheCrossingGuard                           = 1127,
    Pyro_t_HotHand                                    = 1181,
    Pyro_t_PrinnyMachete                              = 30758,
    Demoman_m_GrenadeLauncher                         = 19,
    Demoman_m_GrenadeLauncherR                        = 206,
    Demoman_m_TheLochnLoad                            = 308,
    Demoman_m_AliBabasWeeBooties                      = 405,
    Demoman_m_TheBootlegger                           = 608,
    Demoman_m_TheLooseCannon                          = 996,
    Demoman_m_FestiveGrenadeLauncher                  = 1007,
    Demoman_m_TheBASEJumper                           = 1101,
    Demoman_m_TheIronBomber                           = 1151,
    Demoman_m_Autumn                                  = 15077,
    Demoman_m_MacabreWeb                              = 15079,
    Demoman_m_Rainbow                                 = 15091,
    Demoman_m_SweetDreams                             = 15092,
    Demoman_m_CoffinNail                              = 15116,
    Demoman_m_TopShelf                                = 15117,
    Demoman_m_Warhawk                                 = 15142,
    Demoman_m_ButcherBird                             = 15158,
    Demoman_s_StickybombLauncher                      = 20,
    Demoman_s_StickybombLauncherR                     = 207,
    Demoman_s_TheScottishResistance                   = 130,
    Demoman_s_TheCharginTarge                         = 131,
    Demoman_s_StickyJumper                            = 265,
    Demoman_s_TheSplendidScreen                       = 406,
    Demoman_s_FestiveStickybombLauncher               = 661,
    Demoman_s_SilverBotkillerStickybombLauncherMkI    = 797,
    Demoman_s_GoldBotkillerStickybombLauncherMkI      = 806,
    Demoman_s_RustBotkillerStickybombLauncherMkI      = 886,
    Demoman_s_BloodBotkillerStickybombLauncherMkI     = 895,
    Demoman_s_CarbonadoBotkillerStickybombLauncherMkI = 904,
    Demoman_s_DiamondBotkillerStickybombLauncherMkI   = 913,
    Demoman_s_SilverBotkillerStickybombLauncherMkII   = 962,
    Demoman_s_GoldBotkillerStickybombLauncherMkII     = 971,
    Demoman_s_TheTideTurner                           = 1099,
    Demoman_s_FestiveTarge                            = 1144,
    Demoman_s_TheQuickiebombLauncher                  = 1150,
    Demoman_s_SuddenFlurry                            = 15009,
    Demoman_s_CarpetBomber                            = 15012,
    Demoman_s_BlastedBombardier                       = 15024,
    Demoman_s_RooftopWrangler                         = 15038,
    Demoman_s_LiquidAsset                             = 15045,
    Demoman_s_PinkElephant                            = 15048,
    Demoman_s_Autumn                                  = 15082,
    Demoman_s_PumpkinPatch                            = 15083,
    Demoman_s_MacabreWeb                              = 15084,
    Demoman_s_SweetDreams                             = 15113,
    Demoman_s_CoffinNail                              = 15137,
    Demoman_s_DressedtoKill                           = 15138,
    Demoman_s_Blitzkrieg                              = 15155,
    Demoman_t_Bottle                                  = 1,
    Demoman_t_BottleR                                 = 191,
    Demoman_t_TheEyelander                            = 132,
    Demoman_t_ThePainTrain                            = 154,
    Demoman_t_TheScotsmansSkullcutter                 = 172,
    Demoman_t_HorselessHeadlessHorsemannsHeadtaker    = 266,
    Demoman_t_UllapoolCaber                           = 307,
    Demoman_t_TheClaidheamhMor                        = 327,
    Demoman_t_TheHalfZatoichi                         = 357,
    Demoman_t_ThePersianPersuader                     = 404,
    Demoman_t_TheConscientiousObjector                = 474,
    Demoman_t_NessiesNineIron                         = 482,
    Demoman_t_TheScottishHandshake                    = 609,
    Demoman_t_TheFreedomStaff                         = 880,
    Demoman_t_TheBatOuttaHell                         = 939,
    Demoman_t_TheMemoryMaker                          = 954,
    Demoman_t_TheHamShank                             = 1013,
    Demoman_t_FestiveEyelander                        = 1082,
    Demoman_t_TheNecroSmasher                         = 1123,
    Demoman_t_TheCrossingGuard                        = 1127,
    Demoman_t_PrinnyMachete                           = 30758,
    Heavy_m_Minigun                                   = 15,
    Heavy_m_MinigunR                                  = 202,
    Heavy_m_Natascha                                  = 41,
    Heavy_m_IronCurtain                               = 298,
    Heavy_m_TheBrassBeast                             = 312,
    Heavy_m_Tomislav                                  = 424,
    Heavy_m_FestiveMinigun                            = 654,
    Heavy_m_SilverBotkillerMinigunMkI                 = 793,
    Heavy_m_GoldBotkillerMinigunMkI                   = 802,
    Heavy_m_TheHuoLongHeater                          = 811,
    Heavy_m_TheHuoLongHeaterG                         = 832,
    Heavy_m_Deflector_mvm                             = 850,
    Heavy_m_RustBotkillerMinigunMkI                   = 882,
    Heavy_m_BloodBotkillerMinigunMkI                  = 891,
    Heavy_m_CarbonadoBotkillerMinigunMkI              = 900,
    Heavy_m_DiamondBotkillerMinigunMkI                = 909,
    Heavy_m_SilverBotkillerMinigunMkII                = 958,
    Heavy_m_GoldBotkillerMinigunMkII                  = 967,
    Heavy_m_KingoftheJungle                           = 15004,
    Heavy_m_IronWood                                  = 15020,
    Heavy_m_AntiqueAnnihilator                        = 15026,
    Heavy_m_WarRoom                                   = 15031,
    Heavy_m_CitizenPain                               = 15040,
    Heavy_m_BrickHouse                                = 15055,
    Heavy_m_MacabreWeb                                = 15086,
    Heavy_m_PumpkinPatch                              = 15087,
    Heavy_m_Nutcracker                                = 15088,
    Heavy_m_BrainCandy                                = 15098,
    Heavy_m_MisterCuddles                             = 15099,
    Heavy_m_CoffinNail                                = 15123,
    Heavy_m_DressedtoKill                             = 15124,
    Heavy_m_TopShelf                                  = 15125,
    Heavy_m_ButcherBird                               = 15147,
    Heavy_s_HeavysShotgun                             = 11,
    Heavy_s_ShotgunR                                  = 199,
    Heavy_s_Sandvich                                  = 42,
    Heavy_s_TheDalokohsBar                            = 159,
    Heavy_s_TheBuffaloSteakSandvich                   = 311,
    Heavy_s_TheFamilyBusiness                         = 425,
    Heavy_s_Fishcake                                  = 433,
    Heavy_s_RoboSandvich                              = 863,
    Heavy_s_FestiveSandvich                           = 1002,
    Heavy_s_FestiveShotgun                            = 1141,
    Heavy_s_PanicAttack                               = 1153,
    Heavy_s_SecondBanana                              = 1190,
    Heavy_s_BackwoodsBoomstick                        = 15003,
    Heavy_s_RusticRuiner                              = 15016,
    Heavy_s_CivicDuty                                 = 15044,
    Heavy_s_LightningRod                              = 15047,
    Heavy_s_Autumn                                    = 15085,
    Heavy_s_FlowerPower                               = 15109,
    Heavy_s_CoffinNail                                = 15132,
    Heavy_s_DressedtoKill                             = 15133,
    Heavy_s_RedBear                                   = 15152,
    Heavy_t_Fists                                     = 5,
    Heavy_t_FistsR                                    = 195,
    Heavy_t_TheKillingGlovesofBoxing                  = 43,
    Heavy_t_GlovesofRunningUrgently                   = 239,
    Heavy_t_WarriorsSpirit                            = 310,
    Heavy_t_FistsofSteel                              = 331,
    Heavy_t_TheEvictionNotice                         = 426,
    Heavy_t_TheConscientiousObjector                  = 474,
    Heavy_t_ApocoFists                                = 587,
    Heavy_t_TheHolidayPunch                           = 656,
    Heavy_t_TheFreedomStaff                           = 880,
    Heavy_t_TheBatOuttaHell                           = 939,
    Heavy_t_TheMemoryMaker                            = 954,
    Heavy_t_TheHamShank                               = 1013,
    Heavy_t_FestiveGlovesofRunningUrgently            = 1084,
    Heavy_t_TheBreadBite                              = 1100,
    Heavy_t_TheNecroSmasher                           = 1123,
    Heavy_t_TheCrossingGuard                          = 1127,
    Heavy_t_GlovesofRunningUrgentlyMvM                = 1184,
    Heavy_t_PrinnyMachete                             = 30758,
    Engi_m_EngineersShotgun                           = 9,
    Engi_m_ShotgunR                                   = 199,
    Engi_m_TheFrontierJustice                         = 141,
    Engi_m_TheWidowmaker                              = 527,
    Engi_m_ThePomson6000                              = 588,
    Engi_m_TheRescueRanger                            = 997,
    Engi_m_FestiveFrontierJustice                     = 1004,
    Engi_m_FestiveShotgun                             = 1141,
    Engi_m_PanicAttack                                = 1153,
    Engi_m_BackwoodsBoomstick                         = 15003,
    Engi_m_RusticRuiner                               = 15016,
    Engi_m_CivicDuty                                  = 15044,
    Engi_m_LightningRod                               = 15047,
    Engi_m_Autumn                                     = 15085,
    Engi_m_FlowerPower                                = 15109,
    Engi_m_CoffinNail                                 = 15132,
    Engi_m_DressedtoKill                              = 15133,
    Engi_m_RedBear                                    = 15152,
    Engi_s_EngineersPistol                            = 22,
    Engi_s_PistolR                                    = 209,
    Engi_s_TheWrangler                                = 140,
    Engi_s_VintageLugermorph                          = 160,
    Engi_s_Lugermorph                                 = 294,
    Engi_s_TheShortCircuit                            = 528,
    Engi_s_FestiveWrangler                            = 1086,
    Engi_s_RedRockRoscoe                              = 15013,
    Engi_s_HomemadeHeater                             = 15018,
    Engi_s_HickoryHolepuncher                         = 15035,
    Engi_s_LocalHero                                  = 15041,
    Engi_s_BlackDahlia                                = 15046,
    Engi_s_SandstoneSpecial                           = 15056,
    Engi_s_MacabreWeb                                 = 15060,
    Engi_s_Nutcracker                                 = 15061,
    Engi_s_BlueMew                                    = 15100,
    Engi_s_BrainCandy                                 = 15101,
    Engi_s_ShottoHell                                 = 15102,
    Engi_s_DressedToKill                              = 15126,
    Engi_s_Blitzkrieg                                 = 15148,
    Engi_s_TheCAPPER                                  = 30666,
    Engi_s_TheGigarCounter                            = 30668,
    Engi_t_Wrench                                     = 7,
    Engi_t_WrenchR                                    = 197,
    Engi_t_TheGunslinger                              = 142,
    Engi_t_TheSouthernHospitality                     = 155,
    Engi_t_GoldenWrench                               = 169,
    Engi_t_TheJag                                     = 329,
    Engi_t_TheEurekaEffect                            = 589,
    Engi_t_FestiveWrench                              = 662,
    Engi_t_SilverBotkillerWrenchMkI                   = 795,
    Engi_t_GoldBotkillerWrenchMkI                     = 804,
    Engi_t_RustBotkillerWrenchMkI                     = 884,
    Engi_t_BloodBotkillerWrenchMkI                    = 893,
    Engi_t_CarbonadoBotkillerWrenchMkI                = 902,
    Engi_t_DiamondBotkillerWrenchMkI                  = 911,
    Engi_t_SilverBotkillerWrenchMkII                  = 960,
    Engi_t_GoldBotkillerWrenchMkII                    = 969,
    Engi_t_TheNecroSmasher                            = 1123,
    Engi_t_Nutcracker                                 = 15073,
    Engi_t_Autumn                                     = 15074,
    Engi_t_Boneyard                                   = 15075,
    Engi_t_DressedtoKill                              = 15139,
    Engi_t_TopShelf                                   = 15140,
    Engi_t_TorquedtoHell                              = 15114,
    Engi_t_Airwolf                                    = 15156,
    Engi_t_PrinnyMachete                              = 30758,
    Engi_p_ConstructionPDA                            = 25,
    Engi_p_ConstructionPDAR                           = 737,
    Engi_p_DestructionPDA                             = 26,
    Engi_p_PDA                                        = 28,
    Medic_m_SyringeGun                                = 17,
    Medic_m_SyringeGunR                               = 204,
    Medic_m_TheBlutsauger                             = 36,
    Medic_m_CrusadersCrossbow                         = 305,
    Medic_m_TheOverdose                               = 412,
    Medic_m_FestiveCrusadersCrossbow                  = 1079,
    Medic_s_MediGun                                   = 29,
    Medic_s_MediGunR                                  = 211,
    Medic_s_TheKritzkrieg                             = 35,
    Medic_s_TheQuickFix                               = 411,
    Medic_s_FestiveMediGun                            = 663,
    Medic_s_SilverBotkillerMediGunMkI                 = 796,
    Medic_s_GoldBotkillerMediGunMkI                   = 805,
    Medic_s_RustBotkillerMediGunMkI                   = 885,
    Medic_s_BloodBotkillerMediGunMkI                  = 894,
    Medic_s_CarbonadoBotkillerMediGunMkI              = 903,
    Medic_s_DiamondBotkillerMediGunMkI                = 912,
    Medic_s_SilverBotkillerMediGunMkII                = 961,
    Medic_s_GoldBotkillerMediGunMkII                  = 970,
    Medic_s_TheVaccinator                             = 998,
    Medic_s_MaskedMender                              = 15008,
    Medic_s_WrappedReviver                            = 15010,
    Medic_s_ReclaimedReanimator                       = 15025,
    Medic_s_CivilServant                              = 15039,
    Medic_s_SparkofLife                               = 15050,
    Medic_s_Wildwood                                  = 15078,
    Medic_s_FlowerPower                               = 15097,
    Medic_s_DressedToKill                             = 15121,
    Medic_s_HighRollers                               = 15122,
    Medic_s_CoffinNail                                = 15123,
    Medic_s_Blitzkrieg                                = 15145,
    Medic_s_Corsair                                   = 15146,
    Medic_t_Bonesaw                                   = 8,
    Medic_t_BonesawR                                  = 198,
    Medic_t_TheUbersaw                                = 37,
    Medic_t_TheVitaSaw                                = 173,
    Medic_t_Amputator                                 = 304,
    Medic_t_TheSolemnVow                              = 413,
    Medic_t_TheConscientiousObjector                  = 474,
    Medic_t_TheFreedomStaff                           = 880,
    Medic_t_TheBatOuttaHell                           = 939,
    Medic_t_TheMemoryMaker                            = 954,
    Medic_t_FestiveUbersaw                            = 1003,
    Medic_t_TheHamShank                               = 1013,
    Medic_t_TheNecroSmasher                           = 1123,
    Medic_t_TheCrossingGuard                          = 1127,
    Medic_t_FestiveBonesaw                            = 1143,
    Medic_t_PrinnyMachete                             = 30758,
    Sniper_m_SniperRifle                              = 14,
    Sniper_m_SniperRifleR                             = 201,
    Sniper_m_TheHuntsman                              = 56,
    Sniper_m_TheSydneySleeper                         = 230,
    Sniper_m_TheBazaarBargain                         = 402,
    Sniper_m_TheMachina                               = 526,
    Sniper_m_FestiveSniperRifle                       = 664,
    Sniper_m_TheHitmansHeatmaker                      = 752,
    Sniper_m_SilverBotkillerSniperRifleMkI            = 792,
    Sniper_m_GoldBotkillerSniperRifleMkI              = 801,
    Sniper_m_TheAWPerHand                             = 851,
    Sniper_m_RustBotkillerSniperRifleMkI              = 881,
    Sniper_m_BloodBotkillerSniperRifleMkI             = 890,
    Sniper_m_CarbonadoBotkillerSniperRifleMkI         = 899,
    Sniper_m_DiamondBotkillerSniperRifleMkI           = 908,
    Sniper_m_SilverBotkillerSniperRifleMkII           = 957,
    Sniper_m_GoldBotkillerSniperRifleMkII             = 966,
    Sniper_m_FestiveHuntsman                          = 1005,
    Sniper_m_TheFortifiedCompound                     = 1092,
    Sniper_m_TheClassic                               = 1098,
    Sniper_m_NightOwl                                 = 15000,
    Sniper_m_PurpleRange                              = 15007,
    Sniper_m_LumberFromDownUnder                      = 15019,
    Sniper_m_ShotintheDark                            = 15023,
    Sniper_m_Bogtrotter                               = 15033,
    Sniper_m_Thunderbolt                              = 15059,
    Sniper_m_PumpkinPatch                             = 15070,
    Sniper_m_Boneyard                                 = 15071,
    Sniper_m_Wildwood                                 = 15072,
    Sniper_m_Balloonicorn                             = 15111,
    Sniper_m_Rainbow                                  = 15112,
    Sniper_m_CoffinNail                               = 15135,
    Sniper_m_DressedtoKill                            = 15136,
    Sniper_m_Airwolf                                  = 15154,
    Sniper_m_ShootingStar                             = 30665,
    Sniper_s_SMG                                      = 16,
    Sniper_s_SMGR                                     = 203,
    Sniper_s_TheRazorback                             = 57,
    Sniper_s_Jarate                                   = 58,
    Sniper_s_DarwinsDangerShield                      = 231,
    Sniper_s_CozyCamper                               = 642,
    Sniper_s_TheCleanersCarbine                       = 751,
    Sniper_s_FestiveJarate                            = 1083,
    Sniper_s_TheSelfAwareBeautyMark                   = 1105,
    Sniper_s_FestiveSMG                               = 1149,
    Sniper_s_WoodsyWidowmaker                         = 15001,
    Sniper_s_PlaidPotshotter                          = 15022,
    Sniper_s_TreadplateTormenter                      = 15032,
    Sniper_s_TeamSprayer                              = 15037,
    Sniper_s_LowProfile                               = 15058,
    Sniper_s_Wildwood                                 = 15076,
    Sniper_s_BlueMew                                  = 15110,
    Sniper_s_HighRollers                              = 15134,
    Sniper_s_Blitzkrieg                               = 15153,
    Sniper_t_Kukri                                    = 3,
    Sniper_t_KukriR                                   = 193,
    Sniper_t_TheTribalmansShiv                        = 171,
    Sniper_t_TheBushwacka                             = 232,
    Sniper_t_TheShahanshah                            = 401,
    Sniper_t_TheConscientiousObjector                 = 474,
    Sniper_t_TheFreedomStaff                          = 880,
    Sniper_t_TheBatOuttaHell                          = 939,
    Sniper_t_TheMemoryMaker                           = 954,
    Sniper_t_TheHamShank                              = 1013,
    Sniper_t_TheNecroSmasher                          = 1123,
    Sniper_t_TheCrossingGuard                         = 1127,
    Sniper_t_PrinnyMachete                            = 30758,
    Spy_m_Revolver                                    = 24,
    Spy_m_RevolverR                                   = 210,
    Spy_m_TheAmbassador                               = 61,
    Spy_m_BigKill                                     = 161,
    Spy_m_LEtranger                                   = 224,
    Spy_m_TheEnforcer                                 = 460,
    Spy_m_TheDiamondback                              = 525,
    Spy_m_FestiveAmbassador                           = 1006,
    Spy_m_FestiveRevolver                             = 1142,
    Spy_m_PsychedelicSlugger                          = 15011,
    Spy_m_OldCountry                                  = 15027,
    Spy_m_Mayor                                       = 15042,
    Spy_m_DeadReckoner                                = 15051,
    Spy_m_Boneyard                                    = 15062,
    Spy_m_Wildwood                                    = 15063,
    Spy_m_MacabreWeb                                  = 15064,
    Spy_m_FlowerPower                                 = 15103,
    Spy_m_TopShelf                                    = 15128,
    Spy_m_CoffinNail                                  = 15129,
    Spy_m_Blitzkrieg                                  = 15149,
    Spy_s_Sapper                                      = 735,
    Spy_s_SapperR                                     = 736,
    Spy_s_TheRedTapeRecorder                          = 810,
    Spy_s_TheRedTapeRecorderG                         = 831,
    Spy_s_TheApSapG                                   = 933,
    Spy_s_FestiveSapper                               = 1080,
    Spy_s_TheSnackAttack                              = 1102,
    Spy_t_Knife                                       = 4,
    Spy_t_KnifeR                                      = 194,
    Spy_t_YourEternalReward                           = 225,
    Spy_t_ConniversKunai                              = 356,
    Spy_t_TheBigEarner                                = 461,
    Spy_t_TheWangaPrick                               = 574,
    Spy_t_TheSharpDresser                             = 638,
    Spy_t_TheSpycicle                                 = 649,
    Spy_t_FestiveKnife                                = 665,
    Spy_t_TheBlackRose                                = 727,
    Spy_t_SilverBotkillerKnifeMkI                     = 794,
    Spy_t_GoldBotkillerKnifeMkI                       = 803,
    Spy_t_RustBotkillerKnifeMkI                       = 883,
    Spy_t_BloodBotkillerKnifeMkI                      = 892,
    Spy_t_CarbonadoBotkillerKnifeMkI                  = 901,
    Spy_t_DiamondBotkillerKnifeMkI                    = 910,
    Spy_t_SilverBotkillerKnifeMkII                    = 959,
    Spy_t_GoldBotkillerKnifeMkII                      = 968,
    Spy_t_Boneyard                                    = 15062,
    Spy_t_BlueMew                                     = 15094,
    Spy_t_BrainCandy                                  = 15095,
    Spy_t_StabbedtoHell                               = 15096,
    Spy_t_DressedtoKill                               = 15118,
    Spy_t_TopShelf                                    = 15119,
    Spy_t_Blitzkrieg                                  = 15143,
    Spy_t_Airwolf                                     = 15144,
    Spy_t_PrinnyMachete                               = 30758,
    Spy_d_DisguiseKitPDA                              = 27,
    Spy_w_InvisWatch                                  = 30,
    Spy_w_InvisWatchR                                 = 212,
    Spy_w_TheDeadRinger                               = 59,
    Spy_w_TheCloakandDagger                           = 60,
    Spy_w_EnthusiastsTimepiece                        = 297,
    Spy_w_TheQuackenbirdt                             = 947,
    Misc_t_FryingPan                                  = 264,
    Misc_t_GoldFryingPan                              = 1071,
    Misc_t_Saxxy                                      = 423,
};

typedef enum
{
    DPT_Int = 0,
    DPT_Float,
    DPT_Vector,
    DPT_VectorXY, // Only encodes the XY of a vector, ignores Z
    DPT_String,
    DPT_Array, // An array of the base types (can't be of datatables).
    DPT_DataTable,
    DPT_NUMSendPropTypes
} SendPropType;

class DVariant
{
public:
    DVariant( )
    {
        m_Type = DPT_Float;
    }

    DVariant( float val )
    {
        m_Type  = DPT_Float;
        m_Float = val;
    }

    union
    {
        float       m_Float;
        int         m_Int;
        const char *m_pString;
        void       *m_pData; // For DataTables.
        float       m_Vector[ 3 ];
    };
    SendPropType m_Type;
};

static_assert( sizeof( DVariant ) == 24 );

class CRecvDecoder;

class RecvTable;
class RecvProp;

// This is passed into RecvProxy functions.
class CRecvProxyData
{
public:
    const RecvProp *m_pRecvProp; // The property it's receiving.
    DVariant        m_Value;     // The value given to you to store.
    int             m_iElement;  // Which array element you're getting.
    int             m_ObjectID;  // The object being referred to.
};

static_assert( __builtin_offsetof( CRecvProxyData, m_iElement ) == 32 );
static_assert( __builtin_offsetof( CRecvProxyData, m_ObjectID ) == 36 );

class IClientNetworkable;
// The serial number that gets passed in is used for ehandles.
typedef IClientNetworkable *( *CreateClientClassFn )( int entnum, int serialNum );
typedef IClientNetworkable *( *CreateEventFn )( );

//-----------------------------------------------------------------------------
// pStruct = the base structure of the datatable this variable is in (like C_BaseEntity)
// pOut    = the variable that this this proxy represents (like C_BaseEntity::m_SomeValue).
//
// Convert the network-standard-type value in m_Value into your own format in pStruct/pOut.
//-----------------------------------------------------------------------------
typedef void ( *RecvVarProxyFn )( const CRecvProxyData *pData, void *pStruct, void *pOut );

// ------------------------------------------------------------------------ //
// ArrayLengthRecvProxies are optionally used to get the length of the
// incoming array when it changes.
// ------------------------------------------------------------------------ //
typedef void ( *ArrayLengthRecvProxyFn )( void *pStruct, int objectID, int currentArrayLength );

// NOTE: DataTable receive proxies work differently than the other proxies.
// pData points at the object + the recv table's offset.
// pOut should be set to the location of the object to unpack the data table into.
// If the parent object just contains the child object, the default proxy just does *pOut = pData.
// If the parent object points at the child object, you need to dereference the pointer here.
// NOTE: don't ever return null from a DataTable receive proxy function. Bad things will happen.
typedef void ( *DataTableRecvVarProxyFn )( const RecvProp *pProp, void **pOut, void *pData, int objectID );

class RecvProp
{
    // This info comes from the receive data table.
public:
    const char  *m_pVarName;
    SendPropType m_RecvType;
    int          m_Flags;
    int          m_StringBufferSize;

    bool m_bInsideArray; // Set to true by the engine if this property sits inside an array.

    // Extra data that certain special property types bind to the property here.
    const void *m_pExtraData;

    // If this is an array (DPT_Array).
    RecvProp              *m_pArrayProp;
    ArrayLengthRecvProxyFn m_ArrayLengthProxy;

    RecvVarProxyFn          m_ProxyFn;
    DataTableRecvVarProxyFn m_DataTableProxyFn; // For RDT_DataTable.

    RecvTable *m_pDataTable; // For RDT_DataTable.
    int        m_Offset;

    int m_ElementStride;
    int m_nElements;

    // If it's one of the numbered "000", "001", etc properties in an array, then
    // these can be used to get its array property name for debugging.
    const char *m_pParentArrayPropName;
};

class RecvTable
{
public:
    // Properties described in a table.
    RecvProp     *m_pProps;
    int           m_nProps;
    // The decoder. NOTE: this covers each RecvTable AND all its children (ie: its children
    // will have their own decoders that include props for all their children).
    CRecvDecoder *m_pDecoder;
    const char   *m_pNetTableName; // The name matched between client and server.
    bool          m_bInitialized;
    bool          m_bInMainList;
};

inline constexpr i32 SPROP_UNSIGNED{ 1 << 0 };
inline constexpr i32 SPROP_COORD{ 1 << 1 };
inline constexpr i32 SPROP_NOSCALE{ 1 << 2 };
inline constexpr i32 SPROP_ROUNDDOWN{ 1 << 3 };
inline constexpr i32 SPROP_ROUNDUP{ 1 << 4 };
inline constexpr i32 SPROP_NORMAL{ 1 << 5 };
inline constexpr i32 SPROP_EXCLUDE{ 1 << 6 };
inline constexpr i32 SPROP_XYZE{ 1 << 7 };
inline constexpr i32 SPROP_INSIDEARRAY{ 1 << 8 };
inline constexpr i32 SPROP_PROXY_ALWAYS_YES{ 1 << 9 };
inline constexpr i32 SPROP_CHANGES_OFTEN{ 1 << 10 };
inline constexpr i32 SPROP_IS_A_VECTOR_ELEM{ 1 << 11 };
inline constexpr i32 SPROP_COLLAPSIBLE{ 1 << 12 };
inline constexpr i32 SPROP_COORD_MP{ 1 << 13 };
inline constexpr i32 SPROP_COORD_MP_LOWPRECISION{ 1 << 14 };
inline constexpr i32 SPROP_COORD_MP_INTEGRAL{ 1 << 15 };

inline constexpr i32 COORD_DENOMINATOR{ 1 << 5 };
inline constexpr i32 COORD_DENOMINATOR_LOWPRECISION{ 1 << 3 };
inline constexpr i32 NORMAL_DENOMINATOR{ ( 1 << 11 ) - 1 };

class SendTable;

class SendProp
{
public:
    void     *m_pVTable;
    RecvProp *m_pMatchingRecvProp;

    SendPropType m_Type;
    int          m_nBits;
    float        m_fLowValue;
    float        m_fHighValue;

    SendProp *m_pArrayProp;
    void     *m_ArrayLengthProxy;

    int m_nElements;
    int m_ElementStride;

    const char *m_pExcludeDTName;
    const char *m_pParentArrayPropName;

    const char *m_pVarName;
    float       m_fHighLowMul;

    int         m_Flags;
    void       *m_ProxyFn;
    void       *m_DataTableProxyFn;
    SendTable  *m_pDataTable;
    int         m_Offset;
    const void *m_pExtraData;
};

class SendTable
{
public:
    SendProp   *m_pProps;
    int         m_nProps;
    const char *m_pNetTableName;
    void       *m_pPrecalc;

    bool m_bInitialized : 1;
    bool m_bHasBeenWritten : 1;
    bool m_bHasPropsEncodedAgainstCurrentTickCount : 1;
};

//-----------------------------------------------------------------------------
// Purpose: Client side class definition
//-----------------------------------------------------------------------------
class ClientClass
{
public:
    CreateClientClassFn m_pCreateFn;
    CreateEventFn       m_pCreateEventFn; // Only called for event objects.
    const char         *m_pNetworkName;
    RecvTable          *m_pRecvTable;
    ClientClass        *m_pNext;
    int                 m_ClassID; // Managed by the engine.

    ETFClassID GetClassID( );
};

class CHLClient
{
public:
    ClientClass *GetAllClasses( );
    bool         GetPlayerView( CViewSetup &playerView );
    void         CreateMove( int sequence_number, float input_sample_frametime, bool active );
    bool         WriteUsercmdDeltaToBuffer( bf_write *buf, int from, int to, bool isnewcommand );
};

constexpr int MAX_PLAYER_NAME_LENGTH = 32;
constexpr int SIGNED_GUID_LEN        = 32;
constexpr int MAX_CUSTOM_FILES       = 4;
constexpr int MAX_OSPATH             = 260;

enum ObserverMode_t
{
    OBS_MODE_NONE = 0,
    OBS_MODE_DEATHCAM,
    OBS_MODE_FREEZECAM,
    OBS_MODE_FIXED,
    OBS_MODE_IN_EYE,
    OBS_MODE_CHASE,
    OBS_MODE_POI,
    OBS_MODE_ROAMING,
};

struct player_info_t
{
    char          name[ MAX_PLAYER_NAME_LENGTH ];
    int           userID;
    char          guid[ SIGNED_GUID_LEN + 1 ];
    unsigned int  friendsID;
    char          friendsName[ MAX_PLAYER_NAME_LENGTH ];
    bool          fakeplayer;
    bool          ishltv;
    bool          isreplay;
    CRC32_t       customFiles[ MAX_CUSTOM_FILES ];
    unsigned char filesDownloaded;
};

enum ConnectionStatus_t
{
    CONNECTION_STATE_DISCONNECTED = 0,
    CONNECTION_STATE_CONNECTING,
    CONNECTION_STATE_CONNECTION_FAILED,
    CONNECTION_STATE_CONNECTED
};

inline constexpr int SIGNONSTATE_NONE{ 0 };
inline constexpr int SIGNONSTATE_CHALLENGE{ 1 };
inline constexpr int SIGNONSTATE_CONNECTED{ 2 };
inline constexpr int SIGNONSTATE_NEW{ 3 };
inline constexpr int SIGNONSTATE_PRESPAWN{ 4 };
inline constexpr int SIGNONSTATE_SPAWN{ 5 };
inline constexpr int SIGNONSTATE_FULL{ 6 };
inline constexpr int SIGNONSTATE_CHANGELEVEL{ 7 };

inline constexpr int net_Tick{ 3 };
inline constexpr int net_StringCmd{ 4 };
inline constexpr int net_SetConVar{ 5 };
inline constexpr int clc_Move{ 9 };

inline constexpr int NETMSG_TYPE_BITS{ 6 };
inline constexpr int NUM_NEW_COMMAND_BITS{ 4 };
inline constexpr int MAX_NEW_COMMANDS{ ( 1 << NUM_NEW_COMMAND_BITS ) - 1 };
inline constexpr int NUM_BACKUP_COMMAND_BITS{ 3 };
inline constexpr int MAX_BACKUP_COMMANDS{ ( 1 << NUM_BACKUP_COMMAND_BITS ) - 1 };
inline constexpr int MAX_CMD_BUFFER{ 4000 };

inline constexpr int NET_FRAMES_BACKUP{ 64 };
inline constexpr int MAX_STREAMS{ 2 };
inline constexpr int MAX_SUBCHANNELS{ 8 };
inline constexpr int FRAGMENT_SIZE{ 256 };

inline constexpr float NET_TICK_SCALEUP{ 100000.0f };

enum ENetGroup
{
    NET_GENERIC = 0,
    NET_LOCALPLAYER,
    NET_OTHERPLAYERS,
    NET_ENTITIES,
    NET_SOUNDS,
    NET_EVENTS,
    NET_USERMESSAGES,
    NET_ENTMESSAGES,
    NET_VOICE,
    NET_STRINGTABLE,
    NET_MOVE,
    NET_STRINGCMD,
    NET_SIGNON,
    NET_TOTAL,
};

inline constexpr int MAX_WEAPON_STRING{ 80 };
inline constexpr int MAX_WEAPON_PREFIX{ 16 };
inline constexpr int MAX_WEAPON_AMMO_NAME{ 32 };
inline constexpr int NUM_SHOOT_SOUND_TYPES{ 16 };

using WEAPON_FILE_INFO_HANDLE = unsigned short;

struct WeaponData_t
{
    int   m_nDamage;
    int   m_nBulletsPerShot;
    float m_flRange;
    float m_flSpread;
    float m_flPunchAngle;
    float m_flTimeFireDelay;
    float m_flTimeIdle;
    float m_flTimeIdleEmpty;
    float m_flTimeReloadStart;
    float m_flTimeReload;
    bool  m_bDrawCrosshair;
    int   m_iProjectile;
    int   m_iAmmoPerShot;
    float m_flProjectileSpeed;
    float m_flSmackDelay;
    bool  m_bUseRapidFireCrits;
};

class FileWeaponInfo_t
{
public:
    void *vmt;
    bool  bParsedScript;
    bool  bLoadedHudElements;

    char szClassName[ MAX_WEAPON_STRING ];
    char szPrintName[ MAX_WEAPON_STRING ];
    char szViewModel[ MAX_WEAPON_STRING ];
    char szWorldModel[ MAX_WEAPON_STRING ];
    char szAnimationPrefix[ MAX_WEAPON_PREFIX ];

    int  iSlot;
    int  iPosition;
    int  iMaxClip1;
    int  iMaxClip2;
    int  iDefaultClip1;
    int  iDefaultClip2;
    int  iWeight;
    int  iRumbleEffect;
    bool bAutoSwitchTo;
    bool bAutoSwitchFrom;
    int  iFlags;

    char szAmmo1[ MAX_WEAPON_AMMO_NAME ];
    char szAmmo2[ MAX_WEAPON_AMMO_NAME ];

    char aShootSounds[ NUM_SHOOT_SOUND_TYPES ][ MAX_WEAPON_STRING ];

    int  iAmmoType;
    int  iAmmo2Type;
    bool m_bMeleeWeapon;
    bool m_bBuiltRightHanded;
    bool m_bAllowFlipping;

    int   iSpriteCount;
    void *iconActive;
    void *iconInactive;
    void *iconAmmo;
    void *iconAmmo2;
    void *iconCrosshair;
    void *iconAutoaim;
    void *iconZoomedCrosshair;
    void *iconZoomedAutoaim;
    void *iconSmall;

    bool bShowUsageHint;
};

class CTFWeaponInfo : public FileWeaponInfo_t
{
public:
    const WeaponData_t &GetWeaponData( int mode ) const
    {
        return this->m_WeaponData[ mode ];
    }

    WeaponData_t m_WeaponData[ 2 ];
    i32          m_iWeaponType;
};

struct FireBulletsInfo_t
{
    i32   m_iShots;
    vec3  m_vecSrc;
    vec3  m_vecDirShooting;
    vec3  m_vecSpread;
    float m_flDistance;
    i32   m_iAmmoType;
    i32   m_iTracerFreq;
    float m_flDamage;
    i32   m_iPlayerDamage;
    i32   m_nFlags;
    float m_flDamageForceScale;
};

// BEAAAAAAAAAANS
class C_BaseEntity;

struct BobState_t
{
    float m_flBobTime{ };
    float m_flLastBobTime{ };
    float m_flLastSpeed{ };
    float m_flVerticalBob{ };
    float m_flLateralBob{ };
};

struct BeamInfo_t
{
    i32           m_nType{ };
    C_BaseEntity *m_pStartEnt{ };
    i32           m_nStartAttachment{ };
    C_BaseEntity *m_pEndEnt{ };
    i32           m_nEndAttachment{ };
    vec3          m_vecStart{ };
    vec3          m_vecEnd{ };
    i32           m_nModelIndex{ -1 };
    const char   *m_pszModelName{ };
    i32           m_nHaloIndex{ -1 };
    const char   *m_pszHaloName{ };
    float         m_flHaloScale{ };
    float         m_flLife{ };
    float         m_flWidth{ };
    float         m_flEndWidth{ };
    float         m_flFadeLength{ };
    float         m_flAmplitude{ };
    float         m_flBrightness{ };
    float         m_flSpeed{ };
    i32           m_nStartFrame{ };
    float         m_flFrameRate{ };
    float         m_flRed{ };
    float         m_flGreen{ };
    float         m_flBlue{ };
    bool          m_bRenderable{ true };
    i32           m_nSegments{ -1 };
    i32           m_nFlags{ };
    vec3          m_vecCenter{ };
    float         m_flStartRadius{ };
    float         m_flEndRadius{ };
};

class Beam_t;

class IViewRenderBeams
{
public:
    Beam_t *CreateBeamPoints( BeamInfo_t &info );
    void    DrawBeam( Beam_t *beam );
};

enum MoveType_t : u8
{
    MOVETYPE_NONE = 0,
    MOVETYPE_ISOMETRIC,
    MOVETYPE_WALK,
    MOVETYPE_STEP,
    MOVETYPE_FLY,
    MOVETYPE_FLYGRAVITY,
    MOVETYPE_VPHYSICS,
    MOVETYPE_PUSH,
    MOVETYPE_NOCLIP,
    MOVETYPE_LADDER,
    MOVETYPE_OBSERVER,
    MOVETYPE_CUSTOM,
};

constexpr auto FL_ONGROUND              = ( 1 << 0 ); // At rest / on the ground
constexpr auto FL_DUCKING               = ( 1 << 1 ); // Player flag -- Player is fully crouched
constexpr auto FL_ANIMDUCKING           = ( 1 << 2 ); // Player flag -- Player is in the process of crouching or uncrouching but could be in transition
// examples:                                   Fully ducked:  FL_DUCKING &  FL_ANIMDUCKING
//           Previously fully ducked, unducking in progress:  FL_DUCKING & !FL_ANIMDUCKING
//                                           Fully unducked: !FL_DUCKING & !FL_ANIMDUCKING
//           Previously fully unducked, ducking in progress: !FL_DUCKING &  FL_ANIMDUCKING
constexpr auto FL_WATERJUMP             = ( 1 << 3 ); // player jumping out of water
constexpr auto FL_ONTRAIN               = ( 1 << 4 ); // Player is _controlling_ a train, so movement commands should be ignored on client during prediction.
constexpr auto FL_INRAIN                = ( 1 << 5 ); // Indicates the entity is standing in rain
constexpr auto FL_FROZEN                = ( 1 << 6 ); // Player is frozen for 3rd person camera
constexpr auto FL_ATCONTROLS            = ( 1 << 7 ); // Player can't move, but keeps key inputs for controlling another entity
constexpr auto FL_CLIENT                = ( 1 << 8 ); // Is a player
constexpr auto FL_FAKECLIENT            = ( 1 << 9 ); // Fake client, simulated server side; don't send network messages to them
// NON-PLAYER SPECIFIC (i.e., not used by GameMovement or the client .dll ) -- Can still be applied to players, though
constexpr auto FL_INWATER               = ( 1 << 10 ); // In water
// NOTE if you move things up, make sure to change this value
constexpr auto PLAYER_FLAG_BITS         = 32;
constexpr auto FL_FLY                   = ( 1 << 11 ); // Changes the SV_Movestep() behavior to not need to be on ground
constexpr auto FL_SWIM                  = ( 1 << 12 ); // Changes the SV_Movestep() behavior to not need to be on ground (but stay in water)
constexpr auto FL_CONVEYOR              = ( 1 << 13 );
constexpr auto FL_NPC                   = ( 1 << 14 );
constexpr auto FL_GODMODE               = ( 1 << 15 );
constexpr auto FL_NOTARGET              = ( 1 << 16 );
constexpr auto FL_AIMTARGET             = ( 1 << 17 ); // set if the crosshair needs to aim onto the entity
constexpr auto FL_PARTIALGROUND         = ( 1 << 18 ); // not all corners are valid
constexpr auto FL_STATICPROP            = ( 1 << 19 ); // Eetsa static prop!
constexpr auto FL_GRAPHED               = ( 1 << 20 ); // worldgraph has this ent listed as something that blocks a connection
constexpr auto FL_GRENADE               = ( 1 << 21 );
constexpr auto FL_STEPMOVEMENT          = ( 1 << 22 ); // Changes the SV_Movestep() behavior to not do any processing
constexpr auto FL_DONTTOUCH             = ( 1 << 23 ); // Doesn't generate touch functions, generates Untouch() for anything it was touching when this flag was set
constexpr auto FL_BASEVELOCITY          = ( 1 << 24 ); // Base velocity has been applied this frame (used to convert base velocity into momentum)
constexpr auto FL_WORLDBRUSH            = ( 1 << 25 ); // Not moveable/removeable brush entity (really part of the world, but represented as an entity for transparency or something)
constexpr auto FL_OBJECT                = ( 1 << 26 ); // Terrible name. This is an object that NPCs should see. Missiles, for example.
constexpr auto FL_KILLME                = ( 1 << 27 ); // This entity is marked for death -- will be freed by game DLL
constexpr auto FL_ONFIRE                = ( 1 << 28 ); // You know...
constexpr auto FL_DISSOLVING            = ( 1 << 29 ); // We're dissolving!
constexpr auto FL_TRANSRAGDOLL          = ( 1 << 30 ); // In the process of turning into a client side ragdoll.
constexpr auto FL_UNBLOCKABLE_BY_PLAYER = ( 1 << 31 ); // pusher that can't be blocked by the player

constexpr auto SF_DOOR_NONSOLID_TO_PLAYER = ( 1 << 2 );
constexpr auto SF_DOOR_PASSABLE           = ( 1 << 3 );
constexpr auto SF_DOOR_NO_AUTO_RETURN     = ( 1 << 5 );
constexpr auto SF_DOOR_PTOUCH             = ( 1 << 10 );
constexpr auto SF_DOOR_LOCKED             = ( 1 << 11 );

constexpr auto SF_TRIGGER_ALLOW_CLIENTS = ( 1 << 0 );

enum gamerules_roundstate_t : i32
{
    GR_STATE_INIT,
    GR_STATE_PREGAME,
    GR_STATE_STARTGAME,
    GR_STATE_PREROUND,
    GR_STATE_RND_RUNNING,
    GR_STATE_TEAM_WIN,
    GR_STATE_RESTART,
    GR_STATE_STALEMATE,
    GR_STATE_GAME_OVER,
    GR_STATE_BONUS,
    GR_STATE_BETWEEN_RNDS,
    GR_NUM_ROUND_STATES
};

enum InterpolatedVarFlags_t : i32
{
    LATCH_ANIMATION_VAR                    = ( 1 << 0 ),
    LATCH_SIMULATION_VAR                   = ( 1 << 1 ),
    EXCLUDE_AUTO_LATCH                     = ( 1 << 2 ),
    EXCLUDE_AUTO_INTERPOLATE               = ( 1 << 3 ),
    INTERPOLATE_LINEAR_ONLY                = ( 1 << 4 ),
    INTERPOLATE_OMIT_UPDATE_LAST_NETWORKED = ( 1 << 5 ),
};

enum
{
    INTERPOLATE_STOP = 0,
    INTERPOLATE_CONTINUE
};

constexpr auto STUDIO_LOOPING   = 0x0001;
constexpr auto STUDIO_SNAP      = 0x0002;
constexpr auto STUDIO_DELTA     = 0x0004;
constexpr auto STUDIO_AUTOPLAY  = 0x0008;
constexpr auto STUDIO_POST      = 0x0010;
constexpr auto STUDIO_ALLZEROS  = 0x0020;
constexpr auto STUDIO_CYCLEPOSE = 0x0080;
constexpr auto STUDIO_REALTIME  = 0x0100;
constexpr auto STUDIO_LOCAL     = 0x0200;
constexpr auto STUDIO_HIDDEN    = 0x0400;
constexpr auto STUDIO_OVERRIDE  = 0x0800;
constexpr auto STUDIO_ACTIVITY  = 0x1000;
constexpr auto STUDIO_EVENT     = 0x2000;
constexpr auto STUDIO_WORLD     = 0x4000;

constexpr auto IN_ATTACK     = ( 1 << 0 );
constexpr auto IN_JUMP       = ( 1 << 1 );
constexpr auto IN_DUCK       = ( 1 << 2 );
constexpr auto IN_FORWARD    = ( 1 << 3 );
constexpr auto IN_BACK       = ( 1 << 4 );
constexpr auto IN_USE        = ( 1 << 5 );
constexpr auto IN_CANCEL     = ( 1 << 6 );
constexpr auto IN_LEFT       = ( 1 << 7 );
constexpr auto IN_RIGHT      = ( 1 << 8 );
constexpr auto IN_MOVELEFT   = ( 1 << 9 );
constexpr auto IN_MOVERIGHT  = ( 1 << 10 );
constexpr auto IN_ATTACK2    = ( 1 << 11 );
constexpr auto IN_RUN        = ( 1 << 12 );
constexpr auto IN_RELOAD     = ( 1 << 13 );
constexpr auto IN_ALT1       = ( 1 << 14 );
constexpr auto IN_ALT2       = ( 1 << 15 );
constexpr auto IN_SCORE      = ( 1 << 16 ); // Used by client.dll for when scoreboard is held down
constexpr auto IN_SPEED      = ( 1 << 17 ); // Player is holding the speed key
constexpr auto IN_WALK       = ( 1 << 18 ); // Player holding walk key
constexpr auto IN_ZOOM       = ( 1 << 19 ); // Zoom key for HUD zoom
constexpr auto IN_WEAPON1    = ( 1 << 20 ); // weapon defines these bits
constexpr auto IN_WEAPON2    = ( 1 << 21 ); // weapon defines these bits
constexpr auto IN_BULLRUSH   = ( 1 << 22 );
constexpr auto IN_GRENADE1   = ( 1 << 23 ); // grenade 1
constexpr auto IN_GRENADE2   = ( 1 << 24 ); // grenade 2
constexpr auto IN_ATTACK3    = ( 1 << 25 );
constexpr auto IN_PREDICTION = ( 1 << 26 );

inline constexpr int FLOW_OUTGOING{ 0 };
inline constexpr int FLOW_INCOMING{ 1 };

class CNetChannel;

class INetMessage
{
public:
    virtual ~INetMessage( )
    {
    }

    virtual void         SetNetChannel( CNetChannel *netchan ) = 0;
    virtual void         SetReliable( bool state )             = 0;
    virtual bool         Process( )                            = 0;
    virtual bool         ReadFromBuffer( bf_read &buffer )     = 0;
    virtual bool         WriteToBuffer( bf_write &buffer )     = 0;
    virtual bool         IsReliable( ) const                   = 0;
    virtual int          GetType( ) const                      = 0;
    virtual int          GetGroup( ) const                     = 0;
    virtual const char  *GetName( ) const                      = 0;
    virtual CNetChannel *GetNetChannel( ) const                = 0;
    virtual const char  *ToString( ) const                     = 0;
};

class CNetMessage : public INetMessage
{
public:
    void SetNetChannel( CNetChannel *netchan ) override
    {
        m_NetChannel = netchan;
    }

    void SetReliable( bool state ) override
    {
        m_bReliable = state;
    }

    bool Process( ) override
    {
        return false;
    }

    bool IsReliable( ) const override
    {
        return m_bReliable;
    }

    int GetGroup( ) const override
    {
        return NET_GENERIC;
    }

    CNetChannel *GetNetChannel( ) const override
    {
        return m_NetChannel;
    }

protected:
    bool         m_bReliable{ true };
    CNetChannel *m_NetChannel{ };
    u8           pad0[ 8 ]{ };
};

struct dataFragments_t
{
    void *file{ };
    char  filename[ 260 ]{ };
    u8    pad10C[ 4 ]{ };
    char *buffer{ };
    u32   bytes{ };
    u32   bits{ };
    u32   transferID{ };
    bool  isCompressed{ };
    u8    pad125[ 3 ]{ };
    u32   nUncompressedSize{ };
    i32   numFragments{ };
    i32   ackedFragments{ };
    i32   pendingFragments{ };
};

static_assert( sizeof( dataFragments_t ) == 312 );
static_assert( __builtin_offsetof( dataFragments_t, buffer ) == 272 );
static_assert( __builtin_offsetof( dataFragments_t, numFragments ) == 300 );

class CNetChannel
{
public:
    bool  IsLoopback( );
    bool  IsTimingOut( );
    bool  CanPacket( );
    float GetLatency( int flow );
    void  SetChoked( );
    int   SendDatagram( bf_write *data );
    bool  SendNetMsg( INetMessage &msg, bool bForceReliable, bool bVoice );

    void              *vmt;
    ConnectionStatus_t m_ConnectionState;
    int                m_nOutSequenceNr;
    int                m_nInSequenceNr;
    int                m_nOutSequenceNrAck;
    int                m_nOutReliableState;
    int                m_nInReliableState;
    int                m_nChokedPackets;
    u8                 gap24[ 4 ]{ };
    bf_write           m_StreamReliable{ };
    u8                 gap48[ 16 ]{ };
    bf_write           m_StreamUnreliable{ };
    u8                 gap78[ 0x168 - 0x78 ]{ };
    dataFragments_t    m_ReceiveList[ MAX_STREAMS ]{ };
    u8                 gap3D8[ 0x235C - 0x3D8 ]{ };
    i32                m_PacketDrop{ };
};

static_assert( __builtin_offsetof( CNetChannel, m_StreamReliable ) == 0x28 );
static_assert( __builtin_offsetof( CNetChannel, m_StreamUnreliable ) == 0x58 );
static_assert( __builtin_offsetof( CNetChannel, m_ReceiveList ) == 0x168 );
static_assert( __builtin_offsetof( CNetChannel, m_PacketDrop ) == 0x235C );

class CLC_Move : public CNetMessage
{
public:
    CLC_Move( )
    {
        m_bReliable = false;
    }

    bool ReadFromBuffer( bf_read &buffer ) override;
    bool WriteToBuffer( bf_write &buffer ) override;

    int GetType( ) const override
    {
        return clc_Move;
    }

    int GetGroup( ) const override
    {
        return NET_MOVE;
    }

    const char *GetName( ) const override
    {
        return "clc_Move";
    }

    const char *ToString( ) const override
    {
        return "clc_Move";
    }

    void    *m_pMessageHandler{ };
    int      m_nBackupCommands{ };
    int      m_nNewCommands{ };
    int      m_nLength{ };
    bf_read  m_DataIn{ };
    bf_write m_DataOut{ };
};

class NET_Tick : public CNetMessage
{
public:
    NET_Tick( int tick, float host_frametime, float host_frametime_stddeviation ) :
        m_nTick( tick ),
        m_flHostFrameTime( host_frametime ),
        m_flHostFrameTimeStdDeviation( host_frametime_stddeviation )
    {
        m_bReliable = false;
    }

    bool ReadFromBuffer( bf_read &buffer ) override;
    bool WriteToBuffer( bf_write &buffer ) override;

    int GetType( ) const override
    {
        return net_Tick;
    }

    const char *GetName( ) const override
    {
        return "net_Tick";
    }

    const char *ToString( ) const override
    {
        return "net_Tick";
    }

    void *m_pMessageHandler{ };
    int   m_nTick{ };
    float m_flHostFrameTime{ };
    float m_flHostFrameTimeStdDeviation{ };
};

class NET_StringCmd : public CNetMessage
{
public:
    explicit NET_StringCmd( const char *command ) : m_szCommand( command )
    {
        m_bReliable = true;
    }

    bool ReadFromBuffer( bf_read &buffer ) override
    {
        return false;
    }

    bool WriteToBuffer( bf_write &buffer ) override;

    int GetType( ) const override
    {
        return net_StringCmd;
    }

    int GetGroup( ) const override
    {
        return NET_STRINGCMD;
    }

    const char *GetName( ) const override
    {
        return "net_StringCmd";
    }

    const char *ToString( ) const override
    {
        return "net_StringCmd";
    }

    void       *m_pMessageHandler{ };
    const char *m_szCommand{ };
};

class NET_SetConVar : public CNetMessage
{
public:
    NET_SetConVar( const char *name, const char *value ) : m_name( name ), m_value( value )
    {
    }

    bool ReadFromBuffer( bf_read & ) override
    {
        return false;
    }

    bool WriteToBuffer( bf_write &buffer ) override;

    int GetType( ) const override
    {
        return net_SetConVar;
    }

    int GetGroup( ) const override
    {
        return NET_STRINGCMD;
    }

    const char *GetName( ) const override
    {
        return "net_SetConVar";
    }

    const char *ToString( ) const override
    {
        return "net_SetConVar";
    }

private:
    const char *m_name{ };
    const char *m_value{ };
};

// Item definitions look into this array by hard-coded indices, so do not add entries
// anywhere except the end!
enum ProjectileType_t
{
    TF_PROJECTILE_NONE,
    TF_PROJECTILE_BULLET,
    TF_PROJECTILE_ROCKET,
    TF_PROJECTILE_PIPEBOMB,
    TF_PROJECTILE_PIPEBOMB_REMOTE,
    TF_PROJECTILE_SYRINGE,
    TF_PROJECTILE_FLARE,
    TF_PROJECTILE_JAR,
    TF_PROJECTILE_ARROW,
    TF_PROJECTILE_FLAME_ROCKET,
    TF_PROJECTILE_JAR_MILK,
    TF_PROJECTILE_HEALING_BOLT,
    TF_PROJECTILE_ENERGY_BALL,
    TF_PROJECTILE_ENERGY_RING,
    TF_PROJECTILE_PIPEBOMB_PRACTICE,
    TF_PROJECTILE_CLEAVER,
    TF_PROJECTILE_STICKY_BALL,
    TF_PROJECTILE_CANNONBALL,
    TF_PROJECTILE_BUILDING_REPAIR_BOLT,
    TF_PROJECTILE_FESTIVE_ARROW,
    TF_PROJECTILE_THROWABLE,
    TF_PROJECTILE_SPELL,
    TF_PROJECTILE_FESTIVE_JAR,
    TF_PROJECTILE_FESTIVE_HEALING_BOLT,
    TF_PROJECTILE_BREADMONSTER_JARATE,
    TF_PROJECTILE_BREADMONSTER_MADMILK,

    TF_PROJECTILE_GRAPPLINGHOOK,
    TF_PROJECTILE_SENTRY_ROCKET,
    TF_PROJECTILE_BREAD_MONSTER,

    TF_NUM_PROJECTILES
};

class KeyValues;
class CEngineClient
{
public:
    int          GetLocalPlayer( );
    double       Plat_FloatTime( );
    bool         GetPlayerInfo( int ent_num, player_info_t *pinfo );
    int          GetPlayerForUserID( int userid );
    bool         IsInGame( );
    bool         IsConnected( );
    bool         IsPlayingDemo( );
    void         GetScreenSize( int &width, int &height );
    void         GetViewAngles( vec3 &va );
    void         SetViewAngles( vec3 &va );
    int          GetMaxClients( );
    CNetChannel *GetNetChannelInfo( );
    void         FireEvents( );
    void         ClientCmd_Unrestricted( const char *input );
    void         ServerCmdKeyValues( KeyValues *pKeyValues );
    bool         IsTakingScreenshot( );
    const char  *GetMapEntitiesString( );
};

class CUniformRandomStream
{
public:
    void  SetSeed( int seed );
    float RandomFloat( float minimum, float maximum );
    int   RandomInt( int minimum, int maximum );

private:
    void *m_vtable{ };
    int   m_idum{ };
    int   m_iy{ };
    int   m_iv[ 32 ]{ };
    u64   m_mutex_owner{ };
    int   m_mutex_depth{ };
};

static_assert( sizeof( CUniformRandomStream ) == 160 );

class CUserCmd
{
public:
    CRC32_t GetChecksum( void ) const;
    void    Reset( void );

private:
    void *m_vmt;

public:
    // For matching server and client commands for debugging
    int   command_number;
    // the tick the client created this command
    int   tick_count;
    // Player instantaneous view angles.
    vec3  viewangles;
    // Intended velocities
    //	forward velocity.
    float forwardmove;
    //  sideways velocity.
    float sidemove;
    //  upward velocity.
    float upmove;
    // Attack button states
    int   buttons;
    // Impulse command issued.
    u8    impulse;
    // Current weapon id
    int   weaponselect;
    int   weaponsubtype;
    int   random_seed;        // For shared random functions
    int   server_random_seed; // Only the server populates this seed
    short mousedx;            // mouse accum in x from create move
    short mousedy;            // mouse accum in y from create move
    // Client only, tracks whether we've predicted this command at least once
    bool  hasbeenpredicted;
};

class CVerifiedUserCmd
{
public:
    CUserCmd m_cmd;
    CRC32_t  m_crc;
};

static_assert( sizeof( CUserCmd ) == 0x48 );
static_assert( sizeof( CVerifiedUserCmd ) == 0x50 );

class CInput
{
public:
    void CAM_ToThirdPerson( );
    void CAM_ToFirstPerson( );
    bool CAM_IsThirdPerson( );

private:
    unsigned char m_pad108[ 0x108 ];

public:
    CUserCmd         *m_pCommands;
    CVerifiedUserCmd *m_pVerifiedCommands;
};

static_assert( sizeof( CInput ) == 0x118 );

enum MessageMode_t : int
{
    MM_NONE = 0,
    MM_SAY,
    MM_SAY_TEAM,
    MM_SAY_PARTY,
};

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
class CBaseHudChat
{
private:
    u8 m_pad716[ 716 ];

public:
    void          ChatPrintf( int iPlayerIndex, int iFilter, const char *fmt, ... );
    void          StartMessageMode( int iMessageModeType );
    MessageMode_t m_nMessageMode; // *( _DWORD * )( this + 716 )
};

class ClientModeShared
{
private:
    u8 m_pad48[ 48 ];

public:
    // Message mode handling
    // All modes share a common chat interface
    CBaseHudChat *m_pChatElement;
};

class CBaseHandle;
class ICollideable;
class IClientEntity;
class IClientNetworkable;
class IClientRenderable;
class IClientThinkable;
class IClientUnknown;
class C_BaseEntity;
class IPVSNotify;
class CMouthInfo;
class CClientThinkHandlePtr;
struct SpatializationInfo_t;
struct model_t;

using ClientThinkHandle_t = CClientThinkHandlePtr *;

enum ShouldTransmitState_t : int;
enum ShadowType_t : int;

enum DataUpdateType_t : int
{
    DATA_UPDATE_CREATED = 0,
    DATA_UPDATE_DATATABLE_CHANGED,
};

class IHandleEntity
{
public:
    virtual ~IHandleEntity( )
    {
    }
    virtual void               SetRefEHandle( const CBaseHandle &handle ) = 0;
    virtual const CBaseHandle &GetRefEHandle( ) const                     = 0;
};

class IClientUnknown : public IHandleEntity
{
public:
    virtual ICollideable       *GetCollideable( )       = 0;
    virtual IClientNetworkable *GetClientNetworkable( ) = 0;
    virtual IClientRenderable  *GetClientRenderable( )  = 0;
    virtual IClientEntity      *GetIClientEntity( )     = 0;
    virtual C_BaseEntity       *GetBaseEntity( )        = 0;
    virtual IClientThinkable   *GetClientThinkable( )   = 0;

    template< typename T >
    inline T As( )
    {
        return reinterpret_cast< T >( this );
    }
};

#if defined( ATLAS_DEV )
class CBaseAnimating
{
public:
    void GetBoneTransform( i32 bone, matrix3x4 &bone_to_world );
};

class IServerUnknown : public IHandleEntity
{
public:
    virtual ICollideable   *GetCollideable( ) = 0;
    virtual void           *GetNetworkable( ) = 0;
    virtual CBaseAnimating *GetBaseEntity( )  = 0;
};

inline constexpr i32 FL_EDICT_FREE{ 1 << 1 };

struct edict_t
{
    i32             m_fStateFlags;
    i32             m_NetworkSerialNumber;
    void           *m_pNetworkable;
    IServerUnknown *m_pUnk;
};

class IVEngineServer
{
public:
    edict_t *PEntityOfEntIndex( i32 index );
};
#endif // ATLAS_DEV

class IClientNetworkable
{
public:
    virtual IClientUnknown *GetIClientUnknown( )                                = 0;
    virtual void            Release( )                                          = 0;
    virtual ClientClass    *GetClientClass( )                                   = 0;
    virtual void            NotifyShouldTransmit( ShouldTransmitState_t state ) = 0;
    virtual void            OnPreDataChanged( DataUpdateType_t updateType )     = 0;
    virtual void            OnDataChanged( DataUpdateType_t updateType )        = 0;
    virtual void            PreDataUpdate( DataUpdateType_t updateType )        = 0;
    virtual void            PostDataUpdate( DataUpdateType_t updateType )       = 0;
    virtual bool            IsDormant( )                                        = 0;
    virtual int             entindex( ) const                                   = 0;
    virtual void            ReceiveMessage( int classID, bf_read &msg )         = 0;
    virtual void           *GetDataTableBasePtr( )                              = 0;
    virtual void            SetDestroyedOnRecreateEntities( )                   = 0;
    virtual void            OnDataUnchangedInPVS( )                             = 0;
};

class IClientRenderable
{
public:
    virtual IClientUnknown    *GetIClientUnknown( )                                                                                                 = 0;
    virtual const vec3        &GetRenderOrigin( )                                                                                                   = 0;
    virtual const vec3        &GetRenderAngles( )                                                                                                   = 0;
    virtual bool               ShouldDraw( )                                                                                                        = 0;
    virtual bool               IsTransparent( )                                                                                                     = 0;
    virtual bool               UsesPowerOfTwoFrameBufferTexture( )                                                                                  = 0;
    virtual bool               UsesFullFrameBufferTexture( )                                                                                        = 0;
    virtual unsigned short     GetShadowHandle( ) const                                                                                             = 0;
    virtual unsigned short    &RenderHandle( )                                                                                                      = 0;
    virtual const model_t     *GetModel( ) const                                                                                                    = 0;
    virtual int                DrawModel( int flags )                                                                                               = 0;
    virtual int                GetBody( )                                                                                                           = 0;
    virtual void               ComputeFxBlend( )                                                                                                    = 0;
    virtual int                GetFxBlend( )                                                                                                        = 0;
    virtual void               GetColorModulation( float *color )                                                                                   = 0;
    virtual bool               LODTest( )                                                                                                           = 0;
    virtual bool               SetupBones( matrix3x4 *pBoneToWorldOut, int nMaxBones, int boneMask, float currentTime )                             = 0;
    virtual void               SetupWeights( const matrix3x4 *pBoneToWorld, int nFlexWeightCount, float *pFlexWeights, float *pFlexDelayedWeights ) = 0;
    virtual void               DoAnimationEvents( )                                                                                                 = 0;
    virtual IPVSNotify        *GetPVSNotifyInterface( )                                                                                             = 0;
    virtual void               GetRenderBounds( vec3 &mins, vec3 &maxs )                                                                            = 0;
    virtual void               GetRenderBoundsWorldspace( vec3 &mins, vec3 &maxs )                                                                  = 0;
    virtual void               GetShadowRenderBounds( vec3 &mins, vec3 &maxs, ShadowType_t shadowType )                                             = 0;
    virtual bool               ShouldReceiveProjectedTextures( int flags )                                                                          = 0;
    virtual bool               GetShadowCastDistance( float *pDist, ShadowType_t shadowType ) const                                                 = 0;
    virtual bool               GetShadowCastDirection( vec3 *pDirection, ShadowType_t shadowType ) const                                            = 0;
    virtual bool               IsShadowDirty( )                                                                                                     = 0;
    virtual void               MarkShadowDirty( bool bDirty )                                                                                       = 0;
    virtual IClientRenderable *GetShadowParent( )                                                                                                   = 0;
    virtual IClientRenderable *FirstShadowChild( )                                                                                                  = 0;
    virtual IClientRenderable *NextShadowPeer( )                                                                                                    = 0;
    virtual ShadowType_t       ShadowCastType( )                                                                                                    = 0;
    virtual void               CreateModelInstance( )                                                                                               = 0;
    virtual unsigned short     GetModelInstance( )                                                                                                  = 0;
    virtual const matrix3x4   &RenderableToWorldTransform( )                                                                                        = 0;
    virtual int                LookupAttachment( const char *pAttachmentName )                                                                      = 0;
    virtual bool               GetAttachment( int number, vec3 &origin, vec3 &angles )                                                              = 0;
    virtual bool               GetAttachment( int number, matrix3x4 &matrix )                                                                       = 0;
    virtual float             *GetRenderClipPlane( )                                                                                                = 0;
    virtual int                GetSkin( )                                                                                                           = 0;
    virtual bool               IsTwoPass( )                                                                                                         = 0;
    virtual void               OnThreadedDrawSetup( )                                                                                               = 0;
    virtual bool               UsesFlexDelayedWeights( )                                                                                            = 0;
    virtual void               RecordToolMessage( )                                                                                                 = 0;
    virtual bool               IgnoresZBuffer( ) const                                                                                              = 0;
};

class IClientThinkable
{
public:
    virtual IClientUnknown     *GetIClientUnknown( )                         = 0;
    virtual void                ClientThink( )                               = 0;
    virtual ClientThinkHandle_t GetThinkHandle( )                            = 0;
    virtual void                SetThinkHandle( ClientThinkHandle_t hThink ) = 0;
    virtual void                Release( )                                   = 0;
};

class IClientEntity : public IClientUnknown, public IClientRenderable, public IClientNetworkable, public IClientThinkable
{
public:
    virtual void        Release( )                                           = 0;
    virtual const vec3 &GetAbsOrigin( ) const                                = 0;
    virtual const vec3 &GetAbsAngles( ) const                                = 0;
    virtual CMouthInfo *GetMouth( )                                          = 0;
    virtual bool        GetSoundSpatialization( SpatializationInfo_t &info ) = 0;
};

inline constexpr int MAX_EDICT_BITS{ 11 };
inline constexpr int MAX_EDICTS{ 1 << MAX_EDICT_BITS };
inline constexpr int NUM_ENT_ENTRY_BITS{ MAX_EDICT_BITS + 2 };
inline constexpr int NUM_ENT_ENTRIES{ 1 << NUM_ENT_ENTRY_BITS };
inline constexpr int NUM_SERIAL_NUM_BITS{ 16 };
inline constexpr int NUM_SERIAL_NUM_SHIFT_BITS{ 32 - NUM_SERIAL_NUM_BITS };
inline constexpr int ENT_ENTRY_MASK{ ( 1 << NUM_SERIAL_NUM_SHIFT_BITS ) - 1 };
inline constexpr u32 INVALID_EHANDLE_INDEX{ 0xFFFFFFFFu };

class CBaseHandle
{
public:
    constexpr ATLAS_FORCE_INLINE CBaseHandle( )
    {
        m_Index = INVALID_EHANDLE_INDEX;
    }
    CBaseHandle( const CBaseHandle &other );
    explicit CBaseHandle( unsigned long value );
    CBaseHandle( int iEntry, int iSerialNumber );

    void Init( int iEntry, int iSerialNumber );
    void Term( );

    bool IsValid( ) const;

    int           GetEntryIndex( ) const;
    int           GetSerialNumber( ) const;
    int           ToInt( ) const;
    unsigned long GetIndex( ) const;

    bool operator==( const CBaseHandle &other ) const;
    bool operator!=( const CBaseHandle &other ) const;
    bool operator<( const CBaseHandle &other ) const;

    bool operator==( const IHandleEntity *pEnt ) const;
    bool operator!=( const IHandleEntity *pEnt ) const;
    bool operator<( const IHandleEntity *pEnt ) const;

    const CBaseHandle &operator=( const IHandleEntity *pEntity );
    const CBaseHandle &Set( const IHandleEntity *pEntity );

    IHandleEntity *Get( ) const;

protected:
    unsigned long m_Index;
};

using EntityHandle_t = CBaseHandle;

template< class T >
class CHandle : public CBaseHandle
{
public:
    constexpr CHandle( ) = default;
    CHandle( int iEntry, int iSerialNumber );
    CHandle( const CBaseHandle &handle );
    CHandle( T *pVal );

    static CHandle< T > FromIndex( int index );

    T   *Get( ) const;
    void Set( const T *pVal );

    operator T *( );
    operator T *( ) const;

    bool operator!( ) const;
    bool operator==( T *val ) const;
    bool operator!=( T *val ) const;

    const CBaseHandle &operator=( const T *val );

    T *operator->( ) const;
};

class IClientEntityListener
{
public:
    virtual void OnEntityCreated( C_BaseEntity *entity ) = 0;
    virtual void OnEntityDeleted( C_BaseEntity *entity ) = 0;
};

template< class T >
struct CUtlVector;

class CClientEntityList
{
public:
    IClientEntity *GetClientEntity( int num );
    IClientEntity *GetClientEntityFromHandle( CBaseHandle num );
    int            GetHighestEntityIndex( );

    bool          AddListenerEntity( IClientEntityListener *listener );
    bool          RemoveListenerEntity( IClientEntityListener *listener );
    C_BaseEntity *NextBaseEntity( C_BaseEntity *previous );

private:
    CUtlVector< IClientEntityListener * > &EntityListeners( );
};

template< class T >
inline CHandle< T >::CHandle( int iEntry, int iSerialNumber )
{
    this->Init( iEntry, iSerialNumber );
}

template< class T >
inline CHandle< T >::CHandle( const CBaseHandle &handle ) : CBaseHandle( handle )
{
}

template< class T >
inline CHandle< T >::CHandle( T *pVal )
{
    this->Set( pVal );
}

template< class T >
inline CHandle< T > CHandle< T >::FromIndex( int index )
{
    CHandle< T > h{ };
    h.m_Index = static_cast< unsigned long >( index );
    return h;
}

template< class T >
inline T *CHandle< T >::Get( ) const
{
    return reinterpret_cast< T * >( CBaseHandle::Get( ) );
}

template< class T >
inline void CHandle< T >::Set( const T *pVal )
{
    CBaseHandle::Set( reinterpret_cast< const IHandleEntity * >( pVal ) );
}

template< class T >
inline CHandle< T >::operator T *( )
{
    return this->Get( );
}

template< class T >
inline CHandle< T >::operator T *( ) const
{
    return this->Get( );
}

template< class T >
inline bool CHandle< T >::operator!( ) const
{
    return !this->IsValid( );
}

template< class T >
inline bool CHandle< T >::operator==( T *val ) const
{
    return this->Get( ) == val;
}

template< class T >
inline bool CHandle< T >::operator!=( T *val ) const
{
    return this->Get( ) != val;
}

template< class T >
inline const CBaseHandle &CHandle< T >::operator=( const T *val )
{
    return CBaseHandle::operator=( reinterpret_cast< const IHandleEntity * >( val ) );
}

template< class T >
inline T *CHandle< T >::operator->( ) const
{
    return this->Get( );
}

using EHANDLE = CHandle< C_BaseEntity >;

struct CUtlString
{
    char *m_pString{ };
};

struct CClockDriftMgr
{
    float m_ClockOffsets[ 16 ];
    int   m_iCurClockOffset;
    int   m_nServerTick;
    int   m_nClientTick;
};

namespace vgui
{
    enum CursorCode
    {
        dc_user,
        dc_none,
        dc_arrow,
        dc_ibeam,
        dc_hourglass,
        dc_waitarrow,
        dc_crosshair,
        dc_up,
        dc_sizenwse,
        dc_sizenesw,
        dc_sizewe,
        dc_sizens,
        dc_sizeall,
        dc_no,
        dc_hand,
        dc_blank, // don't show any custom vgui cursor, just let windows do it stuff (for HTML widget)
        dc_last,
        dc_alwaysvisible_push,
        dc_alwaysvisible_pop,
    };

    // handle to an internal vgui panel
    // this is the only handle to a panel that is valid across dll boundaries
    typedef uptr VPANEL;

    // handles to vgui objects
    // NULL values signify an invalid value
    typedef u32  HScheme;
    // Both -1 and 0 are used for invalid textures. Be careful.
    typedef u32  HTexture;
    typedef u32  HCursor;
    typedef u32  HPanel;
    const HPanel INVALID_PANEL = 0xffffffff;
    typedef u32  HFont;
    const HFont  INVALID_FONT = 0; // the value of an invalid font handle

    //-----------------------------------------------------------------------------
    // Purpose: interface from Client panels -> vgui panels
    //-----------------------------------------------------------------------------
    class IPanel
    {
    public:
        const char *GetName( VPANEL vguiPanel );
    };
} // namespace vgui

class CEngineVGui
{
public:
    bool IsGameUIVisible( );
};

//-----------------------------------------------------------------------------
// What kind of material override is it?
//-----------------------------------------------------------------------------
enum OverrideType_t
{
    OVERRIDE_NORMAL = 0,
    OVERRIDE_BUILD_SHADOWS,
    OVERRIDE_DEPTH_WRITE,
    OVERRIDE_SSAO_DEPTH_WRITE,
};

enum MaterialVarFlags_t
{
    MATERIAL_VAR_DEBUG                    = ( 1 << 0 ),
    MATERIAL_VAR_NO_DEBUG_OVERRIDE        = ( 1 << 1 ),
    MATERIAL_VAR_NO_DRAW                  = ( 1 << 2 ),
    MATERIAL_VAR_USE_IN_FILLRATE_MODE     = ( 1 << 3 ),
    MATERIAL_VAR_VERTEXCOLOR              = ( 1 << 4 ),
    MATERIAL_VAR_VERTEXALPHA              = ( 1 << 5 ),
    MATERIAL_VAR_SELFILLUM                = ( 1 << 6 ),
    MATERIAL_VAR_ADDITIVE                 = ( 1 << 7 ),
    MATERIAL_VAR_ALPHATEST                = ( 1 << 8 ),
    MATERIAL_VAR_MULTIPASS                = ( 1 << 9 ),
    MATERIAL_VAR_ZNEARER                  = ( 1 << 10 ),
    MATERIAL_VAR_MODEL                    = ( 1 << 11 ),
    MATERIAL_VAR_FLAT                     = ( 1 << 12 ),
    MATERIAL_VAR_NOCULL                   = ( 1 << 13 ),
    MATERIAL_VAR_NOFOG                    = ( 1 << 14 ),
    MATERIAL_VAR_IGNOREZ                  = ( 1 << 15 ),
    MATERIAL_VAR_DECAL                    = ( 1 << 16 ),
    MATERIAL_VAR_ENVMAPSPHERE             = ( 1 << 17 ),
    MATERIAL_VAR_NOALPHAMOD               = ( 1 << 18 ),
    MATERIAL_VAR_ENVMAPCAMERASPACE        = ( 1 << 19 ),
    MATERIAL_VAR_BASEALPHAENVMAPMASK      = ( 1 << 20 ),
    MATERIAL_VAR_TRANSLUCENT              = ( 1 << 21 ),
    MATERIAL_VAR_NORMALMAPALPHAENVMAPMASK = ( 1 << 22 ),
    MATERIAL_VAR_NEEDS_SOFTWARE_SKINNING  = ( 1 << 23 ),
    MATERIAL_VAR_OPAQUETEXTURE            = ( 1 << 24 ),
    MATERIAL_VAR_ENVMAPMODE               = ( 1 << 25 ),
    MATERIAL_VAR_SUPPRESS_DECALS          = ( 1 << 26 ),
    MATERIAL_VAR_HALFLAMBERT              = ( 1 << 27 ),
    MATERIAL_VAR_WIREFRAME                = ( 1 << 28 ),
    MATERIAL_VAR_ALLOWALPHATOCOVERAGE     = ( 1 << 29 ),
    MATERIAL_VAR_IGNORE_ALPHA_MODULATION  = ( 1 << 30 ),
};

class IMaterialVar;

class IMaterial
{
public:
    const char   *GetName( );
    const char   *GetTextureGroupName( );
    void          ColorModulate( float r, float g, float b );
    void          SetMaterialVarFlag( MaterialVarFlags_t flag, bool on );
    void          IncrementReferenceCount( );
    void          DecrementReferenceCount( );
    void          DeleteIfUnreferenced( );
    IMaterialVar *FindVar( const char *name, bool *found, bool complain = true );
    bool          IsErrorMaterial( );
    bool          IsPrecached( );
    void          Precache( );
};

class IMaterialVar
{
public:
    void SetFloatValue( float value );
    void SetVecValue( float x, float y, float z ); // [[msvc-overload-vtable-order]]
};

struct CProxyAnimatedWeaponSheen
{
    void         *m_vtable;
    IMaterialVar *m_AnimatedTextureVar;
    IMaterialVar *m_AnimatedTextureFrameNumVar;
    float         m_FrameRate;
    bool          m_WrapAnimation;
    IMaterialVar *m_pSheenIndexVar;
    IMaterialVar *m_pTintVar;
};

static_assert( __builtin_offsetof( CProxyAnimatedWeaponSheen, m_pSheenIndexVar ) == 32 );
static_assert( __builtin_offsetof( CProxyAnimatedWeaponSheen, m_pTintVar ) == 40 );

struct EmitSound_t
{
    i32         m_nChannel;
    const char *m_pSoundName;
};

static_assert( __builtin_offsetof( EmitSound_t, m_pSoundName ) == 8 );

struct mstudiobone_t
{
    int       sznameindex;
    int       parent;
    int       bonecontroller[ 6 ];
    vec3      pos;
    float     quat[ 4 ];
    vec3      rot;
    vec3      posscale;
    vec3      rotscale;
    matrix3x4 poseToBone;
    float     qAlignment[ 4 ];
    int       flags;
    int       proctype;
    int       procindex;
    int       physicsbone;
    int       surfacepropidx;
    int       contents;
    int       unused[ 8 ];

    const char *pszName( ) const
    {
        return reinterpret_cast< const char * >( this ) + this->sznameindex;
    }

    const void *pProcedure( ) const
    {
        return reinterpret_cast< const char * >( this ) + this->procindex;
    }
};

static_assert( sizeof( mstudiobone_t ) == 216 );

inline constexpr i32 BONE_ALWAYS_PROCEDURAL{ 0x04 };
inline constexpr i32 STUDIO_PROC_JIGGLE{ 5 };

inline constexpr i32 JIGGLE_IS_FLEXIBLE{ 0x01 };
inline constexpr i32 JIGGLE_HAS_PITCH_CONSTRAINT{ 0x08 };
inline constexpr i32 JIGGLE_HAS_ANGLE_CONSTRAINT{ 0x10 };
inline constexpr i32 JIGGLE_HAS_LENGTH_CONSTRAINT{ 0x20 };
inline constexpr i32 JIGGLE_HAS_BASE_SPRING{ 0x40 };

struct mstudiojigglebone_t
{
    int   flags;
    float length;
    float tipMass;
    float yawStiffness;
    float yawDamping;
    float pitchStiffness;
    float pitchDamping;
    float alongStiffness;
    float alongDamping;
    float angleLimit;
    float minYaw;
    float maxYaw;
    float yawFriction;
    float yawBounce;
    float minPitch;
    float maxPitch;
    float pitchFriction;
    float pitchBounce;
    float baseMass;
    float baseStiffness;
    float baseDamping;
    float baseMinLeft;
    float baseMaxLeft;
    float baseLeftFriction;
    float baseMinUp;
    float baseMaxUp;
    float baseUpFriction;
    float baseMinForward;
    float baseMaxForward;
    float baseForwardFriction;
    float boingImpactSpeed;
    float boingImpactAngle;
    float boingDampingRate;
    float boingFrequency;
    float boingAmplitude;
};

static_assert( sizeof( mstudiojigglebone_t ) == 140 );

enum
{
    HITBOX_HEAD,
    HITBOX_PELVIS,
    HITBOX_SPINE_0,
    HITBOX_SPINE_1,
    HITBOX_SPINE_2,
    HITBOX_SPINE_3,
    HITBOX_UPPERARM_L,
    HITBOX_LOWERARM_L,
    HITBOX_HAND_L,
    HITBOX_UPPERARM_R,
    HITBOX_LOWERARM_R,
    HITBOX_HAND_R,
    HITBOX_HIP_L,
    HITBOX_KNEE_L,
    HITBOX_FOOT_L,
    HITBOX_HIP_R,
    HITBOX_KNEE_R,
    HITBOX_FOOT_R,
    HITBOX_MAX
};

enum
{
    HITGROUP_GENERIC = 0,
    HITGROUP_HEAD,
    HITGROUP_CHEST,
    HITGROUP_STOMACH,
    HITGROUP_LEFTARM,
    HITGROUP_RIGHTARM,
    HITGROUP_LEFTLEG,
    HITGROUP_RIGHTLEG,
};

class mstudiobbox_t
{
public:
    int  bone{ };
    int  group{ };
    vec3 bbmin{ };
    vec3 bbmax{ };
    int  szhitboxnameindex{ };
    int  unused[ 8 ]{ };
};

class mstudiohitboxset_t
{
public:
    int sznameindex{ };
    int numhitboxes{ };
    int hitboxindex{ };

public:
    mstudiobbox_t *pHitbox( int i ) const
    {
        return ( mstudiobbox_t * )( ( ( u8 * )this ) + hitboxindex ) + i;
    };
};

struct mstudioevent_t
{
    float cycle{ };
    int   event{ };
    int   type{ };
    char  options[ 64 ]{ };
    int   szeventindex{ };

    const char *pszEventName( ) const
    {
        return reinterpret_cast< const char * >( this ) + this->szeventindex;
    }
};

static_assert( sizeof( mstudioevent_t ) == 80 );

struct mstudioseqdesc_t
{
    int   baseptr{ };
    int   szlabelindex{ };
    int   szactivitynameindex{ };
    int   flags{ };
    int   activity{ };
    int   actweight{ };
    int   numevents{ };
    int   eventindex{ };
    float bbmin[ 3 ]{ };
    float bbmax[ 3 ]{ };
    int   numblends{ };
    int   animindexindex{ };
    int   movementindex{ };
    int   groupsize[ 2 ]{ };
    int   paramindex[ 2 ]{ };
    float paramstart[ 2 ]{ };
    float paramend[ 2 ]{ };
    int   paramparent{ };
    float fadeintime{ };
    float fadeouttime{ };
    u8    m_rest[ 100 ]{ };

    const char *pszLabel( ) const
    {
        return reinterpret_cast< const char * >( this ) + this->szlabelindex;
    }

    const char *pszActivityName( ) const
    {
        return reinterpret_cast< const char * >( this ) + this->szactivitynameindex;
    }

    const mstudioevent_t *pEvent( int i ) const
    {
        return reinterpret_cast< const mstudioevent_t * >(
                   reinterpret_cast< const u8 * >( this ) + this->eventindex ) +
               i;
    }
};

static_assert( sizeof( mstudioseqdesc_t ) == 212 );

struct mstudioposeparamdesc_t
{
    int   sznameindex{ };
    int   flags{ };
    float start{ };
    float end{ };
    float loop{ };

    const char *pszName( ) const
    {
        return reinterpret_cast< const char * >( this ) + this->sznameindex;
    }
};

static_assert( sizeof( mstudioposeparamdesc_t ) == 20 );

enum EDamageType : i32
{
    DMG_GENERIC               = 0,
    DMG_CRUSH                 = ( 1 << 0 ),
    DMG_BULLET                = ( 1 << 1 ),
    DMG_SLASH                 = ( 1 << 2 ),
    DMG_BURN                  = ( 1 << 3 ),
    DMG_VEHICLE               = ( 1 << 4 ),
    DMG_FALL                  = ( 1 << 5 ),
    DMG_BLAST                 = ( 1 << 6 ),
    DMG_CLUB                  = ( 1 << 7 ),
    DMG_SHOCK                 = ( 1 << 8 ),
    DMG_SONIC                 = ( 1 << 9 ),
    DMG_ENERGYBEAM            = ( 1 << 10 ),
    DMG_PREVENT_PHYSICS_FORCE = ( 1 << 11 ),
    DMG_NEVERGIB              = ( 1 << 12 ),
    DMG_ALWAYSGIB             = ( 1 << 13 ),
    DMG_DROWN                 = ( 1 << 14 ),
    DMG_PARALYZE              = ( 1 << 15 ),
    DMG_NERVEGAS              = ( 1 << 16 ),
    DMG_POISON                = ( 1 << 17 ),
    DMG_RADIATION             = ( 1 << 18 ),
    DMG_DROWNRECOVER          = ( 1 << 19 ),
    DMG_ACID                  = ( 1 << 20 ),
    DMG_SLOWBURN              = ( 1 << 21 ),
    DMG_REMOVENORAGDOLL       = ( 1 << 22 ),
    DMG_PHYSGUN               = ( 1 << 23 ),
    DMG_PLASMA                = ( 1 << 24 ),
    DMG_AIRBOAT               = ( 1 << 25 ),
    DMG_DISSOLVE              = ( 1 << 26 ),
    DMG_BLAST_SURFACE         = ( 1 << 27 ),
    DMG_DIRECT                = ( 1 << 28 ),
    DMG_BUCKSHOT              = ( 1 << 29 ),

    DMG_USE_HITLOCATIONS                    = DMG_AIRBOAT,
    DMG_HALF_FALLOFF                        = DMG_RADIATION,
    DMG_CRITICAL                            = DMG_ACID,
    DMG_RADIUS_MAX                          = DMG_ENERGYBEAM,
    DMG_IGNITE                              = DMG_PLASMA,
    DMG_USEDISTANCEMOD                      = DMG_SLOWBURN,
    DMG_NOCLOSEDISTANCEMOD                  = DMG_POISON,
    DMG_FROM_OTHER_SAPPER                   = DMG_IGNITE,
    DMG_MELEE                               = DMG_BLAST_SURFACE,
    DMG_DONT_COUNT_DAMAGE_TOWARDS_CRIT_RATE = DMG_DISSOLVE,
    DMG_IGNORE_MAXHEALTH                    = DMG_BULLET,
    DMG_IGNORE_DEBUFFS                      = DMG_SLASH,
};

enum ETFDmgCustom : i32
{
    TF_DMG_CUSTOM_NONE = 0,
    TF_DMG_CUSTOM_HEADSHOT,
    TF_DMG_CUSTOM_BACKSTAB,
    TF_DMG_CUSTOM_BURNING,
    TF_DMG_WRENCH_FIX,
    TF_DMG_CUSTOM_MINIGUN,
    TF_DMG_CUSTOM_SUICIDE,
    TF_DMG_CUSTOM_TAUNTATK_HADOUKEN,
    TF_DMG_CUSTOM_BURNING_FLARE,
    TF_DMG_CUSTOM_TAUNTATK_HIGH_NOON,
    TF_DMG_CUSTOM_TAUNTATK_GRAND_SLAM,
    TF_DMG_CUSTOM_PENETRATE_MY_TEAM,
    TF_DMG_CUSTOM_PENETRATE_ALL_PLAYERS,
    TF_DMG_CUSTOM_TAUNTATK_FENCING,
    TF_DMG_CUSTOM_PENETRATE_NONBURNING_TEAMMATE,
    TF_DMG_CUSTOM_TAUNTATK_ARROW_STAB,
    TF_DMG_CUSTOM_TELEFRAG,
    TF_DMG_CUSTOM_BURNING_ARROW,
    TF_DMG_CUSTOM_FLYINGBURN,
    TF_DMG_CUSTOM_PUMPKIN_BOMB,
    TF_DMG_CUSTOM_DECAPITATION,
    TF_DMG_CUSTOM_TAUNTATK_GRENADE,
    TF_DMG_CUSTOM_BASEBALL,
    TF_DMG_CUSTOM_CHARGE_IMPACT,
    TF_DMG_CUSTOM_TAUNTATK_BARBARIAN_SWING,
    TF_DMG_CUSTOM_AIR_STICKY_BURST,
    TF_DMG_CUSTOM_DEFENSIVE_STICKY,
    TF_DMG_CUSTOM_PICKAXE,
    TF_DMG_CUSTOM_ROCKET_DIRECTHIT,
    TF_DMG_CUSTOM_TAUNTATK_UBERSLICE,
    TF_DMG_CUSTOM_PLAYER_SENTRY,
    TF_DMG_CUSTOM_STANDARD_STICKY,
    TF_DMG_CUSTOM_SHOTGUN_REVENGE_CRIT,
    TF_DMG_CUSTOM_TAUNTATK_ENGINEER_GUITAR_SMASH,
    TF_DMG_CUSTOM_BLEEDING,
    TF_DMG_CUSTOM_GOLD_WRENCH,
    TF_DMG_CUSTOM_CARRIED_BUILDING,
    TF_DMG_CUSTOM_COMBO_PUNCH,
    TF_DMG_CUSTOM_TAUNTATK_ENGINEER_ARM_KILL,
    TF_DMG_CUSTOM_FISH_KILL,
    TF_DMG_CUSTOM_TRIGGER_HURT,
    TF_DMG_CUSTOM_DECAPITATION_BOSS,
    TF_DMG_CUSTOM_STICKBOMB_EXPLOSION,
    TF_DMG_CUSTOM_AEGIS_ROUND,
    TF_DMG_CUSTOM_FLARE_EXPLOSION,
    TF_DMG_CUSTOM_BOOTS_STOMP,
    TF_DMG_CUSTOM_PLASMA,
    TF_DMG_CUSTOM_PLASMA_CHARGED,
    TF_DMG_CUSTOM_PLASMA_GIB,
    TF_DMG_CUSTOM_PRACTICE_STICKY,
    TF_DMG_CUSTOM_EYEBALL_ROCKET,
    TF_DMG_CUSTOM_HEADSHOT_DECAPITATION,
    TF_DMG_CUSTOM_TAUNTATK_ARMAGEDDON,
    TF_DMG_CUSTOM_FLARE_PELLET,
    TF_DMG_CUSTOM_CLEAVER,
    TF_DMG_CUSTOM_CLEAVER_CRIT,
    TF_DMG_CUSTOM_SAPPER_RECORDER_DEATH,
    TF_DMG_CUSTOM_MERASMUS_PLAYER_BOMB,
    TF_DMG_CUSTOM_MERASMUS_GRENADE,
    TF_DMG_CUSTOM_MERASMUS_ZAP,
    TF_DMG_CUSTOM_MERASMUS_DECAPITATION,
    TF_DMG_CUSTOM_CANNONBALL_PUSH,
    TF_DMG_CUSTOM_TAUNTATK_ALLCLASS_GUITAR_RIFF,
    TF_DMG_CUSTOM_THROWABLE,
    TF_DMG_CUSTOM_THROWABLE_KILL,
    TF_DMG_CUSTOM_SPELL_TELEPORT,
    TF_DMG_CUSTOM_SPELL_SKELETON,
    TF_DMG_CUSTOM_SPELL_MIRV,
    TF_DMG_CUSTOM_SPELL_METEOR,
    TF_DMG_CUSTOM_SPELL_LIGHTNING,
    TF_DMG_CUSTOM_SPELL_FIREBALL,
    TF_DMG_CUSTOM_SPELL_MONOCULUS,
    TF_DMG_CUSTOM_SPELL_BLASTJUMP,
    TF_DMG_CUSTOM_SPELL_BATS,
    TF_DMG_CUSTOM_SPELL_TINY,
    TF_DMG_CUSTOM_KART,
    TF_DMG_CUSTOM_GIANT_HAMMER,
    TF_DMG_CUSTOM_RUNE_REFLECT,
    TF_DMG_CUSTOM_DRAGONS_FURY_IGNITE,
    TF_DMG_CUSTOM_DRAGONS_FURY_BONUS_BURNING,
    TF_DMG_CUSTOM_SLAP_KILL,
    TF_DMG_CUSTOM_CROC,
    TF_DMG_CUSTOM_TAUNTATK_GASBLAST,
    TF_DMG_CUSTOM_AXTINGUISHER_BOOSTED,
    TF_DMG_CUSTOM_KRAMPUS_MELEE,
    TF_DMG_CUSTOM_KRAMPUS_RANGED,
    TF_DMG_CUSTOM_TAUNTATK_TRICKSHOT,

    TF_DMG_CUSTOM_END
};

struct mstudiomesh_t
{
    i32 material;
    i32 modelindex;
    i32 numvertices;
    i32 vertexoffset;
    i32 numflexes;
    i32 flexindex;
    i32 materialtype;
    i32 materialparam;
    i32 meshid;
    u8  pad24[ 80 ];
};

static_assert( sizeof( mstudiomesh_t ) == 116 );

struct mstudiomodel_t
{
    char  name[ 64 ];
    i32   type;
    float boundingradius;
    i32   nummeshes;
    i32   meshindex;
    u8    pad50[ 68 ];

    const mstudiomesh_t *pMesh( i32 index ) const
    {
        return reinterpret_cast< const mstudiomesh_t * >(
                   reinterpret_cast< const u8 * >( this ) + this->meshindex ) +
               index;
    }
};

static_assert( sizeof( mstudiomodel_t ) == 148 );
static_assert( __builtin_offsetof( mstudiomodel_t, nummeshes ) == 72 );

struct mstudiobodyparts_t
{
    i32 sznameindex;
    i32 nummodels;
    i32 base;
    i32 modelindex;

    const mstudiomodel_t *pModel( i32 index ) const
    {
        return reinterpret_cast< const mstudiomodel_t * >(
                   reinterpret_cast< const u8 * >( this ) + this->modelindex ) +
               index;
    }
};

struct studiohdr_t
{
    int   id{ };
    int   version{ };
    int   checksum{ };
    char  name[ 64 ]{ };
    int   length{ };
    vec3  eyeposition{ };
    vec3  illumposition{ };
    vec3  hull_min{ };
    vec3  hull_max{ };
    vec3  view_bbmin{ };
    vec3  view_bbmax{ };
    int   flags{ };
    int   numbones{ };
    int   boneindex{ };
    int   numbonecontrollers{ };
    int   bonecontrollerindex{ };
    int   numhitboxsets{ };
    int   hitboxsetindex{ };
    int   numlocalanim{ };
    int   localanimindex{ };
    int   numlocalseq{ };
    int   localseqindex{ };
    int   activitylistversion{ };
    int   eventsindexed{ };
    int   numtextures{ };
    int   textureindex{ };
    int   numcdtextures{ };
    int   cdtextureindex{ };
    int   numskinref{ };
    int   numskinfamilies{ };
    int   skinindex{ };
    int   numbodyparts{ };
    int   bodypartindex{ };
    int   numlocalattachments{ };
    int   localattachmentindex{ };
    int   numlocalnodes{ };
    int   localnodeindex{ };
    int   localnodenameindex{ };
    int   numflexdesc{ };
    int   flexdescindex{ };
    int   numflexcontrollers{ };
    int   flexcontrollerindex{ };
    int   numflexrules{ };
    int   flexruleindex{ };
    int   numikchains{ };
    int   ikchainindex{ };
    int   nummouths{ };
    int   mouthindex{ };
    int   numlocalposeparameters{ };
    int   localposeparamindex{ };
    int   surfacepropindex{ };
    int   keyvalueindex{ };
    int   keyvaluesize{ };
    int   numlocalikautoplaylocks{ };
    int   localikautoplaylockindex{ };
    float mass{ };
    int   contents{ };
    int   numincludemodels{ };
    int   includemodelindex{ };
    void *virtualModel{ };
    int   szanimblocknameindex{ };
    int   numanimblocks{ };
    int   animblockindex{ };
    void *animblockModel{ };
    int   bonetablebynameindex{ };
    void *pVertexBase{ };
    void *pIndexBase{ };
    u8    constdirectionallightdot{ };
    u8    rootLOD{ };
    u8    numAllowedRootLODs{ };
    u8    unused[ 1 ]{ };
    int   unused4{ };
    int   numflexcontrollerui{ };
    int   flexcontrolleruiindex{ };
    float flVertAnimFixedPointScale{ };
    int   unused3[ 1 ]{ };
    int   studiohdr2index{ };
    int   unused2[ 1 ]{ };

    const mstudiobone_t *pBone( int index ) const
    {
        return reinterpret_cast< const mstudiobone_t * >(
                   reinterpret_cast< const char * >( this ) + this->boneindex ) +
               index;
    }

    mstudiohitboxset_t *pHitboxSet( int i ) const
    {
        return ( mstudiohitboxset_t * )( ( ( u8 * )this ) + hitboxsetindex ) + i;
    };

    const mstudioposeparamdesc_t *pPoseParameter( int i ) const
    {
        return reinterpret_cast< const mstudioposeparamdesc_t * >(
                   reinterpret_cast< const char * >( this ) + this->localposeparamindex ) +
               i;
    }

    const mstudiobodyparts_t *pBodypart( int index ) const
    {
        return reinterpret_cast< const mstudiobodyparts_t * >(
                   reinterpret_cast< const char * >( this ) + this->bodypartindex ) +
               index;
    }
};

static_assert( __builtin_offsetof( studiohdr_t, numbones ) == 156 );
static_assert( __builtin_offsetof( studiohdr_t, boneindex ) == 160 );
static_assert( __builtin_offsetof( studiohdr_t, numbonecontrollers ) == 164 );
static_assert( __builtin_offsetof( studiohdr_t, numbodyparts ) == 232 );

struct CStudioHdr
{
    studiohdr_t *m_pStudioHdr;
};

struct CVertexBufferDx8
{
    void *m_vertex_buffer;
    u8    pad08[ 16 ];
    i32   m_vertex_count;
    i32   m_vertex_size;
};

static_assert( __builtin_offsetof( CVertexBufferDx8, m_vertex_size ) == 0x1C );

struct CIndexBufferDx8
{
    void *m_index_buffer;
};

struct CMeshDX8
{
    u8                pad00[ 24 ];
    u64               m_vertex_format;
    u8                pad20[ 8 ];
    CVertexBufferDx8 *m_vertex_buffer;
    CIndexBufferDx8  *m_index_buffer;

    void HandleLateCreation( );
};

static_assert( __builtin_offsetof( CMeshDX8, m_vertex_format ) == 0x18 );
static_assert( __builtin_offsetof( CMeshDX8, m_index_buffer ) == 0x30 );

inline constexpr u64 VERTEX_NORMAL{ 0x2 };
inline constexpr u64 VERTEX_BONE_INDEX{ 0x80 };
inline constexpr u64 VERTEX_FORMAT_COMPRESSED{ 0x400 };
inline constexpr u64 VERTEX_BONE_WEIGHT_MASK{ 0x7ull << 11 };

inline constexpr i32 MESHGROUP_IS_HWSKINNED{ 0x2 };
inline constexpr u8  STRIP_IS_TRISTRIP{ 0x2 };

inline constexpr i32 MAX_HARDWARE_BONES{ 53 };

struct BoneStateChangeHeader_t
{
    i32 hardwareID;
    i32 newBoneID;
};

#pragma pack( push, 1 )
struct StripHeader_t
{
    i32 numIndices;
    i32 indexOffset;
    i32 numVerts;
    i32 vertOffset;
    i16 numBones;
    u8  flags;
    i32 numBoneStateChanges;
    i32 boneStateChangeOffset;

    const BoneStateChangeHeader_t *pBoneStateChange( i32 index ) const
    {
        return reinterpret_cast< const BoneStateChangeHeader_t * >(
                   reinterpret_cast< const u8 * >( this ) + this->boneStateChangeOffset ) +
               index;
    }
};
#pragma pack( pop )

static_assert( sizeof( StripHeader_t ) == 27 );

struct studiomeshgroup_t
{
    CMeshDX8      *m_pMesh;
    i32            m_NumStrips;
    i32            m_Flags;
    StripHeader_t *m_pStripData;
    u16           *m_pGroupIndexToMeshIndex;
    i32            m_NumVertices;
    i32           *m_pUniqueTris;
    u16           *m_pIndices;
    bool           m_MeshNeedsRestore;
    i16            m_ColorMeshID;
    void          *m_pMorph;
};

static_assert( sizeof( studiomeshgroup_t ) == 72 );
static_assert( __builtin_offsetof( studiomeshgroup_t, m_MeshNeedsRestore ) == 56 );

struct studiomeshdata_t
{
    i32                m_NumGroup;
    studiomeshgroup_t *m_pMeshGroup;
};

struct studioloddata_t
{
    studiomeshdata_t *m_pMeshData;
    float             m_SwitchPoint;
    i32               numMaterials;
    IMaterial       **ppMaterials;
    i32              *pMaterialFlags;
    i32              *m_pHWMorphDecalBoneRemap;
    i32               m_nDecalBoneCount;
};

static_assert( sizeof( studioloddata_t ) == 48 );

struct studiohwdata_t
{
    i32              m_RootLOD;
    i32              m_NumLODs;
    studioloddata_t *m_pLODs;
    i32              m_NumStudioMeshes;
};

struct DrawModelResults_t;
struct StudioRenderContext_t;

struct FlexWeights_t
{
    float *m_pFlexWeights;
    float *m_pFlexDelayedWeights;
};

struct DrawModelInfo_t
{
    studiohdr_t    *m_pStudioHdr;
    studiohwdata_t *m_pHardwareData;
    void           *m_Decals;
    int             m_Skin;
    int             m_Body;
    int             m_HitboxSet;
    void           *m_pClientEntity;
    int             m_Lod;
};

inline constexpr i32 STUDIORENDER_DRAW_TRANSLUCENT_ONLY{ 0x02 };
inline constexpr i32 STUDIORENDER_DRAW_GROUP_MASK{ 0x03 };
inline constexpr i32 STUDIORENDER_SHADOWDEPTHTEXTURE{ 0x200 };
inline constexpr i32 STUDIORENDER_SSAODEPTHTEXTURE{ 0x1000 };
inline constexpr i32 STUDIO_RENDER{ 0x1 };
inline constexpr i32 STUDIO_NOSHADOWS{ 0x80 };
inline constexpr i32 BONE_USED_BY_ANYTHING{ 0x0007FF00 };

class CStudioRender
{
public:
    void SetColorModulation( const float *color );
    void SetAlphaModulation( const float alpha );
    void SetLocalLights( i32 count, const void *lights );
    void SetAmbientLightColors( const vec3 *colors );
    void ForcedMaterialOverride( IMaterial *newMaterial, OverrideType_t nOverrideType = OVERRIDE_NORMAL );
};

struct model_t;
struct vcollide_t;

enum modtype_t
{
    mod_bad = 0,
    mod_brush,
    mod_sprite,
    mod_studio
};

class CPhysCollide;

inline constexpr i32 VIRTUAL_TERRAIN_COUNT{ 0x8000 };

using MDLHandle_t = u16;

inline constexpr MDLHandle_t MDLHANDLE_INVALID{ 0xFFFF };

class CModelInfoClient
{
public:
    const char    *GetModelName( const model_t *model );
    studiohdr_t   *GetStudiomodel( const model_t *model );
    MDLHandle_t    GetCacheHandle( const model_t *model );
    i32            GetModelIndex( const char *name );
    vcollide_t    *GetVCollide( i32 model_index );
    vcollide_t    *GetVCollide( const model_t *model );
    i32            GetModelType( const model_t *model );
    CPhysCollide  *GetCollideForVirtualTerrain( i32 index );
    const model_t *GetModel( i32 model_index );
    void           GetModelBounds( const model_t *model, vec3 &mins, vec3 &maxs );
};

class IMDLCache
{
public:
    MDLHandle_t     FindMDL( const char *path );
    studiohdr_t    *GetStudioHdr( MDLHandle_t handle );
    studiohwdata_t *GetHardwareData( MDLHandle_t handle );
    void            BeginLock( );
    void            EndLock( );
};

// edict->solid values
// NOTE: Some movetypes will cause collisions independent of SOLID_NOT/SOLID_TRIGGER when the entity moves
// SOLID only effects OTHER entities colliding with this one when they move - UGH!

// Solid type basically describes how the bounding volume of the object is represented
// NOTE: SOLID_BBOX MUST BE 2, and SOLID_VPHYSICS MUST BE 6
// NOTE: These numerical values are used in the FGD by the prop code (see prop_dynamic)
enum SolidType_t
{
    SOLID_NONE     = 0, // no solid model
    SOLID_BSP      = 1, // a BSP tree
    SOLID_BBOX     = 2, // an AABB
    SOLID_OBB      = 3, // an OBB (not implemented yet)
    SOLID_OBB_YAW  = 4, // an OBB, constrained so that it can only yaw
    SOLID_CUSTOM   = 5, // Always call into the entity for tests
    SOLID_VPHYSICS = 6, // solid vphysics object, get vcollide from the model and collide with that
    SOLID_LAST,
};

enum SolidFlags_t
{
    FSOLID_CUSTOMRAYTEST        = 0x0001, // Ignore solid type + always call into the entity for ray tests
    FSOLID_CUSTOMBOXTEST        = 0x0002, // Ignore solid type + always call into the entity for swept box tests
    FSOLID_NOT_SOLID            = 0x0004, // Are we currently not solid?
    FSOLID_TRIGGER              = 0x0008, // This is something may be collideable but fires touch functions
                                          // even when it's not collideable (when the FSOLID_NOT_SOLID flag is set)
    FSOLID_NOT_STANDABLE        = 0x0010, // You can't stand on this
    FSOLID_VOLUME_CONTENTS      = 0x0020, // Contains volumetric contents (like water)
    FSOLID_FORCE_WORLD_ALIGNED  = 0x0040, // Forces the collision rep to be world-aligned even if it's SOLID_BSP or SOLID_VPHYSICS
    FSOLID_USE_TRIGGER_BOUNDS   = 0x0080, // Uses a special trigger bounds separate from the normal OBB
    FSOLID_ROOT_PARENT_ALIGNED  = 0x0100, // Collisions are defined in root parent's local coordinate space
    FSOLID_TRIGGER_TOUCH_DEBRIS = 0x0200, // This trigger will touch debris objects

    FSOLID_MAX_BITS = 10
};

//-----------------------------------------------------------------------------
// Interface for static props
//-----------------------------------------------------------------------------
template< typename T >
struct CUtlVector;

class IPhysicsEnvironment;
class IVPhysicsKeyHandler;

class IStaticPropMgr
{
public:
    void CreateVPhysicsRepresentations( IPhysicsEnvironment *environment, IVPhysicsKeyHandler *defaults, void *game_data );
    bool IsStaticProp( IHandleEntity *pHandleEntity ) const;
    bool IsStaticProp( CBaseHandle handle ) const;
    void GetAllStaticPropsInAABB( const vec3 &mins, const vec3 &maxs, CUtlVector< ICollideable * > *out );
};

constexpr auto CONTENTS_EMPTY = 0; // No contents

constexpr auto CONTENTS_SOLID        = 0x1; // an eye is never valid in a solid
constexpr auto CONTENTS_WINDOW       = 0x2; // translucent, but not watery (glass)
constexpr auto CONTENTS_AUX          = 0x4;
constexpr auto CONTENTS_GRATE        = 0x8; // alpha-tested "grate" textures.  Bullets/sight pass through, but solids don't
constexpr auto CONTENTS_SLIME        = 0x10;
constexpr auto CONTENTS_WATER        = 0x20;
constexpr auto CONTENTS_BLOCKLOS     = 0x40; // block AI line of sight
constexpr auto CONTENTS_OPAQUE       = 0x80; // things that cannot be seen through (may be non-solid though)
constexpr auto LAST_VISIBLE_CONTENTS = 0x80;

constexpr auto ALL_VISIBLE_CONTENTS = ( LAST_VISIBLE_CONTENTS | ( LAST_VISIBLE_CONTENTS - 1 ) );

constexpr auto CONTENTS_TESTFOGVOLUME = 0x100;
constexpr auto CONTENTS_UNUSED        = 0x200;

// unused
// NOTE: If it's visible, grab from the top + update LAST_VISIBLE_CONTENTS
// if not visible, then grab from the bottom.
constexpr auto CONTENTS_UNUSED6 = 0x400;

constexpr auto CONTENTS_TEAM1 = 0x800;  // per team contents used to differentiate collisions
constexpr auto CONTENTS_TEAM2 = 0x1000; // between players and objects on different teams

// ignore CONTENTS_OPAQUE on surfaces that have SURF_NODRAW
constexpr auto CONTENTS_IGNORE_NODRAW_OPAQUE = 0x2000;

// hits entities which are MOVETYPE_PUSH (doors, plats, etc.)
constexpr auto CONTENTS_MOVEABLE = 0x4000;

// remaining contents are non-visible, and don't eat brushes
constexpr auto CONTENTS_AREAPORTAL = 0x8000;

constexpr auto CONTENTS_PLAYERCLIP  = 0x10000;
constexpr auto CONTENTS_MONSTERCLIP = 0x20000;

// currents can be added to any other contents, and may be mixed
constexpr auto CONTENTS_CURRENT_0    = 0x40000;
constexpr auto CONTENTS_CURRENT_90   = 0x80000;
constexpr auto CONTENTS_CURRENT_180  = 0x100000;
constexpr auto CONTENTS_CURRENT_270  = 0x200000;
constexpr auto CONTENTS_CURRENT_UP   = 0x400000;
constexpr auto CONTENTS_CURRENT_DOWN = 0x800000;

constexpr auto CONTENTS_ORIGIN = 0x1000000; // removed before bsping an entity

constexpr auto CONTENTS_MONSTER     = 0x2000000; // should never be on a brush, only in game
constexpr auto CONTENTS_DEBRIS      = 0x4000000;
constexpr auto CONTENTS_DETAIL      = 0x8000000;  // brushes to be added after vis leafs
constexpr auto CONTENTS_TRANSLUCENT = 0x10000000; // auto set if any surface has trans
constexpr auto CONTENTS_LADDER      = 0x20000000;
constexpr auto CONTENTS_HITBOX      = 0x40000000; // use accurate hitboxes on trace

// NOTE: These are stored in a short in the engine now.  Don't use more than 16 bits
constexpr auto SURF_LIGHT    = 0x0001; // value will hold the light strength
constexpr auto SURF_SKY2D    = 0x0002; // don't draw, indicates we should skylight + draw 2d sky but not draw the 3D skybox
constexpr auto SURF_SKY      = 0x0004; // don't draw, but add to skybox
constexpr auto SURF_WARP     = 0x0008; // turbulent water warp
constexpr auto SURF_TRANS    = 0x0010;
constexpr auto SURF_NOPORTAL = 0x0020; // the surface can not have a portal placed on it
constexpr auto SURF_TRIGGER  = 0x0040; // FIXME: This is an xbox hack to work around elimination of trigger surfaces, which breaks occluders
constexpr auto SURF_NODRAW   = 0x0080; // don't bother referencing the texture

constexpr auto SURF_HINT = 0x0100; // make a primary bsp splitter

constexpr auto SURF_SKIP      = 0x0200; // completely ignore, allowing non-closed brushes
constexpr auto SURF_NOLIGHT   = 0x0400; // Don't calculate light
constexpr auto SURF_BUMPLIGHT = 0x0800; // calculate three lightmaps for the surface for bumpmapping
constexpr auto SURF_NOSHADOWS = 0x1000; // Don't receive shadows
constexpr auto SURF_NODECALS  = 0x2000; // Don't receive decals
constexpr auto SURF_NOCHOP    = 0x4000; // Don't subdivide patches on this surface
constexpr auto SURF_HITBOX    = 0x8000; // surface is part of a hitbox

// -----------------------------------------------------
// spatial content masks - used for spatial queries (traceline,etc.)
// -----------------------------------------------------
constexpr auto MASK_ALL                   = ( 0xFFFFFFFF );
// everything that is normally solid
constexpr auto MASK_SOLID                 = ( CONTENTS_SOLID | CONTENTS_MOVEABLE | CONTENTS_WINDOW | CONTENTS_MONSTER | CONTENTS_GRATE );
// everything that blocks player movement
constexpr auto MASK_PLAYERSOLID           = ( CONTENTS_SOLID | CONTENTS_MOVEABLE | CONTENTS_PLAYERCLIP | CONTENTS_WINDOW | CONTENTS_MONSTER | CONTENTS_GRATE );
// blocks npc movement
constexpr auto MASK_NPCSOLID              = ( CONTENTS_SOLID | CONTENTS_MOVEABLE | CONTENTS_MONSTERCLIP | CONTENTS_WINDOW | CONTENTS_MONSTER | CONTENTS_GRATE );
// water physics in these contents
constexpr auto MASK_WATER                 = ( CONTENTS_WATER | CONTENTS_MOVEABLE | CONTENTS_SLIME );
// everything that blocks lighting
constexpr auto MASK_OPAQUE                = ( CONTENTS_SOLID | CONTENTS_MOVEABLE | CONTENTS_OPAQUE );
// everything that blocks lighting, but with monsters added.
constexpr auto MASK_OPAQUE_AND_NPCS       = ( MASK_OPAQUE | CONTENTS_MONSTER );
// everything that blocks line of sight for AI
constexpr auto MASK_BLOCKLOS              = ( CONTENTS_SOLID | CONTENTS_MOVEABLE | CONTENTS_BLOCKLOS );
// everything that blocks line of sight for AI plus NPCs
constexpr auto MASK_BLOCKLOS_AND_NPCS     = ( MASK_BLOCKLOS | CONTENTS_MONSTER );
// everything that blocks line of sight for players
constexpr auto MASK_VISIBLE               = ( MASK_OPAQUE | CONTENTS_IGNORE_NODRAW_OPAQUE );
// everything that blocks line of sight for players, but with monsters added.
constexpr auto MASK_VISIBLE_AND_NPCS      = ( MASK_OPAQUE_AND_NPCS | CONTENTS_IGNORE_NODRAW_OPAQUE );
// bullets see these as solid
constexpr auto MASK_SHOT                  = ( CONTENTS_SOLID | CONTENTS_MOVEABLE | CONTENTS_MONSTER | CONTENTS_WINDOW | CONTENTS_DEBRIS | CONTENTS_HITBOX );
// non-raycasted weapons see this as solid (includes grates)
constexpr auto MASK_SHOT_HULL             = ( CONTENTS_SOLID | CONTENTS_MOVEABLE | CONTENTS_MONSTER | CONTENTS_WINDOW | CONTENTS_DEBRIS | CONTENTS_GRATE );
// hits solids (not grates) and passes through everything else
constexpr auto MASK_SHOT_PORTAL           = ( CONTENTS_SOLID | CONTENTS_MOVEABLE | CONTENTS_WINDOW | CONTENTS_MONSTER );
// everything normally solid, except monsters (world+brush only)
constexpr auto MASK_SOLID_BRUSHONLY       = ( CONTENTS_SOLID | CONTENTS_MOVEABLE | CONTENTS_WINDOW | CONTENTS_GRATE );
// everything normally solid for player movement, except monsters (world+brush only)
constexpr auto MASK_PLAYERSOLID_BRUSHONLY = ( CONTENTS_SOLID | CONTENTS_MOVEABLE | CONTENTS_WINDOW | CONTENTS_PLAYERCLIP | CONTENTS_GRATE );
// everything normally solid for npc movement, except monsters (world+brush only)
constexpr auto MASK_NPCSOLID_BRUSHONLY    = ( CONTENTS_SOLID | CONTENTS_MOVEABLE | CONTENTS_WINDOW | CONTENTS_MONSTERCLIP | CONTENTS_GRATE );
// just the world, used for route rebuilding
constexpr auto MASK_NPCWORLDSTATIC        = ( CONTENTS_SOLID | CONTENTS_WINDOW | CONTENTS_MONSTERCLIP | CONTENTS_GRATE );
// These are things that can split areaportals
constexpr auto MASK_SPLITAREAPORTAL       = ( CONTENTS_WATER | CONTENTS_SLIME );

// UNDONE: This is untested, any moving water
constexpr auto MASK_CURRENT = ( CONTENTS_CURRENT_0 | CONTENTS_CURRENT_90 | CONTENTS_CURRENT_180 | CONTENTS_CURRENT_270 | CONTENTS_CURRENT_UP | CONTENTS_CURRENT_DOWN );

// everything that blocks corpse movement
// UNDONE: Not used yet / may be deleted
constexpr auto MASK_DEADSOLID = ( CONTENTS_SOLID | CONTENTS_PLAYERCLIP | CONTENTS_WINDOW | CONTENTS_GRATE );

constexpr auto MASK_RADIUS_DAMAGE = MASK_SHOT & ( ~CONTENTS_HITBOX ); // TFGameRules()->RadiusDamage

constexpr f32 TF_GRENADE_JUMP_RADIUS{ 146.0f };
constexpr f32 TF_FLARE_RADIUS_FOR_FJS{ 100.0f };

// this is limited by the network fractional bits used for coords
// because net coords will be only be accurate to 5 bits fractional
// Standard collision test epsilon
// 1/32nd inch collision epsilon
constexpr auto DIST_EPSILON = 0.03125;

enum TraceType_t
{
    TRACE_EVERYTHING = 0,
    TRACE_WORLD_ONLY,              // NOTE: This does *not* test static props!!!
    TRACE_ENTITIES_ONLY,           // NOTE: This version will *not* test static props
    TRACE_EVERYTHING_FILTER_PROPS, // NOTE: This version will pass the IHandleEntity for props through the filter, unlike all other filters
};

enum Collision_Group_t
{
    COLLISION_GROUP_NONE = 0,
    COLLISION_GROUP_DEBRIS,             // Collides with nothing but world and static stuff
    COLLISION_GROUP_DEBRIS_TRIGGER,     // Same as debris, but hits triggers
    COLLISION_GROUP_INTERACTIVE_DEBRIS, // Collides with everything except other interactive debris or debris
    COLLISION_GROUP_INTERACTIVE,        // Collides with everything except interactive debris or debris
    COLLISION_GROUP_PLAYER,
    COLLISION_GROUP_BREAKABLE_GLASS,
    COLLISION_GROUP_VEHICLE,
    COLLISION_GROUP_PLAYER_MOVEMENT, // For HL2, same as Collision_Group_Player, for
                                     // TF2, this filters out other players and CBaseObjects
    COLLISION_GROUP_NPC,             // Generic NPC group
    COLLISION_GROUP_IN_VEHICLE,      // for any entity inside a vehicle
    COLLISION_GROUP_WEAPON,          // for any weapons that need collision detection
    COLLISION_GROUP_VEHICLE_CLIP,    // vehicle clip brush to restrict vehicle movement
    COLLISION_GROUP_PROJECTILE,      // Projectiles!
    COLLISION_GROUP_DOOR_BLOCKER,    // Blocks entities not permitted to get near moving doors
    COLLISION_GROUP_PASSABLE_DOOR,   // Doors that the player shouldn't collide with
    COLLISION_GROUP_DISSOLVING,      // Things that are dissolving are in this group
    COLLISION_GROUP_PUSHAWAY,        // Nonsolid on client and server, pushaway in player code

    COLLISION_GROUP_NPC_ACTOR,    // Used so NPCs in scripts ignore the player.
    COLLISION_GROUP_NPC_SCRIPTED, // USed for NPCs in scripts that should not collide with each other

    LAST_SHARED_COLLISION_GROUP
};

enum
{
    TF_COLLISIONGROUP_GRENADES = LAST_SHARED_COLLISION_GROUP,
    TFCOLLISION_GROUP_OBJECT,
    TFCOLLISION_GROUP_OBJECT_SOLIDTOPLAYERMOVEMENT,
    TFCOLLISION_GROUP_COMBATOBJECT,
    TFCOLLISION_GROUP_ROCKETS,
    TFCOLLISION_GROUP_RESPAWNROOMS,
    TFCOLLISION_GROUP_TANK,
    TFCOLLISION_GROUP_ROCKET_BUT_NOT_WITH_OTHER_ROCKETS,
};

class IHandleEntity;

typedef bool ( *ShouldHitFunc_t )( IHandleEntity *pHandleEntity, int contentsMask );
class ITraceFilter
{
public:
    virtual bool        ShouldHitEntity( IHandleEntity *pEntity, int contentsMask ) = 0;
    virtual TraceType_t GetTraceType( ) const                                       = 0;
};

//-----------------------------------------------------------------------------
// Classes are expected to inherit these + implement the ShouldHitEntity method
//-----------------------------------------------------------------------------

// This is the one most normal traces will inherit from
class CTraceFilter : public ITraceFilter
{
public:
    virtual TraceType_t GetTraceType( ) const
    {
        return TRACE_EVERYTHING;
    }
};

//-----------------------------------------------------------------------------
// traceline methods
//-----------------------------------------------------------------------------
class CTraceFilterSimple : public CTraceFilter
{
public:
    CTraceFilterSimple( const IHandleEntity *passentity, int collisionGroup, ShouldHitFunc_t pExtraShouldHitCheckFn = nullptr );
    virtual bool ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask );
    virtual void SetPassEntity( const IHandleEntity *pPassEntity );
    virtual void SetCollisionGroup( int iCollisionGroup );

    const IHandleEntity *GetPassEntity( void );

private:
    const IHandleEntity *m_pPassEnt;
    int                  m_collisionGroup;
    ShouldHitFunc_t      m_pExtraShouldHitCheckFunction;
};

class CTraceFilterEntitiesOnly : public ITraceFilter
{
public:
    virtual TraceType_t GetTraceType( ) const
    {
        return TRACE_ENTITIES_ONLY;
    }
};

// Now make sure there isn't something other than team players in the way.
class CMedigunFilter : public CTraceFilterSimple
{
public:
    CMedigunFilter( C_BaseEntity *pShooter );
    virtual bool ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask );

    C_BaseEntity *m_pShooter;
};

class CTraceFilterWorldOnly : public ITraceFilter
{
public:
    bool ShouldHitEntity( IHandleEntity *pServerEntity, int contentsMask )
    {
        return false;
    }
    virtual TraceType_t GetTraceType( ) const
    {
        return TRACE_WORLD_ONLY;
    }
};

class CTraceFilterWorldAndPropsOnly : public ITraceFilter
{
public:
    bool ShouldHitEntity( IHandleEntity *pServerEntity, int contentsMask )
    {
        return false;
    }
    virtual TraceType_t GetTraceType( ) const
    {
        return TRACE_EVERYTHING;
    }
};

class CTraceFilterHitAll : public CTraceFilter
{
public:
    virtual bool ShouldHitEntity( IHandleEntity *pServerEntity, int contentsMask )
    {
        return true;
    }
};

class CTraceFilterProjectileRedirect : public CTraceFilterSimple
{
public:
    CTraceFilterProjectileRedirect( const IHandleEntity *pass, i32 ignore_team ) :
        CTraceFilterSimple( pass, COLLISION_GROUP_NONE ), m_ignore_team{ ignore_team }
    {
    }

    bool ShouldHitEntity( IHandleEntity *pEntity, int contentsMask ) override;

    i32           m_ignore_team{ };
    C_BaseEntity *m_pSkipTarget{ };
};

class CTraceFilterHitscan : public CTraceFilter
{
public:
    virtual bool        ShouldHitEntity( IHandleEntity *pEntity, int contentsMask ) override;
    virtual TraceType_t GetTraceType( ) const override;

    C_BaseEntity *m_pSkip       = nullptr;
    C_BaseEntity *m_pSkipTarget = nullptr; // the player being judged, whose live model is not where the judged pose stands
};

class CTraceFilterProjectile : public CTraceFilter
{
public:
    virtual bool        ShouldHitEntity( IHandleEntity *pEntity, int contentsMask ) override;
    virtual TraceType_t GetTraceType( ) const override;

    C_BaseEntity *m_pSkip = nullptr;
    C_BaseEntity *m_pSkipTarget{ };
    i32           m_skip_team{ };
    bool          m_players_block = false;
    bool          m_entities_only{ }; // the world and static props are left to vphysics
};

class CTraceFilterWorld : public CTraceFilter
{
public:
    virtual bool        ShouldHitEntity( IHandleEntity *pEntity, int contentsMask ) override;
    virtual TraceType_t GetTraceType( ) const override;

    C_BaseEntity *m_pSkip = nullptr;
};

class CPhysCollide;

struct vcollide_t
{
    unsigned short solidCount : 15;
    unsigned short isPacked : 1;
    unsigned short descSize;
    // VPhysicsSolids
    CPhysCollide **solids;
    char          *pKeyValues;
};

struct cplane_t
{
    vec3  normal;
    float dist;
    u8    type;
    u8    signbits;
    u8    pad[ 2 ];
};

struct csurface_t
{
    const char    *name;
    short          surfaceProps;
    unsigned short flags; // BUGBUG: These are declared per surface, not per material, but this database is per-material now
};

struct Ray_t
{
    vec3_aligned m_Start;
    vec3_aligned m_Delta;
    vec3_aligned m_StartOffset;
    vec3_aligned m_Extents;
    bool         m_IsRay;
    bool         m_IsSwept;

    void Init( vec3 const &start, vec3 const &end )
    {
        m_Delta       = end - start;
        m_IsSwept     = m_Delta.length( ) != 0.0f;
        m_Extents     = vec3_aligned( );
        m_IsRay       = true;
        m_StartOffset = vec3_aligned( );
        m_Start       = start;
    }

    void Init( vec3 const &start, vec3 const &end, vec3 const &mins, vec3 const &maxs )
    {
        m_Delta   = end - start;
        m_IsSwept = m_Delta.length( ) != 0.0f;
        m_Extents = maxs - mins;
        m_Extents *= 0.5f;
        m_IsRay       = ( m_Extents.length( ) < 1e-6f );
        m_StartOffset = mins + maxs;
        m_StartOffset *= 0.5f;
        m_Start = start + m_StartOffset;
        m_StartOffset *= -1.0f;
    }

    vec3 InvDelta( ) const
    {
        vec3 vecInvDelta{ };

        for ( i32 iAxis = 0; iAxis < 3; ++iAxis )
        {
            if ( m_Delta[ iAxis ] != 0.0f )
            {
                vecInvDelta[ iAxis ] = 1.0f / m_Delta[ iAxis ];
            }

            else
            {
                vecInvDelta[ iAxis ] = FLT_MAX;
            }
        }

        return vecInvDelta;
    }

private:
};

class CBaseTrace
{
public:
    vec3     startpos;
    vec3     endpos;
    cplane_t plane;
    float    fraction;
    i32      contents;
    u16      dispFlags;
    bool     allsolid;
    bool     startsolid;
};

class CGameTrace : public CBaseTrace
{
public:
    float         fractionleftsolid;
    csurface_t    surface;
    i32           hitgroup;
    i16           physicsbone;
    C_BaseEntity *m_pEnt;
    i32           hitbox;
};

typedef CGameTrace trace_t;

using FileFindHandle_t = i32;
using FileHandle_t     = void *;

inline constexpr i32 PATH_ADD_TO_HEAD{ 0 };

class IFileSystem
{
public:
    void         AddSearchPath( const char *path, const char *path_id, i32 add_type );
    const char  *RelativePathToFullPath( const char *file_name, const char *path_id,
                                         char *destination, i32 capacity );
    void         CreateDirHierarchy( const char *path, const char *path_id );
    const char  *FindFirstEx( const char *wild_card, const char *path_id, FileFindHandle_t *handle );
    const char  *FindNext( FileFindHandle_t handle );
    bool         FindIsDirectory( FileFindHandle_t handle );
    void         FindClose( FileFindHandle_t handle );
    void        *base( );
    FileHandle_t Open( const char *file_name, const char *options, const char *path_id );
    u32          Size( FileHandle_t file );
    i32          Read( void *destination, i32 size, FileHandle_t file );
    void         Close( FileHandle_t file );
};

template< typename T >
struct CUtlMemory
{
    T  *m_pMemory{ };
    i32 m_nAllocationCount{ };
    i32 m_nGrowSize{ };
};

template< typename T >
struct CUtlVector
{
    CUtlMemory< T > m_Memory{ };
    i32             m_Size{ };
    T              *m_pElements{ };

    CUtlVector( )                               = default;
    CUtlVector( const CUtlVector & )            = delete;
    CUtlVector &operator=( const CUtlVector & ) = delete;

    ~CUtlVector( )
    {
        this->Purge( );
    }

    void Purge( );
};

static_assert( sizeof( CUtlVector< i32 > ) == 32 );
static_assert( __builtin_offsetof( CUtlVector< i32 >, m_Size ) == 16 );
static_assert( __builtin_offsetof( CUtlVector< i32 >, m_pElements ) == 24 );

struct brush_plane_t
{
    vec3 m_normal{ };
    f32  m_distance{ };
};

static_assert( sizeof( brush_plane_t ) == 16 );

class IEntityEnumerator
{
public:
    virtual bool EnumEntity( IHandleEntity *entity ) = 0;
};

class ICollideable
{
public:
    const model_t   *GetCollisionModel( );
    const vec3      &GetCollisionOrigin( );
    const vec3      &GetCollisionAngles( );
    const matrix3x4 &CollisionToWorldTransform( );
    i32              GetSolid( );
    i32              GetSolidFlags( );
};

class CEngineTrace
{
public:
    void          TraceRay( const Ray_t &ray, u32 fMask, ITraceFilter *pTraceFilter, trace_t *pTrace );
    void          TraceHull( );
    void          GetBrushesInAABB( const vec3 &mins, const vec3 &maxs, CUtlVector< i32 > *brushes, i32 mask );
    bool          GetBrushInfo( i32 brush, CUtlVector< brush_plane_t > *planes, i32 *contents );
    CPhysCollide *GetCollidableFromDisplacementsInAABB( const vec3 &mins, const vec3 &maxs );
    void          EnumerateEntities( const vec3 &mins, const vec3 &maxs, IEntityEnumerator *enumerator );
    ICollideable *GetCollideable( IHandleEntity *entity );
    void          ClipRayToEntity( const Ray_t &ray, u32 mask, IHandleEntity *entity, trace_t *trace );
};

enum StencilOperation_t
{
    STENCILOPERATION_KEEP    = 1,
    STENCILOPERATION_REPLACE = 3,
};

enum StencilComparisonFunction_t
{
    STENCILCOMPARISONFUNCTION_EQUAL  = 3,
    STENCILCOMPARISONFUNCTION_ALWAYS = 8,
};

enum MaterialCullMode_t
{
    MATERIAL_CULLMODE_CCW = 0,
    MATERIAL_CULLMODE_CW  = 1,
};

class ITexture
{
public:
    void IncrementReferenceCount( );
    void DecrementReferenceCount( );
    void DeleteIfUnreferenced( );
};

enum RenderTargetSizeMode_t
{
    RT_SIZE_LITERAL = 8,
};

enum ImageFormat
{
    IMAGE_FORMAT_RGBA8888 = 0,
    IMAGE_FORMAT_RGB888   = 2,
};

enum MaterialRenderTargetDepth_t
{
    MATERIAL_RT_DEPTH_SHARED   = 0,
    MATERIAL_RT_DEPTH_SEPARATE = 1,
};

inline constexpr u32 TEXTUREFLAGS_CLAMPS{ 0x4 };
inline constexpr u32 TEXTUREFLAGS_CLAMPT{ 0x8 };
inline constexpr u32 TEXTUREFLAGS_EIGHTBITALPHA{ 0x2000 };
inline constexpr u32 CREATERENDERTARGETFLAGS_HDR{ 0x1 };

struct VPlane
{
    vec3  m_Normal{ };
    float m_Dist{ };
};

inline constexpr i32 FRUSTUM_NUMPLANES{ 6 };

enum MaterialFogMode_t
{
    MATERIAL_FOG_NONE   = 0,
    MATERIAL_FOG_LINEAR = 1,
};

class IRefCounted
{
public:
    virtual i32 AddRef( )  = 0;
    virtual i32 Release( ) = 0;
};

class CFunctor : public IRefCounted
{
public:
    virtual ~CFunctor( )        = default;
    virtual void operator( )( ) = 0;

    u32 m_nUserID{ };
};

class ICallQueue
{
public:
    void QueueFunctor( CFunctor *functor );
};

inline constexpr i32 MATERIAL_VIEW{ 0 };
inline constexpr i32 MATERIAL_PROJECTION{ 1 };

class IMatRenderContext
{
public:
    void        BindLocalCubemap( ITexture *texture );
    void        SetAmbientLight( float r, float g, float b );
    void        SetAmbientLightCube( const float ( *cube )[ 4 ] );
    void        SetLightingOrigin( vec3 origin );
    void        DepthRange( float zNear, float zFar );
    void        ClearBuffers( bool bClearColor, bool bClearDepth, bool bClearStencil );
    void        SetStencilEnable( bool onoff );
    void        SetStencilFailOperation( StencilOperation_t op );
    void        SetStencilZFailOperation( StencilOperation_t op );
    void        SetStencilPassOperation( StencilOperation_t op );
    void        SetStencilCompareFunction( StencilComparisonFunction_t fn );
    void        SetStencilReferenceValue( int ref );
    void        SetStencilTestMask( u32 msk );
    void        SetStencilWriteMask( u32 msk );
    void        SetRenderTarget( ITexture *texture );
    void        Viewport( int x, int y, int width, int height );
    void        CullMode( MaterialCullMode_t mode );
    void        ClearColor4ub( u8 r, u8 g, u8 b, u8 a );
    void        DrawScreenSpaceRectangle( IMaterial *material, int dest_x, int dest_y, int width, int height,
                                          float src_x0, float src_y0, float src_x1, float src_y1,
                                          int src_width, int src_height );
    void        PushRenderTargetAndViewport( );
    void        PopRenderTargetAndViewport( );
    ICallQueue *GetCallQueue( );
    void        GetMatrix( i32 mode, VMatrix *matrix );
    void        Release( );
    void        FogMode( MaterialFogMode_t mode );
    void        FogStart( float start );
    void        FogEnd( float end );
    void        FogColor3fv( const float *rgb );
    void        FogMaxDensity( float max_density );
};

using MaterialHandle_t = u16;

class CMaterialSystem
{
public:
    IMaterial         *CreateMaterial( const char *pMaterialName, KeyValues *pVMTKeyValues );
    IMatRenderContext *GetRenderContext( );
    IMaterial         *FindMaterial( const char *pMaterialName, const char *pTextureGroupName, bool complain = true, const char *pComplainPrefix = nullptr );
    ITexture          *FindTexture( const char *pTextureName, const char *pTextureGroupName, bool complain = true );
    ITexture          *CreateNamedRenderTargetTextureEx( const char *pRTName, int w, int h, RenderTargetSizeMode_t sizeMode,
                                                         ImageFormat format, MaterialRenderTargetDepth_t depth,
                                                         u32 textureFlags, u32 renderTargetFlags );
    MaterialHandle_t   FirstMaterial( );
    MaterialHandle_t   NextMaterial( MaterialHandle_t handle );
    MaterialHandle_t   InvalidMaterial( );
    IMaterial         *GetMaterial( MaterialHandle_t handle );
};

class IVModelRender
{
public:
    void SuppressEngineLighting( bool suppress );
};

class CViewRender
{
public:
};

class INetworkStringTable
{
public:
    i32 AddString( bool isServer, const char *value, i32 length = -1, const void *userdata = nullptr );
    i32 FindStringIndex( const char *value );
};

class INetworkStringTableContainer
{
public:
    void                 RemoveAllTables( );
    INetworkStringTable *FindTable( const char *name );
    INetworkStringTable *CreateStringTableEx( const char *name, i32 max_entries, i32 user_data_fixed_size = 0,
                                              i32 user_data_network_bits = 0, bool is_filenames = false );
    bool                &allow_creation( )
    {
        return *reinterpret_cast< bool * >( reinterpret_cast< uptr >( this ) + 0x8 );
    }
};

inline constexpr i32 FMODELLOADER_CLIENT{ 1 << 2 };
inline constexpr i32 INVALID_STRING_INDEX{ 0xFFFF };

class IModelLoader
{
public:
    model_t *GetModelForName( const char *name, i32 type );
};

class CMatSystemSurface
{
public:
    void UnlockCursor( );
    void SetCursorAlwaysVisible( bool visible );
    void PlaySound( const char *file_name );
};

class CVRenderView
{
public:
    void  GetMatricesForView( const CViewSetup &view, VMatrix *pWorldToView, VMatrix *pViewToProjection, VMatrix *pWorldToProjection, VMatrix *pWorldToPixels );
    void  Push3DView( const CViewSetup &view, i32 flags, ITexture *render_target, VPlane *frustum );
    void  PopView( VPlane *frustum );
    void  SetBlend( float blend );
    float GetBlend( );
    void  SetColorModulation( const float *color );
    void  GetColorModulation( float *color );
};

class CMoveData;
class C_BaseEntity;
class C_BasePlayer;
class CMoveHelperClient
{
public:
    void ProcessImpacts( );
    void SetHost( C_BasePlayer *host );
    void ResetTouchList( );
};

enum fieldtype_t : int
{
    FIELD_VOID = 0,
    FIELD_FLOAT,
    FIELD_STRING,
    FIELD_VECTOR,
    FIELD_QUATERNION,
    FIELD_INTEGER,
    FIELD_BOOLEAN,
    FIELD_SHORT,
    FIELD_CHARACTER,
    FIELD_COLOR32,
    FIELD_EMBEDDED,
    FIELD_CUSTOM,

    FIELD_CLASSPTR,
    FIELD_EHANDLE,
    FIELD_EDICT,

    FIELD_POSITION_VECTOR,
    FIELD_TIME,
    FIELD_TICK,
    FIELD_MODELNAME,
    FIELD_SOUNDNAME,

    FIELD_INPUT,
    FIELD_FUNCTION,

    FIELD_VMATRIX,
    FIELD_VMATRIX_WORLDSPACE,
    FIELD_MATRIX3X4_WORLDSPACE,

    FIELD_INTERVAL,
    FIELD_MODELINDEX,
    FIELD_MATERIALINDEX,

    FIELD_VECTOR2D,

    FIELD_TYPECOUNT,
};

enum
{
    TD_OFFSET_NORMAL = 0,
    TD_OFFSET_PACKED = 1,
    TD_OFFSET_COUNT,
};

inline constexpr short FTYPEDESC_GLOBAL{ 0x0001 };
inline constexpr short FTYPEDESC_SAVE{ 0x0002 };
inline constexpr short FTYPEDESC_KEY{ 0x0004 };
inline constexpr short FTYPEDESC_INPUT{ 0x0008 };
inline constexpr short FTYPEDESC_OUTPUT{ 0x0010 };
inline constexpr short FTYPEDESC_FUNCTIONTABLE{ 0x0020 };
inline constexpr short FTYPEDESC_PTR{ 0x0040 };
inline constexpr short FTYPEDESC_OVERRIDE{ 0x0080 };

inline constexpr short FTYPEDESC_INSENDTABLE{ 0x0100 };
inline constexpr short FTYPEDESC_PRIVATE{ 0x0200 };
inline constexpr short FTYPEDESC_NOERRORCHECK{ 0x0400 };

inline constexpr short FTYPEDESC_MODELINDEX{ 0x0800 };
inline constexpr short FTYPEDESC_INDEX{ 0x1000 };

inline constexpr short FTYPEDESC_ONLY_ERROR_IF_ABOVE_ZERO_TO_ZERO_OR_BELOW_ETC{ 0x2000 };
inline constexpr float TD_MSECTOLERANCE{ 0.001f };

struct datamap_t;
class ISaveRestoreOps;

struct typedescription_t
{
    fieldtype_t        fieldType{ };
    const char        *fieldName{ };
    int                fieldOffset[ TD_OFFSET_COUNT ]{ };
    unsigned short     fieldSize{ };
    short              flags{ };
    const char        *externalName{ };
    ISaveRestoreOps   *pSaveRestoreOps{ };
    void              *inputFunc{ };
    datamap_t         *td{ };
    int                fieldSizeInBytes{ };
    typedescription_t *override_field{ };
    int                override_count{ };
    float              fieldTolerance{ };
};

struct datamap_t
{
    typedescription_t *dataDesc{ };
    int                dataNumFields{ };
    const char        *dataClassName{ };
    datamap_t         *baseMap{ };

    bool chains_validated{ };
    bool packed_offsets_computed{ };
    int  packed_size{ };
};

static_assert( sizeof( typedescription_t ) == 88 );
static_assert( sizeof( datamap_t ) == 40 );

enum
{
    PC_EVERYTHING = 0,
    PC_NON_NETWORKED_ONLY,
    PC_NETWORKED_ONLY,
};

inline constexpr bool PC_DATA_PACKED{ true };
inline constexpr bool PC_DATA_NORMAL{ false };

using FN_FIELD_COMPARE = void ( * )(
    const char *classname, const char *fieldname, const char *fieldtype,
    bool networked, bool noterrorchecked, bool differs, bool withintolerance,
    const char *value );

using FN_FIELD_DESCRIPTION = void ( * )(
    const char *classname, const char *fieldname, const char *fieldtype,
    bool networked, const char *value );

class CPredictionCopy
{
public:
    enum difftype_t
    {
        DIFFERS = 0,
        IDENTICAL,
        WITHINTOLERANCE,
    };

    CPredictionCopy( int type, void *dest, bool dest_packed, const void *src, bool src_packed ) :
        m_nType{ type },
        m_pDest{ dest },
        m_pSrc{ src },
        m_nDestOffsetIndex{ dest_packed ? TD_OFFSET_PACKED : TD_OFFSET_NORMAL },
        m_nSrcOffsetIndex{ src_packed ? TD_OFFSET_PACKED : TD_OFFSET_NORMAL },
        m_bPerformCopy{ true }
    {
    }

    int TransferData( const char *operation, int entindex, datamap_t *dmap );

private:
    int         m_nType{ };
    void       *m_pDest{ };
    const void *m_pSrc{ };
    int         m_nDestOffsetIndex{ };
    int         m_nSrcOffsetIndex{ };

    bool m_bErrorCheck{ };
    bool m_bReportErrors{ };
    bool m_bDescribeFields{ };

    typedescription_t *m_pCurrentField{ };
    const char        *m_pCurrentClassName{ };
    datamap_t         *m_pCurrentMap{ };
    bool               m_bShouldReport{ };
    bool               m_bShouldDescribe{ };
    int                m_nErrorCount{ };
    bool               m_bPerformCopy{ };

    FN_FIELD_COMPARE m_FieldCompareFunc{ };

    typedescription_t *m_pWatchField{ };
    const char        *m_pOperation{ };
};

static_assert( sizeof( CPredictionCopy ) == 104 );

class CPredictionDescribeData
{
public:
    void DumpDescription( datamap_t *pMap );

private:
    const void *m_pSrc{ };
    int         m_nSrcOffsetIndex{ };

    typedescription_t *m_pCurrentField{ };
    const char        *m_pCurrentClassName{ };
    datamap_t         *m_pCurrentMap{ };

    bool m_bShouldReport{ };

    FN_FIELD_DESCRIPTION m_FieldDescFunc{ };
};
class IPredictionSystem
{
public:
    IPredictionSystem *g_pPredictionSystems;
    IPredictionSystem *m_pNextSystem;
    bool               m_bSuppressEvent;
    C_BaseEntity      *m_pSuppressHost;
    int                m_nStatusPushed;
};

static_assert( sizeof( IPredictionSystem ) == 40 );

class __declspec( align( 8 ) ) CPrediction
{
public:
    void GetLocalViewAngles( vec3 &angles );
    void SetLocalViewAngles( vec3 &angles );
    void RunCommand( C_BasePlayer *player, CUserCmd *ucmd, CMoveHelperClient *moveHelper );
    void SetupMove( C_BasePlayer *player, CUserCmd *ucmd, CMoveHelperClient *moveHelper, CMoveData *move );
    void FinishMove( C_BasePlayer *player, CUserCmd *ucmd, CMoveData *move );

public:
    void       *m_vmt; // either my IDA has ball cancer or my debugger is mentally ill
    CBaseHandle m_hLastGround;
    bool        m_bInPrediction;
    bool        m_bFirstTimePredicted;
    bool        m_bOldCLPredictValue;
    bool        m_bEnginePaused;

    // Last network origin for local player
    int m_nPreviousStartFrame;

    int           m_nCommandsPredicted;
    int           m_nServerCommandsAcknowledged;
    int           m_bPreviousAckHadErrors;
    int           m_bPreviousAckErrorTriggersFullLatchReset;
    C_BaseEntity *m_EntsWithPredictionErrorsInLastAck;
    int           m_nIncomingPacketNumber;
    float         m_flIdealPitch;
};

class CRendering3dView
{
public:
};

//-----------------------------------------------------------------------------
// Purpose: Interface to shared data repository for KeyValues (included in vgui_controls.lib)
//			allows for central data storage point of KeyValues symbol table
//-----------------------------------------------------------------------------
class IKeyValuesSystem
{
public:
    // allocates/frees a KeyValues object from the shared mempool
    void *AllocKeyValuesMemory( int size );
};

//-----------------------------------------------------------------------------
// Purpose: Simple recursive data access class
//			Used in vgui for message parameters and resource files
//			Destructor deletes all child KeyValues nodes
//			Data is stored in key (string names) - (string/int/float)value pairs called nodes.
//
//	About KeyValues Text File Format:

//	It has 3 control characters '{', '}' and '"'. Names and values may be quoted or
//	not. The quote '"' character must not be used within name or values, only for
//	quoting whole tokens. You may use escape sequences wile parsing and add within a
//	quoted token a \" to add quotes within your name or token. When using Escape
//	Sequence the parser must now that by setting KeyValues::UsesEscapeSequences( true ),
//	which it's off by default. Non-quoted tokens ends with a whitespace, '{', '}' and '"'.
//	So you may use '{' and '}' within quoted tokens, but not for non-quoted tokens.
//  An open bracket '{' after a key name indicates a list of subkeys which is finished
//  with a closing bracket '}'. Subkeys use the same definitions recursively.
//  Whitespaces are space, return, newline and tabulator. Allowed Escape sequences
//	are \n, \t, \\, \n and \". The number character '#' is used for macro purposes
//	(eg #include), don't use it as first character in key names.
//-----------------------------------------------------------------------------
struct Color_t;
class KeyValues
{
public:
    KeyValues( const char *name );

    void SetString( const char *keyName, const char *value );
    void SetInt( const char *keyName, int value );

    // Memory allocation (optimized)
    void *operator new( usize iAllocSize );

    u8 m_pad[ 64 ];
};

class IMemAlloc
{
public:
    void *Alloc( size_t nSize );
    void *Realloc( void *mem, size_t size );
    void *Free( void *mem );
};

class CEventInfo
{
public:
    i16         m_class_id;
    u8          gap2[ 2 ];
    f32         m_fire_delay;
    u8          gap8[ 88 ];
    CEventInfo *m_prev;
    CEventInfo *m_next;
};

class CClientState
{
public:
    u8               gap0[ 24 ];
    int              m_Socket;
    CNetChannel     *m_NetChannel;
    unsigned int     m_nChallengeNr;
    double           m_flConnectTime;
    int              m_nRetryNumber;
    char             m_szRetryAddress[ 260 ];
    CUtlString       m_sRetrySourceTag;
    int              m_retryChallenge;
    int              m_nSignonState;
    double           m_flNextCmdTime;
    int              m_nServerCount;
    unsigned __int64 m_ulGameServerSteamID;
    int              m_nCurrentSequence;
    CClockDriftMgr   m_ClockDriftMgr;
    int              m_nDeltaTick;
    bool             m_bPaused;
    float            m_flPausedExpireTime;
    int              m_nViewEntity;
    int              m_nPlayerSlot;
    char             m_szLevelFileName[ 128 ];
    u8               gap24C[ 132 ];
    char             m_szLevelBaseName[ 128 ];
    u8               gap350[ 116 ];
    int              m_nMaxClients;
    u8               gap3C8[ 34856 ];
    void            *m_StringTableContainer;
    bool             m_bRestrictServerCommands;
    bool             m_bRestrictClientCommands;
    u8               gap8BFC[ 106 ];
    bool             insimulation;
    int              oldtickcount;
    float            m_tickRemainder;
    float            m_frameTime;
    int              lastoutgoingcommand;
    int              chokedcommands;
    int              last_command_ack;
    int              command_ack;
    int              m_nSoundSequence;
    bool             ishltv;
    bool             isreplay;
    u8               gap8C8A[ 278 ];
    int              demonum;
    CUtlString       demos[ 32 ];
    u8               gap8EE8[ 344184 ];
    bool             m_bMarkedCRCsUnverified;

public:
    bool IsConnected( ) const;
    bool IsActive( ) const;
    bool IsPaused( ) const;
    void ForceFullUpdate( );
};

class IDemoRecorder
{
public:
    bool IsRecording( );
    bool RecordUserInput( int command_number );
};

class IDemoPlayer
{
public:
    bool IsPlayingBack( );
};

constexpr int FCVAR_NONE                    = 0;
constexpr int FCVAR_UNREGISTERED            = ( 1 << 0 );
constexpr int FCVAR_DEVELOPMENTONLY         = ( 1 << 1 );
constexpr int FCVAR_GAMEDLL                 = ( 1 << 2 );
constexpr int FCVAR_CLIENTDLL               = ( 1 << 3 );
constexpr int FCVAR_HIDDEN                  = ( 1 << 4 );
constexpr int FCVAR_PROTECTED               = ( 1 << 5 );
constexpr int FCVAR_SPONLY                  = ( 1 << 6 );
constexpr int FCVAR_ARCHIVE                 = ( 1 << 7 );
constexpr int FCVAR_NOTIFY                  = ( 1 << 8 );
constexpr int FCVAR_USERINFO                = ( 1 << 9 );
constexpr int FCVAR_PRINTABLEONLY           = ( 1 << 10 );
constexpr int FCVAR_UNLOGGED                = ( 1 << 11 );
constexpr int FCVAR_NEVER_AS_STRING         = ( 1 << 12 );
constexpr int FCVAR_REPLICATED              = ( 1 << 13 );
constexpr int FCVAR_CHEAT                   = ( 1 << 14 );
constexpr int FCVAR_INTERNAL_USE            = ( 1 << 15 );
constexpr int FCVAR_DEMO                    = ( 1 << 16 );
constexpr int FCVAR_DONTRECORD              = ( 1 << 17 );
constexpr int FCVAR_ALLOWED_IN_COMPETITIVE  = ( 1 << 18 );
constexpr int FCVAR_RELOAD_MATERIALS        = ( 1 << 20 );
constexpr int FCVAR_RELOAD_TEXTURES         = ( 1 << 21 );
constexpr int FCVAR_NOT_CONNECTED           = ( 1 << 22 );
constexpr int FCVAR_MATERIAL_SYSTEM_THREAD  = ( 1 << 23 );
constexpr int FCVAR_ARCHIVE_XBOX            = ( 1 << 24 );
constexpr int FCVAR_ACCESSIBLE_FROM_THREADS = ( 1 << 25 );
constexpr int FCVAR_SERVER_CAN_EXECUTE      = ( 1 << 28 );
constexpr int FCVAR_SERVER_CANNOT_QUERY     = ( 1 << 29 );
constexpr int FCVAR_CLIENTCMD_CAN_EXECUTE   = ( 1 << 30 );
constexpr int FCVAR_EXEC_DESPITE_DEFAULT    = ( 1 << 31 );
constexpr int FCVAR_MATERIAL_THREAD_MASK    = ( FCVAR_RELOAD_MATERIALS | FCVAR_RELOAD_TEXTURES | FCVAR_MATERIAL_SYSTEM_THREAD );

class IConVar
{
public:
    void        SetValue( const char *pValue );
    void        SetValue( float flValue );
    void        SetValue( int nValue );
    const char *GetName( void ) const;
    bool        IsFlagSet( int nFlag ) const;

    void *vmt;
};

class ConCommandBase
{
public:
    void *vmt;

    ConCommandBase *m_pNext;

    bool m_bRegistered;

    const char *m_pszName;
    const char *m_pszHelpString;

    int m_nFlags;
};

typedef void ( *FnChangeCallback_t )( IConVar *var, const char *pOldValue, float flOldValue );

class ConVar : public ConCommandBase, public IConVar
{
public:
    ConVar *m_pParent;

    const char *m_pszDefaultValue;

    char *m_pszString;
    int   m_StringLength;

    float m_fValue;
    int   m_nValue;

    bool  m_bHasMin;
    float m_fMinVal;
    bool  m_bHasMax;
    float m_fMaxVal;

    bool  m_bHasCompMin;
    float m_fCompMinVal;
    bool  m_bHasCompMax;
    float m_fCompMaxVal;

    bool m_bCompetitiveRestrictions;

    FnChangeCallback_t m_fnChangeCallback;

public:
    int         GetInt( ) const;
    bool        GetBool( ) const;
    float       GetFloat( ) const;
    const char *GetString( ) const;
};

static_assert( sizeof( ConCommandBase ) == 0x30 );

class CCommand
{
public:
    int         m_nArgc;
    int         m_nArgv0Size;
    char        m_pArgSBuffer[ 512 ];
    char        m_pArgvBuffer[ 512 ];
    const char *m_ppArgv[ 64 ];
};

class ConCommand : public ConCommandBase
{
public:
    void ( *m_fnCommandCallback )( const CCommand &command );
    void *m_fnCompletionCallback;
    bool  m_bHasCompletionCallback : 1;
    bool  m_bUsingNewCommandCallback : 1;
    bool  m_bUsingCommandCallbackInterface : 1;
};

static_assert( sizeof( ConCommand ) == 0x48 );

class ICvar
{
public:
    ConVar         *FindVar( const char *var_name );
    void            ConsoleColorPrintf( const Color_t &clr, const char *pFormat, ... );
    ConCommandBase *GetCommands( );
};

class CGameMovement
{
public:
    void ProcessMovement( C_BasePlayer *pPlayer, CMoveData *pMove );
    void StartTrackPredictionErrors( C_BasePlayer *pPlayer );
    void FinishTrackPredictionErrors( C_BasePlayer *pPlayer );
};

enum
{
    SPEED_CROPPED_RESET  = 0,
    SPEED_CROPPED_DUCK   = 1,
    SPEED_CROPPED_WEAPON = 2,
};

struct objectparams_t
{
    vec3       *massCenterOverride{ };
    float       mass{ };
    float       inertia{ };
    float       damping{ };
    float       rotdamping{ };
    float       rotInertiaLimit{ };
    const char *pName{ };
    void       *pGameData{ };
    float       volume{ };
    float       dragCoefficient{ };
    bool        enableCollisions{ };
};

const objectparams_t g_PhysDefaultObjectParams{ nullptr, 1.0f, 1.0f, 0.1f, 0.1f, 0.05f, "DEFAULT", nullptr, 0.0f, 1.0f, true };

struct solid_t
{
    i32            index{ };
    char           name[ 512 ]{ };
    char           parent[ 512 ]{ };
    char           surfaceprop[ 512 ]{ };
    vec3           massCenterOverride{ };
    objectparams_t params{ };
};
static_assert( sizeof( solid_t ) == 0x650 );

class IVPhysicsKeyHandler
{
public:
    virtual void ParseKeyValue( void *pData, const char *pKey, const char *pValue ) = 0;
    virtual void SetDefaults( void *pData )                                         = 0;
};

constexpr float k_flMaxVelocity        = 2000.0f;
constexpr float k_flMaxAngularVelocity = 360.0f * 10.0f;

constexpr float DEFAULT_MIN_FRICTION_MASS = 10.0f;
constexpr float DEFAULT_MAX_FRICTION_MASS = 2500.0f;

struct physics_performanceparams_t
{
    int   maxCollisionsPerObjectPerTimestep{ };
    int   maxCollisionChecksPerTimestep{ };
    float maxVelocity{ };
    float maxAngularVelocity{ };
    float lookAheadTimeObjectsVsWorld{ };
    float lookAheadTimeObjectsVsObject{ };
    float minFrictionMass{ };
    float maxFrictionMass{ };

    void Defaults( )
    {
        maxCollisionsPerObjectPerTimestep = 6;
        maxCollisionChecksPerTimestep     = 250;
        maxVelocity                       = k_flMaxVelocity;
        maxAngularVelocity                = k_flMaxAngularVelocity;
        lookAheadTimeObjectsVsWorld       = 1.0f;
        lookAheadTimeObjectsVsObject      = 0.5f;
        minFrictionMass                   = DEFAULT_MIN_FRICTION_MASS;
        maxFrictionMass                   = DEFAULT_MAX_FRICTION_MASS;
    }
};

class CPhysCollide;

typedef vec3 AngularImpulse;

class IVP_Real_Object;
class IPhysicsShadowController;

class IPhysicsObject
{
private:
    void *thisptr{ };

public:
    void                     *m_pGameData{ };
    IVP_Real_Object          *m_pObject{ };
    const CPhysCollide       *m_pCollide{ };
    IPhysicsShadowController *m_pShadow{ };
    vec3                      m_dragBasis{ };
    vec3                      m_angDragBasis{ };
    bool                      m_shadowTempGravityDisable : 5;
    bool                      m_hasTouchedDynamic : 1;
    bool                      m_asleepSinceCreation : 1;
    bool                      m_forceSilentDelete : 1;
    unsigned char             m_sleepState : 2;
    unsigned char             m_hingedAxis : 3;
    unsigned char             m_collideType : 3;
    unsigned short            m_gameIndex{ };
    unsigned short            m_materialIndex{ };
    unsigned short            m_activeIndex{ };
    unsigned short            m_callbacks{ };
    unsigned short            m_gameFlags{ };
    unsigned int              m_contentsMask{ };
    float                     m_volume{ };
    float                     m_buoyancyRatio{ };
    float                     m_dragCoefficient{ };
    float                     m_angDragCoefficient{ };

public:
    bool IsAsleep( );
    void Wake( );
    void SetDragCoefficient( float *pDrag, float *pAngularDrag );
    void SetPosition( const vec3 &worldPosition, const vec3 &angles, bool isTeleport );
    void GetPosition( vec3 *worldPosition, vec3 *angles );
    void SetVelocity( const vec3 *velocity, const AngularImpulse *angularVelocity );
    void GetVelocity( vec3 *velocity, AngularImpulse *angularVelocity );
};

class IPhysicsCollisionData
{
public:
    void GetSurfaceNormal( vec3 &out );
    void GetContactPoint( vec3 &out );
};

struct vcollisionevent_t
{
    IPhysicsObject        *pObjects[ 2 ]{ };
    int                    surfaceProps[ 2 ]{ };
    bool                   isCollision{ };
    bool                   isShadowCollision{ };
    float                  deltaCollisionTime{ };
    float                  collisionSpeed{ };
    IPhysicsCollisionData *pInternalData{ };
};

class IPhysicsFluidController;

class IPhysicsCollisionEvent
{
public:
    virtual void PreCollision( vcollisionevent_t *pEvent )                                                                              = 0;
    virtual void PostCollision( vcollisionevent_t *pEvent )                                                                             = 0;
    virtual void Friction( IPhysicsObject *pObject, float energy, int surfaceProps, int surfacePropsHit, IPhysicsCollisionData *pData ) = 0;
    virtual void StartTouch( IPhysicsObject *pObject1, IPhysicsObject *pObject2, IPhysicsCollisionData *pTouchData )                    = 0;
    virtual void EndTouch( IPhysicsObject *pObject1, IPhysicsObject *pObject2, IPhysicsCollisionData *pTouchData )                      = 0;
    virtual void FluidStartTouch( IPhysicsObject *pObject, IPhysicsFluidController *pFluid )                                            = 0;
    virtual void FluidEndTouch( IPhysicsObject *pObject, IPhysicsFluidController *pFluid )                                              = 0;
    virtual void PostSimulationFrame( )                                                                                                 = 0;
    virtual void ObjectEnterTrigger( IPhysicsObject *pTrigger, IPhysicsObject *pObject )                                                = 0;
    virtual void ObjectLeaveTrigger( IPhysicsObject *pTrigger, IPhysicsObject *pObject )                                                = 0;
};

class IPhysicsEnvironment
{
public:
    void            SetGravity( const vec3 &gravityVector );
    void            SetAirDensity( float density );
    IPhysicsObject *CreatePolyObject( const CPhysCollide *pCollisionModel, int materialIndex, const vec3 &position, const vec3 &angles, objectparams_t *pParams );
    IPhysicsObject *CreatePolyObjectStatic( const CPhysCollide *pCollisionModel, int materialIndex, const vec3 &position, const vec3 &angles, objectparams_t *pParams );
    void            DestroyObject( IPhysicsObject *object );
    void            Simulate( float deltaTime );
    void            SetSimulationTimestep( float timestep );
    void            ResetSimulationClock( );
    void            SetCollisionEventHandler( IPhysicsCollisionEvent *pCollisionEvents );
    void            SetPerformanceSettings( const physics_performanceparams_t *pSettings );
};

class IPhysics
{
public:
    IPhysicsEnvironment *CreateEnvironment( );
    void                 DestroyEnvironment( IPhysicsEnvironment *environment );
};

struct surfacephysicsparams_t
{
    f32 friction{ };
    f32 elasticity{ };
    f32 density{ };
    f32 thickness{ };
    f32 dampening{ };
};

struct surfaceaudioparams_t
{
    f32 reflectivity{ };
    f32 hardnessFactor{ };
    f32 roughnessFactor{ };
    f32 roughThreshold{ };
    f32 hardThreshold{ };
    f32 hardVelocityThreshold{ };
};

struct surfacesoundnames_t
{
    u16 stepleft{ };
    u16 stepright{ };
    u16 impactSoft{ };
    u16 impactHard{ };
    u16 scrapeSmooth{ };
    u16 scrapeRough{ };
    u16 bulletImpact{ };
    u16 rolling{ };
    u16 breakSound{ };
    u16 strainSound{ };
};

struct surfacegameprops_t
{
    f32 maxSpeedFactor{ };
    f32 jumpFactor{ };
    u16 material{ };
    u8  climbable{ };
    u8  pad{ };
};

struct surfacedata_t
{
    surfacephysicsparams_t physics{ };
    surfaceaudioparams_t   audio{ };
    surfacesoundnames_t    sounds{ };
    surfacegameprops_t     game{ };
};

static_assert( __builtin_offsetof( surfacedata_t, game.material ) == 72 );

class IPhysicsSurfaceProps
{
public:
    i32            GetSurfaceIndex( const char *name ) const;
    surfacedata_t *GetSurfaceData( i32 surface_data_index );
    void           GetPhysicsProperties( i32 surface_data_index, f32 *density, f32 *thickness,
                                         f32 *friction, f32 *elasticity ) const;
};

class CPhysConvex;

struct polyhedron_line_t
{
    u16 m_points[ 2 ];
};

struct polyhedron_reference_t
{
    u16 m_line;
    u8  m_endpoint;
};

struct polyhedron_polygon_t
{
    u16  m_first;
    u16  m_count;
    vec3 m_normal;
};

class CPolyhedron
{
public:
    virtual ~CPolyhedron( ) = default;
    virtual void Release( ) = 0;

    vec3                   *m_vertices;
    polyhedron_line_t      *m_lines;
    polyhedron_reference_t *m_references;
    polyhedron_polygon_t   *m_polygons;
    u16                     m_vertex_count;
    u16                     m_line_count;
    u16                     m_reference_count;
    u16                     m_polygon_count;
};

static_assert( sizeof( polyhedron_reference_t ) == 4 );
static_assert( sizeof( polyhedron_polygon_t ) == 16 );
static_assert( sizeof( CPolyhedron ) == 48 );

class IPhysicsCollision
{
public:
    CPhysCollide *BBoxToCollide( const vec3 &mins, const vec3 &maxs );
    void          CollideGetAABB( vec3 *mins, vec3 *maxs, const CPhysCollide *collide, const vec3 &origin, const vec3 &angles );
    CPhysConvex  *ConvexFromPlanes( float *planes, i32 count, float merge_distance );
    void          ConvexFree( CPhysConvex *convex );
    CPhysCollide *ConvertConvexToCollide( CPhysConvex **convexes, i32 count );
    void          DestroyCollide( CPhysCollide *collide );
    i32           GetConvexesUsedInCollideable( const CPhysCollide *collide, CPhysConvex **convexes, i32 capacity );
    CPolyhedron  *PolyhedronFromConvex( CPhysConvex *convex, bool temporary );
    i32           CreateDebugMesh( const CPhysCollide *collide, vec3 **vertices );
    void          DestroyDebugMesh( i32 count, vec3 *vertices );
    vec3          CollideGetExtent( const CPhysCollide *collide, const vec3 &origin, const vec3 &angles, const vec3 &direction );
    void          TraceCollide( const vec3 &start, const vec3 &end, const CPhysCollide *sweep, const vec3 &angles,
                                const CPhysCollide *collide, const vec3 &origin, const vec3 &collide_angles, trace_t *trace );
};

enum PaintMode_t
{
    PAINT_UIPANELS     = ( 1 << 0 ),
    PAINT_INGAMEPANELS = ( 1 << 1 ),
    PAINT_CURSOR       = ( 1 << 2 ),
};

enum
{
    LIFE_ALIVE = 0,
    LIFE_DYING = 1,
    LIFE_DEAD  = 2
};

enum
{
    TF_STATE_ACTIVE = 0,
    TF_STATE_WELCOME,
    TF_STATE_OBSERVER,
    TF_STATE_DYING,
    TF_STATE_COUNT
};

enum ETFClass : i32
{
    TF_CLASS_UNDEFINED    = 0,
    TF_CLASS_SCOUT        = 1,
    TF_CLASS_SNIPER       = 2,
    TF_CLASS_SOLDIER      = 3,
    TF_CLASS_DEMOMAN      = 4,
    TF_CLASS_MEDIC        = 5,
    TF_CLASS_HEAVYWEAPONS = 6,
    TF_CLASS_PYRO         = 7,
    TF_CLASS_SPY          = 8,
    TF_CLASS_ENGINEER     = 9,
};

class IGameEvent
{
public:
    virtual ~IGameEvent( ) { };
    virtual const char *GetName( ) const                                                                = 0;
    virtual bool        IsReliable( ) const                                                             = 0;
    virtual bool        IsLocal( ) const                                                                = 0;
    virtual bool        IsEmpty( const char *keyName = nullptr )                                        = 0;
    virtual bool        GetBool( const char *keyName = nullptr, bool defaultValue = false ) const       = 0;
    virtual int         GetInt( const char *keyName = nullptr, int defaultValue = 0 ) const             = 0;
    virtual float       GetFloat( const char *keyName = nullptr, float defaultValue = 0.0f ) const      = 0;
    virtual const char *GetString( const char *keyName = nullptr, const char *defaultValue = "" ) const = 0;
    virtual void        SetBool( const char *keyName, bool value )                                      = 0;
    virtual void        SetInt( const char *keyName, int value )                                        = 0;
    virtual void        SetFloat( const char *keyName, float value )                                    = 0;
    virtual void        SetString( const char *keyName, const char *value )                             = 0;
};

class IGameEventListener2
{
public:
    virtual ~IGameEventListener2( ) { };
    virtual void FireGameEvent( IGameEvent *event ) = 0;
    virtual int  GetEventDebugID( )                 = 0;
};

class IGameEventManager2
{
public:
    virtual ~IGameEventManager2( ) { };
    virtual int         LoadEventsFromFile( const char *filename )                                      = 0;
    virtual void        Reset( )                                                                        = 0;
    virtual bool        AddListener( IGameEventListener2 *listener, const char *name, bool serverside ) = 0;
    virtual bool        FindListener( IGameEventListener2 *listener, const char *name )                 = 0;
    virtual bool        RemoveListener( IGameEventListener2 *listener )                                 = 0;
    virtual IGameEvent *CreateEvent( const char *name, bool force = false )                             = 0;
    virtual bool        FireEvent( IGameEvent *event, bool dont_broadcast = false )                     = 0;
    virtual bool        FireEventClientSide( IGameEvent *event )                                        = 0;
    virtual IGameEvent *DuplicateEvent( IGameEvent *event )                                             = 0;
    virtual void        FreeEvent( IGameEvent *event )                                                  = 0;
};

inline constexpr i32 TELEPORTER_STATE_IDLE{ 1 };
inline constexpr i32 TELEPORTER_STATE_READY{ 2 };
inline constexpr i32 TELEPORTER_STATE_SENDING{ 3 };
inline constexpr i32 TELEPORTER_STATE_RECEIVING{ 4 };
inline constexpr i32 TELEPORTER_STATE_RECEIVING_RELEASE{ 5 };
inline constexpr i32 TELEPORTER_STATE_RECHARGING{ 6 };
inline constexpr i32 TELEPORTER_STATE_UPGRADING{ 7 };

inline constexpr i32 TF_TEAM_RED{ 2 };
inline constexpr i32 TF_TEAM_BLUE{ 3 };
inline constexpr i32 TF_TEAM_PVE_INVADERS_GIANTS{ 4 };

inline constexpr i32 TF_FLAGINFO_HOME{ 0 };
inline constexpr i32 TF_FLAGINFO_STOLEN{ 1 };
inline constexpr i32 TF_FLAGINFO_DROPPED{ 2 };

inline constexpr i32 TF_DETONATE_MODE_DOT{ 1 };
inline constexpr i32 FLAREGUN_DETONATE{ 1 };

inline constexpr i32 SENTRYGUN_MAX_SHELLS_1{ 150 };
inline constexpr i32 SENTRYGUN_MAX_SHELLS_2{ 200 };
inline constexpr i32 SENTRYGUN_MAX_ROCKETS{ 20 };

inline constexpr float TF_GRENADE_DAMAGE_RADIUS{ 146.0f };
inline constexpr float TF_ITEM_RESPAWN_TIME{ 10.0f };
inline constexpr float ITEM_PICKUP_BOX_BLOAT{ 24.0f };

typedef enum
{
    ACT_INVALID = -1, // So we have something more succint to check for than '-1'
    ACT_RESET   = 0,  // Set m_Activity to this invalid value to force a reset to m_IdealActivity
    ACT_IDLE,
    ACT_TRANSITION,
    ACT_COVER,     // FIXME: obsolete? redundant with ACT_COVER_LOW?
    ACT_COVER_MED, // FIXME: unsupported?
    ACT_COVER_LOW, // FIXME: rename ACT_IDLE_CROUCH?
    ACT_WALK,
    ACT_WALK_AIM,
    ACT_WALK_CROUCH,
    ACT_WALK_CROUCH_AIM,
    ACT_RUN,
    ACT_RUN_AIM,
    ACT_RUN_CROUCH,
    ACT_RUN_CROUCH_AIM,
    ACT_RUN_PROTECTED,
    ACT_SCRIPT_CUSTOM_MOVE,
    ACT_RANGE_ATTACK1,
    ACT_RANGE_ATTACK2,
    ACT_RANGE_ATTACK1_LOW, // FIXME: not used yet, crouched versions of the range attack
    ACT_RANGE_ATTACK2_LOW, // FIXME: not used yet, crouched versions of the range attack
    ACT_DIESIMPLE,
    ACT_DIEBACKWARD,
    ACT_DIEFORWARD,
    ACT_DIEVIOLENT,
    ACT_DIERAGDOLL,
    ACT_FLY, // Fly (and flap if appropriate)
    ACT_HOVER,
    ACT_GLIDE,
    ACT_SWIM,
    ACT_JUMP,
    ACT_HOP,  // vertical jump
    ACT_LEAP, // long forward jump
    ACT_LAND,
    ACT_CLIMB_UP,
    ACT_CLIMB_DOWN,
    ACT_CLIMB_DISMOUNT,
    ACT_SHIPLADDER_UP,
    ACT_SHIPLADDER_DOWN,
    ACT_STRAFE_LEFT,
    ACT_STRAFE_RIGHT,
    ACT_ROLL_LEFT,  // tuck and roll, left
    ACT_ROLL_RIGHT, // tuck and roll, right
    ACT_TURN_LEFT,  // turn quickly left (stationary)
    ACT_TURN_RIGHT, // turn quickly right (stationary)
    ACT_CROUCH,     // FIXME: obsolete? only used be soldier (the act of crouching down from a standing position)
    ACT_CROUCHIDLE, // FIXME: obsolete? only used be soldier (holding body in crouched position (loops))
    ACT_STAND,      // FIXME: obsolete? should be transition (the act of standing from a crouched position)
    ACT_USE,
    ACT_SIGNAL1,
    ACT_SIGNAL2,
    ACT_SIGNAL3,

    ACT_SIGNAL_ADVANCE, // Squad handsignals, specific.
    ACT_SIGNAL_FORWARD,
    ACT_SIGNAL_GROUP,
    ACT_SIGNAL_HALT,
    ACT_SIGNAL_LEFT,
    ACT_SIGNAL_RIGHT,
    ACT_SIGNAL_TAKECOVER,

    ACT_LOOKBACK_RIGHT, // look back over shoulder without turning around.
    ACT_LOOKBACK_LEFT,
    ACT_COWER,        // FIXME: unused, should be more extreme version of crouching
    ACT_SMALL_FLINCH, // FIXME: needed? shouldn't flinching be down with overlays?
    ACT_BIG_FLINCH,
    ACT_MELEE_ATTACK1,
    ACT_MELEE_ATTACK2,
    ACT_RELOAD,
    ACT_RELOAD_START,
    ACT_RELOAD_FINISH,
    ACT_RELOAD_LOW,
    ACT_ARM,    // pull out gun, for instance
    ACT_DISARM, // reholster gun
    ACT_DROP_WEAPON,
    ACT_DROP_WEAPON_SHOTGUN,
    ACT_PICKUP_GROUND, // pick up something in front of you on the ground
    ACT_PICKUP_RACK,   // pick up something from a rack or shelf in front of you.
    ACT_IDLE_ANGRY,    // FIXME: being used as an combat ready idle?  alternate idle animation in which the monster is clearly agitated. (loop)

    ACT_IDLE_RELAXED,
    ACT_IDLE_STIMULATED,
    ACT_IDLE_AGITATED,
    ACT_IDLE_STEALTH,
    ACT_IDLE_HURT,

    ACT_WALK_RELAXED,
    ACT_WALK_STIMULATED,
    ACT_WALK_AGITATED,
    ACT_WALK_STEALTH,

    ACT_RUN_RELAXED,
    ACT_RUN_STIMULATED,
    ACT_RUN_AGITATED,
    ACT_RUN_STEALTH,

    ACT_IDLE_AIM_RELAXED,
    ACT_IDLE_AIM_STIMULATED,
    ACT_IDLE_AIM_AGITATED,
    ACT_IDLE_AIM_STEALTH,

    ACT_WALK_AIM_RELAXED,
    ACT_WALK_AIM_STIMULATED,
    ACT_WALK_AIM_AGITATED,
    ACT_WALK_AIM_STEALTH,

    ACT_RUN_AIM_RELAXED,
    ACT_RUN_AIM_STIMULATED,
    ACT_RUN_AIM_AGITATED,
    ACT_RUN_AIM_STEALTH,

    ACT_CROUCHIDLE_STIMULATED,
    ACT_CROUCHIDLE_AIM_STIMULATED,
    ACT_CROUCHIDLE_AGITATED,

    ACT_WALK_HURT,       // limp  (loop)
    ACT_RUN_HURT,        // limp  (loop)
    ACT_SPECIAL_ATTACK1, // very monster specific special attacks.
    ACT_SPECIAL_ATTACK2,
    ACT_COMBAT_IDLE, // FIXME: unused?  agitated idle.
    ACT_WALK_SCARED,
    ACT_RUN_SCARED,
    ACT_VICTORY_DANCE, // killed a player, do a victory dance.
    ACT_DIE_HEADSHOT,  // die, hit in head.
    ACT_DIE_CHESTSHOT, // die, hit in chest
    ACT_DIE_GUTSHOT,   // die, hit in gut
    ACT_DIE_BACKSHOT,  // die, hit in back
    ACT_FLINCH_HEAD,
    ACT_FLINCH_CHEST,
    ACT_FLINCH_STOMACH,
    ACT_FLINCH_LEFTARM,
    ACT_FLINCH_RIGHTARM,
    ACT_FLINCH_LEFTLEG,
    ACT_FLINCH_RIGHTLEG,
    ACT_FLINCH_PHYSICS,

    ACT_IDLE_ON_FIRE, // ON FIRE animations
    ACT_WALK_ON_FIRE,
    ACT_RUN_ON_FIRE,

    ACT_RAPPEL_LOOP, // Rappel down a rope!

    ACT_180_LEFT, // 180 degree left turn
    ACT_180_RIGHT,

    ACT_90_LEFT, // 90 degree turns
    ACT_90_RIGHT,

    ACT_STEP_LEFT, // Single steps
    ACT_STEP_RIGHT,
    ACT_STEP_BACK,
    ACT_STEP_FORE,

    ACT_GESTURE_RANGE_ATTACK1,
    ACT_GESTURE_RANGE_ATTACK2,
    ACT_GESTURE_MELEE_ATTACK1,
    ACT_GESTURE_MELEE_ATTACK2,
    ACT_GESTURE_RANGE_ATTACK1_LOW, // FIXME: not used yet, crouched versions of the range attack
    ACT_GESTURE_RANGE_ATTACK2_LOW, // FIXME: not used yet, crouched versions of the range attack

    ACT_MELEE_ATTACK_SWING_GESTURE,

    ACT_GESTURE_SMALL_FLINCH,
    ACT_GESTURE_BIG_FLINCH,
    ACT_GESTURE_FLINCH_BLAST, // Startled by an explosion
    ACT_GESTURE_FLINCH_BLAST_SHOTGUN,
    ACT_GESTURE_FLINCH_BLAST_DAMAGED, // Damaged by an explosion
    ACT_GESTURE_FLINCH_BLAST_DAMAGED_SHOTGUN,
    ACT_GESTURE_FLINCH_HEAD,
    ACT_GESTURE_FLINCH_CHEST,
    ACT_GESTURE_FLINCH_STOMACH,
    ACT_GESTURE_FLINCH_LEFTARM,
    ACT_GESTURE_FLINCH_RIGHTARM,
    ACT_GESTURE_FLINCH_LEFTLEG,
    ACT_GESTURE_FLINCH_RIGHTLEG,

    ACT_GESTURE_TURN_LEFT,
    ACT_GESTURE_TURN_RIGHT,
    ACT_GESTURE_TURN_LEFT45,
    ACT_GESTURE_TURN_RIGHT45,
    ACT_GESTURE_TURN_LEFT90,
    ACT_GESTURE_TURN_RIGHT90,
    ACT_GESTURE_TURN_LEFT45_FLAT,
    ACT_GESTURE_TURN_RIGHT45_FLAT,
    ACT_GESTURE_TURN_LEFT90_FLAT,
    ACT_GESTURE_TURN_RIGHT90_FLAT,

    // HALF-LIFE 1 compatability stuff goes here. Temporary!
    ACT_BARNACLE_HIT,   // barnacle tongue hits a monster
    ACT_BARNACLE_PULL,  // barnacle is lifting the monster ( loop )
    ACT_BARNACLE_CHOMP, // barnacle latches on to the monster
    ACT_BARNACLE_CHEW,  // barnacle is holding the monster in its mouth ( loop )

    // Sometimes, you just want to set an NPC's sequence to a sequence that doesn't actually
    // have an activity. The AI will reset the NPC's sequence to whatever its IDEAL activity
    // is, though. So if you set ideal activity to DO_NOT_DISTURB, the AI will not interfere
    // with the NPC's current sequence. (SJB)
    ACT_DO_NOT_DISTURB,

    // viewmodel (weapon) activities
    // FIXME: move these to the specific viewmodels, no need to make global
    ACT_VM_DRAW,
    ACT_VM_HOLSTER,
    ACT_VM_IDLE,
    ACT_VM_FIDGET,
    ACT_VM_PULLBACK,
    ACT_VM_PULLBACK_HIGH,
    ACT_VM_PULLBACK_LOW,
    ACT_VM_THROW,
    ACT_VM_PULLPIN,
    ACT_VM_PRIMARYATTACK,   // fire
    ACT_VM_SECONDARYATTACK, // alt. fire
    ACT_VM_RELOAD,
    ACT_VM_RELOAD_START,
    ACT_VM_RELOAD_FINISH,
    ACT_VM_DRYFIRE,     // fire with no ammo loaded.
    ACT_VM_HITLEFT,     // bludgeon, swing to left - hit (primary attk)
    ACT_VM_HITLEFT2,    // bludgeon, swing to left - hit (secondary attk)
    ACT_VM_HITRIGHT,    // bludgeon, swing to right - hit (primary attk)
    ACT_VM_HITRIGHT2,   // bludgeon, swing to right - hit (secondary attk)
    ACT_VM_HITCENTER,   // bludgeon, swing center - hit (primary attk)
    ACT_VM_HITCENTER2,  // bludgeon, swing center - hit (secondary attk)
    ACT_VM_MISSLEFT,    // bludgeon, swing to left - miss (primary attk)
    ACT_VM_MISSLEFT2,   // bludgeon, swing to left - miss (secondary attk)
    ACT_VM_MISSRIGHT,   // bludgeon, swing to right - miss (primary attk)
    ACT_VM_MISSRIGHT2,  // bludgeon, swing to right - miss (secondary attk)
    ACT_VM_MISSCENTER,  // bludgeon, swing center - miss (primary attk)
    ACT_VM_MISSCENTER2, // bludgeon, swing center - miss (secondary attk)
    ACT_VM_HAULBACK,    // bludgeon, haul the weapon back for a hard strike (secondary attk)
    ACT_VM_SWINGHARD,   // bludgeon, release the hard strike (secondary attk)
    ACT_VM_SWINGMISS,
    ACT_VM_SWINGHIT,
    ACT_VM_IDLE_TO_LOWERED,
    ACT_VM_IDLE_LOWERED,
    ACT_VM_LOWERED_TO_IDLE,
    ACT_VM_RECOIL1,
    ACT_VM_RECOIL2,
    ACT_VM_RECOIL3,
    ACT_VM_PICKUP,
    ACT_VM_RELEASE,

    ACT_VM_ATTACH_SILENCER,
    ACT_VM_DETACH_SILENCER,

    // TF2 Scout Pack
    ACT_VM_DRAW_SPECIAL,
    ACT_VM_HOLSTER_SPECIAL,
    ACT_VM_IDLE_SPECIAL,
    ACT_VM_PULLBACK_SPECIAL,
    ACT_VM_PRIMARYATTACK_SPECIAL,
    ACT_VM_SECONDARYATTACK_SPECIAL,
    ACT_VM_HITCENTER_SPECIAL,
    ACT_VM_SWINGHARD_SPECIAL,
    ACT_VM_IDLE_TO_LOWERED_SPECIAL,
    ACT_VM_IDLE_LOWERED_SPECIAL,
    ACT_VM_LOWERED_TO_IDLE_SPECIAL,

    ACT_FISTS_VM_HITLEFT,
    ACT_FISTS_VM_HITRIGHT,
    ACT_FISTS_VM_SWINGHARD,
    ACT_FISTS_VM_IDLE,
    ACT_FISTS_VM_DRAW,

    //===========================
    // HL2 Specific Activities
    //===========================
    // SLAM	Specialty Activities
    ACT_SLAM_STICKWALL_IDLE,
    ACT_SLAM_STICKWALL_ND_IDLE,
    ACT_SLAM_STICKWALL_ATTACH,
    ACT_SLAM_STICKWALL_ATTACH2,
    ACT_SLAM_STICKWALL_ND_ATTACH,
    ACT_SLAM_STICKWALL_ND_ATTACH2,
    ACT_SLAM_STICKWALL_DETONATE,
    ACT_SLAM_STICKWALL_DETONATOR_HOLSTER,
    ACT_SLAM_STICKWALL_DRAW,
    ACT_SLAM_STICKWALL_ND_DRAW,
    ACT_SLAM_STICKWALL_TO_THROW,
    ACT_SLAM_STICKWALL_TO_THROW_ND,
    ACT_SLAM_STICKWALL_TO_TRIPMINE_ND,
    ACT_SLAM_THROW_IDLE,
    ACT_SLAM_THROW_ND_IDLE,
    ACT_SLAM_THROW_THROW,
    ACT_SLAM_THROW_THROW2,
    ACT_SLAM_THROW_THROW_ND,
    ACT_SLAM_THROW_THROW_ND2,
    ACT_SLAM_THROW_DRAW,
    ACT_SLAM_THROW_ND_DRAW,
    ACT_SLAM_THROW_TO_STICKWALL,
    ACT_SLAM_THROW_TO_STICKWALL_ND,
    ACT_SLAM_THROW_DETONATE,
    ACT_SLAM_THROW_DETONATOR_HOLSTER,
    ACT_SLAM_THROW_TO_TRIPMINE_ND,
    ACT_SLAM_TRIPMINE_IDLE,
    ACT_SLAM_TRIPMINE_DRAW,
    ACT_SLAM_TRIPMINE_ATTACH,
    ACT_SLAM_TRIPMINE_ATTACH2,
    ACT_SLAM_TRIPMINE_TO_STICKWALL_ND,
    ACT_SLAM_TRIPMINE_TO_THROW_ND,
    ACT_SLAM_DETONATOR_IDLE,
    ACT_SLAM_DETONATOR_DRAW,
    ACT_SLAM_DETONATOR_DETONATE,
    ACT_SLAM_DETONATOR_HOLSTER,
    ACT_SLAM_DETONATOR_STICKWALL_DRAW,
    ACT_SLAM_DETONATOR_THROW_DRAW,

    // Shotgun Specialty Activities
    ACT_SHOTGUN_RELOAD_START,
    ACT_SHOTGUN_RELOAD_FINISH,
    ACT_SHOTGUN_PUMP,

    // SMG2 special activities
    ACT_SMG2_IDLE2,
    ACT_SMG2_FIRE2,
    ACT_SMG2_DRAW2,
    ACT_SMG2_RELOAD2,
    ACT_SMG2_DRYFIRE2,
    ACT_SMG2_TOAUTO,
    ACT_SMG2_TOBURST,

    // Physcannon special activities
    ACT_PHYSCANNON_UPGRADE,

    // weapon override activities
    ACT_RANGE_ATTACK_AR1,
    ACT_RANGE_ATTACK_AR2,
    ACT_RANGE_ATTACK_AR2_LOW,
    ACT_RANGE_ATTACK_AR2_GRENADE,
    ACT_RANGE_ATTACK_HMG1,
    ACT_RANGE_ATTACK_ML,
    ACT_RANGE_ATTACK_SMG1,
    ACT_RANGE_ATTACK_SMG1_LOW,
    ACT_RANGE_ATTACK_SMG2,
    ACT_RANGE_ATTACK_SHOTGUN,
    ACT_RANGE_ATTACK_SHOTGUN_LOW,
    ACT_RANGE_ATTACK_PISTOL,
    ACT_RANGE_ATTACK_PISTOL_LOW,
    ACT_RANGE_ATTACK_SLAM,
    ACT_RANGE_ATTACK_TRIPWIRE,
    ACT_RANGE_ATTACK_THROW,
    ACT_RANGE_ATTACK_SNIPER_RIFLE,
    ACT_RANGE_ATTACK_RPG,
    ACT_MELEE_ATTACK_SWING,

    ACT_RANGE_AIM_LOW,
    ACT_RANGE_AIM_SMG1_LOW,
    ACT_RANGE_AIM_PISTOL_LOW,
    ACT_RANGE_AIM_AR2_LOW,

    ACT_COVER_PISTOL_LOW,
    ACT_COVER_SMG1_LOW,

    // weapon override activities
    ACT_GESTURE_RANGE_ATTACK_AR1,
    ACT_GESTURE_RANGE_ATTACK_AR2,
    ACT_GESTURE_RANGE_ATTACK_AR2_GRENADE,
    ACT_GESTURE_RANGE_ATTACK_HMG1,
    ACT_GESTURE_RANGE_ATTACK_ML,
    ACT_GESTURE_RANGE_ATTACK_SMG1,
    ACT_GESTURE_RANGE_ATTACK_SMG1_LOW,
    ACT_GESTURE_RANGE_ATTACK_SMG2,
    ACT_GESTURE_RANGE_ATTACK_SHOTGUN,
    ACT_GESTURE_RANGE_ATTACK_PISTOL,
    ACT_GESTURE_RANGE_ATTACK_PISTOL_LOW,
    ACT_GESTURE_RANGE_ATTACK_SLAM,
    ACT_GESTURE_RANGE_ATTACK_TRIPWIRE,
    ACT_GESTURE_RANGE_ATTACK_THROW,
    ACT_GESTURE_RANGE_ATTACK_SNIPER_RIFLE,
    ACT_GESTURE_MELEE_ATTACK_SWING,

    ACT_IDLE_RIFLE,
    ACT_IDLE_SMG1,
    ACT_IDLE_ANGRY_SMG1,
    ACT_IDLE_PISTOL,
    ACT_IDLE_ANGRY_PISTOL,
    ACT_IDLE_ANGRY_SHOTGUN,
    ACT_IDLE_STEALTH_PISTOL,

    ACT_IDLE_PACKAGE,
    ACT_WALK_PACKAGE,
    ACT_IDLE_SUITCASE,
    ACT_WALK_SUITCASE,

    ACT_IDLE_SMG1_RELAXED,
    ACT_IDLE_SMG1_STIMULATED,
    ACT_WALK_RIFLE_RELAXED,
    ACT_RUN_RIFLE_RELAXED,
    ACT_WALK_RIFLE_STIMULATED,
    ACT_RUN_RIFLE_STIMULATED,

    ACT_IDLE_AIM_RIFLE_STIMULATED,
    ACT_WALK_AIM_RIFLE_STIMULATED,
    ACT_RUN_AIM_RIFLE_STIMULATED,

    ACT_IDLE_SHOTGUN_RELAXED,
    ACT_IDLE_SHOTGUN_STIMULATED,
    ACT_IDLE_SHOTGUN_AGITATED,

    // Policing activities
    ACT_WALK_ANGRY,
    ACT_POLICE_HARASS1,
    ACT_POLICE_HARASS2,

    // Manned guns
    ACT_IDLE_MANNEDGUN,

    // Melee weapon
    ACT_IDLE_MELEE,
    ACT_IDLE_ANGRY_MELEE,

    // RPG activities
    ACT_IDLE_RPG_RELAXED,
    ACT_IDLE_RPG,
    ACT_IDLE_ANGRY_RPG,
    ACT_COVER_LOW_RPG,
    ACT_WALK_RPG,
    ACT_RUN_RPG,
    ACT_WALK_CROUCH_RPG,
    ACT_RUN_CROUCH_RPG,
    ACT_WALK_RPG_RELAXED,
    ACT_RUN_RPG_RELAXED,

    ACT_WALK_RIFLE,
    ACT_WALK_AIM_RIFLE,
    ACT_WALK_CROUCH_RIFLE,
    ACT_WALK_CROUCH_AIM_RIFLE,
    ACT_RUN_RIFLE,
    ACT_RUN_AIM_RIFLE,
    ACT_RUN_CROUCH_RIFLE,
    ACT_RUN_CROUCH_AIM_RIFLE,
    ACT_RUN_STEALTH_PISTOL,

    ACT_WALK_AIM_SHOTGUN,
    ACT_RUN_AIM_SHOTGUN,

    ACT_WALK_PISTOL,
    ACT_RUN_PISTOL,
    ACT_WALK_AIM_PISTOL,
    ACT_RUN_AIM_PISTOL,
    ACT_WALK_STEALTH_PISTOL,
    ACT_WALK_AIM_STEALTH_PISTOL,
    ACT_RUN_AIM_STEALTH_PISTOL,

    // Reloads
    ACT_RELOAD_PISTOL,
    ACT_RELOAD_PISTOL_LOW,
    ACT_RELOAD_SMG1,
    ACT_RELOAD_SMG1_LOW,
    ACT_RELOAD_SHOTGUN,
    ACT_RELOAD_SHOTGUN_LOW,

    ACT_GESTURE_RELOAD,
    ACT_GESTURE_RELOAD_PISTOL,
    ACT_GESTURE_RELOAD_SMG1,
    ACT_GESTURE_RELOAD_SHOTGUN,

    // Busy animations
    ACT_BUSY_LEAN_LEFT,
    ACT_BUSY_LEAN_LEFT_ENTRY,
    ACT_BUSY_LEAN_LEFT_EXIT,
    ACT_BUSY_LEAN_BACK,
    ACT_BUSY_LEAN_BACK_ENTRY,
    ACT_BUSY_LEAN_BACK_EXIT,
    ACT_BUSY_SIT_GROUND,
    ACT_BUSY_SIT_GROUND_ENTRY,
    ACT_BUSY_SIT_GROUND_EXIT,
    ACT_BUSY_SIT_CHAIR,
    ACT_BUSY_SIT_CHAIR_ENTRY,
    ACT_BUSY_SIT_CHAIR_EXIT,
    ACT_BUSY_STAND,
    ACT_BUSY_QUEUE,

    // Dodge animations
    ACT_DUCK_DODGE,

    // For NPCs being lifted/eaten by barnacles:
    // being swallowed by a barnacle
    ACT_DIE_BARNACLE_SWALLOW,
    // being lifted by a barnacle
    ACT_GESTURE_BARNACLE_STRANGLE,

    ACT_PHYSCANNON_DETACH,       // An activity to be played if we're picking this up with the physcannon
    ACT_PHYSCANNON_ANIMATE,      // An activity to be played by an object being picked up with the physcannon, but has different behavior to DETACH
    ACT_PHYSCANNON_ANIMATE_PRE,  // An activity to be played by an object being picked up with the physcannon, before playing the ACT_PHYSCANNON_ANIMATE
    ACT_PHYSCANNON_ANIMATE_POST, // An activity to be played by an object being picked up with the physcannon, after playing the ACT_PHYSCANNON_ANIMATE

    ACT_DIE_FRONTSIDE,
    ACT_DIE_RIGHTSIDE,
    ACT_DIE_BACKSIDE,
    ACT_DIE_LEFTSIDE,

    ACT_OPEN_DOOR,

    // Dynamic interactions
    ACT_DI_ALYX_ZOMBIE_MELEE,
    ACT_DI_ALYX_ZOMBIE_TORSO_MELEE,
    ACT_DI_ALYX_HEADCRAB_MELEE,
    ACT_DI_ALYX_ANTLION,

    ACT_DI_ALYX_ZOMBIE_SHOTGUN64,
    ACT_DI_ALYX_ZOMBIE_SHOTGUN26,

    ACT_READINESS_RELAXED_TO_STIMULATED,
    ACT_READINESS_RELAXED_TO_STIMULATED_WALK,
    ACT_READINESS_AGITATED_TO_STIMULATED,
    ACT_READINESS_STIMULATED_TO_RELAXED,

    ACT_READINESS_PISTOL_RELAXED_TO_STIMULATED,
    ACT_READINESS_PISTOL_RELAXED_TO_STIMULATED_WALK,
    ACT_READINESS_PISTOL_AGITATED_TO_STIMULATED,
    ACT_READINESS_PISTOL_STIMULATED_TO_RELAXED,

    ACT_IDLE_CARRY,
    ACT_WALK_CARRY,

    //===========================
    // TF2 Specific Activities
    //===========================
    ACT_STARTDYING,
    ACT_DYINGLOOP,
    ACT_DYINGTODEAD,

    ACT_RIDE_MANNED_GUN,

    // All viewmodels
    ACT_VM_SPRINT_ENTER,
    ACT_VM_SPRINT_IDLE,
    ACT_VM_SPRINT_LEAVE,

    // Looping weapon firing
    ACT_FIRE_START,
    ACT_FIRE_LOOP,
    ACT_FIRE_END,

    ACT_CROUCHING_GRENADEIDLE,
    ACT_CROUCHING_GRENADEREADY,
    ACT_CROUCHING_PRIMARYATTACK,
    ACT_OVERLAY_GRENADEIDLE,
    ACT_OVERLAY_GRENADEREADY,
    ACT_OVERLAY_PRIMARYATTACK,
    ACT_OVERLAY_SHIELD_UP,
    ACT_OVERLAY_SHIELD_DOWN,
    ACT_OVERLAY_SHIELD_UP_IDLE,
    ACT_OVERLAY_SHIELD_ATTACK,
    ACT_OVERLAY_SHIELD_KNOCKBACK,
    ACT_SHIELD_UP,
    ACT_SHIELD_DOWN,
    ACT_SHIELD_UP_IDLE,
    ACT_SHIELD_ATTACK,
    ACT_SHIELD_KNOCKBACK,
    ACT_CROUCHING_SHIELD_UP,
    ACT_CROUCHING_SHIELD_DOWN,
    ACT_CROUCHING_SHIELD_UP_IDLE,
    ACT_CROUCHING_SHIELD_ATTACK,
    ACT_CROUCHING_SHIELD_KNOCKBACK,

    // turning in place
    ACT_TURNRIGHT45,
    ACT_TURNLEFT45,

    ACT_TURN,

    ACT_OBJ_ASSEMBLING,
    ACT_OBJ_DISMANTLING,
    ACT_OBJ_STARTUP,
    ACT_OBJ_RUNNING,
    ACT_OBJ_IDLE,
    ACT_OBJ_PLACING,
    ACT_OBJ_DETERIORATING,
    ACT_OBJ_UPGRADING,

    // Deploy
    ACT_DEPLOY,
    ACT_DEPLOY_IDLE,
    ACT_UNDEPLOY,

    //===========================
    // HL1 Specific Activities
    //===========================
    // Grenades
    ACT_GRENADE_ROLL,
    ACT_GRENADE_TOSS,

    // Hand grenade
    ACT_HANDGRENADE_THROW1,
    ACT_HANDGRENADE_THROW2,
    ACT_HANDGRENADE_THROW3,

    // Shotgun
    ACT_SHOTGUN_IDLE_DEEP,
    ACT_SHOTGUN_IDLE4,

    // Glock
    ACT_GLOCK_SHOOTEMPTY,
    ACT_GLOCK_SHOOT_RELOAD,

    // RPG
    ACT_RPG_DRAW_UNLOADED,
    ACT_RPG_HOLSTER_UNLOADED,
    ACT_RPG_IDLE_UNLOADED,
    ACT_RPG_FIDGET_UNLOADED,

    // Crossbow
    ACT_CROSSBOW_DRAW_UNLOADED,
    ACT_CROSSBOW_IDLE_UNLOADED,
    ACT_CROSSBOW_FIDGET_UNLOADED,

    // Gauss
    ACT_GAUSS_SPINUP,
    ACT_GAUSS_SPINCYCLE,

    // Tripmine
    ACT_TRIPMINE_GROUND,
    ACT_TRIPMINE_WORLD,

    //===========================
    // CSPort Specific Activities
    //===========================

    ACT_VM_PRIMARYATTACK_SILENCED, // fire
    ACT_VM_RELOAD_SILENCED,
    ACT_VM_DRYFIRE_SILENCED, // fire with no ammo loaded.
    ACT_VM_IDLE_SILENCED,
    ACT_VM_DRAW_SILENCED,
    ACT_VM_IDLE_EMPTY_LEFT,
    ACT_VM_DRYFIRE_LEFT,

    ACT_PLAYER_IDLE_FIRE,
    ACT_PLAYER_CROUCH_FIRE,
    ACT_PLAYER_CROUCH_WALK_FIRE,
    ACT_PLAYER_WALK_FIRE,
    ACT_PLAYER_RUN_FIRE,

    ACT_IDLETORUN,
    ACT_RUNTOIDLE,

    //===========================
    // DoD Specific Activities
    //===========================
    ACT_SPRINT,

    ACT_GET_DOWN_STAND,
    ACT_GET_UP_STAND,
    ACT_GET_DOWN_CROUCH,
    ACT_GET_UP_CROUCH,
    ACT_PRONE_FORWARD,
    ACT_PRONE_IDLE,

    ACT_DEEPIDLE1,
    ACT_DEEPIDLE2,
    ACT_DEEPIDLE3,
    ACT_DEEPIDLE4,

    ACT_VM_RELOAD_DEPLOYED,
    ACT_VM_RELOAD_IDLE,

    ACT_VM_DRAW_DEPLOYED,

    // Weapon is empty activities
    ACT_VM_DRAW_EMPTY,
    ACT_VM_PRIMARYATTACK_EMPTY,
    ACT_VM_RELOAD_EMPTY,
    ACT_VM_IDLE_EMPTY,
    ACT_VM_IDLE_DEPLOYED_EMPTY,

    ACT_VM_IDLE_8,
    ACT_VM_IDLE_7,
    ACT_VM_IDLE_6,
    ACT_VM_IDLE_5,
    ACT_VM_IDLE_4,
    ACT_VM_IDLE_3,
    ACT_VM_IDLE_2,
    ACT_VM_IDLE_1,

    ACT_VM_IDLE_DEPLOYED,
    ACT_VM_IDLE_DEPLOYED_8,
    ACT_VM_IDLE_DEPLOYED_7,
    ACT_VM_IDLE_DEPLOYED_6,
    ACT_VM_IDLE_DEPLOYED_5,
    ACT_VM_IDLE_DEPLOYED_4,
    ACT_VM_IDLE_DEPLOYED_3,
    ACT_VM_IDLE_DEPLOYED_2,
    ACT_VM_IDLE_DEPLOYED_1,

    // Animation from prone idle to standing/crouch idle. Number designates bullets left
    ACT_VM_UNDEPLOY,
    ACT_VM_UNDEPLOY_8,
    ACT_VM_UNDEPLOY_7,
    ACT_VM_UNDEPLOY_6,
    ACT_VM_UNDEPLOY_5,
    ACT_VM_UNDEPLOY_4,
    ACT_VM_UNDEPLOY_3,
    ACT_VM_UNDEPLOY_2,
    ACT_VM_UNDEPLOY_1,
    ACT_VM_UNDEPLOY_EMPTY,

    // Animation from standing/crouch idle to prone idle. Number designates bullets left
    ACT_VM_DEPLOY,
    ACT_VM_DEPLOY_8,
    ACT_VM_DEPLOY_7,
    ACT_VM_DEPLOY_6,
    ACT_VM_DEPLOY_5,
    ACT_VM_DEPLOY_4,
    ACT_VM_DEPLOY_3,
    ACT_VM_DEPLOY_2,
    ACT_VM_DEPLOY_1,
    ACT_VM_DEPLOY_EMPTY,

    // Shooting animations for standing/crouch position.  Number designates bullets left at START of animation
    ACT_VM_PRIMARYATTACK_8,
    ACT_VM_PRIMARYATTACK_7,
    ACT_VM_PRIMARYATTACK_6,
    ACT_VM_PRIMARYATTACK_5,
    ACT_VM_PRIMARYATTACK_4,
    ACT_VM_PRIMARYATTACK_3,
    ACT_VM_PRIMARYATTACK_2,
    ACT_VM_PRIMARYATTACK_1,

    // Shooting animations for prone position. Number designates bullets left at START of animation
    ACT_VM_PRIMARYATTACK_DEPLOYED,
    ACT_VM_PRIMARYATTACK_DEPLOYED_8,
    ACT_VM_PRIMARYATTACK_DEPLOYED_7,
    ACT_VM_PRIMARYATTACK_DEPLOYED_6,
    ACT_VM_PRIMARYATTACK_DEPLOYED_5,
    ACT_VM_PRIMARYATTACK_DEPLOYED_4,
    ACT_VM_PRIMARYATTACK_DEPLOYED_3,
    ACT_VM_PRIMARYATTACK_DEPLOYED_2,
    ACT_VM_PRIMARYATTACK_DEPLOYED_1,
    ACT_VM_PRIMARYATTACK_DEPLOYED_EMPTY,

    // Player anim ACTs
    ACT_DOD_DEPLOYED,
    ACT_DOD_PRONE_DEPLOYED,
    ACT_DOD_IDLE_ZOOMED,
    ACT_DOD_WALK_ZOOMED,
    ACT_DOD_CROUCH_ZOOMED,
    ACT_DOD_CROUCHWALK_ZOOMED,
    ACT_DOD_PRONE_ZOOMED,
    ACT_DOD_PRONE_FORWARD_ZOOMED,
    ACT_DOD_PRIMARYATTACK_DEPLOYED,
    ACT_DOD_PRIMARYATTACK_PRONE_DEPLOYED,
    ACT_DOD_RELOAD_DEPLOYED,
    ACT_DOD_RELOAD_PRONE_DEPLOYED,
    ACT_DOD_PRIMARYATTACK_PRONE,
    ACT_DOD_SECONDARYATTACK_PRONE,
    ACT_DOD_RELOAD_CROUCH,
    ACT_DOD_RELOAD_PRONE,
    ACT_DOD_STAND_IDLE,
    ACT_DOD_STAND_AIM,
    ACT_DOD_CROUCH_IDLE,
    ACT_DOD_CROUCH_AIM,
    ACT_DOD_CROUCHWALK_IDLE,
    ACT_DOD_CROUCHWALK_AIM,
    ACT_DOD_WALK_IDLE,
    ACT_DOD_WALK_AIM,
    ACT_DOD_RUN_IDLE,
    ACT_DOD_RUN_AIM,

    // Positions
    ACT_DOD_STAND_AIM_PISTOL,
    ACT_DOD_CROUCH_AIM_PISTOL,
    ACT_DOD_CROUCHWALK_AIM_PISTOL,
    ACT_DOD_WALK_AIM_PISTOL,
    ACT_DOD_RUN_AIM_PISTOL,
    ACT_DOD_PRONE_AIM_PISTOL,
    ACT_DOD_STAND_IDLE_PISTOL,
    ACT_DOD_CROUCH_IDLE_PISTOL,
    ACT_DOD_CROUCHWALK_IDLE_PISTOL,
    ACT_DOD_WALK_IDLE_PISTOL,
    ACT_DOD_RUN_IDLE_PISTOL,
    ACT_DOD_SPRINT_IDLE_PISTOL,
    ACT_DOD_PRONEWALK_IDLE_PISTOL,

    ACT_DOD_STAND_AIM_C96,
    ACT_DOD_CROUCH_AIM_C96,
    ACT_DOD_CROUCHWALK_AIM_C96,
    ACT_DOD_WALK_AIM_C96,
    ACT_DOD_RUN_AIM_C96,
    ACT_DOD_PRONE_AIM_C96,
    ACT_DOD_STAND_IDLE_C96,
    ACT_DOD_CROUCH_IDLE_C96,
    ACT_DOD_CROUCHWALK_IDLE_C96,
    ACT_DOD_WALK_IDLE_C96,
    ACT_DOD_RUN_IDLE_C96,
    ACT_DOD_SPRINT_IDLE_C96,
    ACT_DOD_PRONEWALK_IDLE_C96,

    ACT_DOD_STAND_AIM_RIFLE,
    ACT_DOD_CROUCH_AIM_RIFLE,
    ACT_DOD_CROUCHWALK_AIM_RIFLE,
    ACT_DOD_WALK_AIM_RIFLE,
    ACT_DOD_RUN_AIM_RIFLE,
    ACT_DOD_PRONE_AIM_RIFLE,
    ACT_DOD_STAND_IDLE_RIFLE,
    ACT_DOD_CROUCH_IDLE_RIFLE,
    ACT_DOD_CROUCHWALK_IDLE_RIFLE,
    ACT_DOD_WALK_IDLE_RIFLE,
    ACT_DOD_RUN_IDLE_RIFLE,
    ACT_DOD_SPRINT_IDLE_RIFLE,
    ACT_DOD_PRONEWALK_IDLE_RIFLE,

    ACT_DOD_STAND_AIM_BOLT,
    ACT_DOD_CROUCH_AIM_BOLT,
    ACT_DOD_CROUCHWALK_AIM_BOLT,
    ACT_DOD_WALK_AIM_BOLT,
    ACT_DOD_RUN_AIM_BOLT,
    ACT_DOD_PRONE_AIM_BOLT,
    ACT_DOD_STAND_IDLE_BOLT,
    ACT_DOD_CROUCH_IDLE_BOLT,
    ACT_DOD_CROUCHWALK_IDLE_BOLT,
    ACT_DOD_WALK_IDLE_BOLT,
    ACT_DOD_RUN_IDLE_BOLT,
    ACT_DOD_SPRINT_IDLE_BOLT,
    ACT_DOD_PRONEWALK_IDLE_BOLT,

    ACT_DOD_STAND_AIM_TOMMY,
    ACT_DOD_CROUCH_AIM_TOMMY,
    ACT_DOD_CROUCHWALK_AIM_TOMMY,
    ACT_DOD_WALK_AIM_TOMMY,
    ACT_DOD_RUN_AIM_TOMMY,
    ACT_DOD_PRONE_AIM_TOMMY,
    ACT_DOD_STAND_IDLE_TOMMY,
    ACT_DOD_CROUCH_IDLE_TOMMY,
    ACT_DOD_CROUCHWALK_IDLE_TOMMY,
    ACT_DOD_WALK_IDLE_TOMMY,
    ACT_DOD_RUN_IDLE_TOMMY,
    ACT_DOD_SPRINT_IDLE_TOMMY,
    ACT_DOD_PRONEWALK_IDLE_TOMMY,

    ACT_DOD_STAND_AIM_MP40,
    ACT_DOD_CROUCH_AIM_MP40,
    ACT_DOD_CROUCHWALK_AIM_MP40,
    ACT_DOD_WALK_AIM_MP40,
    ACT_DOD_RUN_AIM_MP40,
    ACT_DOD_PRONE_AIM_MP40,
    ACT_DOD_STAND_IDLE_MP40,
    ACT_DOD_CROUCH_IDLE_MP40,
    ACT_DOD_CROUCHWALK_IDLE_MP40,
    ACT_DOD_WALK_IDLE_MP40,
    ACT_DOD_RUN_IDLE_MP40,
    ACT_DOD_SPRINT_IDLE_MP40,
    ACT_DOD_PRONEWALK_IDLE_MP40,

    ACT_DOD_STAND_AIM_MP44,
    ACT_DOD_CROUCH_AIM_MP44,
    ACT_DOD_CROUCHWALK_AIM_MP44,
    ACT_DOD_WALK_AIM_MP44,
    ACT_DOD_RUN_AIM_MP44,
    ACT_DOD_PRONE_AIM_MP44,
    ACT_DOD_STAND_IDLE_MP44,
    ACT_DOD_CROUCH_IDLE_MP44,
    ACT_DOD_CROUCHWALK_IDLE_MP44,
    ACT_DOD_WALK_IDLE_MP44,
    ACT_DOD_RUN_IDLE_MP44,
    ACT_DOD_SPRINT_IDLE_MP44,
    ACT_DOD_PRONEWALK_IDLE_MP44,

    ACT_DOD_STAND_AIM_GREASE,
    ACT_DOD_CROUCH_AIM_GREASE,
    ACT_DOD_CROUCHWALK_AIM_GREASE,
    ACT_DOD_WALK_AIM_GREASE,
    ACT_DOD_RUN_AIM_GREASE,
    ACT_DOD_PRONE_AIM_GREASE,
    ACT_DOD_STAND_IDLE_GREASE,
    ACT_DOD_CROUCH_IDLE_GREASE,
    ACT_DOD_CROUCHWALK_IDLE_GREASE,
    ACT_DOD_WALK_IDLE_GREASE,
    ACT_DOD_RUN_IDLE_GREASE,
    ACT_DOD_SPRINT_IDLE_GREASE,
    ACT_DOD_PRONEWALK_IDLE_GREASE,

    ACT_DOD_STAND_AIM_MG,
    ACT_DOD_CROUCH_AIM_MG,
    ACT_DOD_CROUCHWALK_AIM_MG,
    ACT_DOD_WALK_AIM_MG,
    ACT_DOD_RUN_AIM_MG,
    ACT_DOD_PRONE_AIM_MG,
    ACT_DOD_STAND_IDLE_MG,
    ACT_DOD_CROUCH_IDLE_MG,
    ACT_DOD_CROUCHWALK_IDLE_MG,
    ACT_DOD_WALK_IDLE_MG,
    ACT_DOD_RUN_IDLE_MG,
    ACT_DOD_SPRINT_IDLE_MG,
    ACT_DOD_PRONEWALK_IDLE_MG,

    ACT_DOD_STAND_AIM_30CAL,
    ACT_DOD_CROUCH_AIM_30CAL,
    ACT_DOD_CROUCHWALK_AIM_30CAL,
    ACT_DOD_WALK_AIM_30CAL,
    ACT_DOD_RUN_AIM_30CAL,
    ACT_DOD_PRONE_AIM_30CAL,
    ACT_DOD_STAND_IDLE_30CAL,
    ACT_DOD_CROUCH_IDLE_30CAL,
    ACT_DOD_CROUCHWALK_IDLE_30CAL,
    ACT_DOD_WALK_IDLE_30CAL,
    ACT_DOD_RUN_IDLE_30CAL,
    ACT_DOD_SPRINT_IDLE_30CAL,
    ACT_DOD_PRONEWALK_IDLE_30CAL,

    ACT_DOD_STAND_AIM_GREN_FRAG,
    ACT_DOD_CROUCH_AIM_GREN_FRAG,
    ACT_DOD_CROUCHWALK_AIM_GREN_FRAG,
    ACT_DOD_WALK_AIM_GREN_FRAG,
    ACT_DOD_RUN_AIM_GREN_FRAG,
    ACT_DOD_PRONE_AIM_GREN_FRAG,
    ACT_DOD_SPRINT_AIM_GREN_FRAG,
    ACT_DOD_PRONEWALK_AIM_GREN_FRAG,
    ACT_DOD_STAND_AIM_GREN_STICK,
    ACT_DOD_CROUCH_AIM_GREN_STICK,
    ACT_DOD_CROUCHWALK_AIM_GREN_STICK,
    ACT_DOD_WALK_AIM_GREN_STICK,
    ACT_DOD_RUN_AIM_GREN_STICK,
    ACT_DOD_PRONE_AIM_GREN_STICK,
    ACT_DOD_SPRINT_AIM_GREN_STICK,
    ACT_DOD_PRONEWALK_AIM_GREN_STICK,

    ACT_DOD_STAND_AIM_KNIFE,
    ACT_DOD_CROUCH_AIM_KNIFE,
    ACT_DOD_CROUCHWALK_AIM_KNIFE,
    ACT_DOD_WALK_AIM_KNIFE,
    ACT_DOD_RUN_AIM_KNIFE,
    ACT_DOD_PRONE_AIM_KNIFE,
    ACT_DOD_SPRINT_AIM_KNIFE,
    ACT_DOD_PRONEWALK_AIM_KNIFE,

    ACT_DOD_STAND_AIM_SPADE,
    ACT_DOD_CROUCH_AIM_SPADE,
    ACT_DOD_CROUCHWALK_AIM_SPADE,
    ACT_DOD_WALK_AIM_SPADE,
    ACT_DOD_RUN_AIM_SPADE,
    ACT_DOD_PRONE_AIM_SPADE,
    ACT_DOD_SPRINT_AIM_SPADE,
    ACT_DOD_PRONEWALK_AIM_SPADE,

    ACT_DOD_STAND_AIM_BAZOOKA,
    ACT_DOD_CROUCH_AIM_BAZOOKA,
    ACT_DOD_CROUCHWALK_AIM_BAZOOKA,
    ACT_DOD_WALK_AIM_BAZOOKA,
    ACT_DOD_RUN_AIM_BAZOOKA,
    ACT_DOD_PRONE_AIM_BAZOOKA,
    ACT_DOD_STAND_IDLE_BAZOOKA,
    ACT_DOD_CROUCH_IDLE_BAZOOKA,
    ACT_DOD_CROUCHWALK_IDLE_BAZOOKA,
    ACT_DOD_WALK_IDLE_BAZOOKA,
    ACT_DOD_RUN_IDLE_BAZOOKA,
    ACT_DOD_SPRINT_IDLE_BAZOOKA,
    ACT_DOD_PRONEWALK_IDLE_BAZOOKA,

    ACT_DOD_STAND_AIM_PSCHRECK,
    ACT_DOD_CROUCH_AIM_PSCHRECK,
    ACT_DOD_CROUCHWALK_AIM_PSCHRECK,
    ACT_DOD_WALK_AIM_PSCHRECK,
    ACT_DOD_RUN_AIM_PSCHRECK,
    ACT_DOD_PRONE_AIM_PSCHRECK,
    ACT_DOD_STAND_IDLE_PSCHRECK,
    ACT_DOD_CROUCH_IDLE_PSCHRECK,
    ACT_DOD_CROUCHWALK_IDLE_PSCHRECK,
    ACT_DOD_WALK_IDLE_PSCHRECK,
    ACT_DOD_RUN_IDLE_PSCHRECK,
    ACT_DOD_SPRINT_IDLE_PSCHRECK,
    ACT_DOD_PRONEWALK_IDLE_PSCHRECK,

    ACT_DOD_STAND_AIM_BAR,
    ACT_DOD_CROUCH_AIM_BAR,
    ACT_DOD_CROUCHWALK_AIM_BAR,
    ACT_DOD_WALK_AIM_BAR,
    ACT_DOD_RUN_AIM_BAR,
    ACT_DOD_PRONE_AIM_BAR,
    ACT_DOD_STAND_IDLE_BAR,
    ACT_DOD_CROUCH_IDLE_BAR,
    ACT_DOD_CROUCHWALK_IDLE_BAR,
    ACT_DOD_WALK_IDLE_BAR,
    ACT_DOD_RUN_IDLE_BAR,
    ACT_DOD_SPRINT_IDLE_BAR,
    ACT_DOD_PRONEWALK_IDLE_BAR,

    // Zoomed aims
    ACT_DOD_STAND_ZOOM_RIFLE,
    ACT_DOD_CROUCH_ZOOM_RIFLE,
    ACT_DOD_CROUCHWALK_ZOOM_RIFLE,
    ACT_DOD_WALK_ZOOM_RIFLE,
    ACT_DOD_RUN_ZOOM_RIFLE,
    ACT_DOD_PRONE_ZOOM_RIFLE,

    ACT_DOD_STAND_ZOOM_BOLT,
    ACT_DOD_CROUCH_ZOOM_BOLT,
    ACT_DOD_CROUCHWALK_ZOOM_BOLT,
    ACT_DOD_WALK_ZOOM_BOLT,
    ACT_DOD_RUN_ZOOM_BOLT,
    ACT_DOD_PRONE_ZOOM_BOLT,

    ACT_DOD_STAND_ZOOM_BAZOOKA,
    ACT_DOD_CROUCH_ZOOM_BAZOOKA,
    ACT_DOD_CROUCHWALK_ZOOM_BAZOOKA,
    ACT_DOD_WALK_ZOOM_BAZOOKA,
    ACT_DOD_RUN_ZOOM_BAZOOKA,
    ACT_DOD_PRONE_ZOOM_BAZOOKA,

    ACT_DOD_STAND_ZOOM_PSCHRECK,
    ACT_DOD_CROUCH_ZOOM_PSCHRECK,
    ACT_DOD_CROUCHWALK_ZOOM_PSCHRECK,
    ACT_DOD_WALK_ZOOM_PSCHRECK,
    ACT_DOD_RUN_ZOOM_PSCHRECK,
    ACT_DOD_PRONE_ZOOM_PSCHRECK,

    // Deployed Aim
    ACT_DOD_DEPLOY_RIFLE,
    ACT_DOD_DEPLOY_TOMMY,
    ACT_DOD_DEPLOY_MG,
    ACT_DOD_DEPLOY_30CAL,

    // Prone Deployed Aim
    ACT_DOD_PRONE_DEPLOY_RIFLE,
    ACT_DOD_PRONE_DEPLOY_TOMMY,
    ACT_DOD_PRONE_DEPLOY_MG,
    ACT_DOD_PRONE_DEPLOY_30CAL,

    // Attacks

    // Rifle
    ACT_DOD_PRIMARYATTACK_RIFLE,
    ACT_DOD_SECONDARYATTACK_RIFLE,
    ACT_DOD_PRIMARYATTACK_PRONE_RIFLE,
    ACT_DOD_SECONDARYATTACK_PRONE_RIFLE,
    ACT_DOD_PRIMARYATTACK_PRONE_DEPLOYED_RIFLE,
    ACT_DOD_PRIMARYATTACK_DEPLOYED_RIFLE,

    // Bolt
    ACT_DOD_PRIMARYATTACK_BOLT,
    ACT_DOD_SECONDARYATTACK_BOLT,
    ACT_DOD_PRIMARYATTACK_PRONE_BOLT,
    ACT_DOD_SECONDARYATTACK_PRONE_BOLT,

    // Tommy
    ACT_DOD_PRIMARYATTACK_TOMMY,
    ACT_DOD_PRIMARYATTACK_PRONE_TOMMY,
    ACT_DOD_SECONDARYATTACK_TOMMY,
    ACT_DOD_SECONDARYATTACK_PRONE_TOMMY,

    // MP40
    ACT_DOD_PRIMARYATTACK_MP40,
    ACT_DOD_PRIMARYATTACK_PRONE_MP40,
    ACT_DOD_SECONDARYATTACK_MP40,
    ACT_DOD_SECONDARYATTACK_PRONE_MP40,

    // MP44
    ACT_DOD_PRIMARYATTACK_MP44,
    ACT_DOD_PRIMARYATTACK_PRONE_MP44,

    // Greasegun
    ACT_DOD_PRIMARYATTACK_GREASE,
    ACT_DOD_PRIMARYATTACK_PRONE_GREASE,

    // Pistols (Colt, Luger)
    ACT_DOD_PRIMARYATTACK_PISTOL,
    ACT_DOD_PRIMARYATTACK_PRONE_PISTOL,
    ACT_DOD_PRIMARYATTACK_C96,
    ACT_DOD_PRIMARYATTACK_PRONE_C96,

    // Mgs (mg42, mg34)
    ACT_DOD_PRIMARYATTACK_MG,
    ACT_DOD_PRIMARYATTACK_PRONE_MG,
    ACT_DOD_PRIMARYATTACK_PRONE_DEPLOYED_MG,
    ACT_DOD_PRIMARYATTACK_DEPLOYED_MG,

    // 30cal
    ACT_DOD_PRIMARYATTACK_30CAL,
    ACT_DOD_PRIMARYATTACK_PRONE_30CAL,
    ACT_DOD_PRIMARYATTACK_DEPLOYED_30CAL,
    ACT_DOD_PRIMARYATTACK_PRONE_DEPLOYED_30CAL,

    // Grenades
    ACT_DOD_PRIMARYATTACK_GREN_FRAG,
    ACT_DOD_PRIMARYATTACK_PRONE_GREN_FRAG,
    ACT_DOD_PRIMARYATTACK_GREN_STICK,
    ACT_DOD_PRIMARYATTACK_PRONE_GREN_STICK,

    // Knife
    ACT_DOD_PRIMARYATTACK_KNIFE,
    ACT_DOD_PRIMARYATTACK_PRONE_KNIFE,

    // Spade
    ACT_DOD_PRIMARYATTACK_SPADE,
    ACT_DOD_PRIMARYATTACK_PRONE_SPADE,

    // Bazooka
    ACT_DOD_PRIMARYATTACK_BAZOOKA,
    ACT_DOD_PRIMARYATTACK_PRONE_BAZOOKA,

    // Pschreck
    ACT_DOD_PRIMARYATTACK_PSCHRECK,
    ACT_DOD_PRIMARYATTACK_PRONE_PSCHRECK,

    // Bar
    ACT_DOD_PRIMARYATTACK_BAR,
    ACT_DOD_PRIMARYATTACK_PRONE_BAR,

    // Reloads
    ACT_DOD_RELOAD_GARAND,
    ACT_DOD_RELOAD_K43,
    ACT_DOD_RELOAD_BAR,
    ACT_DOD_RELOAD_MP40,
    ACT_DOD_RELOAD_MP44,
    ACT_DOD_RELOAD_BOLT,
    ACT_DOD_RELOAD_M1CARBINE,
    ACT_DOD_RELOAD_TOMMY,
    ACT_DOD_RELOAD_GREASEGUN,
    ACT_DOD_RELOAD_PISTOL,
    ACT_DOD_RELOAD_FG42,
    ACT_DOD_RELOAD_RIFLE,
    ACT_DOD_RELOAD_RIFLEGRENADE,
    ACT_DOD_RELOAD_C96,

    // Crouch
    ACT_DOD_RELOAD_CROUCH_BAR,
    ACT_DOD_RELOAD_CROUCH_RIFLE,
    ACT_DOD_RELOAD_CROUCH_RIFLEGRENADE,
    ACT_DOD_RELOAD_CROUCH_BOLT,
    ACT_DOD_RELOAD_CROUCH_MP44,
    ACT_DOD_RELOAD_CROUCH_MP40,
    ACT_DOD_RELOAD_CROUCH_TOMMY,
    ACT_DOD_RELOAD_CROUCH_BAZOOKA,
    ACT_DOD_RELOAD_CROUCH_PSCHRECK,
    ACT_DOD_RELOAD_CROUCH_PISTOL,
    ACT_DOD_RELOAD_CROUCH_M1CARBINE,
    ACT_DOD_RELOAD_CROUCH_C96,

    // Bazookas
    ACT_DOD_RELOAD_BAZOOKA,
    ACT_DOD_ZOOMLOAD_BAZOOKA,
    ACT_DOD_RELOAD_PSCHRECK,
    ACT_DOD_ZOOMLOAD_PSCHRECK,

    // Deployed
    ACT_DOD_RELOAD_DEPLOYED_FG42,
    ACT_DOD_RELOAD_DEPLOYED_30CAL,
    ACT_DOD_RELOAD_DEPLOYED_MG,
    ACT_DOD_RELOAD_DEPLOYED_MG34,
    ACT_DOD_RELOAD_DEPLOYED_BAR,

    // Prone
    ACT_DOD_RELOAD_PRONE_PISTOL,
    ACT_DOD_RELOAD_PRONE_GARAND,
    ACT_DOD_RELOAD_PRONE_M1CARBINE,
    ACT_DOD_RELOAD_PRONE_BOLT,
    ACT_DOD_RELOAD_PRONE_K43,
    ACT_DOD_RELOAD_PRONE_MP40,
    ACT_DOD_RELOAD_PRONE_MP44,
    ACT_DOD_RELOAD_PRONE_BAR,
    ACT_DOD_RELOAD_PRONE_GREASEGUN,
    ACT_DOD_RELOAD_PRONE_TOMMY,
    ACT_DOD_RELOAD_PRONE_FG42,
    ACT_DOD_RELOAD_PRONE_RIFLE,
    ACT_DOD_RELOAD_PRONE_RIFLEGRENADE,
    ACT_DOD_RELOAD_PRONE_C96,

    // Prone bazooka
    ACT_DOD_RELOAD_PRONE_BAZOOKA,
    ACT_DOD_ZOOMLOAD_PRONE_BAZOOKA,
    ACT_DOD_RELOAD_PRONE_PSCHRECK,
    ACT_DOD_ZOOMLOAD_PRONE_PSCHRECK,

    // Prone deployed
    ACT_DOD_RELOAD_PRONE_DEPLOYED_BAR,
    ACT_DOD_RELOAD_PRONE_DEPLOYED_FG42,
    ACT_DOD_RELOAD_PRONE_DEPLOYED_30CAL,
    ACT_DOD_RELOAD_PRONE_DEPLOYED_MG,
    ACT_DOD_RELOAD_PRONE_DEPLOYED_MG34,

    // Prone zoomed aim
    ACT_DOD_PRONE_ZOOM_FORWARD_RIFLE,
    ACT_DOD_PRONE_ZOOM_FORWARD_BOLT,
    ACT_DOD_PRONE_ZOOM_FORWARD_BAZOOKA,
    ACT_DOD_PRONE_ZOOM_FORWARD_PSCHRECK,

    // Crouch attack
    ACT_DOD_PRIMARYATTACK_CROUCH,
    ACT_DOD_PRIMARYATTACK_CROUCH_SPADE,
    ACT_DOD_PRIMARYATTACK_CROUCH_KNIFE,
    ACT_DOD_PRIMARYATTACK_CROUCH_GREN_FRAG,
    ACT_DOD_PRIMARYATTACK_CROUCH_GREN_STICK,
    ACT_DOD_SECONDARYATTACK_CROUCH,
    ACT_DOD_SECONDARYATTACK_CROUCH_TOMMY,
    ACT_DOD_SECONDARYATTACK_CROUCH_MP40,

    // Hand Signals
    ACT_DOD_HS_IDLE,
    ACT_DOD_HS_CROUCH,
    ACT_DOD_HS_IDLE_30CAL,
    ACT_DOD_HS_IDLE_BAZOOKA,
    ACT_DOD_HS_IDLE_PSCHRECK,
    ACT_DOD_HS_IDLE_KNIFE,
    ACT_DOD_HS_IDLE_MG42,
    ACT_DOD_HS_IDLE_PISTOL,
    ACT_DOD_HS_IDLE_STICKGRENADE,
    ACT_DOD_HS_IDLE_TOMMY,
    ACT_DOD_HS_IDLE_MP44,
    ACT_DOD_HS_IDLE_K98,
    ACT_DOD_HS_CROUCH_30CAL,
    ACT_DOD_HS_CROUCH_BAZOOKA,
    ACT_DOD_HS_CROUCH_PSCHRECK,
    ACT_DOD_HS_CROUCH_KNIFE,
    ACT_DOD_HS_CROUCH_MG42,
    ACT_DOD_HS_CROUCH_PISTOL,
    ACT_DOD_HS_CROUCH_STICKGRENADE,
    ACT_DOD_HS_CROUCH_TOMMY,
    ACT_DOD_HS_CROUCH_MP44,
    ACT_DOD_HS_CROUCH_K98,

    ACT_DOD_STAND_IDLE_TNT,
    ACT_DOD_CROUCH_IDLE_TNT,
    ACT_DOD_CROUCHWALK_IDLE_TNT,
    ACT_DOD_WALK_IDLE_TNT,
    ACT_DOD_RUN_IDLE_TNT,
    ACT_DOD_SPRINT_IDLE_TNT,
    ACT_DOD_PRONEWALK_IDLE_TNT,

    ACT_DOD_PLANT_TNT,
    ACT_DOD_DEFUSE_TNT,

    // HL2MP
    ACT_HL2MP_IDLE,
    ACT_HL2MP_RUN,
    ACT_HL2MP_IDLE_CROUCH,
    ACT_HL2MP_WALK_CROUCH,
    ACT_HL2MP_GESTURE_RANGE_ATTACK,
    ACT_HL2MP_GESTURE_RELOAD,
    ACT_HL2MP_JUMP,

    ACT_HL2MP_IDLE_PISTOL,
    ACT_HL2MP_RUN_PISTOL,
    ACT_HL2MP_IDLE_CROUCH_PISTOL,
    ACT_HL2MP_WALK_CROUCH_PISTOL,
    ACT_HL2MP_GESTURE_RANGE_ATTACK_PISTOL,
    ACT_HL2MP_GESTURE_RELOAD_PISTOL,
    ACT_HL2MP_JUMP_PISTOL,

    ACT_HL2MP_IDLE_SMG1,
    ACT_HL2MP_RUN_SMG1,
    ACT_HL2MP_IDLE_CROUCH_SMG1,
    ACT_HL2MP_WALK_CROUCH_SMG1,
    ACT_HL2MP_GESTURE_RANGE_ATTACK_SMG1,
    ACT_HL2MP_GESTURE_RELOAD_SMG1,
    ACT_HL2MP_JUMP_SMG1,

    ACT_HL2MP_IDLE_AR2,
    ACT_HL2MP_RUN_AR2,
    ACT_HL2MP_IDLE_CROUCH_AR2,
    ACT_HL2MP_WALK_CROUCH_AR2,
    ACT_HL2MP_GESTURE_RANGE_ATTACK_AR2,
    ACT_HL2MP_GESTURE_RELOAD_AR2,
    ACT_HL2MP_JUMP_AR2,

    ACT_HL2MP_IDLE_SHOTGUN,
    ACT_HL2MP_RUN_SHOTGUN,
    ACT_HL2MP_IDLE_CROUCH_SHOTGUN,
    ACT_HL2MP_WALK_CROUCH_SHOTGUN,
    ACT_HL2MP_GESTURE_RANGE_ATTACK_SHOTGUN,
    ACT_HL2MP_GESTURE_RELOAD_SHOTGUN,
    ACT_HL2MP_JUMP_SHOTGUN,

    ACT_HL2MP_IDLE_RPG,
    ACT_HL2MP_RUN_RPG,
    ACT_HL2MP_IDLE_CROUCH_RPG,
    ACT_HL2MP_WALK_CROUCH_RPG,
    ACT_HL2MP_GESTURE_RANGE_ATTACK_RPG,
    ACT_HL2MP_GESTURE_RELOAD_RPG,
    ACT_HL2MP_JUMP_RPG,

    ACT_HL2MP_IDLE_GRENADE,
    ACT_HL2MP_RUN_GRENADE,
    ACT_HL2MP_IDLE_CROUCH_GRENADE,
    ACT_HL2MP_WALK_CROUCH_GRENADE,
    ACT_HL2MP_GESTURE_RANGE_ATTACK_GRENADE,
    ACT_HL2MP_GESTURE_RELOAD_GRENADE,
    ACT_HL2MP_JUMP_GRENADE,

    ACT_HL2MP_IDLE_PHYSGUN,
    ACT_HL2MP_RUN_PHYSGUN,
    ACT_HL2MP_IDLE_CROUCH_PHYSGUN,
    ACT_HL2MP_WALK_CROUCH_PHYSGUN,
    ACT_HL2MP_GESTURE_RANGE_ATTACK_PHYSGUN,
    ACT_HL2MP_GESTURE_RELOAD_PHYSGUN,
    ACT_HL2MP_JUMP_PHYSGUN,

    ACT_HL2MP_IDLE_CROSSBOW,
    ACT_HL2MP_RUN_CROSSBOW,
    ACT_HL2MP_IDLE_CROUCH_CROSSBOW,
    ACT_HL2MP_WALK_CROUCH_CROSSBOW,
    ACT_HL2MP_GESTURE_RANGE_ATTACK_CROSSBOW,
    ACT_HL2MP_GESTURE_RELOAD_CROSSBOW,
    ACT_HL2MP_JUMP_CROSSBOW,

    ACT_HL2MP_IDLE_MELEE,
    ACT_HL2MP_RUN_MELEE,
    ACT_HL2MP_IDLE_CROUCH_MELEE,
    ACT_HL2MP_WALK_CROUCH_MELEE,
    ACT_HL2MP_GESTURE_RANGE_ATTACK_MELEE,
    ACT_HL2MP_GESTURE_RELOAD_MELEE,
    ACT_HL2MP_JUMP_MELEE,

    ACT_HL2MP_IDLE_SLAM,
    ACT_HL2MP_RUN_SLAM,
    ACT_HL2MP_IDLE_CROUCH_SLAM,
    ACT_HL2MP_WALK_CROUCH_SLAM,
    ACT_HL2MP_GESTURE_RANGE_ATTACK_SLAM,
    ACT_HL2MP_GESTURE_RELOAD_SLAM,
    ACT_HL2MP_JUMP_SLAM,

    // Portal!
    ACT_VM_FIZZLE,

    // Multiplayer
    ACT_MP_STAND_IDLE,
    ACT_MP_CROUCH_IDLE,
    ACT_MP_CROUCH_DEPLOYED_IDLE,
    ACT_MP_CROUCH_DEPLOYED,
    ACT_MP_CROUCHWALK_DEPLOYED,
    ACT_MP_DEPLOYED_IDLE,
    ACT_MP_RUN,
    ACT_MP_WALK,
    ACT_MP_AIRWALK,
    ACT_MP_CROUCHWALK,
    ACT_MP_SPRINT,
    ACT_MP_JUMP,
    ACT_MP_JUMP_START,
    ACT_MP_JUMP_FLOAT,
    ACT_MP_JUMP_LAND,
    ACT_MP_DOUBLEJUMP,
    ACT_MP_SWIM,
    ACT_MP_DEPLOYED,
    ACT_MP_SWIM_DEPLOYED,
    ACT_MP_VCD,

    ACT_MP_ATTACK_STAND_PRIMARYFIRE,
    ACT_MP_ATTACK_STAND_PRIMARYFIRE_DEPLOYED,
    ACT_MP_ATTACK_STAND_SECONDARYFIRE,
    ACT_MP_ATTACK_STAND_GRENADE,
    ACT_MP_ATTACK_CROUCH_PRIMARYFIRE,
    ACT_MP_ATTACK_CROUCH_PRIMARYFIRE_DEPLOYED,
    ACT_MP_ATTACK_CROUCH_SECONDARYFIRE,
    ACT_MP_ATTACK_CROUCH_GRENADE,
    ACT_MP_ATTACK_SWIM_PRIMARYFIRE,
    ACT_MP_ATTACK_SWIM_SECONDARYFIRE,
    ACT_MP_ATTACK_SWIM_GRENADE,
    ACT_MP_ATTACK_AIRWALK_PRIMARYFIRE,
    ACT_MP_ATTACK_AIRWALK_SECONDARYFIRE,
    ACT_MP_ATTACK_AIRWALK_GRENADE,
    ACT_MP_RELOAD_STAND,
    ACT_MP_RELOAD_STAND_LOOP,
    ACT_MP_RELOAD_STAND_END,
    ACT_MP_RELOAD_CROUCH,
    ACT_MP_RELOAD_CROUCH_LOOP,
    ACT_MP_RELOAD_CROUCH_END,
    ACT_MP_RELOAD_SWIM,
    ACT_MP_RELOAD_SWIM_LOOP,
    ACT_MP_RELOAD_SWIM_END,
    ACT_MP_RELOAD_AIRWALK,
    ACT_MP_RELOAD_AIRWALK_LOOP,
    ACT_MP_RELOAD_AIRWALK_END,
    ACT_MP_ATTACK_STAND_PREFIRE,
    ACT_MP_ATTACK_STAND_POSTFIRE,
    ACT_MP_ATTACK_STAND_STARTFIRE,
    ACT_MP_ATTACK_CROUCH_PREFIRE,
    ACT_MP_ATTACK_CROUCH_POSTFIRE,
    ACT_MP_ATTACK_SWIM_PREFIRE,
    ACT_MP_ATTACK_SWIM_POSTFIRE,

    // Multiplayer - Primary
    ACT_MP_STAND_PRIMARY,
    ACT_MP_CROUCH_PRIMARY,
    ACT_MP_RUN_PRIMARY,
    ACT_MP_WALK_PRIMARY,
    ACT_MP_AIRWALK_PRIMARY,
    ACT_MP_CROUCHWALK_PRIMARY,
    ACT_MP_JUMP_PRIMARY,
    ACT_MP_JUMP_START_PRIMARY,
    ACT_MP_JUMP_FLOAT_PRIMARY,
    ACT_MP_JUMP_LAND_PRIMARY,
    ACT_MP_SWIM_PRIMARY,
    ACT_MP_DEPLOYED_PRIMARY,
    ACT_MP_SWIM_DEPLOYED_PRIMARY,
    ACT_MP_CROUCHWALK_DEPLOYED_PRIMARY,
    ACT_MP_CROUCH_DEPLOYED_IDLE_PRIMARY,

    ACT_MP_ATTACK_STAND_PRIMARY, // RUN, WALK
    ACT_MP_ATTACK_STAND_PRIMARY_DEPLOYED,
    ACT_MP_ATTACK_CROUCH_PRIMARY, // CROUCHWALK
    ACT_MP_ATTACK_CROUCH_PRIMARY_DEPLOYED,
    ACT_MP_ATTACK_SWIM_PRIMARY,
    ACT_MP_ATTACK_AIRWALK_PRIMARY,

    ACT_MP_RELOAD_STAND_PRIMARY, // RUN, WALK
    ACT_MP_RELOAD_STAND_PRIMARY_LOOP,
    ACT_MP_RELOAD_STAND_PRIMARY_END,
    ACT_MP_RELOAD_CROUCH_PRIMARY, // CROUCHWALK
    ACT_MP_RELOAD_CROUCH_PRIMARY_LOOP,
    ACT_MP_RELOAD_CROUCH_PRIMARY_END,
    ACT_MP_RELOAD_SWIM_PRIMARY,
    ACT_MP_RELOAD_SWIM_PRIMARY_LOOP,
    ACT_MP_RELOAD_SWIM_PRIMARY_END,
    ACT_MP_RELOAD_AIRWALK_PRIMARY,
    ACT_MP_RELOAD_AIRWALK_PRIMARY_LOOP,
    ACT_MP_RELOAD_AIRWALK_PRIMARY_END,

    ACT_MP_RELOAD_STAND_PRIMARY_2,
    ACT_MP_RELOAD_STAND_PRIMARY_LOOP_2,
    ACT_MP_RELOAD_STAND_PRIMARY_END_2,
    ACT_MP_RELOAD_CROUCH_PRIMARY_2,
    ACT_MP_RELOAD_CROUCH_PRIMARY_LOOP_2,
    ACT_MP_RELOAD_CROUCH_PRIMARY_END_2,
    ACT_MP_RELOAD_SWIM_PRIMARY_2,
    ACT_MP_RELOAD_SWIM_PRIMARY_LOOP_2,
    ACT_MP_RELOAD_SWIM_PRIMARY_END_2,
    ACT_MP_RELOAD_AIRWALK_PRIMARY_2,
    ACT_MP_RELOAD_AIRWALK_PRIMARY_LOOP_2,
    ACT_MP_RELOAD_AIRWALK_PRIMARY_END_2,

    // PRIMARY ALT
    ACT_MP_ATTACK_STAND_PRIMARY_ALT,
    ACT_MP_ATTACK_CROUCH_PRIMARY_ALT,
    ACT_MP_ATTACK_SWIM_PRIMARY_ALT,
    ACT_MP_RELOAD_STAND_PRIMARY_ALT,
    ACT_MP_RELOAD_CROUCH_PRIMARY_ALT,
    ACT_MP_RELOAD_AIRWALK_PRIMARY_ALT,
    ACT_MP_RELOAD_STAND_PRIMARY_LOOP_ALT,
    ACT_MP_RELOAD_CROUCH_PRIMARY_LOOP_ALT,
    ACT_MP_RELOAD_AIRWALK_PRIMARY_LOOP_ALT,
    ACT_MP_RELOAD_STAND_PRIMARY_END_ALT,
    ACT_MP_RELOAD_CROUCH_PRIMARY_END_ALT,
    ACT_MP_RELOAD_AIRWALK_PRIMARY_END_ALT,
    ACT_MP_RELOAD_SWIM_PRIMARY_ALT,
    ACT_MP_ATTACK_STAND_PRIMARY_SUPER,
    ACT_MP_ATTACK_CROUCH_PRIMARY_SUPER,
    ACT_MP_ATTACK_SWIM_PRIMARY_SUPER,

    ACT_MP_ATTACK_STAND_GRENADE_PRIMARY,  // RUN, WALK
    ACT_MP_ATTACK_CROUCH_GRENADE_PRIMARY, // CROUCHWALK
    ACT_MP_ATTACK_SWIM_GRENADE_PRIMARY,
    ACT_MP_ATTACK_AIRWALK_GRENADE_PRIMARY,

    // Secondary
    ACT_MP_STAND_SECONDARY,
    ACT_MP_CROUCH_SECONDARY,
    ACT_MP_RUN_SECONDARY,
    ACT_MP_WALK_SECONDARY,
    ACT_MP_AIRWALK_SECONDARY,
    ACT_MP_CROUCHWALK_SECONDARY,
    ACT_MP_JUMP_SECONDARY,
    ACT_MP_JUMP_START_SECONDARY,
    ACT_MP_JUMP_FLOAT_SECONDARY,
    ACT_MP_JUMP_LAND_SECONDARY,
    ACT_MP_SWIM_SECONDARY,

    ACT_MP_ATTACK_STAND_SECONDARY,  // RUN, WALK
    ACT_MP_ATTACK_CROUCH_SECONDARY, // CROUCHWALK
    ACT_MP_ATTACK_SWIM_SECONDARY,
    ACT_MP_ATTACK_AIRWALK_SECONDARY,

    ACT_MP_RELOAD_STAND_SECONDARY, // RUN, WALK
    ACT_MP_RELOAD_STAND_SECONDARY_LOOP,
    ACT_MP_RELOAD_STAND_SECONDARY_END,
    ACT_MP_RELOAD_CROUCH_SECONDARY, // CROUCHWALK
    ACT_MP_RELOAD_CROUCH_SECONDARY_LOOP,
    ACT_MP_RELOAD_CROUCH_SECONDARY_END,
    ACT_MP_RELOAD_SWIM_SECONDARY,
    ACT_MP_RELOAD_SWIM_SECONDARY_LOOP,
    ACT_MP_RELOAD_SWIM_SECONDARY_END,
    ACT_MP_RELOAD_AIRWALK_SECONDARY,
    ACT_MP_RELOAD_AIRWALK_SECONDARY_LOOP,
    ACT_MP_RELOAD_AIRWALK_SECONDARY_END,

    ACT_MP_RELOAD_STAND_SECONDARY_2,
    ACT_MP_RELOAD_CROUCH_SECONDARY_2,
    ACT_MP_RELOAD_SWIM_SECONDARY_2,
    ACT_MP_RELOAD_AIRWALK_SECONDARY_2,

    ACT_MP_ATTACK_STAND_GRENADE_SECONDARY,  // RUN, WALK
    ACT_MP_ATTACK_CROUCH_GRENADE_SECONDARY, // CROUCHWALK
    ACT_MP_ATTACK_SWIM_GRENADE_SECONDARY,
    ACT_MP_ATTACK_AIRWALK_GRENADE_SECONDARY,

    // Secondary2
    ACT_MP_STAND_SECONDARY2,
    ACT_MP_CROUCH_SECONDARY2,
    ACT_MP_RUN_SECONDARY2,
    ACT_MP_WALK_SECONDARY2,
    ACT_MP_AIRWALK_SECONDARY2,
    ACT_MP_CROUCHWALK_SECONDARY2,
    ACT_MP_JUMP_SECONDARY2,
    ACT_MP_JUMP_START_SECONDARY2,
    ACT_MP_JUMP_FLOAT_SECONDARY2,
    ACT_MP_JUMP_LAND_SECONDARY2,
    ACT_MP_SWIM_SECONDARY2,

    ACT_MP_ATTACK_STAND_SECONDARY2,  // RUN, WALK
    ACT_MP_ATTACK_CROUCH_SECONDARY2, // CROUCHWALK
    ACT_MP_ATTACK_SWIM_SECONDARY2,
    ACT_MP_ATTACK_AIRWALK_SECONDARY2,

    ACT_MP_RELOAD_STAND_SECONDARY2, // RUN, WALK
    ACT_MP_RELOAD_STAND_SECONDARY2_LOOP,
    ACT_MP_RELOAD_STAND_SECONDARY2_END,
    ACT_MP_RELOAD_CROUCH_SECONDARY2, // CROUCHWALK
    ACT_MP_RELOAD_CROUCH_SECONDARY2_LOOP,
    ACT_MP_RELOAD_CROUCH_SECONDARY2_END,
    ACT_MP_RELOAD_SWIM_SECONDARY2,
    ACT_MP_RELOAD_SWIM_SECONDARY2_LOOP,
    ACT_MP_RELOAD_SWIM_SECONDARY2_END,
    ACT_MP_RELOAD_AIRWALK_SECONDARY2,
    ACT_MP_RELOAD_AIRWALK_SECONDARY2_LOOP,
    ACT_MP_RELOAD_AIRWALK_SECONDARY2_END,

    // Melee
    ACT_MP_STAND_MELEE,
    ACT_MP_CROUCH_MELEE,
    ACT_MP_RUN_MELEE,
    ACT_MP_WALK_MELEE,
    ACT_MP_AIRWALK_MELEE,
    ACT_MP_CROUCHWALK_MELEE,
    ACT_MP_JUMP_MELEE,
    ACT_MP_JUMP_START_MELEE,
    ACT_MP_JUMP_FLOAT_MELEE,
    ACT_MP_JUMP_LAND_MELEE,
    ACT_MP_SWIM_MELEE,

    ACT_MP_ATTACK_STAND_MELEE, // RUN, WALK
    ACT_MP_ATTACK_STAND_MELEE_SECONDARY,
    ACT_MP_ATTACK_CROUCH_MELEE, // CROUCHWALK
    ACT_MP_ATTACK_CROUCH_MELEE_SECONDARY,
    ACT_MP_ATTACK_SWIM_MELEE,
    ACT_MP_ATTACK_AIRWALK_MELEE,

    ACT_MP_ATTACK_STAND_GRENADE_MELEE,  // RUN, WALK
    ACT_MP_ATTACK_CROUCH_GRENADE_MELEE, // CROUCHWALK
    ACT_MP_ATTACK_SWIM_GRENADE_MELEE,
    ACT_MP_ATTACK_AIRWALK_GRENADE_MELEE,

    // Item1
    ACT_MP_STAND_ITEM1,
    ACT_MP_CROUCH_ITEM1,
    ACT_MP_RUN_ITEM1,
    ACT_MP_WALK_ITEM1,
    ACT_MP_AIRWALK_ITEM1,
    ACT_MP_CROUCHWALK_ITEM1,
    ACT_MP_JUMP_ITEM1,
    ACT_MP_JUMP_START_ITEM1,
    ACT_MP_JUMP_FLOAT_ITEM1,
    ACT_MP_JUMP_LAND_ITEM1,
    ACT_MP_SWIM_ITEM1,

    ACT_MP_ATTACK_STAND_ITEM1, // RUN, WALK
    ACT_MP_ATTACK_STAND_ITEM1_SECONDARY,
    ACT_MP_ATTACK_CROUCH_ITEM1, // CROUCHWALK
    ACT_MP_ATTACK_CROUCH_ITEM1_SECONDARY,
    ACT_MP_ATTACK_SWIM_ITEM1,
    ACT_MP_ATTACK_AIRWALK_ITEM1,

    ACT_MP_DEPLOYED_ITEM1,
    ACT_MP_DEPLOYED_IDLE_ITEM1,
    ACT_MP_CROUCHWALK_DEPLOYED_ITEM1,
    ACT_MP_CROUCH_DEPLOYED_IDLE_ITEM1,
    ACT_MP_ATTACK_STAND_PRIMARY_DEPLOYED_ITEM1,
    ACT_MP_ATTACK_CROUCH_PRIMARY_DEPLOYED_ITEM1,

    // Item2
    ACT_MP_STAND_ITEM2,
    ACT_MP_CROUCH_ITEM2,
    ACT_MP_RUN_ITEM2,
    ACT_MP_WALK_ITEM2,
    ACT_MP_AIRWALK_ITEM2,
    ACT_MP_CROUCHWALK_ITEM2,
    ACT_MP_JUMP_ITEM2,
    ACT_MP_JUMP_START_ITEM2,
    ACT_MP_JUMP_FLOAT_ITEM2,
    ACT_MP_JUMP_LAND_ITEM2,
    ACT_MP_SWIM_ITEM2,

    ACT_MP_ATTACK_STAND_ITEM2, // RUN, WALK
    ACT_MP_ATTACK_STAND_ITEM2_SECONDARY,
    ACT_MP_ATTACK_CROUCH_ITEM2, // CROUCHWALK
    ACT_MP_ATTACK_CROUCH_ITEM2_SECONDARY,
    ACT_MP_ATTACK_SWIM_ITEM2,
    ACT_MP_ATTACK_AIRWALK_ITEM2,

    ACT_MP_ATTACK_STAND_HARD_ITEM2,
    ACT_MP_ATTACK_CROUCH_HARD_ITEM2,
    ACT_MP_ATTACK_SWIM_HARD_ITEM2,

    ACT_MP_DEPLOYED_ITEM2,
    ACT_MP_DEPLOYED_IDLE_ITEM2,
    ACT_MP_CROUCHWALK_DEPLOYED_ITEM2,
    ACT_MP_CROUCH_DEPLOYED_IDLE_ITEM2,
    ACT_MP_ATTACK_STAND_PRIMARY_DEPLOYED_ITEM2,
    ACT_MP_ATTACK_CROUCH_PRIMARY_DEPLOYED_ITEM2,

    ACT_MP_RELOAD_STAND_ITEM2, // RUN, WALK
    ACT_MP_RELOAD_STAND_ITEM2_LOOP,
    ACT_MP_RELOAD_STAND_ITEM2_END,
    ACT_MP_RELOAD_CROUCH_ITEM2, // CROUCHWALK
    ACT_MP_RELOAD_CROUCH_ITEM2_LOOP,
    ACT_MP_RELOAD_CROUCH_ITEM2_END,
    ACT_MP_RELOAD_SWIM_ITEM2,
    ACT_MP_RELOAD_SWIM_ITEM2_LOOP,
    ACT_MP_RELOAD_SWIM_ITEM2_END,
    ACT_MP_RELOAD_AIRWALK_ITEM2,
    ACT_MP_RELOAD_AIRWALK_ITEM2_LOOP,
    ACT_MP_RELOAD_AIRWALK_ITEM2_END,
    ACT_MP_RELOAD_NO_AMMO_ITEM2,

    ACT_MP_ATTACK_STAND_GRENADE_ITEM2,  // RUN, WALK
    ACT_MP_ATTACK_CROUCH_GRENADE_ITEM2, // CROUCHWALK
    ACT_MP_ATTACK_SWIM_GRENADE_ITEM2,
    ACT_MP_ATTACK_AIRWALK_GRENADE_ITEM2,

    // Item3
    ACT_MP_STAND_ITEM3,
    ACT_MP_CROUCH_ITEM3,
    ACT_MP_RUN_ITEM3,
    ACT_MP_WALK_ITEM3,
    ACT_MP_AIRWALK_ITEM3,
    ACT_MP_CROUCHWALK_ITEM3,
    ACT_MP_JUMP_ITEM3,
    ACT_MP_JUMP_START_ITEM3,
    ACT_MP_JUMP_FLOAT_ITEM3,
    ACT_MP_JUMP_LAND_ITEM3,
    ACT_MP_SWIM_ITEM3,

    ACT_MP_ATTACK_STAND_ITEM3, // RUN, WALK
    ACT_MP_ATTACK_STAND_ITEM3_SECONDARY,
    ACT_MP_ATTACK_CROUCH_ITEM3, // CROUCHWALK
    ACT_MP_ATTACK_CROUCH_ITEM3_SECONDARY,
    ACT_MP_ATTACK_SWIM_ITEM3,
    ACT_MP_ATTACK_AIRWALK_ITEM3,

    ACT_MP_DEPLOYED_ITEM3,
    ACT_MP_DEPLOYED_IDLE_ITEM3,
    ACT_MP_CROUCHWALK_DEPLOYED_ITEM3,
    ACT_MP_CROUCH_DEPLOYED_IDLE_ITEM3,
    ACT_MP_ATTACK_STAND_PRIMARY_DEPLOYED_ITEM3,
    ACT_MP_ATTACK_CROUCH_PRIMARY_DEPLOYED_ITEM3,

    // Item4
    ACT_MP_STAND_ITEM4,
    ACT_MP_CROUCH_ITEM4,
    ACT_MP_RUN_ITEM4,
    ACT_MP_WALK_ITEM4,
    ACT_MP_AIRWALK_ITEM4,
    ACT_MP_CROUCHWALK_ITEM4,
    ACT_MP_JUMP_ITEM4,
    ACT_MP_JUMP_START_ITEM4,
    ACT_MP_JUMP_FLOAT_ITEM4,
    ACT_MP_JUMP_LAND_ITEM4,
    ACT_MP_SWIM_ITEM4,

    ACT_MP_ATTACK_STAND_ITEM4, // RUN, WALK
    ACT_MP_ATTACK_STAND_ITEM4_SECONDARY,
    ACT_MP_ATTACK_CROUCH_ITEM4, // CROUCHWALK
    ACT_MP_ATTACK_CROUCH_ITEM4_SECONDARY,
    ACT_MP_ATTACK_SWIM_ITEM4,
    ACT_MP_ATTACK_AIRWALK_ITEM4,

    ACT_MP_DEPLOYED_ITEM4,
    ACT_MP_DEPLOYED_IDLE_ITEM4,
    ACT_MP_CROUCHWALK_DEPLOYED_ITEM4,
    ACT_MP_CROUCH_DEPLOYED_IDLE_ITEM4,
    ACT_MP_ATTACK_STAND_PRIMARY_DEPLOYED_ITEM4,
    ACT_MP_ATTACK_CROUCH_PRIMARY_DEPLOYED_ITEM4,

    // Passtime
    ACT_MP_STAND_PASSTIME,
    ACT_MP_RUN_PASSTIME,
    ACT_MP_CROUCHWALK_PASSTIME,

    // Flinches
    ACT_MP_GESTURE_FLINCH,
    ACT_MP_GESTURE_FLINCH_PRIMARY,
    ACT_MP_GESTURE_FLINCH_SECONDARY,
    ACT_MP_GESTURE_FLINCH_MELEE,
    ACT_MP_GESTURE_FLINCH_ITEM1,
    ACT_MP_GESTURE_FLINCH_ITEM2,

    ACT_MP_GESTURE_FLINCH_HEAD,
    ACT_MP_GESTURE_FLINCH_CHEST,
    ACT_MP_GESTURE_FLINCH_STOMACH,
    ACT_MP_GESTURE_FLINCH_LEFTARM,
    ACT_MP_GESTURE_FLINCH_RIGHTARM,
    ACT_MP_GESTURE_FLINCH_LEFTLEG,
    ACT_MP_GESTURE_FLINCH_RIGHTLEG,

    // Team Fortress specific - medic heal, medic infect, etc.....
    ACT_MP_GRENADE1_DRAW,
    ACT_MP_GRENADE1_IDLE,
    ACT_MP_GRENADE1_ATTACK,
    ACT_MP_GRENADE2_DRAW,
    ACT_MP_GRENADE2_IDLE,
    ACT_MP_GRENADE2_ATTACK,

    ACT_MP_PRIMARY_GRENADE1_DRAW,
    ACT_MP_PRIMARY_GRENADE1_IDLE,
    ACT_MP_PRIMARY_GRENADE1_ATTACK,
    ACT_MP_PRIMARY_GRENADE2_DRAW,
    ACT_MP_PRIMARY_GRENADE2_IDLE,
    ACT_MP_PRIMARY_GRENADE2_ATTACK,

    ACT_MP_SECONDARY_GRENADE1_DRAW,
    ACT_MP_SECONDARY_GRENADE1_IDLE,
    ACT_MP_SECONDARY_GRENADE1_ATTACK,
    ACT_MP_SECONDARY_GRENADE2_DRAW,
    ACT_MP_SECONDARY_GRENADE2_IDLE,
    ACT_MP_SECONDARY_GRENADE2_ATTACK,

    ACT_MP_MELEE_GRENADE1_DRAW,
    ACT_MP_MELEE_GRENADE1_IDLE,
    ACT_MP_MELEE_GRENADE1_ATTACK,
    ACT_MP_MELEE_GRENADE2_DRAW,
    ACT_MP_MELEE_GRENADE2_IDLE,
    ACT_MP_MELEE_GRENADE2_ATTACK,

    ACT_MP_ITEM1_GRENADE1_DRAW,
    ACT_MP_ITEM1_GRENADE1_IDLE,
    ACT_MP_ITEM1_GRENADE1_ATTACK,
    ACT_MP_ITEM1_GRENADE2_DRAW,
    ACT_MP_ITEM1_GRENADE2_IDLE,
    ACT_MP_ITEM1_GRENADE2_ATTACK,

    ACT_MP_ITEM2_GRENADE1_DRAW,
    ACT_MP_ITEM2_GRENADE1_IDLE,
    ACT_MP_ITEM2_GRENADE1_ATTACK,
    ACT_MP_ITEM2_GRENADE2_DRAW,
    ACT_MP_ITEM2_GRENADE2_IDLE,
    ACT_MP_ITEM2_GRENADE2_ATTACK,

    // Building
    ACT_MP_STAND_BUILDING,
    ACT_MP_CROUCH_BUILDING,
    ACT_MP_RUN_BUILDING,
    ACT_MP_WALK_BUILDING,
    ACT_MP_AIRWALK_BUILDING,
    ACT_MP_CROUCHWALK_BUILDING,
    ACT_MP_JUMP_BUILDING,
    ACT_MP_JUMP_START_BUILDING,
    ACT_MP_JUMP_FLOAT_BUILDING,
    ACT_MP_JUMP_LAND_BUILDING,
    ACT_MP_SWIM_BUILDING,

    ACT_MP_ATTACK_STAND_BUILDING,  // RUN, WALK
    ACT_MP_ATTACK_CROUCH_BUILDING, // CROUCHWALK
    ACT_MP_ATTACK_SWIM_BUILDING,
    ACT_MP_ATTACK_AIRWALK_BUILDING,

    ACT_MP_ATTACK_STAND_GRENADE_BUILDING,  // RUN, WALK
    ACT_MP_ATTACK_CROUCH_GRENADE_BUILDING, // CROUCHWALK
    ACT_MP_ATTACK_SWIM_GRENADE_BUILDING,
    ACT_MP_ATTACK_AIRWALK_GRENADE_BUILDING,

    // Building
    ACT_MP_STAND_BUILDING_DEPLOYED,
    ACT_MP_CROUCH_BUILDING_DEPLOYED,
    ACT_MP_RUN_BUILDING_DEPLOYED,
    ACT_MP_WALK_BUILDING_DEPLOYED,
    ACT_MP_AIRWALK_BUILDING_DEPLOYED,
    ACT_MP_CROUCHWALK_BUILDING_DEPLOYED,
    ACT_MP_JUMP_BUILDING_DEPLOYED,
    ACT_MP_JUMP_START_BUILDING_DEPLOYED,
    ACT_MP_JUMP_FLOAT_BUILDING_DEPLOYED,
    ACT_MP_JUMP_LAND_BUILDING_DEPLOYED,
    ACT_MP_SWIM_BUILDING_DEPLOYED,

    ACT_MP_ATTACK_STAND_BUILDING_DEPLOYED,  // RUN, WALK
    ACT_MP_ATTACK_CROUCH_BUILDING_DEPLOYED, // CROUCHWALK
    ACT_MP_ATTACK_SWIM_BUILDING_DEPLOYED,
    ACT_MP_ATTACK_AIRWALK_BUILDING_DEPLOYED,

    ACT_MP_ATTACK_STAND_GRENADE_BUILDING_DEPLOYED,  // RUN, WALK
    ACT_MP_ATTACK_CROUCH_GRENADE_BUILDING_DEPLOYED, // CROUCHWALK
    ACT_MP_ATTACK_SWIM_GRENADE_BUILDING_DEPLOYED,
    ACT_MP_ATTACK_AIRWALK_GRENADE_BUILDING_DEPLOYED,

    ACT_MP_STAND_PDA,
    ACT_MP_CROUCH_PDA,
    ACT_MP_RUN_PDA,
    ACT_MP_WALK_PDA,
    ACT_MP_AIRWALK_PDA,
    ACT_MP_CROUCHWALK_PDA,
    ACT_MP_JUMP_PDA,
    ACT_MP_JUMP_START_PDA,
    ACT_MP_JUMP_FLOAT_PDA,
    ACT_MP_JUMP_LAND_PDA,
    ACT_MP_SWIM_PDA,

    ACT_MP_ATTACK_STAND_PDA,
    ACT_MP_ATTACK_SWIM_PDA,

    ACT_MP_STAND_LOSERSTATE,
    ACT_MP_CROUCH_LOSERSTATE,
    ACT_MP_RUN_LOSERSTATE,
    ACT_MP_WALK_LOSERSTATE,
    ACT_MP_AIRWALK_LOSERSTATE,
    ACT_MP_CROUCHWALK_LOSERSTATE,
    ACT_MP_JUMP_LOSERSTATE,
    ACT_MP_JUMP_START_LOSERSTATE,
    ACT_MP_JUMP_FLOAT_LOSERSTATE,
    ACT_MP_JUMP_LAND_LOSERSTATE,
    ACT_MP_SWIM_LOSERSTATE,
    ACT_MP_DOUBLEJUMP_LOSERSTATE,

    ACT_MP_DOUBLEJUMP_CROUCH,
    ACT_MP_DOUBLEJUMP_CROUCH_PRIMARY,
    ACT_MP_DOUBLEJUMP_CROUCH_SECONDARY,
    ACT_MP_DOUBLEJUMP_CROUCH_MELEE,
    ACT_MP_DOUBLEJUMP_CROUCH_ITEM1,
    ACT_MP_DOUBLEJUMP_CROUCH_ITEM2,
    ACT_MP_DOUBLEJUMP_CROUCH_LOSERSTATE,
    ACT_MP_DOUBLEJUMP_CROUCH_PASSTIME,

    ACT_MP_GESTURE_VC_HANDMOUTH,
    ACT_MP_GESTURE_VC_FINGERPOINT,
    ACT_MP_GESTURE_VC_FISTPUMP,
    ACT_MP_GESTURE_VC_THUMBSUP,
    ACT_MP_GESTURE_VC_NODYES,
    ACT_MP_GESTURE_VC_NODNO,

    ACT_MP_GESTURE_VC_HANDMOUTH_PRIMARY,
    ACT_MP_GESTURE_VC_FINGERPOINT_PRIMARY,
    ACT_MP_GESTURE_VC_FISTPUMP_PRIMARY,
    ACT_MP_GESTURE_VC_THUMBSUP_PRIMARY,
    ACT_MP_GESTURE_VC_NODYES_PRIMARY,
    ACT_MP_GESTURE_VC_NODNO_PRIMARY,

    ACT_MP_GESTURE_VC_HANDMOUTH_SECONDARY,
    ACT_MP_GESTURE_VC_FINGERPOINT_SECONDARY,
    ACT_MP_GESTURE_VC_FISTPUMP_SECONDARY,
    ACT_MP_GESTURE_VC_THUMBSUP_SECONDARY,
    ACT_MP_GESTURE_VC_NODYES_SECONDARY,
    ACT_MP_GESTURE_VC_NODNO_SECONDARY,

    ACT_MP_GESTURE_VC_HANDMOUTH_MELEE,
    ACT_MP_GESTURE_VC_FINGERPOINT_MELEE,
    ACT_MP_GESTURE_VC_FISTPUMP_MELEE,
    ACT_MP_GESTURE_VC_THUMBSUP_MELEE,
    ACT_MP_GESTURE_VC_NODYES_MELEE,
    ACT_MP_GESTURE_VC_NODNO_MELEE,

    ACT_MP_GESTURE_VC_HANDMOUTH_ITEM1,
    ACT_MP_GESTURE_VC_FINGERPOINT_ITEM1,
    ACT_MP_GESTURE_VC_FISTPUMP_ITEM1,
    ACT_MP_GESTURE_VC_THUMBSUP_ITEM1,
    ACT_MP_GESTURE_VC_NODYES_ITEM1,
    ACT_MP_GESTURE_VC_NODNO_ITEM1,

    ACT_MP_GESTURE_VC_HANDMOUTH_ITEM2,
    ACT_MP_GESTURE_VC_FINGERPOINT_ITEM2,
    ACT_MP_GESTURE_VC_FISTPUMP_ITEM2,
    ACT_MP_GESTURE_VC_THUMBSUP_ITEM2,
    ACT_MP_GESTURE_VC_NODYES_ITEM2,
    ACT_MP_GESTURE_VC_NODNO_ITEM2,

    ACT_MP_GESTURE_VC_HANDMOUTH_BUILDING,
    ACT_MP_GESTURE_VC_FINGERPOINT_BUILDING,
    ACT_MP_GESTURE_VC_FISTPUMP_BUILDING,
    ACT_MP_GESTURE_VC_THUMBSUP_BUILDING,
    ACT_MP_GESTURE_VC_NODYES_BUILDING,
    ACT_MP_GESTURE_VC_NODNO_BUILDING,

    ACT_MP_GESTURE_VC_HANDMOUTH_PDA,
    ACT_MP_GESTURE_VC_FINGERPOINT_PDA,
    ACT_MP_GESTURE_VC_FISTPUMP_PDA,
    ACT_MP_GESTURE_VC_THUMBSUP_PDA,
    ACT_MP_GESTURE_VC_NODYES_PDA,
    ACT_MP_GESTURE_VC_NODNO_PDA,

    ACT_MP_STUN_BEGIN,
    ACT_MP_STUN_MIDDLE,
    ACT_MP_STUN_END,

    ACT_MP_PASSTIME_THROW_BEGIN,
    ACT_MP_PASSTIME_THROW_MIDDLE,
    ACT_MP_PASSTIME_THROW_END,

    ACT_VM_UNUSABLE,
    ACT_VM_UNUSABLE_TO_USABLE,
    ACT_VM_USABLE_TO_UNUSABLE,

    // Specific viewmodel activities for weapon roles
    ACT_PRIMARY_VM_DRAW,
    ACT_PRIMARY_VM_HOLSTER,
    ACT_PRIMARY_VM_IDLE,
    ACT_PRIMARY_VM_PULLBACK,
    ACT_PRIMARY_VM_PRIMARYATTACK,
    ACT_PRIMARY_VM_SECONDARYATTACK,
    ACT_PRIMARY_VM_RELOAD,
    ACT_PRIMARY_RELOAD_START,
    ACT_PRIMARY_RELOAD_FINISH,
    ACT_PRIMARY_VM_DRYFIRE,
    ACT_PRIMARY_VM_IDLE_TO_LOWERED,
    ACT_PRIMARY_VM_IDLE_LOWERED,
    ACT_PRIMARY_VM_LOWERED_TO_IDLE,
    ACT_PRIMARY_VM_RELOAD_2,
    ACT_PRIMARY_RELOAD_START_2,
    ACT_PRIMARY_RELOAD_FINISH_2,
    ACT_PRIMARY_VM_RELOAD_3,
    ACT_PRIMARY_RELOAD_START_3,
    ACT_PRIMARY_RELOAD_FINISH_3,
    ACT_PRIMARY_VM_PRIMARYATTACK_3,

    ACT_SECONDARY_VM_DRAW,
    ACT_SECONDARY_VM_HOLSTER,
    ACT_SECONDARY_VM_IDLE,
    ACT_SECONDARY_VM_PULLBACK,
    ACT_SECONDARY_VM_PRIMARYATTACK,
    ACT_SECONDARY_VM_SECONDARYATTACK,
    ACT_SECONDARY_VM_RELOAD,
    ACT_SECONDARY_RELOAD_START,
    ACT_SECONDARY_RELOAD_FINISH,
    ACT_SECONDARY_VM_RELOAD2,
    ACT_SECONDARY_VM_DRYFIRE,
    ACT_SECONDARY_VM_IDLE_TO_LOWERED,
    ACT_SECONDARY_VM_IDLE_LOWERED,
    ACT_SECONDARY_VM_LOWERED_TO_IDLE,

    ACT_SECONDARY_VM_DRAW_2,
    ACT_SECONDARY_VM_IDLE_2,
    ACT_SECONDARY_VM_PRIMARYATTACK_2,
    ACT_SECONDARY_VM_RELOAD_2,

    ACT_MELEE_VM_DRAW,
    ACT_MELEE_VM_HOLSTER,
    ACT_MELEE_VM_IDLE,
    ACT_MELEE_VM_PULLBACK,
    ACT_MELEE_VM_PRIMARYATTACK,
    ACT_MELEE_VM_SECONDARYATTACK,
    ACT_MELEE_VM_RELOAD,
    ACT_MELEE_VM_DRYFIRE,
    ACT_MELEE_VM_IDLE_TO_LOWERED,
    ACT_MELEE_VM_IDLE_LOWERED,
    ACT_MELEE_VM_LOWERED_TO_IDLE,
    ACT_MELEE_VM_STUN,
    ACT_MELEE_VM_HITCENTER,
    ACT_MELEE_VM_SWINGHARD,

    ACT_PDA_VM_DRAW,
    ACT_PDA_VM_HOLSTER,
    ACT_PDA_VM_IDLE,
    ACT_PDA_VM_PULLBACK,
    ACT_PDA_VM_PRIMARYATTACK,
    ACT_PDA_VM_SECONDARYATTACK,
    ACT_PDA_VM_RELOAD,
    ACT_PDA_VM_DRYFIRE,
    ACT_PDA_VM_IDLE_TO_LOWERED,
    ACT_PDA_VM_IDLE_LOWERED,
    ACT_PDA_VM_LOWERED_TO_IDLE,

    ACT_ENGINEER_PDA1_VM_DRAW,
    ACT_ENGINEER_PDA2_VM_DRAW,
    ACT_ENGINEER_BLD_VM_DRAW,
    ACT_ENGINEER_PDA1_VM_IDLE,
    ACT_ENGINEER_PDA2_VM_IDLE,
    ACT_ENGINEER_BLD_VM_IDLE,

    ACT_ITEM1_VM_DRAW,
    ACT_ITEM1_VM_HOLSTER,
    ACT_ITEM1_VM_IDLE,
    ACT_ITEM1_VM_IDLE_2,
    ACT_ITEM1_VM_PULLBACK,
    ACT_ITEM1_VM_PRIMARYATTACK,
    ACT_ITEM1_VM_SECONDARYATTACK,
    ACT_ITEM1_VM_RELOAD,
    ACT_ITEM1_VM_DRYFIRE,
    ACT_ITEM1_VM_IDLE_TO_LOWERED,
    ACT_ITEM1_VM_IDLE_LOWERED,
    ACT_ITEM1_VM_LOWERED_TO_IDLE,
    ACT_ITEM1_RELOAD_START,
    ACT_ITEM1_RELOAD_FINISH,
    ACT_ITEM1_VM_HITCENTER,
    ACT_ITEM1_VM_SWINGHARD,
    ACT_ITEM1_BACKSTAB_VM_UP,
    ACT_ITEM1_BACKSTAB_VM_DOWN,
    ACT_ITEM1_BACKSTAB_VM_IDLE,
    ACT_MELEE_VM_ITEM1_STUN,

    ACT_ITEM2_VM_DRAW,
    ACT_ITEM2_VM_HOLSTER,
    ACT_ITEM2_VM_IDLE,
    ACT_ITEM2_VM_PULLBACK,
    ACT_ITEM2_VM_PRIMARYATTACK,
    ACT_ITEM2_VM_SECONDARYATTACK,
    ACT_ITEM2_VM_RELOAD,
    ACT_ITEM2_VM_DRYFIRE,
    ACT_ITEM2_VM_IDLE_TO_LOWERED,
    ACT_ITEM2_VM_IDLE_LOWERED,
    ACT_ITEM2_VM_LOWERED_TO_IDLE,
    ACT_ITEM2_VM_CHARGE,
    ACT_ITEM2_VM_IDLE_2,
    ACT_ITEM2_VM_IDLE_3,
    ACT_ITEM2_VM_CHARGE_IDLE_3,
    ACT_ITEM2_VM_HITCENTER,
    ACT_ITEM2_VM_SWINGHARD,
    ACT_ITEM2_BACKSTAB_VM_UP,
    ACT_ITEM2_BACKSTAB_VM_DOWN,
    ACT_ITEM2_BACKSTAB_VM_IDLE,
    ACT_MELEE_VM_ITEM2_STUN,

    ACT_ITEM3_VM_DRAW,
    ACT_ITEM3_VM_HOLSTER,
    ACT_ITEM3_VM_IDLE,
    ACT_ITEM3_VM_PULLBACK,
    ACT_ITEM3_VM_PRIMARYATTACK,
    ACT_ITEM3_VM_SECONDARYATTACK,
    ACT_ITEM3_VM_RELOAD,
    ACT_ITEM3_VM_DRYFIRE,
    ACT_ITEM3_VM_IDLE_TO_LOWERED,
    ACT_ITEM3_VM_IDLE_LOWERED,
    ACT_ITEM3_VM_LOWERED_TO_IDLE,
    ACT_ITEM3_VM_CHARGE,
    ACT_ITEM3_VM_IDLE_2,
    ACT_ITEM3_VM_IDLE_3,
    ACT_ITEM3_VM_CHARGE_IDLE_3,
    ACT_ITEM3_VM_HITCENTER,
    ACT_ITEM3_VM_SWINGHARD,

    ACT_ITEM4_VM_DRAW,
    ACT_ITEM4_VM_HOLSTER,
    ACT_ITEM4_VM_IDLE,
    ACT_ITEM4_VM_PULLBACK,
    ACT_ITEM4_VM_PRIMARYATTACK,
    ACT_ITEM4_VM_SECONDARYATTACK,
    ACT_ITEM4_VM_RELOAD,
    ACT_ITEM4_VM_DRYFIRE,
    ACT_ITEM4_VM_IDLE_TO_LOWERED,
    ACT_ITEM4_VM_IDLE_LOWERED,
    ACT_ITEM4_VM_LOWERED_TO_IDLE,
    ACT_ITEM4_VM_CHARGE,
    ACT_ITEM4_VM_IDLE_2,
    ACT_ITEM4_VM_IDLE_3,
    ACT_ITEM4_VM_CHARGE_IDLE_3,
    ACT_ITEM4_VM_HITCENTER,
    ACT_ITEM4_VM_SWINGHARD,

    ACT_SECONDARY2_VM_DRAW,
    ACT_SECONDARY2_VM_HOLSTER,
    ACT_SECONDARY2_VM_IDLE,
    ACT_SECONDARY2_VM_PULLBACK,
    ACT_SECONDARY2_VM_PRIMARYATTACK,
    ACT_SECONDARY2_VM_SECONDARY2ATTACK,
    ACT_SECONDARY2_VM_RELOAD,
    ACT_SECONDARY2_RELOAD_START,
    ACT_SECONDARY2_RELOAD_FINISH,
    ACT_SECONDARY2_VM_RELOAD2,
    ACT_SECONDARY2_VM_DRYFIRE,
    ACT_SECONDARY2_VM_IDLE_TO_LOWERED,
    ACT_SECONDARY2_VM_IDLE_LOWERED,
    ACT_SECONDARY2_VM_LOWERED_TO_IDLE,

    ACT_BACKSTAB_VM_UP,
    ACT_BACKSTAB_VM_DOWN,
    ACT_BACKSTAB_VM_IDLE,

    ACT_PRIMARY_ATTACK_STAND_PREFIRE,
    ACT_PRIMARY_ATTACK_STAND_POSTFIRE,
    ACT_PRIMARY_ATTACK_STAND_STARTFIRE,
    ACT_PRIMARY_ATTACK_CROUCH_PREFIRE,
    ACT_PRIMARY_ATTACK_CROUCH_POSTFIRE,
    ACT_PRIMARY_ATTACK_SWIM_PREFIRE,
    ACT_PRIMARY_ATTACK_SWIM_POSTFIRE,

    ACT_SECONDARY_ATTACK_STAND_PREFIRE,
    ACT_SECONDARY_ATTACK_STAND_POSTFIRE,
    ACT_SECONDARY_ATTACK_STAND_STARTFIRE,
    ACT_SECONDARY_ATTACK_CROUCH_PREFIRE,
    ACT_SECONDARY_ATTACK_CROUCH_POSTFIRE,
    ACT_SECONDARY_ATTACK_SWIM_PREFIRE,
    ACT_SECONDARY_ATTACK_SWIM_POSTFIRE,

    ACT_MELEE_ATTACK_STAND_PREFIRE,
    ACT_MELEE_ATTACK_STAND_POSTFIRE,
    ACT_MELEE_ATTACK_STAND_STARTFIRE,
    ACT_MELEE_ATTACK_CROUCH_PREFIRE,
    ACT_MELEE_ATTACK_CROUCH_POSTFIRE,
    ACT_MELEE_ATTACK_SWIM_PREFIRE,
    ACT_MELEE_ATTACK_SWIM_POSTFIRE,

    ACT_ITEM1_ATTACK_STAND_PREFIRE,
    ACT_ITEM1_ATTACK_STAND_POSTFIRE,
    ACT_ITEM1_ATTACK_STAND_STARTFIRE,
    ACT_ITEM1_ATTACK_CROUCH_PREFIRE,
    ACT_ITEM1_ATTACK_CROUCH_POSTFIRE,
    ACT_ITEM1_ATTACK_SWIM_PREFIRE,
    ACT_ITEM1_ATTACK_SWIM_POSTFIRE,

    ACT_ITEM2_ATTACK_STAND_PREFIRE,
    ACT_ITEM2_ATTACK_STAND_POSTFIRE,
    ACT_ITEM2_ATTACK_STAND_STARTFIRE,
    ACT_ITEM2_ATTACK_CROUCH_PREFIRE,
    ACT_ITEM2_ATTACK_CROUCH_POSTFIRE,
    ACT_ITEM2_ATTACK_SWIM_PREFIRE,
    ACT_ITEM2_ATTACK_SWIM_POSTFIRE,

    ACT_ITEM3_ATTACK_STAND_PREFIRE,
    ACT_ITEM3_ATTACK_STAND_POSTFIRE,
    ACT_ITEM3_ATTACK_STAND_STARTFIRE,
    ACT_ITEM3_ATTACK_CROUCH_PREFIRE,
    ACT_ITEM3_ATTACK_CROUCH_POSTFIRE,
    ACT_ITEM3_ATTACK_SWIM_PREFIRE,
    ACT_ITEM3_ATTACK_SWIM_POSTFIRE,

    ACT_ITEM4_ATTACK_STAND_PREFIRE,
    ACT_ITEM4_ATTACK_STAND_POSTFIRE,
    ACT_ITEM4_ATTACK_STAND_STARTFIRE,
    ACT_ITEM4_ATTACK_CROUCH_PREFIRE,
    ACT_ITEM4_ATTACK_CROUCH_POSTFIRE,
    ACT_ITEM4_ATTACK_SWIM_PREFIRE,
    ACT_ITEM4_ATTACK_SWIM_POSTFIRE,

    ACT_MP_STAND_MELEE_ALLCLASS,
    ACT_MP_CROUCH_MELEE_ALLCLASS,
    ACT_MP_RUN_MELEE_ALLCLASS,
    ACT_MP_WALK_MELEE_ALLCLASS,
    ACT_MP_AIRWALK_MELEE_ALLCLASS,
    ACT_MP_CROUCHWALK_MELEE_ALLCLASS,
    ACT_MP_JUMP_MELEE_ALLCLASS,
    ACT_MP_JUMP_START_MELEE_ALLCLASS,
    ACT_MP_JUMP_FLOAT_MELEE_ALLCLASS,
    ACT_MP_JUMP_LAND_MELEE_ALLCLASS,
    ACT_MP_SWIM_MELEE_ALLCLASS,

    ACT_MP_ATTACK_STAND_MELEE_ALLCLASS, // RUN, WALK
    ACT_MP_ATTACK_STAND_MELEE_SECONDARY_ALLCLASS,
    ACT_MP_ATTACK_CROUCH_MELEE_ALLCLASS, // CROUCHWALK
    ACT_MP_ATTACK_CROUCH_MELEE_SECONDARY_ALLCLASS,
    ACT_MP_ATTACK_SWIM_MELEE_ALLCLASS,
    ACT_MP_ATTACK_AIRWALK_MELEE_ALLCLASS,

    ACT_MELEE_ALLCLASS_VM_DRAW,
    ACT_MELEE_ALLCLASS_VM_HOLSTER,
    ACT_MELEE_ALLCLASS_VM_IDLE,
    ACT_MELEE_ALLCLASS_VM_PULLBACK,
    ACT_MELEE_ALLCLASS_VM_PRIMARYATTACK,
    ACT_MELEE_ALLCLASS_VM_SECONDARYATTACK,
    ACT_MELEE_ALLCLASS_VM_RELOAD,
    ACT_MELEE_ALLCLASS_VM_DRYFIRE,
    ACT_MELEE_ALLCLASS_VM_IDLE_TO_LOWERED,
    ACT_MELEE_ALLCLASS_VM_IDLE_LOWERED,
    ACT_MELEE_ALLCLASS_VM_LOWERED_TO_IDLE,
    ACT_MELEE_ALLCLASS_VM_STUN,
    ACT_MELEE_ALLCLASS_VM_HITCENTER,
    ACT_MELEE_ALLCLASS_VM_SWINGHARD,

    // BOMB activities for TD mode.
    ACT_MP_STAND_BOMB,
    ACT_MP_JUMP_START_BOMB,
    ACT_MP_JUMP_FLOAT_BOMB,
    ACT_MP_JUMP_LAND_BOMB,
    ACT_MP_RUN_BOMB,
    ACT_MP_SWIM_BOMB,

    // More Primary VM activities for Soldier Quake RL
    ACT_VM_DRAW_QRL,
    ACT_VM_IDLE_QRL,
    ACT_VM_PULLBACK_QRL,
    ACT_VM_PRIMARYATTACK_QRL,
    ACT_VM_RELOAD_QRL,
    ACT_VM_RELOAD_START_QRL,
    ACT_VM_RELOAD_FINISH_QRL,

    // Third person anims for the Soldier Quake RL
    ACT_MP_RELOAD_STAND_PRIMARY3,
    ACT_MP_RELOAD_CROUCH_PRIMARY3,
    ACT_MP_RELOAD_AIRWALK_PRIMARY3,
    ACT_MP_RELOAD_STAND_PRIMARY3_LOOP,
    ACT_MP_RELOAD_CROUCH_PRIMARY3_LOOP,
    ACT_MP_RELOAD_AIRWALK_PRIMARY3_LOOP,
    ACT_MP_RELOAD_STAND_PRIMARY3_END,
    ACT_MP_RELOAD_CROUCH_PRIMARY3_END,
    ACT_MP_RELOAD_AIRWALK_PRIMARY3_END,
    ACT_MP_RELOAD_SWIM_PRIMARY3,

    // Throwable Animations
    ACT_MP_THROW,
    ACT_THROWABLE_VM_DRAW,
    ACT_THROWABLE_VM_IDLE,
    ACT_THROWABLE_VM_FIRE,

    // Spell Animations
    ACT_SPELL_VM_DRAW,
    ACT_SPELL_VM_IDLE,
    ACT_SPELL_VM_ARM,
    ACT_SPELL_VM_FIRE,

    // Bread Monster Sapper
    ACT_BREADSAPPER_VM_DRAW,
    ACT_BREADSAPPER_VM_IDLE,

    // Bread Gloves
    ACT_BREADGLOVES_VM_HITLEFT,
    ACT_BREADGLOVES_VM_HITRIGHT,
    ACT_BREADGLOVES_VM_SWINGHARD,
    ACT_BREADGLOVES_VM_IDLE,
    ACT_BREADGLOVES_VM_DRAW,

    ACT_BREADMONSTER_GLOVES_IDLE,
    ACT_BREADMONSTER_GLOVES_HITRIGHT,
    ACT_BREADMONSTER_GLOVES_HITUP,

    ACT_BREADMONSTER_VM_DRAW,
    ACT_BREADMONSTER_VM_IDLE,
    ACT_BREADMONSTER_VM_PRIMARYATTACK,

    ACT_PARACHUTE_DEPLOY,
    ACT_PARACHUTE_DEPLOY_IDLE,
    ACT_PARACHUTE_RETRACT,
    ACT_PARACHUTE_RETRACT_IDLE,

    ACT_BOT_SPAWN,
    ACT_BOT_PANIC,
    ACT_BOT_PRIMARY_MOVEMENT,
    ACT_BOT_GESTURE_FLINCH,
    ACT_BOT_PANIC_START,
    ACT_BOT_PANIC_END,

    ACT_ENGINEER_REVOLVER_DRAW,
    ACT_ENGINEER_REVOLVER_IDLE,
    ACT_ENGINEER_REVOLVER_PRIMARYATTACK,
    ACT_ENGINEER_REVOLVER_RELOAD,

    // Kart!
    ACT_KART_IDLE,
    ACT_KART_ACTION_SHOOT,
    ACT_KART_ACTION_DASH,
    ACT_KART_JUMP_START,
    ACT_KART_JUMP_FLOAT,
    ACT_KART_JUMP_LAND,
    ACT_KART_IMPACT,
    ACT_KART_IMPACT_BIG,
    ACT_KART_GESTURE_POSITIVE,
    ACT_KART_GESTURE_NEGATIVE,

    // grappling hook
    ACT_GRAPPLE_DRAW,
    ACT_GRAPPLE_IDLE,
    ACT_GRAPPLE_FIRE_START,
    ACT_GRAPPLE_FIRE_IDLE,
    ACT_GRAPPLE_PULL_START,
    ACT_GRAPPLE_PULL_IDLE,
    ACT_GRAPPLE_PULL_END,

    // inspect
    ACT_PRIMARY_VM_INSPECT_START,
    ACT_PRIMARY_VM_INSPECT_IDLE,
    ACT_PRIMARY_VM_INSPECT_END,

    ACT_SECONDARY_VM_INSPECT_START,
    ACT_SECONDARY_VM_INSPECT_IDLE,
    ACT_SECONDARY_VM_INSPECT_END,

    ACT_MELEE_VM_INSPECT_START,
    ACT_MELEE_VM_INSPECT_IDLE,
    ACT_MELEE_VM_INSPECT_END,

    // passtime
    ACT_BALL_VM_CATCH,
    ACT_BALL_VM_PICKUP,
    ACT_BALL_VM_IDLE,
    ACT_BALL_VM_THROW_START,
    ACT_BALL_VM_THROW_LOOP,
    ACT_BALL_VM_THROW_END,

    // Competitive loser
    ACT_MP_COMPETITIVE_LOSERSTATE,
    ACT_MP_COMPETITIVE_WINNERSTATE,

    // Shortstop alt-fire
    ACT_SECONDARY_VM_ALTATTACK,
    ACT_MP_PUSH_STAND_SECONDARY,
    ACT_MP_PUSH_CROUCH_SECONDARY,
    ACT_MP_PUSH_SWIM_SECONDARY,

    ACT_MELEE_VM_ITEM1_DRAW,

    ACT_MP_FALLING_STOMP,
    ACT_MP_FALLING_STOMP_PRIMARY,
    ACT_MP_FALLING_STOMP_MELEE,

    ACT_ITEM_DEPLOY,
    ACT_ITEM_UNDEPLOY,

    // more inspect
    ACT_MELEE_ALLCLASS_VM_INSPECT_START,
    ACT_MELEE_ALLCLASS_VM_INSPECT_IDLE,
    ACT_MELEE_ALLCLASS_VM_INSPECT_END,

    ACT_SPECIAL_VM_INSPECT_START,
    ACT_SPECIAL_VM_INSPECT_IDLE,
    ACT_SPECIAL_VM_INSPECT_END,

    ACT_ITEM1_VM_INSPECT_START,
    ACT_ITEM1_VM_INSPECT_IDLE,
    ACT_ITEM1_VM_INSPECT_END,

    ACT_ITEM2_VM_INSPECT_START,
    ACT_ITEM2_VM_INSPECT_IDLE,
    ACT_ITEM2_VM_INSPECT_END,

    ACT_ITEM3_VM_INSPECT_START,
    ACT_ITEM3_VM_INSPECT_IDLE,
    ACT_ITEM3_VM_INSPECT_END,

    ACT_ITEM4_VM_INSPECT_START,
    ACT_ITEM4_VM_INSPECT_IDLE,
    ACT_ITEM4_VM_INSPECT_END,

    ACT_PRIMARY_ALT1_VM_INSPECT_START,
    ACT_PRIMARY_ALT1_VM_INSPECT_IDLE,
    ACT_PRIMARY_ALT1_VM_INSPECT_END,

    ACT_SECONDARY_ALT1_VM_INSPECT_START,
    ACT_SECONDARY_ALT1_VM_INSPECT_IDLE,
    ACT_SECONDARY_ALT1_VM_INSPECT_END,

    ACT_MELEE_ALT1_VM_INSPECT_START,
    ACT_MELEE_ALT1_VM_INSPECT_IDLE,
    ACT_MELEE_ALT1_VM_INSPECT_END,

    ACT_MELEE_ALT2_VM_INSPECT_START,
    ACT_MELEE_ALT2_VM_INSPECT_IDLE,
    ACT_MELEE_ALT2_VM_INSPECT_END,

    ACT_MELEE_ALT3_VM_INSPECT_START,
    ACT_MELEE_ALT3_VM_INSPECT_IDLE,
    ACT_MELEE_ALT3_VM_INSPECT_END,

    ACT_BUILDING_VM_INSPECT_START,
    ACT_BUILDING_VM_INSPECT_IDLE,
    ACT_BUILDING_VM_INSPECT_END,

    ACT_BREADMONSTER_VM_INSPECT_START,
    ACT_BREADMONSTER_VM_INSPECT_IDLE,
    ACT_BREADMONSTER_VM_INSPECT_END,

    ACT_SLAP_VM_IDLE,
    ACT_SLAP_VM_DRAW,
    ACT_SLAP_VM,

    ACT_MP_CYOA_PDA_INTRO,
    ACT_MP_CYOA_PDA_IDLE,
    ACT_MP_CYOA_PDA_OUTRO,

    // this is the end of the global activities, private per-monster activities start here.
    LAST_SHARED_ACTIVITY,
} Activity;

struct lighting_state_t
{
    vec3  m_box_color[ 6 ]{ };
    i32   m_light_count{ };
    void *m_local_light[ 4 ]{ };
};

constexpr i32 MAX_STUDIO_BONES{ 128 };
constexpr i32 MAX_TRACKED_PLAYERS{ 101 };

struct bone_accessor_t
{
    matrix3x4 *m_bones;
    i32        m_readable;
    i32        m_writable;
};

// the CUtlVector every bone consumer ultimately reads through; the accessor above just points at
// its base
struct bone_cache_t
{
    matrix3x4 *m_bones;
    i32        m_allocation_count;
    i32        m_grow_size;
    i32        m_count;
};

struct C_AnimationLayer
{
    i32   m_nSequence;
    float m_flPrevCycle;
    float m_flWeight;
    i32   m_nOrder;
    float m_flPlaybackRate;
    float m_flCycle;
    float m_flLayerAnimtime;
    float m_flLayerFadeOuttime;
    float m_flBlendIn;
    float m_flBlendOut;
    bool  m_bClientBlend;
};

static_assert( sizeof( C_AnimationLayer ) == 44 );

inline constexpr i32 GESTURE_SLOT_ATTACK_AND_RELOAD{ 0 };

struct anim_overlay_t
{
    C_AnimationLayer *m_layers;
    i32               m_allocation_count;
    i32               m_grow_size;
    i32               m_count;

    i32 count( ) const
    {
        return this->m_layers ? this->m_count : 0;
    }

    const C_AnimationLayer &operator[]( i32 index ) const
    {
        return this->m_layers[ index ];
    }

    C_AnimationLayer &operator[]( i32 index )
    {
        return this->m_layers[ index ];
    }
};

struct CEconItemAttribute
{
    void *m_vtable;
    u16   m_iAttributeDefinitionIndex;
    u32   m_iRawValue32;
    i32   m_nRefundableCurrency;
};

static_assert( sizeof( CEconItemAttribute ) == 24 );
static_assert( __builtin_offsetof( CEconItemAttribute, m_iRawValue32 ) == 12 );

struct CAttributeList
{
    void               *m_vtable;
    CEconItemAttribute *m_pMemory;
    i32                 m_nAllocationCount;
    i32                 m_nGrowSize;
    i32                 m_Size;
    CEconItemAttribute *m_pElements;
    void               *m_pManager;
};

static_assert( __builtin_offsetof( CAttributeList, m_Size ) == 24 );
static_assert( __builtin_offsetof( CAttributeList, m_pManager ) == 40 );

struct material_overrides_t
{
    IMaterial *m_teams[ 5 ];
};

struct item_material_overrides_t
{
    IMaterial *m_teams[ 4 ];
    bool       m_initialized[ 4 ];
};

enum econ_attribute_t : u16
{
    ATTRIB_SET_ATTACHED_PARTICLE        = 134,
    ATTRIB_ADD_JINGLE_TO_FOOTSTEPS      = 364,
    ATTRIB_ITEM_STYLE_OVERRIDE          = 542,
    ATTRIB_SET_ITEM_TEXTURE_WEAR        = 725,
    ATTRIB_PAINTKIT_PROTO_DEF_INDEX     = 834,
    ATTRIB_CUSTOM_PAINTKIT_SEED_LO      = 866,
    ATTRIB_CUSTOM_PAINTKIT_SEED_HI      = 867,
    ATTRIB_HALLOWEEN_FOOTSTEP_TYPE      = 1005,
    ATTRIB_HALLOWEEN_VOICE_MODULATION   = 1006,
    ATTRIB_HALLOWEEN_PUMPKIN_EXPLOSIONS = 1007,
    ATTRIB_HALLOWEEN_GREEN_FLAMES       = 1008,
    ATTRIB_KILLSTREAK_EFFECT            = 2013,
    ATTRIB_KILLSTREAK_IDLEEFFECT        = 2014,
    ATTRIB_KILLSTREAK_TIER              = 2025,
    ATTRIB_IS_AUSTRALIUM_ITEM           = 2027,
    ATTRIB_IS_FESTIVIZED                = 2053,
};

enum weapon_unusual_t : u32
{
    WEAPON_UNUSUAL_COMMUNITY_SPARKLE = 4,
    WEAPON_UNUSUAL_HOT               = 701,
    WEAPON_UNUSUAL_ISOTOPE           = 702,
    WEAPON_UNUSUAL_COOL              = 703,
    WEAPON_UNUSUAL_ENERGY_ORB        = 704,
};

enum killstreaker_t : u32
{
    KILLSTREAKER_FIRE_HORNS         = 2002,
    KILLSTREAKER_CEREBRAL_DISCHARGE = 2003,
    KILLSTREAKER_TORNADO            = 2004,
    KILLSTREAKER_FLAMES             = 2005,
    KILLSTREAKER_SINGULARITY        = 2006,
    KILLSTREAKER_INCINERATOR        = 2007,
    KILLSTREAKER_HYPNO_BEAM         = 2008,
};

enum holiday_t : i32
{
    kHoliday_None                = 0,
    kHoliday_Halloween           = 2,
    kHoliday_Christmas           = 3,
    kHoliday_FullMoon            = 8,
    kHoliday_HalloweenOrFullMoon = 9,
};

// CTFPlayerShared::m_nStreaks
enum streak_t : i32
{
    kTFStreak_Kills = 0,
    kTFStreak_COUNT = 4,
};

inline constexpr i32 TF_DEATH_DOMINATION{ 0x0001 };
inline constexpr i32 TF_DEATH_ASSISTER_DOMINATION{ 0x0002 };
inline constexpr i32 TF_DEATH_REVENGE{ 0x0004 };
inline constexpr i32 TF_DEATH_ASSISTER_REVENGE{ 0x0008 };
inline constexpr i32 TF_DEATH_FIRST_BLOOD{ 0x0010 };
inline constexpr i32 TF_DEATH_FEIGN_DEATH{ 0x0020 };
inline constexpr i32 TF_DEATH_INTERRUPTED{ 0x0040 };
inline constexpr i32 TF_DEATH_GIBBED{ 0x0080 };
inline constexpr i32 TF_DEATH_PURGATORY{ 0x0100 };
inline constexpr i32 TF_DEATH_MINIBOSS{ 0x0200 };
inline constexpr i32 TF_DEATH_AUSTRALIUM{ 0x0400 };

inline constexpr i32 EF_NODRAW{ 0x0020 };

struct handle_vector_t
{
    EHANDLE *m_handles;
    i32      m_allocation_count;
    i32      m_grow_size;
    i32      m_count;
};

struct CRecvPropExtra_UtlVector
{
    void *m_DataTableProxyFn;
    void *m_ProxyFn;
    void *m_ResizeFn;
    void *m_EnsureCapacityFn;
    i32   m_ElementStride;
    i32   m_Offset;
    i32   m_nMaxElements;
};

static_assert( __builtin_offsetof( CRecvPropExtra_UtlVector, m_Offset ) == 36 );

static constexpr i32 BONE_USED_BY_HITBOX{ 0x00000100 };
static constexpr i32 BONE_USED_BY_ATTACHMENT{ 0x00000200 };
static constexpr i32 BONE_USED_BY_VERTEX_LOD0{ 0x00000400 };
static constexpr i32 BONE_USED_BY_BONE_MERGE{ 0x00040000 };

static constexpr i32 STUDIOHDR_FLAGS_STATIC_PROP{ 0x00000010 };

static constexpr i32 SERVER_BONE_MASK{ BONE_USED_BY_HITBOX | BONE_USED_BY_ATTACHMENT |
                                       BONE_USED_BY_BONE_MERGE };

static constexpr i32 MAX_OVERLAYS{ 15 };
static constexpr i32 MAX_POSE_PARAMETERS{ 24 };
static constexpr i32 MAX_BONE_CONTROLLERS{ 4 };

struct bone_setup_state_t
{
    const CStudioHdr *m_studio;
    i32               m_bone_mask;
    const float      *m_pose_parameters;
    void             *m_pose_debugger;
};

static_assert( sizeof( bone_setup_state_t ) == 32 );

struct anim_layer_t
{
    i32   m_sequence{ -1 };
    i32   m_order{ MAX_OVERLAYS };
    float m_cycle{ };
    float m_weight{ };
};

constexpr i32 MAX_WEAPONS{ 48 };

struct steamid_t
{
    union steamid__
    {
        struct component_t
        {
            u32 account_id : 32;
            u32 account_instance : 20;
            u32 account_type : 4;
            i32 universe : 8;
        } m_comp;
    } m_steamid;
};

inline constexpr usize MAX_PARTY_SIZE{ 6 };
inline constexpr usize MAX_PARTY_CHAT_MSG{ 256 };
inline constexpr usize MAX_PARTY_TEXT{ MAX_PARTY_CHAT_MSG - 1 };
inline constexpr i32   TF_PARTY_CHAT_MEMBER{ 1 };
inline constexpr i32   TF_PARTY_CHAT_MEMBER_JOIN{ 1000 };
inline constexpr i32   TF_PARTY_CHAT_MEMBER_LEAVE{ 1001 };
inline constexpr i32   TF_PARTY_CHAT_SEND_FAILED{ 1002 };

class IPlayerGroup
{
public:
    i32 GetNumMembers( );
    u32 GetMemberAccount( i32 index );
};

class C_TFParty
{
private:
    u8 m_pad184[ 184 ];

public:
    IPlayerGroup m_PlayerGroup; // ( this + 184 )
};

class C_TFPartyClient
{
private:
    u8 m_pad48[ 48 ];

public:
    void       SendPartyChat( const char *msg );
    C_TFParty *m_pActiveParty; // *( _QWORD * )( this + 48 )
};

class CTFLobbyPlayerProto
{
private:
    u8 m_pad24[ 24 ];

public:
    u64 m_id; // ( this + 24 )

private:
    u8 m_pad48[ 16 ];

public:
    u64 m_original_party_id; // ( this + 48 )
};

class CTFGSLobby
{
public:
    i32                  GetNumMembers( );
    CTFLobbyPlayerProto *GetMemberDetails( i32 index );
};

class CTFGCClientSystem
{
public:
    CTFGSLobby *GetLobby( );
};

class IGameResources;

enum medigun_weapontypes_t
{
    MEDIGUN_STANDARD = 0,
    MEDIGUN_UBER,
    MEDIGUN_QUICKFIX,
    MEDIGUN_RESIST,
};

enum medigun_resist_types_t
{
    MEDIGUN_BULLET_RESIST = 0,
    MEDIGUN_BLAST_RESIST,
    MEDIGUN_FIRE_RESIST,
    MEDIGUN_NUM_RESISTS
};

// https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/public/worldsize.h#L32
constexpr f32 MAX_TRACE_LENGTH{ 1.732050807569f * 2.0f * 16384.0f };

//-----------------------------------------------------------------------------
// Grenade Launcher mode (for pipebombs).
//-----------------------------------------------------------------------------
enum
{
    TF_GL_MODE_REGULAR = 0,
    TF_GL_MODE_REMOTE_DETONATE,
    TF_GL_MODE_REMOTE_DETONATE_PRACTICE,
    TF_GL_MODE_CANNONBALL,

    //
    // ADD NEW ITEMS HERE TO AVOID BREAKING DEMOS
    //
};

/*

Command execution takes a null terminated string, breaks it into tokens,
then searches for a command or variable that matches the first token.

Commands can come from three sources, but the handler functions may choose
to dissallow the action or forward it to a remote server if the source is
not apropriate.

*/

enum cmd_source_t
{
    src_client, // came in over a net connection as a clc_stringcmd
                // host_client will be valid during this state.
    src_command // from the command buffer
};

/**
 * Base class for notifications, but is generic enough to use
 */
class CEconNotification
{
public:
    void Destroy( );

    void       *vmt;
    const char *m_pText;
    const char *m_pSoundFilename;
    float       m_flExpireTime;
};
