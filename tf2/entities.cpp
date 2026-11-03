#include "entities.hpp"

#include "damage.hpp"
#include "global.hpp"

#include "steam.hpp"

datamap_t *C_BaseEntity::GetPredDescMap( )
{
    return util::call_virtual< datamap_t *( * )( void * ) >(
        this, g_tf2.m_base_entity_get_pred_desc_map )( this );
}

void C_BaseEntity::OnLatchInterpolatedVariables( i32 flags )
{
    util::call_virtual< void( ATLAS_THISCALL * )( void *, i32 ) >(
        this, g_tf2.m_base_entity_on_latch_interpolated_variables )( this, flags );
}

vec3 C_BaseEntity::WorldSpaceCenter( )
{
    const vec3 origin{ GetAbsOrigin( ) };
    return vec3{ origin.x, origin.y, origin.z + m_vecMaxs( ).z * 0.5f };
}

void C_BaseEntity::EstimateAbsVelocity( )
{
    return g_tf2.m_CBaseEntity_CalcAbsoluteVelocity.call_as< void >( this );
}

const vec3 &C_BaseEntity::GetAbsVelocity( )
{
    reinterpret_cast< C_BaseEntity * >( this )->EstimateAbsVelocity( );
    return m_vecAbsVelocity( );
}

SolidType_t C_BaseEntity::GetSolid( )
{
    return util::call_virtual< SolidType_t ( * )( void * ) >(
        this, g_tf2.m_base_entity_get_solid )( this );
}

bool C_BaseEntity::ShouldCollide( i32 collision_group, i32 contents_mask )
{
    return util::call_virtual< bool ( * )( void *, i32, i32 ) >(
        this, g_tf2.m_base_entity_should_collide )( this, collision_group, contents_mask );
}

// https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/game/server/player.cpp#L8125
vec3 C_BasePlayer::Weapon_ShootPosition( )
{
    constexpr f32 HALF_STEP{ 128.0f / static_cast< f32 >( ( 1 << 20 ) - 1 ) };
    vec3          view{ m_vecViewOffset( ) };
    C_TFPlayer   *player{ this->As< C_TFPlayer * >( ) };
    const f32     standing{ player->GetClassEyeHeight( ).z };
    const f32     crouched{ TFGameRules( )->GetViewVectors( )->m_vDuckView.z * player->m_flModelScale( ) };
    if ( math::abs( view.z - standing ) <= HALF_STEP )
    {
        view.z = standing;
    } else if ( math::abs( view.z - crouched ) <= HALF_STEP )
    {
        view.z = crouched;
    }
    return GetAbsOrigin( ) + view;
}

void C_BasePlayer::ThirdPersonSwitch( bool thirdperson )
{
    util::call_virtual< void ( * )( void *, bool ) >(
        this, g_tf2.m_base_player_third_person_switch )( this, thirdperson );
}

void C_TFMinigun::StartMuzzleEffect( )
{
    g_tf2.m_CTFMinigun_StartMuzzleEffect.call_as< void, C_TFMinigun * >( this );
}

void C_TFMinigun::StartBrassEffect( )
{
    g_tf2.m_CTFMinigun_StartBrassEffect.call_as< void, C_TFMinigun * >( this );
}

float C_BaseAnimating::FrameAdvance( float interval )
{
    return g_tf2.m_CBaseAnimating_FrameAdvance.call_as< float, C_BaseAnimating * >(
        this, interval );
}

void C_BaseAnimating::InvalidateBoneCache( )
{
    g_tf2.m_CBaseAnimating_InvalidateBoneCache.call_as< void >( this );
}

bool C_BaseAnimating::GetAttachment( i32 number, vec3 &origin )
{
    return util::call_virtual< bool ( * )( void *, i32, vec3 & ) >(
        this, g_tf2.m_base_animating_get_attachment )( this, number, origin );
}

const CViewVectors *C_TFGameRules::GetViewVectors( )
{
    return util::call_virtual< const CViewVectors *( * )( void * ) >(
        this, g_tf2.m_gamerules_get_view_vectors )( this );
}

vec3 C_TFPlayer::GetClassEyeHeight( )
{
    vec3 result{ };
    g_tf2.m_CTFPlayer_GetClassEyeHeight.call_as< vec3 * >( this, &result );
    return result;
}

bool C_TFPlayer::InCond( const ETFCond cond )
{
    const i32 _cond{ static_cast< i32 >( cond ) };
    switch ( _cond / 32 )
    {
        case 0:
        {
            const u32 bit{ 1u << _cond };
            if ( ( static_cast< u32 >( this->m_nPlayerCond( ) ) & bit ) == bit ||
                 ( static_cast< u32 >( this->_condition_bits( ) ) & bit ) == bit )
                return true;
            break;
        }
        case 1:
        {
            const u32 bit{ 1u << ( _cond - 32 ) };
            if ( ( static_cast< u32 >( this->m_nPlayerCondEx( ) ) & bit ) == bit )
                return true;
            break;
        }
        case 2:
        {
            const u32 bit{ 1u << ( _cond - 64 ) };
            if ( ( static_cast< u32 >( this->m_nPlayerCondEx2( ) ) & bit ) == bit )
                return true;
            break;
        }
        case 3:
        {
            const u32 bit{ 1u << ( _cond - 96 ) };
            if ( ( static_cast< u32 >( this->m_nPlayerCondEx3( ) ) & bit ) == bit )
                return true;
            break;
        }
        case 4:
        {
            const u32 bit{ 1u << ( _cond - 128 ) };
            if ( ( static_cast< u32 >( this->m_nPlayerCondEx4( ) ) & bit ) == bit )
                return true;
            break;
        }
        default: break;
    }
    return false;
}

bool C_TFPlayer::IsAlive( )
{
    return m_lifeState( ) == LIFE_ALIVE;
}

CTFPlayerShared *C_TFPlayer::GetSharedPlayerData( )
{
    static auto offset{ g_netvars.get_offset( HASH_CT( "DT_TFPlayer" ), HASH_CT( "m_Shared" ) ) };
    return reinterpret_cast< CTFPlayerShared * >( reinterpret_cast< uptr >( this ) + offset );
}

C_TFWeaponBase *C_TFPlayer::Weapon_OwnsThisID( i32 weapon_id )
{
    EHANDLE *weapons{ m_hMyWeapons( ) };

    for ( i32 index{ }; index < MAX_WEAPONS; ++index )
    {
        C_BaseEntity *entity{ weapons[ index ].Get( ) };
        if ( !entity )
        {
            continue;
        }

        C_TFWeaponBase *weapon{ entity->As< C_TFWeaponBase * >( ) };
        if ( weapon->GetWeaponID( ) == weapon_id )
        {
            return weapon;
        }
    }

    return nullptr;
}

bool C_TFPlayer::IsOnGround( )
{
    return m_fFlags( ) & FL_ONGROUND;
}

bool C_TFPlayer::IsDucked( )
{
    return m_fFlags( ) & FL_DUCKING;
}

bool C_TFPlayer::IsBurning( )
{
    return InCond( TF_COND_BURNING ) || InCond( TF_COND_BURNING_PYRO );
}

bool C_TFPlayer::IsCritBoosted( )
{
    int cond    = m_nPlayerCond( );
    int cond_ex = m_nPlayerCondEx( );

    if ( cond & TFCond_Kritzkrieged || _condition_bits( ) & TFCond_Kritzkrieged || cond_ex & TFCondEx_CritCanteen ||
         cond_ex & TFCondEx_CritOnFirstBlood || cond_ex & TFCondEx_CritOnWin || cond_ex & TFCondEx_CritOnKill ||
         cond_ex & TFCondEx_CritOnFlagCapture || cond_ex & TFCondEx_HalloweenCritCandy ||
         InCond( TF_COND_CRITBOOSTED_CARD_EFFECT ) || InCond( TF_COND_CRITBOOSTED_RUNE_TEMP ) )
    {
        return true;
    }

    C_BaseEntity   *entity{ this->m_hActiveWeapon( ).Get( ) };
    C_TFWeaponBase *weapon{ entity ? entity->As< C_TFWeaponBase * >( ) : nullptr };
    if ( !weapon )
    {
        return false;
    }

    constexpr i32        primary_weapon_type{ 0 }; // TF_WPN_TYPE_PRIMARY
    const CTFWeaponInfo *info{ weapon->GetWeaponInfo( ) };
    if ( ( cond_ex & TFCondEx_PyroCrits ) && info && info->m_iWeaponType == primary_weapon_type )
    {
        return true;
    }

    const float threshold{ g_cl.attrib_hook( weapon, 1.0f,
                                             ATLAS_XORSTR( "mult_crit_when_health_is_below_percent" ).get( ) ) };
    if ( threshold >= 1.0f )
    {
        return false;
    }

    const i32   index{ this->entindex( ) };
    const i32   maximum{ g_cl.m_resource && index > 0 && index <= g_tf2.m_globals->maxClients
                             ? g_cl.m_resource->m_iMaxHealth( )[ index ]
                             : 1 };
    const float fraction{ maximum == 0 ? 1.0f
                                       : math::clamp( static_cast< float >( this->m_iHealth( ) ) / static_cast< float >( maximum ), 0.0f, 1.0f ) };
    return fraction < threshold;
}

bool C_TFPlayer::IsMiniCritBoosted( )
{
    return m_nPlayerCond( ) & TFCond_MiniCrits;
}

// RemapValClamped( m_iCritMult, 0, 255, 1.0, 4.0 )
float C_TFPlayer::GetCritMult( )
{
    const float t{ math::clamp( static_cast< float >( m_iCritMult( ) ) / 255.0f, 0.0f, 1.0f ) };
    return 1.0f + t * 3.0f;
}

bool C_TFPlayer::IsBonked( )
{
    return m_nPlayerCond( ) & TFCond_Bonked;
}

bool C_TFPlayer::IsDisguised( )
{
    return InCond( TF_COND_DISGUISED );
}

bool C_TFPlayer::IsCharging( )
{
    return m_nPlayerCond( ) & TFCond_Charging;
}

bool C_TFPlayer::IsTaunting( )
{
    return InCond( TF_COND_TAUNTING );
}

bool C_TFPlayer::IsMarkedForDeath( )
{
    return m_nPlayerCond( ) & TFCond_MarkedForDeath ||
           m_nPlayerCondEx( ) & TFCondEx_MarkedForDeathSilent;
}

bool C_TFPlayer::IsJarated( )
{
    return m_nPlayerCond( ) & TFCond_Jarated;
}

bool C_TFPlayer::IsStunned( )
{
    return m_nPlayerCond( ) & TFCond_Stunned;
}

bool C_TFPlayer::IsInKart( )
{
    return m_nPlayerCondEx( ) & TFCondEx_InKart;
}

bool C_TFPlayer::IsGhost( )
{
    return m_nPlayerCondEx2( ) & TFCondEx2_HalloweenGhostMode;
}

bool C_TFPlayer::IsCloaked( )
{
    return m_nPlayerCond( ) & TFCond_Cloaked ||
           m_nPlayerCondEx2( ) & TFCondEx2_Stealthed ||
           m_nPlayerCondEx2( ) & TFCondEx2_StealthedUserBuffFade;
}

bool C_TFPlayer::IsZoomed( )
{
    return m_nPlayerCond( ) & TFCond_Zoomed;
}

bool C_TFPlayer::IsUbered( )
{
    return m_nPlayerCond( ) & TFCond_Ubercharged || m_nPlayerCondEx( ) & TFCondEx_UberchargedCanteen;
}

bool C_TFPlayer::IsQuickFixUbered( )
{
    return m_nPlayerCond( ) & TFCond_MegaHeal;
}

bool C_TFPlayer::IsRegenBuffed( )
{
    return m_nPlayerCond( ) & TFCond_RegenBuffed;
}

bool C_TFPlayer::IsDefenseBuffed( )
{
    return m_nPlayerCond( ) & TFCond_DefenseBuffed;
}

bool C_TFPlayer::IsBannerBuffed( )
{
    return m_nPlayerCond( ) & TFCond_Buffed;
}

bool C_TFPlayer::IsBleeding( )
{
    return m_nPlayerCond( ) & TFCond_Bleeding;
}

bool C_TFPlayer::IsMilked( )
{
    return m_nPlayerCond( ) & TFCond_Milked;
}

bool C_TFPlayer::IsSlowed( )
{
    return m_nPlayerCond( ) & TFCond_Slowed;
}

bool C_TFPlayer::IsBulletResistant( )
{
    return m_nPlayerCondEx( ) & TFCondEx_BulletResistance;
}

bool C_TFPlayer::IsBlastResistant( )
{
    return m_nPlayerCondEx( ) & TFCondEx_ExplosiveResistance;
}

bool C_TFPlayer::IsFireResistant( )
{
    return m_nPlayerCondEx( ) & TFCondEx_FireResistance;
}

bool C_TFPlayer::IsBulletUbered( )
{
    return m_nPlayerCondEx( ) & TFCondEx_BulletCharge;
}

bool C_TFPlayer::IsBlastUbered( )
{
    return m_nPlayerCondEx( ) & TFCondEx_ExplosiveCharge;
}

bool C_TFPlayer::IsFireUbered( )
{
    return m_nPlayerCondEx( ) & TFCondEx_FireCharge;
}

bool C_TFPlayer::IsHighKD( )
{
    if ( ( f32 )m_iKills( ) / ( f32 )m_iDeaths( ) > 15.f )
    {
        return true;
    }

    return false;
}

bool C_TFPlayer::IsFeigned( )
{
    return m_bFeignDeathReady( ) || InCond( TF_COND_FEIGN_DEATH );
}

bool C_TFPlayer::IsFrozen( )
{
    return m_fFlags( ) & FL_FROZEN;
}

f32 C_TFPlayer::TeamFortress_CalculateMaxSpeed( bool bIgnoreSpecialAbility ) const
{
    return g_tf2.m_C_TFPlayer_TeamFortress_CalculateMaxSpeed.call_as< f32 >( this, bIgnoreSpecialAbility );
}

bool C_TFPlayer::IsPlayerOnSteamFriendsList( C_TFPlayer *player )
{
    if ( !g_cl.m_local || this != g_cl.m_local )
    {
        return false;
    }

    player_info_t info{ };
    if ( !g_tf2.m_engine->GetPlayerInfo( player->entindex( ), &info ) )
    {
        return false;
    }

    return g_steam.is_friend( info.friendsID );
}

i32 C_TFWeaponBase::GetWeaponID( )
{
    return util::call_virtual< i32 ( * )( void * ) >(
        this, g_tf2.m_tf_weapon_base_get_weapon_id )( this );
}

i32 C_TFWeaponBase::GetDamageType( )
{
    return util::call_virtual< i32 ( * )( void * ) >(
        this, g_tf2.m_tf_weapon_base_get_damage_type )( this );
}

const char *C_BaseEntity::GetClassname( )
{
    return g_tf2.m_CBaseEntity_GetClassname.call_as< const char * >( this );
}

float C_TFWeaponBase::ApplyFireDelay( float delay )
{
    return util::call_virtual< float ( * )( void *, float ) >(
        this, g_tf2.m_tf_weapon_base_apply_fire_delay )( this, delay );
}

CTFWeaponInfo *C_TFWeaponBase::GetWeaponInfo( )
{
    const char *class_name{ this->GetClassname( ) };
    if ( !class_name )
    {
        return nullptr;
    }

    const auto slot{ g_tf2.m_LookupWeaponInfoSlot.call_as< WEAPON_FILE_INFO_HANDLE >( class_name ) };

    if ( slot == static_cast< WEAPON_FILE_INFO_HANDLE >( -1 ) )
    {
        return nullptr;
    }

    return g_tf2.m_GetFileWeaponInfoFromHandle.call_as< CTFWeaponInfo * >( slot );
}

i32 C_TFWeaponBase::GetSlot( )
{
    return util::call_virtual< int ( * )( void * ) >(
        this, g_tf2.m_tf_weapon_base_get_slot )( this );
}

bool C_TFWeaponBase::AreRandomCritsEnabled( )
{
    return util::call_virtual< bool ( * )( void * ) >(
        this, g_tf2.m_tf_weapon_base_are_random_crits_enabled )( this );
}

f32 C_TFWeaponBase::GetProjectileBlastRadius( )
{
    f32       radius{ };
    const i32 id{ GetWeaponID( ) };
    switch ( id )
    {
        case TF_WEAPON_ROCKETLAUNCHER:
        case TF_WEAPON_ROCKETLAUNCHER_DIRECTHIT:
        case TF_WEAPON_PARTICLE_CANNON:
        case TF_WEAPON_PIPEBOMBLAUNCHER: radius = 146.0f; break;
        case TF_WEAPON_FLAREGUN:
        case TF_WEAPON_FLAREGUN_REVENGE:
        {
            const i32 mode{ static_cast< i32 >( g_cl.attrib_hook( this, 0.0f, ATLAS_XORSTR( "set_weapon_mode" ).get( ) ) ) };
            if ( mode == 1 || mode == 2 )
            {
                radius = 110.0f;
            }
            break;
        }

        case TF_WEAPON_JAR:
        case TF_WEAPON_JAR_MILK:
        {
            radius = 200.f;
            break;
        }

        default: return 0.0f;
    }
    radius = g_cl.attrib_hook( this, radius, ATLAS_XORSTR( "mult_explosion_radius" ).get( ) );
    if ( ( id == TF_WEAPON_ROCKETLAUNCHER || id == TF_WEAPON_ROCKETLAUNCHER_DIRECTHIT || id == TF_WEAPON_PARTICLE_CANNON ) &&
         g_cl.m_local->InCond( TF_COND_BLASTJUMPING ) &&
         g_cl.attrib_hook( this, 1.0f, ATLAS_XORSTR( "rocketjump_attackrate_bonus" ).get( ) ) != 1.0f )
    {
        radius *= 0.8f;
    }
    return math::bigger( radius, 0.0f );
}

bool C_TFWeaponBase::HasPrimaryAmmoForShot( C_BasePlayer *owner )
{
    if ( this->GetWeaponID( ) == TF_WEAPON_PARTICLE_CANNON )
    {
        return this->m_flEnergy( ) >= 5.0f;
    }

    const i32 clip{ this->m_iClip1( ) };
    if ( clip >= 0 )
    {
        return clip > 0;
    }

    const i32 ammo_type{ this->m_iPrimaryAmmoType( ) };
    return ammo_type < 0 || ( owner && ( &owner->m_iAmmo( ) )[ ammo_type ] > 0 );
}

const char *C_TFWeaponBase::GetWeaponName( )
{
    switch ( m_iItemDefinitionIndex( ) )
    {
        case Scout_m_ForceANature:
        case Scout_m_FestiveForceANature: return ATLAS_XORSTR_STATIC( "FORCE-A-NATURE" );
        case Scout_m_FestiveScattergun: return ATLAS_XORSTR_STATIC( "SCATTERGUN" );
        case Scout_m_BackcountryBlaster: return ATLAS_XORSTR_STATIC( "BACK SCATTER" );
        case Scout_s_MutatedMilk: return ATLAS_XORSTR_STATIC( "MAD MILK" );
        case Scout_s_TheWinger: return ATLAS_XORSTR_STATIC( "WINGER" );
        case Scout_s_FestiveBonk:
        case Scout_s_BonkAtomicPunch: return ATLAS_XORSTR_STATIC( "BONK ATOMIC PUNCH" );
        case Scout_s_PrettyBoysPocketPistol: return ATLAS_XORSTR_STATIC( "POCKET PISTOL" );
        case Scout_s_CritaCola: return ATLAS_XORSTR_STATIC( "CRIT-A-COLA" );
        case Scout_t_FestiveBat: return ATLAS_XORSTR_STATIC( "BAT" );
        case Scout_t_FestiveHolyMackerel: return ATLAS_XORSTR_STATIC( "HOLY MACKEREL" );
        case Scout_t_TheAtomizer: return ATLAS_XORSTR_STATIC( "ATOMIZER" );
        case Scout_t_TheCandyCane: return ATLAS_XORSTR_STATIC( "CANDY CANE" );
        case Scout_t_TheFanOWar: return ATLAS_XORSTR_STATIC( "FAN-O-WAR" );
        case Scout_t_SunonaStick: return ATLAS_XORSTR_STATIC( "SUN ON A STICK" );
        case Scout_t_TheBostonBasher: return ATLAS_XORSTR_STATIC( "BOSTON BASHER" );
        case Scout_t_ThreeRuneBlade: return ATLAS_XORSTR_STATIC( "THREE RUNE BLADE" );
        case Scout_t_TheFreedomStaff: return ATLAS_XORSTR_STATIC( "FREEDOM STAFF" );
        case Scout_t_TheBatOuttaHell: return ATLAS_XORSTR_STATIC( "BAT OUTTA HELL" );
        case Scout_s_Lugermorph:
        case Scout_s_VintageLugermorph: return ATLAS_XORSTR_STATIC( "LUGERMORPH" );
        case Scout_s_TheCAPPER: return ATLAS_XORSTR_STATIC( "CAPPER" );
        case Scout_t_UnarmedCombat: return ATLAS_XORSTR_STATIC( "UNARMED COMBAT" );
        case Scout_t_Batsaber: return ATLAS_XORSTR_STATIC( "BATSABER" );
        case Scout_t_TheHamShank: return ATLAS_XORSTR_STATIC( "HAM SHANK" );
        case Scout_t_TheNecroSmasher: return ATLAS_XORSTR_STATIC( "NECRO SMASHER" );
        case Scout_t_TheConscientiousObjector: return ATLAS_XORSTR_STATIC( "OBJECTOR" );
        case Scout_t_TheCrossingGuard: return ATLAS_XORSTR_STATIC( "CROSSING GUARD" );
        case Scout_t_TheMemoryMaker: return ATLAS_XORSTR_STATIC( "MEMORY MAKER" );
        case Soldier_m_FestiveRocketLauncher: return ATLAS_XORSTR_STATIC( "ROCKET LAUNCHER" );
        case Soldier_m_RocketJumper: return ATLAS_XORSTR_STATIC( "ROCKET JUMPER" );
        case Soldier_m_TheAirStrike: return ATLAS_XORSTR_STATIC( "AIR STRIKE" );
        case Soldier_m_TheLibertyLauncher: return ATLAS_XORSTR_STATIC( "LIBERTY LAUNCHER" );
        case Soldier_m_TheOriginal: return ATLAS_XORSTR_STATIC( "ORIGINAL" );
        case Soldier_m_FestiveBlackBox:
        case Soldier_m_TheBlackBox: return ATLAS_XORSTR_STATIC( "BLACK BOX" );
        case Soldier_m_TheBeggarsBazooka: return ATLAS_XORSTR_STATIC( "BEGGARS BAZOOKA" );
        case Soldier_s_FestiveShotgun: return ATLAS_XORSTR_STATIC( "SHOTGUN" );
        case Soldier_s_FestiveBuffBanner: return ATLAS_XORSTR_STATIC( "BUFF BANNER" );
        case Soldier_s_TheConcheror: return ATLAS_XORSTR_STATIC( "CONCHEROR" );
        case Soldier_s_TheBattalionsBackup: return ATLAS_XORSTR_STATIC( "BATTALIONS BACKUP" );
        case Soldier_s_PanicAttack: return ATLAS_XORSTR_STATIC( "PANIC ATTACK" );
        case Soldier_t_TheMarketGardener: return ATLAS_XORSTR_STATIC( "MARKET GARDENER" );
        case Soldier_t_TheDisciplinaryAction: return ATLAS_XORSTR_STATIC( "DISCIPLINARY ACTION" );
        case Soldier_t_TheEqualizer: return ATLAS_XORSTR_STATIC( "EQUALIZER" );
        case Soldier_t_ThePainTrain: return ATLAS_XORSTR_STATIC( "PAIN TRAIN" );
        case Soldier_t_TheHalfZatoichi: return ATLAS_XORSTR_STATIC( "HALF ZATOICHI" );
        case Pyro_m_FestiveFlameThrower: return ATLAS_XORSTR_STATIC( "FLAMETHROWER" );
        case Pyro_m_ThePhlogistinator: return ATLAS_XORSTR_STATIC( "PHLOGISTINATOR" );
        case Pyro_m_FestiveBackburner:
        case Pyro_m_TheBackburner: return ATLAS_XORSTR_STATIC( "BACKBURNER" );
        case Pyro_m_TheRainblower: return ATLAS_XORSTR_STATIC( "RAINBLOWER" );
        case Pyro_m_TheDegreaser: return ATLAS_XORSTR_STATIC( "DEGREASER" );
        case Pyro_m_NostromoNapalmer: return ATLAS_XORSTR_STATIC( "NOSTROMO NAPALMER" );
        case Pyro_s_FestiveFlareGun: return ATLAS_XORSTR_STATIC( "FLARE GUN" );
        case Pyro_s_TheScorchShot: return ATLAS_XORSTR_STATIC( "SCORCH SHOT" );
        case Pyro_s_TheDetonator: return ATLAS_XORSTR_STATIC( "DETONATOR" );
        case Pyro_s_TheReserveShooter: return ATLAS_XORSTR_STATIC( "RESERVE SHOOTER" );
        case Pyro_t_TheFestiveAxtinguisher:
        case Pyro_t_TheAxtinguisher: return ATLAS_XORSTR_STATIC( "AXTINGUISHER" );
        case Pyro_t_Homewrecker: return ATLAS_XORSTR_STATIC( "HOMEWRECKER" );
        case Pyro_t_ThePowerjack: return ATLAS_XORSTR_STATIC( "POWERJACK" );
        case Pyro_t_TheBackScratcher: return ATLAS_XORSTR_STATIC( "BACK SCRATCHER" );
        case Pyro_t_TheThirdDegree: return ATLAS_XORSTR_STATIC( "THIRD DEGREE" );
        case Pyro_t_ThePostalPummeler: return ATLAS_XORSTR_STATIC( "POSTAL PUMMELER" );
        case Pyro_t_PrinnyMachete: return ATLAS_XORSTR_STATIC( "PRINNY MACHETE" );
        case Pyro_t_SharpenedVolcanoFragment: return ATLAS_XORSTR_STATIC( "VOLCANO FRAGMENT" );
        case Pyro_t_TheMaul: return ATLAS_XORSTR_STATIC( "MAUL" );
        case Pyro_t_TheLollichop: return ATLAS_XORSTR_STATIC( "LOLLICHOP" );
        case Demoman_m_FestiveGrenadeLauncher: return ATLAS_XORSTR_STATIC( "GRENADE LAUNCHER" );
        case Demoman_m_TheIronBomber: return ATLAS_XORSTR_STATIC( "IRON BOMBER" );
        case Demoman_m_TheLochnLoad: return ATLAS_XORSTR_STATIC( "LOCH-N-LOAD" );
        case Demoman_s_FestiveStickybombLauncher: return ATLAS_XORSTR_STATIC( "STICKYBOMB LAUNCHER" );
        case Demoman_s_StickyJumper: return ATLAS_XORSTR_STATIC( "STICKY JUMPER" );
        case Demoman_s_TheQuickiebombLauncher: return ATLAS_XORSTR_STATIC( "QUICKIEBOMB LAUNCHER" );
        case Demoman_s_TheScottishResistance: return ATLAS_XORSTR_STATIC( "SCOTTISH RESISTANCE" );
        case Demoman_t_HorselessHeadlessHorsemannsHeadtaker: return ATLAS_XORSTR_STATIC( "HEADTAKER" );
        case Demoman_t_TheScottishHandshake: return ATLAS_XORSTR_STATIC( "SCOTTISH HANDSHAKE" );
        case Demoman_t_FestiveEyelander:
        case Demoman_t_TheEyelander: return ATLAS_XORSTR_STATIC( "EYELANDER" );
        case Demoman_t_TheScotsmansSkullcutter: return ATLAS_XORSTR_STATIC( "SCOTSMAN SKULLCUTTER" );
        case Demoman_t_ThePersianPersuader: return ATLAS_XORSTR_STATIC( "PERSIAN PERSUADER" );
        case Demoman_t_NessiesNineIron: return ATLAS_XORSTR_STATIC( "NESSIES NINE IRON" );
        case Demoman_t_TheClaidheamhMor: return ATLAS_XORSTR_STATIC( "CLAIDHEAMH MOR" );
        case Heavy_m_IronCurtain: return ATLAS_XORSTR_STATIC( "IRON CURTAIN" );
        case Heavy_m_FestiveMinigun: return ATLAS_XORSTR_STATIC( "MINIGUN" );
        case Heavy_m_Tomislav: return ATLAS_XORSTR_STATIC( "TOMISLAV" );
        case Heavy_m_TheBrassBeast: return ATLAS_XORSTR_STATIC( "BRASS BEAST" );
        case Heavy_m_Natascha: return ATLAS_XORSTR_STATIC( "NATASCHA" );
        case Heavy_m_TheHuoLongHeaterG:
        case Heavy_m_TheHuoLongHeater: return ATLAS_XORSTR_STATIC( "HUO-LONG HEATER" );
        case Heavy_s_TheFamilyBusiness: return ATLAS_XORSTR_STATIC( "FAMILY BUSINESS" );
        case Heavy_s_FestiveSandvich:
        case Heavy_s_RoboSandvich:
        case Heavy_s_Sandvich: return ATLAS_XORSTR_STATIC( "SANDVICH" );
        case Heavy_s_Fishcake: return ATLAS_XORSTR_STATIC( "FISHCAKE" );
        case Heavy_s_SecondBanana: return ATLAS_XORSTR_STATIC( "BANANA" );
        case Heavy_s_TheDalokohsBar: return ATLAS_XORSTR_STATIC( "DALOKOHS BAR" );
        case Heavy_s_TheBuffaloSteakSandvich: return ATLAS_XORSTR_STATIC( "BUFFALO STEAK SANDVICH" );
        case Heavy_t_FistsofSteel: return ATLAS_XORSTR_STATIC( "FISTS OF STEEL" );
        case Heavy_t_TheHolidayPunch: return ATLAS_XORSTR_STATIC( "HOLIDAY PUNCH" );
        case Heavy_t_WarriorsSpirit: return ATLAS_XORSTR_STATIC( "WARRIORS SPIRIT" );
        case Heavy_t_TheEvictionNotice: return ATLAS_XORSTR_STATIC( "EVICTION NOTICE" );
        case Heavy_t_TheKillingGlovesofBoxing: return ATLAS_XORSTR_STATIC( "KILLING GLOVES OF BOXING" );
        case Heavy_t_ApocoFists: return ATLAS_XORSTR_STATIC( "APOCO FISTS" );
        case Heavy_t_FestiveGlovesofRunningUrgently:
        case Heavy_t_GlovesofRunningUrgently: return ATLAS_XORSTR_STATIC( "GLOVES OF RUNNING URGENTLY" );
        case Heavy_t_TheBreadBite: return ATLAS_XORSTR_STATIC( "BREAD BITE" );
        case Engi_m_FestiveFrontierJustice: return ATLAS_XORSTR_STATIC( "FRONTIER JUSTICE" );
        case Engi_m_TheWidowmaker: return ATLAS_XORSTR_STATIC( "WIDOWMAKER" );
        case Engi_s_TheGigarCounter:
        case Engi_s_FestiveWrangler: return ATLAS_XORSTR_STATIC( "WRANGLER" );
        case Engi_s_TheShortCircuit: return ATLAS_XORSTR_STATIC( "SHORT CIRCUIT" );
        case Engi_t_FestiveWrench: return ATLAS_XORSTR_STATIC( "WRENCH" );
        case Engi_t_GoldenWrench: return ATLAS_XORSTR_STATIC( "GOLDEN WRENCH" );
        case Engi_t_TheGunslinger: return ATLAS_XORSTR_STATIC( "GUNSLINGER" );
        case Engi_t_TheJag: return ATLAS_XORSTR_STATIC( "JAG" );
        case Engi_t_TheEurekaEffect: return ATLAS_XORSTR_STATIC( "EUREKA EFFECT" );
        case Engi_t_TheSouthernHospitality: return ATLAS_XORSTR_STATIC( "SOUTHERN HOSPITALITY" );
        case Medic_m_FestiveCrusadersCrossbow: return ATLAS_XORSTR_STATIC( "CRUSADERS CROSSBOW" );
        case Medic_m_TheOverdose: return ATLAS_XORSTR_STATIC( "OVERDOSE" );
        case Medic_m_TheBlutsauger: return ATLAS_XORSTR_STATIC( "BLUTSAUGER" );
        case Medic_s_FestiveMediGun: return ATLAS_XORSTR_STATIC( "MEDIGUN" );
        case Medic_s_TheQuickFix: return ATLAS_XORSTR_STATIC( "QUICK-FIX" );
        case Medic_s_TheKritzkrieg: return ATLAS_XORSTR_STATIC( "KRITZKRIEG" );
        case Medic_s_TheVaccinator: return ATLAS_XORSTR_STATIC( "VACCINATOR" );
        case Medic_t_FestiveBonesaw: return ATLAS_XORSTR_STATIC( "BONESAW" );
        case Medic_t_FestiveUbersaw:
        case Medic_t_TheUbersaw: return ATLAS_XORSTR_STATIC( "UBERSAW" );
        case Medic_t_TheVitaSaw: return ATLAS_XORSTR_STATIC( "VITASAW" );
        case Medic_t_TheSolemnVow: return ATLAS_XORSTR_STATIC( "SOLEMN VOW" );
        case Medic_t_Amputator: return ATLAS_XORSTR_STATIC( "AMPUTATOR" );
        case Sniper_m_FestiveSniperRifle: return ATLAS_XORSTR_STATIC( "SNIPER RIFLE" );
        case Sniper_m_FestiveHuntsman:
        case Sniper_m_TheHuntsman: return ATLAS_XORSTR_STATIC( "HUNTSMAN" );
        case Sniper_m_TheMachina: return ATLAS_XORSTR_STATIC( "MACHINA" );
        case Sniper_m_TheAWPerHand: return ATLAS_XORSTR_STATIC( "AWPER HAND" );
        case Sniper_m_TheHitmansHeatmaker: return ATLAS_XORSTR_STATIC( "HITMANS HEATMAKER" );
        case Sniper_m_TheSydneySleeper: return ATLAS_XORSTR_STATIC( "SYDNEY SLEEPER" );
        case Sniper_m_ShootingStar: return ATLAS_XORSTR_STATIC( "SHOOTING STAR" );
        case Sniper_s_FestiveJarate:
        case Sniper_s_TheSelfAwareBeautyMark: return ATLAS_XORSTR_STATIC( "JARATE" );
        case Sniper_s_FestiveSMG: return ATLAS_XORSTR_STATIC( "SMG" );
        case Sniper_t_TheBushwacka: return ATLAS_XORSTR_STATIC( "BUSHWACKA" );
        case Sniper_t_KukriR:
        case Sniper_t_Kukri: return ATLAS_XORSTR_STATIC( "KUKRI" );
        case Sniper_t_TheShahanshah: return ATLAS_XORSTR_STATIC( "SHAHANSHAH" );
        case Sniper_t_TheTribalmansShiv: return ATLAS_XORSTR_STATIC( "TRIBALMANS SHIV" );
        case Spy_m_FestiveRevolver: return ATLAS_XORSTR_STATIC( "REVOLVER" );
        case Spy_m_FestiveAmbassador:
        case Spy_m_TheAmbassador: return ATLAS_XORSTR_STATIC( "AMBASSADOR" );
        case Spy_m_BigKill: return ATLAS_XORSTR_STATIC( "BIG KILL" );
        case Spy_m_TheDiamondback: return ATLAS_XORSTR_STATIC( "DIAMONDBACK" );
        case Spy_m_TheEnforcer: return ATLAS_XORSTR_STATIC( "ENFORCER" );
        case Spy_m_LEtranger: return ATLAS_XORSTR_STATIC( "LETRANGER" );
        case Spy_s_Sapper:
        case Spy_s_SapperR:
        case Spy_s_FestiveSapper: return ATLAS_XORSTR_STATIC( "SAPPER" );
        case Spy_s_TheRedTapeRecorder:
        case Spy_s_TheRedTapeRecorderG: return ATLAS_XORSTR_STATIC( "RED TAPE RECORDER" );
        case Spy_s_TheApSapG: return ATLAS_XORSTR_STATIC( "AP-SAP" );
        case Spy_s_TheSnackAttack: return ATLAS_XORSTR_STATIC( "SNACK ATTACK" );
        case Spy_t_FestiveKnife: return ATLAS_XORSTR_STATIC( "KNIFE" );
        case Spy_t_ConniversKunai: return ATLAS_XORSTR_STATIC( "CONNIVERS KUNAI" );
        case Spy_t_YourEternalReward: return ATLAS_XORSTR_STATIC( "YOUR ETERNAL REWARD" );
        case Spy_t_TheBigEarner: return ATLAS_XORSTR_STATIC( "BIG EARNER" );
        case Spy_t_TheSpycicle: return ATLAS_XORSTR_STATIC( "SPYCICLE" );
        case Spy_t_TheSharpDresser: return ATLAS_XORSTR_STATIC( "SHARP DRESSER" );
        case Spy_t_TheWangaPrick: return ATLAS_XORSTR_STATIC( "WANGA PRICK" );
        case Spy_t_TheBlackRose: return ATLAS_XORSTR_STATIC( "BLACK ROSE" );
        case Heavy_m_Deflector_mvm: return ATLAS_XORSTR_STATIC( "DEFLECTOR" );
        case Misc_t_FryingPan: return ATLAS_XORSTR_STATIC( "FRYING PAN" );
        case Misc_t_GoldFryingPan: return ATLAS_XORSTR_STATIC( "GOLDEN PAN" );
        case Misc_t_Saxxy: return ATLAS_XORSTR_STATIC( "SAXXY" );
        default: break;
    }

    switch ( this->GetWeaponID( ) )
    {
        case TF_WEAPON_SCATTERGUN: return ATLAS_XORSTR_STATIC( "SCATTERGUN" );
        case TF_WEAPON_HANDGUN_SCOUT_PRIMARY: return ATLAS_XORSTR_STATIC( "SHORTSTOP" );
        case TF_WEAPON_HANDGUN_SCOUT_SECONDARY: return ATLAS_XORSTR_STATIC( "PISTOL" );
        case TF_WEAPON_SODA_POPPER: return ATLAS_XORSTR_STATIC( "SODA POPPER" );
        case TF_WEAPON_PEP_BRAWLER_BLASTER: return ATLAS_XORSTR_STATIC( "BABY FACES BLASTER" );
        case TF_WEAPON_PISTOL_SCOUT: return ATLAS_XORSTR_STATIC( "PISTOL" );
        case TF_WEAPON_JAR_MILK: return ATLAS_XORSTR_STATIC( "MAD MILK" );
        case TF_WEAPON_CLEAVER: return ATLAS_XORSTR_STATIC( "CLEAVER" );
        case TF_WEAPON_BAT: return ATLAS_XORSTR_STATIC( "BAT" );
        case TF_WEAPON_BAT_WOOD: return ATLAS_XORSTR_STATIC( "SANDMAN" );
        case TF_WEAPON_BAT_FISH: return ATLAS_XORSTR_STATIC( "HOLY MACKEREL" );
        case TF_WEAPON_BAT_GIFTWRAP: return ATLAS_XORSTR_STATIC( "WRAP ASSASSIN" );
        case TF_WEAPON_ROCKETLAUNCHER: return ATLAS_XORSTR_STATIC( "ROCKET LAUNCHER" );
        case TF_WEAPON_ROCKETLAUNCHER_DIRECTHIT: return ATLAS_XORSTR_STATIC( "DIRECT HIT" );
        case TF_WEAPON_PARTICLE_CANNON: return ATLAS_XORSTR_STATIC( "COW-MANGLER 5000" );
        case TF_WEAPON_SHOTGUN_SOLDIER: return ATLAS_XORSTR_STATIC( "SHOTGUN" );
        case TF_WEAPON_BUFF_ITEM: return ATLAS_XORSTR_STATIC( "BUFF BANNER" );
        case TF_WEAPON_RAYGUN: return ATLAS_XORSTR_STATIC( "RIGHTEOUS BISON" );
        case TF_WEAPON_SHOVEL: return ATLAS_XORSTR_STATIC( "SHOVEL" );
        case TF_WEAPON_FLAMETHROWER: return ATLAS_XORSTR_STATIC( "FLAMETHROWER" );
        case TF_WEAPON_FLAME_BALL: return ATLAS_XORSTR_STATIC( "DRAGONS FURY" );
        case TF_WEAPON_SHOTGUN_PYRO: return ATLAS_XORSTR_STATIC( "SHOTGUN" );
        case TF_WEAPON_FLAREGUN: return ATLAS_XORSTR_STATIC( "FLARE GUN" );
        case TF_WEAPON_FLAREGUN_REVENGE: return ATLAS_XORSTR_STATIC( "MANMELTER" );
        case TF_WEAPON_JAR_GAS: return ATLAS_XORSTR_STATIC( "GAS PASSER" );
        case TF_WEAPON_ROCKETPACK: return ATLAS_XORSTR_STATIC( "THERMAL THRUSTER" );
        case TF_WEAPON_FIREAXE: return ATLAS_XORSTR_STATIC( "FIRE AXE" );
        case TF_WEAPON_BREAKABLE_SIGN: return ATLAS_XORSTR_STATIC( "NEON ANNIHILATOR" );
        case TF_WEAPON_SLAP: return ATLAS_XORSTR_STATIC( "HOT HAND" );
        case TF_WEAPON_GRENADELAUNCHER: return ATLAS_XORSTR_STATIC( "GRENADE LAUNCHER" );
        case TF_WEAPON_PIPEBOMBLAUNCHER: return ATLAS_XORSTR_STATIC( "STICKYBOMB LAUNCHER" );
        case TF_WEAPON_CANNON: return ATLAS_XORSTR_STATIC( "LOOSE CANNON" );
        case TF_WEAPON_BOTTLE: return ATLAS_XORSTR_STATIC( "BOTTLE" );
        case TF_WEAPON_SWORD: return ATLAS_XORSTR_STATIC( "SWORD" );
        case TF_WEAPON_STICKBOMB: return ATLAS_XORSTR_STATIC( "ULLAPOOL CABER" );
        case TF_WEAPON_MINIGUN: return ATLAS_XORSTR_STATIC( "MINIGUN" );
        case TF_WEAPON_SHOTGUN_HWG: return ATLAS_XORSTR_STATIC( "SHOTGUN" );
        case TF_WEAPON_LUNCHBOX: return ATLAS_XORSTR_STATIC( "LUNCHBOX" );
        case TF_WEAPON_FISTS: return ATLAS_XORSTR_STATIC( "FISTS" );
        case TF_WEAPON_SHOTGUN_PRIMARY: return ATLAS_XORSTR_STATIC( "SHOTGUN" );
        case TF_WEAPON_SHOTGUN_BUILDING_RESCUE: return ATLAS_XORSTR_STATIC( "RESCUE RANGER" );
        case TF_WEAPON_SENTRY_REVENGE: return ATLAS_XORSTR_STATIC( "FRONTIER JUSTICE" );
        case TF_WEAPON_DRG_POMSON: return ATLAS_XORSTR_STATIC( "POMSON 6000" );
        case TF_WEAPON_PISTOL: return ATLAS_XORSTR_STATIC( "PISTOL" );
        case TF_WEAPON_LASER_POINTER: return ATLAS_XORSTR_STATIC( "WRANGLER" );
        case TF_WEAPON_MECHANICAL_ARM: return ATLAS_XORSTR_STATIC( "GUNSLINGER" );
        case TF_WEAPON_WRENCH: return ATLAS_XORSTR_STATIC( "WRENCH" );
        case TF_WEAPON_PDA_ENGINEER_DESTROY: return ATLAS_XORSTR_STATIC( "DESTRUCTION PDA" );
        case TF_WEAPON_PDA_ENGINEER_BUILD: return ATLAS_XORSTR_STATIC( "CONSTRUCTION PDA" );
        case TF_WEAPON_BUILDER: return ATLAS_XORSTR_STATIC( "TOOLBOX" );
        case TF_WEAPON_SYRINGEGUN_MEDIC: return ATLAS_XORSTR_STATIC( "SYRINGE GUN" );
        case TF_WEAPON_CROSSBOW: return ATLAS_XORSTR_STATIC( "CROSSBOW" );
        case TF_WEAPON_MEDIGUN: return ATLAS_XORSTR_STATIC( "MEDIGUN" );
        case TF_WEAPON_BONESAW: return ATLAS_XORSTR_STATIC( "BONESAW" );
        case TF_WEAPON_SNIPERRIFLE: return ATLAS_XORSTR_STATIC( "SNIPER RIFLE" );
        case TF_WEAPON_COMPOUND_BOW: return ATLAS_XORSTR_STATIC( "COMPOUND BOW" );
        case TF_WEAPON_SNIPERRIFLE_DECAP: return ATLAS_XORSTR_STATIC( "BAZAAR BARGAIN" );
        case TF_WEAPON_SNIPERRIFLE_CLASSIC: return ATLAS_XORSTR_STATIC( "CLASSIC" );
        case TF_WEAPON_SMG: return ATLAS_XORSTR_STATIC( "SMG" );
        case TF_WEAPON_CHARGED_SMG: return ATLAS_XORSTR_STATIC( "CLEANERS CARBINE" );
        case TF_WEAPON_JAR: return ATLAS_XORSTR_STATIC( "JARATE" );
        case TF_WEAPON_CLUB: return ATLAS_XORSTR_STATIC( "CLUB" );
        case TF_WEAPON_REVOLVER: return ATLAS_XORSTR_STATIC( "REVOLVER" );
        case TF_WEAPON_PDA_SPY_BUILD: return ATLAS_XORSTR_STATIC( "SAPPER" );
        case TF_WEAPON_KNIFE: return ATLAS_XORSTR_STATIC( "KNIFE" );
        case TF_WEAPON_PDA_SPY: return ATLAS_XORSTR_STATIC( "DISGUISE KIT" );
        case TF_WEAPON_INVIS: return ATLAS_XORSTR_STATIC( "INVIS WATCH" );
        case TF_WEAPON_GRAPPLINGHOOK: return ATLAS_XORSTR_STATIC( "GRAPPLING HOOK" );
        default: return nullptr;
    }
}

void CTFPlayerAnimState::Update( float eye_yaw, float eye_pitch )
{
    this->ClearStaleSwimming( );
    util::call_virtual< void ( * )( void *, float, float ) >(
        this, g_tf2.m_tf_player_anim_state_update )( this, eye_yaw, eye_pitch );
}

void CTFPlayerAnimState::ClearStaleSwimming( )
{
    if ( !this->m_bInSwim( ) && this->m_nSpecificMainSequence( ) < 0 )
    {
        return;
    }

    C_BaseEntity *entity{ this->m_pEntity( ) };
    if ( !entity )
    {
        return;
    }

    C_TFPlayer *player{ entity->As< C_TFPlayer * >( ) };
    if ( player->m_nWaterLevel( ) >= 2 || player->InCond( TF_COND_SWIMMING_CURSE ) ||
         player->InCond( TF_COND_SWIMMING_NO_EFFECTS ) || player->InCond( TF_COND_HALLOWEEN_GHOST_MODE ) ||
         player->InCond( TF_COND_TAUNTING ) )
    {
        return;
    }

    if ( this->m_bInSwim( ) )
    {
        // CalcMainActivity skips HandleSwimming while jumping/ducking
        this->m_bInSwim( )         = false;
        this->m_bFirstSwimFrame( ) = true;
    }

    const i32 sequence{ this->m_nSpecificMainSequence( ) };
    if ( sequence < 0 )
    {
        return;
    }

    CStudioHdr *studio{ g_tf2.m_CBaseAnimating_GetModelPtr.call_as< CStudioHdr * >( player ) };
    if ( !studio || !g_tf2.m_CStudioHdr_SequencesAvailable.call_as< bool, CStudioHdr * >( studio ) ||
         sequence >= g_tf2.m_CStudioHdr_GetNumSeq.call_as< i32, CStudioHdr * >( studio ) )
    {
        return;
    }

    switch ( g_tf2.m_CBaseAnimating_GetSequenceActivity.call_as< i32, C_TFPlayer *, i32 >( player, sequence ) )
    {
        case ACT_SWIM:
        case ACT_MP_SWIM:
        case ACT_MP_SWIM_DEPLOYED:
        case ACT_MP_SWIM_PRIMARY:
        case ACT_MP_SWIM_DEPLOYED_PRIMARY:
        case ACT_MP_SWIM_SECONDARY:
        case ACT_MP_SWIM_SECONDARY2:
        case ACT_MP_SWIM_MELEE:
        case ACT_MP_SWIM_ITEM1:
        case ACT_MP_SWIM_ITEM2:
        case ACT_MP_SWIM_ITEM3:
        case ACT_MP_SWIM_ITEM4:
        case ACT_MP_SWIM_BUILDING:
        case ACT_MP_SWIM_BUILDING_DEPLOYED:
        case ACT_MP_SWIM_PDA:
        case ACT_MP_SWIM_LOSERSTATE:
        case ACT_MP_SWIM_MELEE_ALLCLASS:
        case ACT_MP_SWIM_BOMB:
        case ACT_MP_PUSH_SWIM_SECONDARY: this->m_nSpecificMainSequence( ) = -1; break;
        default: break;
    }
}

f32 C_TFWeaponBaseGun::GetProjectileDamage( )
{
    return util::call_virtual< f32 ( * )( void * ) >(
        this, g_tf2.m_tf_weapon_base_get_projectile_damage )( this );
}

ATLAS_FORCE_INLINE f32 building_damage( C_TFWeaponBaseGun *weapon, C_BaseEntity *target, ETFClassID class_id, f32 base )
{
    f32 damage{ g_cl.attrib_hook( weapon, base, ATLAS_XORSTR( "mult_dmg_vs_buildings" ).get( ) ) };
    if ( class_id != ETFClassID::CObjectSentrygun )
    {
        return damage;
    }

    const auto sentry{ target->As< C_ObjectSentrygun * >( ) };

    if ( weapon->GetWeaponID( ) == TF_WEAPON_MINIGUN )
    {
        switch ( sentry->m_iUpgradeLevel( ) )
        {
            case 2: damage *= 1.0f - 0.15f; break;
            case 3: damage *= 1.0f - 0.20f; break;
            default: break;
        }
    }

    // SHIELD_NORMAL_VALUE, SHIELD_MAX_VALUE
    if ( sentry->m_nShieldLevel( ) > 0 && !sentry->m_bHasSapper( ) && !sentry->m_bPlasmaDisable( ) )
    {
        damage *= sentry->m_nShieldLevel( ) == 1 ? 0.33f : 0.10f;
    }

    return damage;
}

// MerasmusModifyDamage
ATLAS_FORCE_INLINE f32 merasmus_damage_scale( i32 weapon_id )
{
    switch ( weapon_id )
    {
        case TF_WEAPON_MINIGUN: return 0.5f;
        case TF_WEAPON_SODA_POPPER: return 1.5f;
        case TF_WEAPON_HANDGUN_SCOUT_PRIMARY: return 1.75f;
        case TF_WEAPON_SCATTERGUN:
        case TF_WEAPON_REVOLVER: return 2.0f;
        case TF_WEAPON_SNIPERRIFLE:
        case TF_WEAPON_SNIPERRIFLE_DECAP:
        case TF_WEAPON_SNIPERRIFLE_CLASSIC:
        case TF_WEAPON_PEP_BRAWLER_BLASTER: return 3.0f;
        default: return 1.0f;
    }
}

f32 C_TFWeaponBaseGun::CalcRangedDamage( C_BaseEntity *target_ent, bool force_crit )
{
    const auto owner_entity{ m_hOwnerEntity( ).Get( ) };
    if ( !owner_entity )
    {
        return 0.0f;
    }

    const auto player{ owner_entity->As< C_TFPlayer * >( ) };
    Assert( target_ent != player && "You're calling CalcRangedDamage on the person who owns this gun. Either you have a code bug or a needed visit with a psychologist." );

    // the client's CTFWeaponBaseGun::GetProjectileDamage: m_nDamage with mult_dmg, mult_dmg_disguised and rage_damage
    const f32        base{ GetProjectileDamage( ) };
    const i32        weapon_id{ GetWeaponID( ) };
    const bool       crit_boosted{ player->IsCritBoosted( ) };
    const ETFClassID class_id{ target_ent->GetClientClass( )->GetClassID( ) };

    switch ( class_id )
    {
        case ETFClassID::CTFPlayer:
        case ETFClassID::CTFTankBoss: break;
        case ETFClassID::CObjectSentrygun:
        case ETFClassID::CObjectDispenser:
        case ETFClassID::CObjectTeleporter: return building_damage( this, target_ent, class_id, base );
        case ETFClassID::CHeadlessHatman: return crit_boosted ? base * TF_DAMAGE_CRIT_MULTIPLIER : base;
        case ETFClassID::CMerasmus: return base * merasmus_damage_scale( weapon_id ) * ( crit_boosted ? TF_DAMAGE_CRIT_MULTIPLIER : 1.0f );
        case ETFClassID::CEyeballBoss: return base * ( weapon_id == TF_WEAPON_MINIGUN ? 0.25f : 1.0f ) * ( crit_boosted ? 2.0f : 1.0f );
        default: return base;
    }

    f32  damage{ g_cl.attrib_hook( this, base, ATLAS_XORSTR( "mult_dmg_vs_players" ).get( ) ) };
    bool crit{ crit_boosted || force_crit };
    bool minicrit{ !crit && ( player->InCond( TF_COND_OFFENSEBUFF ) || // Buff Banner
                              player->InCond( TF_COND_NOHEALINGDAMAGEBUFF ) ||
                              player->InCond( TF_COND_ENERGY_BUFF ) ) }; // Crit-a-Cola

    if ( class_id == ETFClassID::CTFPlayer )
    {
        const auto victim{ target_ent->As< C_TFPlayer * >( ) };

        minicrit = minicrit || ( !crit && ( victim->InCond( TF_COND_URINE ) ||
                                            victim->InCond( TF_COND_MARKEDFORDEATH ) ||
                                            victim->InCond( TF_COND_MARKEDFORDEATH_SILENT ) ||
                                            victim->InCond( TF_COND_PASSTIME_PENALTY_DEBUFF ) ) );

        // Battalion's Backup blocks crits and minicrits outright, then resists on top
        if ( victim->InCond( TF_COND_DEFENSEBUFF ) )
        {
            crit     = false;
            minicrit = false;
        }

        if ( victim->InCond( TF_COND_DEFENSEBUFF_HIGH ) )
        {
            damage *= 0.25f;
        } else if ( victim->InCond( TF_COND_DEFENSEBUFF ) || victim->InCond( TF_COND_DEFENSEBUFF_NO_CRIT_BLOCK ) )
        {
            damage *= 0.65f;
        }
    }

    if ( crit )
    {
        damage *= TF_DAMAGE_CRIT_MULTIPLIER;
    } else
    {
        const f32 distance{ target_ent->WorldSpaceCenter( ).dist_to( player->WorldSpaceCenter( ) ) };
        damage += base * ( distance_scale( weapon_id, distance_spread( weapon_id ), distance, minicrit ) - 1.0f );

        if ( minicrit )
        {
            damage *= TF_DAMAGE_MINICRIT_MULTIPLIER;
        }
    }

    // CTFTankBoss::ModifyDamage
    if ( class_id == ETFClassID::CTFTankBoss && weapon_id == TF_WEAPON_MINIGUN )
    {
        damage *= 0.25f;
    }

    return damage;
}

i32 C_WeaponMedigun::GetMedigunType( ) const
{
    i32 mode{ 0 };
    mode = g_cl.attrib_hook( this, mode, ATLAS_XORSTR( "set_weapon_mode" ).get( ) );
    return mode;
}

bool C_TFRevolver::CanHeadshot( )
{
    i32 mode{ 0 };
    mode = g_cl.attrib_hook( this, mode, ATLAS_XORSTR( "set_weapon_mode" ).get( ) );
    return mode == 1;
}

bool C_TFRevolver::IsReadyForHeadshot( )
{
    Assert( CanHeadshot( ) == true );
    auto delta{ g_tf2.m_globals->curtime - m_flLastAccuracyCheck( ) };
    return delta > 1.f;
}

// https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/game/shared/tf/tf_weapon_sniperrifle.cpp#L1043
bool C_TFSniperRifle::IsReadyForHeadshot( )
{
    const auto owner{ m_hOwnerEntity( ).Get( ) };
    if ( !owner )
    {
        return false;
    }

    const auto owner_player{ owner->As< C_TFPlayer * >( ) };
    return owner_player->IsZoomed( ) &&
           TICKS_TO_TIME( owner_player->m_nTickBase( ) ) - owner_player->m_flFOVTime( ) >= TF_WEAPON_SNIPERRIFLE_NO_CRIT_AFTER_ZOOM_TIME;
}

bool C_TFRevolver::CanHeadshotTarget( C_TFPlayer *target )
{
    Assert( target != nullptr );
    const auto owner{ m_hOwnerEntity( ).Get( ) };
    if ( !owner )
    {
        return false;
    }

    const auto owner_player{ owner->As< C_TFPlayer * >( ) };

    // Always can headshot while critboosted.
    if ( owner_player->IsCritBoosted( ) )
    {
        return true;
    }

    // Range of the Ambassador's headshot is capped.
    if ( ( owner_player->GetAbsOrigin( ) - target->GetAbsOrigin( ) ).length2d_sqr( ) > math::square( 1200.f ) )
    {
        return false;
    }

    return true;
}
