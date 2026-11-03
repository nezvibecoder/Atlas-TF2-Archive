#include "skins.hpp"

#include "gui/gui.hpp"
#include "misc.hpp"
#include "skin_share.hpp"

ATLAS_NO_DESTROY skins g_skins{ };

constexpr twin_t TWINS[]{
    { 0, 190 }, { 1, 191 }, { 2, 192 }, { 3, 193 }, { 4, 194 }, { 5, 195 }, { 6, 196 }, { 7, 197 }, { 8, 198 }, { 9, 199 }, { 10, 199 }, { 11, 199 }, { 12, 199 }, { 13, 200 }, { 14, 201 }, { 15, 202 }, { 16, 203 }, { 17, 204 }, { 18, 205 }, { 19, 206 }, { 20, 207 }, { 21, 208 }, { 22, 209 }, { 23, 209 }, { 24, 210 }, { 29, 211 }, { 735, 736 }
};

constexpr i32 KINDS[ skins::KIND_COUNT ]{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 13, 14, 15,
                                          18, 20, 21, 22, 24, 29, 526, 37, 38, 39, 40, 45,
                                          46, 56, 61, 132, 140, 141, 153, 221, 225, 228, 239, 325, 735 };

constexpr melee_kind_t MELEE_KINDS[]{
    { 0, TF_CLASS_SCOUT, role_t::melee },
    { 1, TF_CLASS_DEMOMAN, role_t::melee },
    { 2, TF_CLASS_PYRO, role_t::melee },
    { 3, TF_CLASS_SNIPER, role_t::melee },
    { 4, TF_CLASS_SPY, role_t::melee },
    { 5, TF_CLASS_HEAVYWEAPONS, role_t::melee },
    { 6, TF_CLASS_SOLDIER, role_t::melee },
    { 7, TF_CLASS_ENGINEER, role_t::melee },
    { 8, TF_CLASS_MEDIC, role_t::melee },
    { 37, TF_CLASS_MEDIC, role_t::melee },
    { 38, TF_CLASS_PYRO, role_t::melee },
    { 132, TF_CLASS_DEMOMAN, role_t::item1 },
    { 153, TF_CLASS_PYRO, role_t::melee },
    { 221, TF_CLASS_SCOUT, role_t::melee },
    { 225, TF_CLASS_SPY, role_t::melee },
    { 239, TF_CLASS_HEAVYWEAPONS, role_t::melee },
    { 325, TF_CLASS_SCOUT, role_t::melee },
};

constexpr usize STOCK_MELEE_KINDS{ 9 };

struct all_class_t
{
    i32         m_definition;
    const char *m_name;
    i32         m_style;
};

constexpr u32 class_bit( i32 player_class )
{
    return 1u << player_class;
}

constexpr i32 MODEL_INDEX_BASE{ 0x4000 };

static const reskin_t *reskins( usize &count )
{
    static const reskin_t TABLE[]{
        { 0, 30667, ATLAS_XORSTR_STATIC( "Batsaber" ) },
        { 0, 660, ATLAS_XORSTR_STATIC( "Festive Bat" ) },
        { 1, 609, ATLAS_XORSTR_STATIC( "Scottish Handshake" ) },
        { 2, 739, ATLAS_XORSTR_STATIC( "Lollichop" ) },
        { 4, 727, ATLAS_XORSTR_STATIC( "Black Rose" ) },
        { 4, 665, ATLAS_XORSTR_STATIC( "Festive Knife" ) },
        { 5, 587, ATLAS_XORSTR_STATIC( "Apoco-Fists" ) },
        { 7, 169, ATLAS_XORSTR_STATIC( "Golden Wrench" ) },
        { 7, 662, ATLAS_XORSTR_STATIC( "Festive Wrench" ) },
        { 8, 1143, ATLAS_XORSTR_STATIC( "Festive Bonesaw" ) },
        { 13, 669, ATLAS_XORSTR_STATIC( "Festive Scattergun" ) },
        { 14, 851, ATLAS_XORSTR_STATIC( "AWPer Hand" ) },
        { 14, 664, ATLAS_XORSTR_STATIC( "Festive Sniper Rifle" ) },
        { 15, 298, ATLAS_XORSTR_STATIC( "Iron Curtain" ) },
        { 15, 654, ATLAS_XORSTR_STATIC( "Festive Minigun" ) },
        { 18, 658, ATLAS_XORSTR_STATIC( "Festive Rocket Launcher" ) },
        { 20, 661, ATLAS_XORSTR_STATIC( "Festive Stickybomb Launcher" ) },
        { 21, 30474, ATLAS_XORSTR_STATIC( "Nostromo Napalmer" ) },
        { 21, 659, ATLAS_XORSTR_STATIC( "Festive Flamethrower" ) },
        { 22, 160, ATLAS_XORSTR_STATIC( "Lugermorph" ) },
        { 22, 30666, ATLAS_XORSTR_STATIC( "C.A.P.P.E.R" ) },
        { 24, 161, ATLAS_XORSTR_STATIC( "Big Kill" ) },
        { 24, 1142, ATLAS_XORSTR_STATIC( "Festive Revolver" ) },
        { 29, 663, ATLAS_XORSTR_STATIC( "Festive Medigun" ) },
        { 526, 30665, ATLAS_XORSTR_STATIC( "Shooting Star" ) },
        { 37, 1003, ATLAS_XORSTR_STATIC( "Festive Ubersaw" ) },
        { 38, 457, ATLAS_XORSTR_STATIC( "Postal Pummeler" ) },
        { 38, 1000, ATLAS_XORSTR_STATIC( "Festive Axtinguisher" ) },
        { 39, 1081, ATLAS_XORSTR_STATIC( "Festive Flare Gun" ) },
        { 40, 1146, ATLAS_XORSTR_STATIC( "Festive Backburner" ) },
        { 45, 1078, ATLAS_XORSTR_STATIC( "Festive Force-a-Nature" ) },
        { 46, 1145, ATLAS_XORSTR_STATIC( "Festive Bonk! Atomic Punch" ) },
        { 56, 1092, ATLAS_XORSTR_STATIC( "Fortified Compound" ) },
        { 56, 1005, ATLAS_XORSTR_STATIC( "Festive Huntsman" ) },
        { 61, 1006, ATLAS_XORSTR_STATIC( "Festive Ambassador" ) },
        { 132, 266, ATLAS_XORSTR_STATIC( "Horseless Headless Horsemann's Headtaker" ) },
        { 132, 482, ATLAS_XORSTR_STATIC( "Nessie's Nine Iron" ) },
        { 132, 1082, ATLAS_XORSTR_STATIC( "Festive Eyelander" ) },
        { 140, 1086, ATLAS_XORSTR_STATIC( "Festive Wrangler" ) },
        { 141, 1004, ATLAS_XORSTR_STATIC( "Festive Frontier Justice" ) },
        { 153, 466, ATLAS_XORSTR_STATIC( "The Maul" ) },
        { 221, 999, ATLAS_XORSTR_STATIC( "Festive Holy Mackerel" ) },
        { 225, 574, ATLAS_XORSTR_STATIC( "Wanga Prick" ) },
        { 228, 1085, ATLAS_XORSTR_STATIC( "Festive Black Box" ) },
        { 239, 1084, ATLAS_XORSTR_STATIC( "Festive Gloves of Running Urgently" ) },
        { 325, 452, ATLAS_XORSTR_STATIC( "Three-Rune Blade" ) },
        { 735, 933, ATLAS_XORSTR_STATIC( "Ap-Sap" ) },
        { 735, 1080, ATLAS_XORSTR_STATIC( "Festive Sapper" ) },
        { 735, 1102, ATLAS_XORSTR_STATIC( "Snack Attack" ) },
    };

    count = sizeof( TABLE ) / sizeof( TABLE[ 0 ] );
    return TABLE;
}

static const all_class_t *all_class_melees( usize &count )
{
    static const all_class_t TABLE[]{
        { 264, ATLAS_XORSTR_STATIC( "Frying Pan" ), -1 },
        { 1071, ATLAS_XORSTR_STATIC( "Golden Frying Pan" ), 0 },
        { 474, ATLAS_XORSTR_STATIC( "Conscientious Objector" ), -1 },
        { 880, ATLAS_XORSTR_STATIC( "Freedom Staff" ), -1 },
        { 939, ATLAS_XORSTR_STATIC( "Bat Outta Hell" ), -1 },
        { 954, ATLAS_XORSTR_STATIC( "Memory Maker" ), -1 },
        { 1013, ATLAS_XORSTR_STATIC( "Ham Shank" ), -1 },
        { 1123, ATLAS_XORSTR_STATIC( "Necro Smasher" ), -1 },
        { 1127, ATLAS_XORSTR_STATIC( "Crossing Guard" ), -1 },
        { 30758, ATLAS_XORSTR_STATIC( "Prinny Machete" ), -1 },
        { 423, ATLAS_XORSTR_STATIC( "Saxxy" ), -1 },
    };

    count = sizeof( TABLE ) / sizeof( TABLE[ 0 ] );
    return TABLE;
}

constexpr i32 AUSTRALIUM_KEYS[]{ 4, 7, 13, 14, 15, 16, 18, 19, 20, 21, 29, 36, 38,
                                 45, 61, 132, 141, 228, 266, 424, 482, 1082, 1085, 1126, 1149 };

constexpr u32 UNUSUAL_EFFECTS[ skins::UNUSUAL_COUNT ]{ 0, WEAPON_UNUSUAL_COMMUNITY_SPARKLE, WEAPON_UNUSUAL_HOT,
                                                       WEAPON_UNUSUAL_ISOTOPE, WEAPON_UNUSUAL_COOL,
                                                       WEAPON_UNUSUAL_ENERGY_ORB };
constexpr u32 KILLSTREAKERS[ skins::KILLSTREAKER_COUNT ]{ 0,
                                                          KILLSTREAKER_FIRE_HORNS,
                                                          KILLSTREAKER_CEREBRAL_DISCHARGE,
                                                          KILLSTREAKER_TORNADO,
                                                          KILLSTREAKER_FLAMES,
                                                          KILLSTREAKER_SINGULARITY,
                                                          KILLSTREAKER_INCINERATOR,
                                                          KILLSTREAKER_HYPNO_BEAM };

// EconWear_ToIntCategory
constexpr float WEARS[ skins::WEAR_COUNT ]{ 0.2f, 0.4f, 0.6f, 0.8f, 1.0f };
constexpr float FOOTPRINTS[ skins::FOOTPRINT_COUNT ]{ 0.0f, 1.0f, 2.0f, 8421376.0f,
                                                      3100495.0f, 5322826.0f, 13595446.0f, 8208497.0f };

constexpr usize DUMP_ROWS_MAX{ 1024 };
constexpr usize DUMP_DEFINITIONS_MAX{ 512 };
constexpr usize DUMP_NAME_MAX{ 64 };
constexpr usize GAME_FILE_MAX{ 32u * 1024u * 1024u };

ATLAS_NO_DESTROY static atl::vector< row_t >        s_effects{ };
ATLAS_NO_DESTROY static atl::vector< const char * > s_effect_labels{ };
ATLAS_NO_DESTROY static atl::vector< row_t >        s_paintkits{ };
ATLAS_NO_DESTROY static atl::vector< const char * > s_paintkit_labels{ };
ATLAS_NO_DESTROY static atl::vector< i32 >          s_paintkit_definitions{ };
ATLAS_NO_DESTROY static atl::vector< char >         s_dump_text{ };

static const row_t *effects( usize &count )
{
    count = s_effects.size( );
    return s_effects.data( );
}

static const row_t *paintkits( usize &count )
{
    count = s_paintkits.size( );
    return s_paintkits.data( );
}

static char *read_game_file( const char *relative, usize &size )
{
    FileHandle_t file{ g_tf2.m_filesystem->Open( relative, ATLAS_XORSTR( "rb" ).get( ),
                                                 ATLAS_XORSTR( "GAME" ).get( ) ) };

    if ( !file )
    {
        return nullptr;
    }

    const u32 bytes{ g_tf2.m_filesystem->Size( file ) };

    if ( !bytes || bytes > GAME_FILE_MAX )
    {
        g_tf2.m_filesystem->Close( file );
        return nullptr;
    }

    auto *data{ static_cast< char * >( atl::malloc( bytes ) ) };

    if ( !data )
    {
        g_tf2.m_filesystem->Close( file );
        return nullptr;
    }

    const i32 read{ g_tf2.m_filesystem->Read( data, static_cast< i32 >( bytes ), file ) };

    g_tf2.m_filesystem->Close( file );

    if ( read <= 0 )
    {
        atl::free( data );
        return nullptr;
    }

    size = static_cast< usize >( read );
    return data;
}

static bool is_layout( char value )
{
    return str::is_space( value ) || value == '\r' || value == '\n';
}

static u32 read_token( const char *data, usize size, usize at, usize &end, bool &numeric )
{
    u32 value{ };

    end     = at + 1;
    numeric = end < size && data[ end ] != '"';

    for ( ; end < size && data[ end ] != '"'; ++end )
    {
        if ( str::is_digit( data[ end ] ) )
        {
            value = value * 10u + static_cast< u32 >( data[ end ] - '0' );
            continue;
        }

        numeric = false;
    }

    return value;
}

static bool opens_block( const char *data, usize size, usize end )
{
    usize at{ end + 1 };

    while ( at < size && is_layout( data[ at ] ) )
    {
        ++at;
    }

    return at < size && data[ at ] == '{';
}

static void parse_effect_ids( const char *data, usize size, atl::vector< u32 > &ids )
{
    const char *section{ ATLAS_XORSTR_STATIC( "\"cosmetic_unusual_effects\"" ) };
    usize       at{ str::find( data, size, section, str::length( section ) ) };

    if ( at == str::npos )
    {
        return;
    }

    while ( at < size && data[ at ] != '{' )
    {
        ++at;
    }

    usize depth{ };

    for ( ; at < size && ids.size( ) < DUMP_ROWS_MAX; ++at )
    {
        if ( data[ at ] == '{' )
        {
            ++depth;
            continue;
        }

        if ( data[ at ] == '}' )
        {
            if ( --depth == 0 )
            {
                return;
            }

            continue;
        }

        if ( data[ at ] != '"' || depth != 1 )
        {
            continue;
        }

        usize end{ };
        bool  numeric{ };
        u32   id{ read_token( data, size, at, end, numeric ) };

        if ( numeric && opens_block( data, size, end ) )
        {
            ids.push_back( id );
        }

        at = end;
    }
}

struct kv_span_t
{
    const char *m_data{ };
    usize       m_size{ };
};

struct prefab_t
{
    kv_span_t m_name{ };
    kv_span_t m_body{ };
    bool      m_festive{ };
};

struct dumped_item_t
{
    i32   m_definition{ };
    u32   m_classes{ };
    usize m_icon{ str::npos };
    usize m_model{ str::npos };
    usize m_festive{ str::npos };
    usize m_attached{ str::npos };
    usize m_prefab{ str::npos };

    // vv only set on botkillers
    usize m_label{ str::npos };
    usize m_head{ str::npos };
    usize m_head_vm{ str::npos };
};

constexpr usize PREFAB_DEPTH_MAX{ 8 };
constexpr usize PREFABS_MAX{ 512 };

ATLAS_NO_DESTROY static atl::vector< i32 >           s_festive_definitions{ };
ATLAS_NO_DESTROY static atl::vector< dumped_item_t > s_dumped_items{ };
ATLAS_NO_DESTROY static atl::vector< prefab_t >      s_prefabs{ };

static bool next_pair( kv_span_t span, usize &from, kv_span_t &key, kv_span_t &value, bool &block )
{
    for ( usize at{ from }; at < span.m_size; ++at )
    {
        if ( span.m_data[ at ] == '/' && at + 1 < span.m_size && span.m_data[ at + 1 ] == '/' )
        {
            while ( at < span.m_size && span.m_data[ at ] != '\n' )
            {
                ++at;
            }

            continue;
        }

        if ( span.m_data[ at ] != '"' )
        {
            continue;
        }

        usize end{ };
        bool  numeric{ };

        read_token( span.m_data, span.m_size, at, end, numeric );

        usize next{ end + 1 };

        while ( next < span.m_size && is_layout( span.m_data[ next ] ) )
        {
            ++next;
        }

        if ( next >= span.m_size )
        {
            return false;
        }

        key = kv_span_t{ span.m_data + at + 1, end - at - 1 };

        if ( span.m_data[ next ] == '{' )
        {
            usize depth{ };
            usize close{ next };

            while ( close < span.m_size )
            {
                if ( span.m_data[ close ] == '{' )
                {
                    ++depth;
                } else if ( span.m_data[ close ] == '}' && --depth == 0 )
                {
                    break;
                }

                ++close;
            }

            if ( close >= span.m_size )
            {
                return false;
            }

            value = kv_span_t{ span.m_data + next + 1, close - next - 1 };
            block = true;
            from  = close + 1;
            return true;
        }

        if ( span.m_data[ next ] != '"' )
        {
            at = end;
            continue;
        }

        usize value_end{ };

        read_token( span.m_data, span.m_size, next, value_end, numeric );

        value = kv_span_t{ span.m_data + next + 1, value_end - next - 1 };
        block = false;
        from  = value_end + 1;
        return true;
    }

    return false;
}

static bool find_key( kv_span_t span, const char *key, usize length, kv_span_t &value, bool &block, usize &from )
{
    kv_span_t name{ };

    while ( next_pair( span, from, name, value, block ) )
    {
        if ( name.m_size == length && str::equals( name.m_data, length, key, length ) )
        {
            return true;
        }
    }

    return false;
}

static bool find_key( kv_span_t span, const char *key, usize length, kv_span_t &value, bool &block )
{
    usize from{ };
    return find_key( span, key, length, value, block, from );
}

static const prefab_t *find_prefab( const char *name, usize length )
{
    for ( const prefab_t &prefab : s_prefabs )
    {
        if ( prefab.m_name.m_size == length &&
             str::equals( prefab.m_name.m_data, length, name, length ) )
        {
            return &prefab;
        }
    }

    return nullptr;
}

static bool resolve_path( kv_span_t span, const char *const *path, usize count, kv_span_t &value )
{
    const usize length{ str::length( path[ 0 ] ) };

    kv_span_t child{ };
    bool      block{ };
    usize     from{ };

    while ( find_key( span, path[ 0 ], length, child, block, from ) )
    {
        if ( count == 1 )
        {
            value = child;
            return true;
        }

        if ( block && resolve_path( child, path + 1, count - 1, value ) )
        {
            return true;
        }
    }

    return false;
}

static bool resolve( kv_span_t definition, const char *const *path, usize count, kv_span_t &value,
                     usize depth = 0 )
{
    if ( count && resolve_path( definition, path, count, value ) )
    {
        return true;
    }

    kv_span_t chain{ };
    bool      block{ };

    if ( depth >= PREFAB_DEPTH_MAX ||
         !find_key( definition, ATLAS_XORSTR_STATIC( "prefab" ), 6, chain, block ) || block )
    {
        return false;
    }

    for ( usize end{ chain.m_size }; end; )
    {
        usize start{ end };

        while ( start && chain.m_data[ start - 1 ] != ' ' )
        {
            --start;
        }

        const prefab_t *prefab{ end > start ? find_prefab( chain.m_data + start, end - start ) : nullptr };

        if ( prefab && resolve( prefab->m_body, path, count, value, depth + 1 ) )
        {
            return true;
        }

        end = start ? start - 1 : 0;
    }

    return false;
}

static const char *const *festive_path( )
{
    static const char *const PATH[]{ ATLAS_XORSTR_STATIC( "visuals" ),
                                     ATLAS_XORSTR_STATIC( "attached_models_festive" ),
                                     ATLAS_XORSTR_STATIC( "0" ), ATLAS_XORSTR_STATIC( "model" ) };
    return PATH;
}

// the 2011 festives keep the stock model and hang their lights off this
static const char *const *attached_path( )
{
    static const char *const PATH[]{ ATLAS_XORSTR_STATIC( "visuals" ),
                                     ATLAS_XORSTR_STATIC( "attached_models" ),
                                     ATLAS_XORSTR_STATIC( "0" ), ATLAS_XORSTR_STATIC( "model" ) };
    return PATH;
}

static void index_prefabs( kv_span_t section )
{
    usize     from{ };
    kv_span_t name{ };
    kv_span_t body{ };
    bool      block{ };

    while ( s_prefabs.size( ) < PREFABS_MAX && next_pair( section, from, name, body, block ) )
    {
        if ( block )
        {
            s_prefabs.push_back( prefab_t{ name, body, false } );
        }
    }

    for ( prefab_t &prefab : s_prefabs )
    {
        kv_span_t value{ };
        prefab.m_festive = resolve( prefab.m_body, festive_path( ), 4, value );
    }
}

static usize store( kv_span_t value )
{
    const usize offset{ s_dump_text.size( ) };

    for ( usize index{ }; index < value.m_size; ++index )
    {
        s_dump_text.push_back( value.m_data[ index ] );
    }

    s_dump_text.push_back( '\0' );
    return offset;
}

static usize store_field( kv_span_t definition, const char *const *path, usize count )
{
    kv_span_t value{ };
    return resolve( definition, path, count, value ) ? store( value ) : str::npos;
}

static u32 usable_classes( kv_span_t definition )
{
    static const struct
    {
        const char *m_name;
        i32         m_class;
    } NAMES[]{
        { ATLAS_XORSTR_STATIC( "scout" ), TF_CLASS_SCOUT },
        { ATLAS_XORSTR_STATIC( "sniper" ), TF_CLASS_SNIPER },
        { ATLAS_XORSTR_STATIC( "soldier" ), TF_CLASS_SOLDIER },
        { ATLAS_XORSTR_STATIC( "demoman" ), TF_CLASS_DEMOMAN },
        { ATLAS_XORSTR_STATIC( "medic" ), TF_CLASS_MEDIC },
        { ATLAS_XORSTR_STATIC( "heavy" ), TF_CLASS_HEAVYWEAPONS },
        { ATLAS_XORSTR_STATIC( "pyro" ), TF_CLASS_PYRO },
        { ATLAS_XORSTR_STATIC( "spy" ), TF_CLASS_SPY },
        { ATLAS_XORSTR_STATIC( "engineer" ), TF_CLASS_ENGINEER },
    };

    const char *path[]{ ATLAS_XORSTR_STATIC( "used_by_classes" ) };
    kv_span_t   block{ };

    if ( !resolve( definition, path, 1, block ) )
    {
        return 0;
    }

    u32 classes{ };

    for ( const auto &entry : NAMES )
    {
        kv_span_t value{ };
        bool      nested{ };

        if ( find_key( block, entry.m_name, str::length( entry.m_name ), value, nested ) )
        {
            classes |= class_bit( entry.m_class );
        }
    }

    return classes;
}

static bool dumped_definition( i32 definition )
{
    usize count{ };

    const reskin_t *reskin_rows{ reskins( count ) };

    for ( usize index{ }; index < count; ++index )
    {
        if ( reskin_rows[ index ].m_definition == definition )
        {
            return true;
        }
    }

    const all_class_t *melee_rows{ all_class_melees( count ) };

    for ( usize index{ }; index < count; ++index )
    {
        if ( melee_rows[ index ].m_definition == definition )
        {
            return true;
        }
    }

    for ( const twin_t &twin : TWINS )
    {
        if ( twin.m_upgradeable == definition )
        {
            return true;
        }
    }

    return false;
}

static bool is_kind( i32 definition )
{
    for ( const i32 kind : KINDS )
    {
        if ( kind == definition )
        {
            return true;
        }
    }

    return false;
}

// "Silver Botkiller Rocket Launcher Mk.I" -> "Silver Botkiller Mk.I", the weapon is already the
// list the row sits in
static usize store_botkiller_label( kv_span_t name, usize botkiller )
{
    const char *word{ ATLAS_XORSTR_STATIC( " Botkiller" ) };
    const char *mark{ ATLAS_XORSTR_STATIC( " Mk." ) };
    const usize variant_end{ botkiller + str::length( word ) };
    const usize mark_at{ str::rfind( name.m_data, name.m_size, mark, str::length( mark ) ) };
    const usize offset{ s_dump_text.size( ) };

    for ( usize index{ }; index < variant_end; ++index )
    {
        s_dump_text.push_back( name.m_data[ index ] );
    }

    for ( usize index{ mark_at }; mark_at != str::npos && index < name.m_size; ++index )
    {
        s_dump_text.push_back( name.m_data[ index ] );
    }

    s_dump_text.push_back( '\0' );
    return offset;
}

static bool chain_has( kv_span_t chain, const char *wanted )
{
    const usize length{ str::length( wanted ) };

    for ( usize word{ }; word < chain.m_size; ++word )
    {
        if ( chain.m_data[ word ] == ' ' )
        {
            continue;
        }

        usize end{ word };

        while ( end < chain.m_size && chain.m_data[ end ] != ' ' )
        {
            ++end;
        }

        if ( end - word == length && str::equals( chain.m_data + word, length, wanted, length ) )
        {
            return true;
        }

        word = end;
    }

    return false;
}

static bool chain_festive( kv_span_t chain )
{
    for ( usize word{ }; word < chain.m_size; ++word )
    {
        if ( chain.m_data[ word ] == ' ' )
        {
            continue;
        }

        usize end{ word };

        while ( end < chain.m_size && chain.m_data[ end ] != ' ' )
        {
            ++end;
        }

        const prefab_t *prefab{ find_prefab( chain.m_data + word, end - word ) };

        if ( prefab && prefab->m_festive )
        {
            return true;
        }

        word = end;
    }

    return false;
}

static u32 attribute_value( kv_span_t definition, const char *name )
{
    const char *path[]{ ATLAS_XORSTR_STATIC( "static_attrs" ), name };
    kv_span_t   value{ };

    if ( !resolve( definition, path, 2, value ) || !value.m_size )
    {
        return 0;
    }

    u32 parsed{ };

    for ( usize index{ }; index < value.m_size; ++index )
    {
        if ( !str::is_digit( value.m_data[ index ] ) )
        {
            return 0;
        }

        parsed = parsed * 10u + static_cast< u32 >( value.m_data[ index ] - '0' );
    }

    return parsed;
}

static kv_span_t root_section( const char *data, usize size, const char *name )
{
    const usize length{ str::length( name ) };
    usize       depth{ };

    for ( usize at{ }; at < size; ++at )
    {
        if ( data[ at ] == '{' )
        {
            ++depth;
            continue;
        }

        if ( data[ at ] == '}' )
        {
            if ( depth )
            {
                --depth;
            }

            continue;
        }

        if ( data[ at ] != '"' )
        {
            continue;
        }

        usize end{ };
        bool  numeric{ };

        read_token( data, size, at, end, numeric );

        if ( depth != 1 || end - at - 1 != length ||
             !str::equals( data + at + 1, length, name, length ) || !opens_block( data, size, end ) )
        {
            at = end;
            continue;
        }

        usize open{ end };

        while ( open < size && data[ open ] != '{' )
        {
            ++open;
        }

        usize inner{ };
        usize close{ open };

        while ( close < size )
        {
            if ( data[ close ] == '{' )
            {
                ++inner;
            } else if ( data[ close ] == '}' && --inner == 0 )
            {
                break;
            }

            ++close;
        }

        return close < size ? kv_span_t{ data + open + 1, close - open - 1 } : kv_span_t{ };
    }

    return kv_span_t{ };
}

static void parse_item_tables( kv_span_t items, atl::vector< u32 > &kits, atl::vector< i32 > &definitions )
{
    const char *tool{ ATLAS_XORSTR_STATIC( "paintkit_tool" ) };
    const char *base{ ATLAS_XORSTR_STATIC( "paintkit_base" ) };
    const char *kit{ ATLAS_XORSTR_STATIC( "paintkit_proto_def_index" ) };
    const char *model_path[]{ ATLAS_XORSTR_STATIC( "model_player" ) };
    const char *icon_path[]{ ATLAS_XORSTR_STATIC( "item_iconname" ) };
    const char *log_path[]{ ATLAS_XORSTR_STATIC( "item_logname" ) };
    const char *head_path[]{ ATLAS_XORSTR_STATIC( "extra_wearable" ) };
    const char *head_vm_path[]{ ATLAS_XORSTR_STATIC( "extra_wearable_vm" ) };
    const char *botkiller_word{ ATLAS_XORSTR_STATIC( " Botkiller " ) };

    usize     from{ };
    kv_span_t key{ };
    kv_span_t body{ };
    bool      block{ };

    while ( next_pair( items, from, key, body, block ) )
    {
        if ( !block || !key.m_size )
        {
            continue;
        }

        u32  definition{ };
        bool numeric{ true };

        for ( usize at{ }; at < key.m_size; ++at )
        {
            if ( !str::is_digit( key.m_data[ at ] ) )
            {
                numeric = false;
                break;
            }

            definition = definition * 10u + static_cast< u32 >( key.m_data[ at ] - '0' );
        }

        if ( !numeric )
        {
            continue;
        }

        const i32 index{ static_cast< i32 >( definition ) };

        kv_span_t  chain{ };
        bool       scalar{ };
        const bool prefabbed{ find_key( body, ATLAS_XORSTR_STATIC( "prefab" ), 6, chain, scalar ) && !scalar };

        if ( prefabbed )
        {
            if ( chain_has( chain, base ) && definitions.size( ) < DUMP_ROWS_MAX )
            {
                definitions.push_back( index );
            } else if ( chain_has( chain, tool ) && kits.size( ) < DUMP_ROWS_MAX )
            {
                if ( const u32 value{ attribute_value( body, kit ) } )
                {
                    kits.push_back( value );
                }
            }
        }

        kv_span_t  festive{ };
        const bool festivized{ resolve_path( body, festive_path( ), 4, festive ) || chain_festive( chain ) };

        if ( festivized && s_festive_definitions.size( ) < DUMP_DEFINITIONS_MAX )
        {
            s_festive_definitions.push_back( index );
        }

        kv_span_t name{ };
        bool      name_block{ };
        usize     botkiller{ str::npos };

        if ( find_key( body, ATLAS_XORSTR_STATIC( "name" ), 4, name, name_block ) && !name_block )
        {
            botkiller = str::find( name.m_data, name.m_size, botkiller_word, str::length( botkiller_word ) );
        }

        if ( botkiller == str::npos && !is_kind( index ) && !dumped_definition( index ) )
        {
            continue;
        }

        dumped_item_t item{ };

        item.m_definition = index;
        item.m_classes    = usable_classes( body );
        item.m_model      = store_field( body, model_path, 1 );
        item.m_icon       = store_field( body, icon_path, 1 );
        item.m_festive    = festivized ? store_field( body, festive_path( ), 4 ) : str::npos;
        item.m_attached   = store_field( body, attached_path( ), 4 );
        item.m_prefab     = prefabbed ? store( chain ) : str::npos;

        if ( item.m_icon == str::npos )
        {
            item.m_icon = store_field( body, log_path, 1 );
        }

        if ( botkiller != str::npos )
        {
            item.m_label   = store_botkiller_label( name, botkiller );
            item.m_head    = store_field( body, head_path, 1 );
            item.m_head_vm = store_field( body, head_vm_path, 1 );
        }

        s_dumped_items.push_back( item );
    }
}

static bool wide_equals( const wchar_t *value, const char *ascii, usize length )
{
    for ( usize index{ }; index < length; ++index )
    {
        if ( value[ index ] != static_cast< wchar_t >( ascii[ index ] ) )
        {
            return false;
        }
    }

    return true;
}

static usize narrow( const wchar_t *value, usize length, char *out, usize capacity )
{
    usize written{ };

    for ( usize index{ }; index < length; ++index )
    {
        const u32 code{ value[ index ] };

        if ( code >= 0xD800 && code <= 0xDFFF )
        {
            continue;
        }

        const usize needed{ code < 0x80 ? 1u : ( code < 0x800 ? 2u : 3u ) };

        if ( written + needed >= capacity )
        {
            break;
        }

        if ( needed == 1 )
        {
            out[ written++ ] = static_cast< char >( code );
        } else if ( needed == 2 )
        {
            out[ written++ ] = static_cast< char >( 0xC0 | ( code >> 6 ) );
            out[ written++ ] = static_cast< char >( 0x80 | ( code & 0x3F ) );
        } else
        {
            out[ written++ ] = static_cast< char >( 0xE0 | ( code >> 12 ) );
            out[ written++ ] = static_cast< char >( 0x80 | ( ( code >> 6 ) & 0x3F ) );
            out[ written++ ] = static_cast< char >( 0x80 | ( code & 0x3F ) );
        }
    }

    out[ written ] = '\0';
    return written;
}

// "<prefix><id><suffix>" "<name>"
static void parse_localized_names( const wchar_t *data, usize length, const char *prefix, const char *suffix,
                                   const atl::vector< u32 > &ids, atl::vector< usize > &offsets )
{
    const usize prefix_length{ str::length( prefix ) };
    const usize suffix_length{ str::length( suffix ) };

    for ( usize at{ }; at + prefix_length < length; ++at )
    {
        if ( !wide_equals( data + at, prefix, prefix_length ) )
        {
            continue;
        }

        usize end{ at + prefix_length };
        u32   id{ };
        usize digits{ };

        for ( ; end < length && data[ end ] >= L'0' && data[ end ] <= L'9'; ++end, ++digits )
        {
            id = id * 10u + static_cast< u32 >( data[ end ] - L'0' );
        }

        at = end;

        if ( !digits || end + suffix_length >= length ||
             !wide_equals( data + end, suffix, suffix_length ) )
        {
            continue;
        }

        usize row{ };

        while ( row < ids.size( ) && ids[ row ] != id )
        {
            ++row;
        }

        usize value{ end + suffix_length };
        if ( row >= ids.size( ) || offsets[ row ] != str::npos || data[ value ] != L'"' )
        {
            continue;
        }

        for ( ++value; value < length && ( data[ value ] == L' ' || data[ value ] == L'\t' ); ++value )
        {
        }

        if ( value >= length || data[ value ] != L'"' )
        {
            continue;
        }

        usize name_end{ ++value };

        while ( name_end < length && data[ name_end ] != L'"' )
        {
            ++name_end;
        }

        char name[ DUMP_NAME_MAX ]{ };

        if ( narrow( data + value, name_end - value, name, sizeof( name ) ) )
        {
            offsets[ row ] = s_dump_text.size( );

            for ( usize index{ }; name[ index ]; ++index )
            {
                s_dump_text.push_back( name[ index ] );
            }

            s_dump_text.push_back( '\0' );
        }

        at = name_end;
    }
}

static void read_localized_names( const char *relative, const char *prefix, const char *suffix,
                                  const atl::vector< u32 > &ids, atl::vector< usize > &offsets )
{
    if ( ids.empty( ) || !offsets.resize( ids.size( ), str::npos ) )
    {
        return;
    }

    usize length{ };
    char *file{ read_game_file( relative, length ) };

    if ( file && length > sizeof( wchar_t ) )
    {
        parse_localized_names( reinterpret_cast< const wchar_t * >( file ) + 1,
                               length / sizeof( wchar_t ) - 1, prefix, suffix, ids, offsets );
    }

    atl::free( file );
}

static void fill_rows( const atl::vector< u32 > &ids, const atl::vector< usize > &offsets,
                       atl::vector< row_t > &rows, atl::vector< const char * > &labels )
{
    rows.push_back( row_t{ 0, ATLAS_XORSTR_STATIC( "None" ) } );

    for ( usize row{ }; row < offsets.size( ); ++row )
    {
        if ( offsets[ row ] != str::npos && ids[ row ] )
        {
            rows.push_back( row_t{ ids[ row ], s_dump_text.data( ) + offsets[ row ] } );
        }
    }

    for ( const row_t &row : rows )
    {
        labels.push_back( row.m_label );
    }
}

static void build_game_tables( )
{
    atl::vector< u32 >   effect_ids{ };
    atl::vector< u32 >   paint_ids{ };
    atl::vector< usize > effect_offsets{ };
    atl::vector< usize > paint_offsets{ };
    usize                size{ };

    if ( char *items{ read_game_file( ATLAS_XORSTR( "scripts/items/items_game.txt" ).get( ), size ) } )
    {
        index_prefabs( root_section( items, size, ATLAS_XORSTR_STATIC( "prefabs" ) ) );

        parse_effect_ids( items, size, effect_ids );

        effect_ids.push_back( WEAPON_UNUSUAL_COMMUNITY_SPARKLE );

        parse_item_tables( root_section( items, size, ATLAS_XORSTR_STATIC( "items" ) ), paint_ids,
                           s_paintkit_definitions );

        // the index points into the file, and nothing outside the walk reads it
        s_prefabs.reset( );
        atl::free( items );
    }

    read_localized_names( ATLAS_XORSTR( "resource/tf_english.txt" ).get( ),
                          ATLAS_XORSTR_STATIC( "\"Attrib_Particle" ), ATLAS_XORSTR_STATIC( "" ),
                          effect_ids, effect_offsets );

    read_localized_names( ATLAS_XORSTR( "resource/tf_proto_obj_defs_english.txt" ).get( ),
                          ATLAS_XORSTR_STATIC( "\"9_" ),
                          ATLAS_XORSTR_STATIC( "_field { field_number: 2 }" ),
                          paint_ids, paint_offsets );

    fill_rows( effect_ids, effect_offsets, s_effects, s_effect_labels );
    fill_rows( paint_ids, paint_offsets, s_paintkits, s_paintkit_labels );
}

struct reskin_model_t
{
    i32         m_definition;
    const char *m_icon;
    const char *m_model;
    const char *m_festive;
    const char *m_attached;
    i32         m_style;
    const char *m_head;
    const char *m_head_vm;
};

static reskin_model_t s_models[ skins::KIND_COUNT ][ skins::MODEL_ROWS_MAX ]{ };
static const char    *s_model_labels[ skins::KIND_COUNT ][ skins::MODEL_ROWS_MAX ]{ };
static usize          s_model_counts[ skins::KIND_COUNT ]{ };

static i32 melee_class( i32 kind )
{
    for ( const melee_kind_t &melee : MELEE_KINDS )
    {
        if ( melee.m_kind == kind )
        {
            return melee.m_class;
        }
    }

    return 0;
}

static i32 melee_kind( i32 player_class )
{
    for ( usize index{ }; index < STOCK_MELEE_KINDS; ++index )
    {
        if ( MELEE_KINDS[ index ].m_class == player_class )
        {
            return MELEE_KINDS[ index ].m_kind;
        }
    }

    return -1;
}

static role_t kind_role( i32 kind )
{
    for ( const melee_kind_t &melee : MELEE_KINDS )
    {
        if ( melee.m_kind == kind )
        {
            return melee.m_role;
        }
    }

    return role_t::none;
}

static const dumped_item_t *dumped( i32 definition )
{
    for ( const dumped_item_t &item : s_dumped_items )
    {
        if ( item.m_definition == definition )
        {
            return &item;
        }
    }

    return nullptr;
}

static const char *dumped_text( usize offset )
{
    return offset == str::npos ? nullptr : s_dump_text.data( ) + offset;
}

static void build_model_rows( )
{
    usize              reskin_count{ };
    const reskin_t    *reskin_rows{ reskins( reskin_count ) };
    usize              melee_count{ };
    const all_class_t *melee_rows{ all_class_melees( melee_count ) };

    for ( usize kind{ }; kind < skins::KIND_COUNT; ++kind )
    {
        usize rows{ };

        s_models[ kind ][ rows ]       = reskin_model_t{ -1, nullptr, nullptr, nullptr, nullptr, -1 };
        s_model_labels[ kind ][ rows ] = ATLAS_XORSTR_STATIC( "Default" );
        ++rows;

        for ( usize index{ }; index < reskin_count && rows < skins::MODEL_ROWS_MAX; ++index )
        {
            const reskin_t &row{ reskin_rows[ index ] };

            if ( row.m_kind != KINDS[ kind ] )
            {
                continue;
            }

            const dumped_item_t *item{ dumped( row.m_definition ) };
            if ( !item || item->m_model == str::npos )
            {
                continue;
            }

            s_models[ kind ][ rows ]       = reskin_model_t{ row.m_definition, dumped_text( item->m_icon ),
                                                             dumped_text( item->m_model ),
                                                             dumped_text( item->m_festive ),
                                                             dumped_text( item->m_attached ), -1 };
            s_model_labels[ kind ][ rows ] = row.m_name;
            ++rows;
        }

        const i32 player_class{ melee_class( KINDS[ kind ] ) };

        for ( usize index{ }; player_class && index < melee_count && rows < skins::MODEL_ROWS_MAX; ++index )
        {
            const all_class_t   &row{ melee_rows[ index ] };
            const dumped_item_t *item{ dumped( row.m_definition ) };

            if ( !item || item->m_model == str::npos || !( item->m_classes & class_bit( player_class ) ) )
            {
                continue;
            }

            s_models[ kind ][ rows ]       = reskin_model_t{ row.m_definition, dumped_text( item->m_icon ),
                                                             dumped_text( item->m_model ),
                                                             dumped_text( item->m_festive ),
                                                             dumped_text( item->m_attached ), row.m_style };
            s_model_labels[ kind ][ rows ] = row.m_name;
            ++rows;
        }

        const dumped_item_t *weapon{ dumped( KINDS[ kind ] ) };
        const char          *prefab{ weapon ? dumped_text( weapon->m_prefab ) : nullptr };

        for ( usize index{ }; prefab && index < s_dumped_items.size( ) && rows < skins::MODEL_ROWS_MAX; ++index )
        {
            const dumped_item_t &item{ s_dumped_items[ index ] };

            if ( item.m_head == str::npos || item.m_model == str::npos || item.m_prefab == str::npos ||
                 !str::equals( dumped_text( item.m_prefab ), prefab ) )
            {
                continue;
            }

            s_models[ kind ][ rows ]       = reskin_model_t{ item.m_definition, dumped_text( item.m_icon ),
                                                             dumped_text( item.m_model ), nullptr, nullptr, -1,
                                                             dumped_text( item.m_head ), dumped_text( item.m_head_vm ) };
            s_model_labels[ kind ][ rows ] = dumped_text( item.m_label );
            ++rows;
        }

        s_model_counts[ kind ] = rows;
    }
}

static usize kind_for( i32 key )
{
    if ( key == 23 )
    {
        key = 22;
    }

    for ( usize kind{ }; kind < skins::KIND_COUNT; ++kind )
    {
        if ( KINDS[ kind ] == key )
        {
            return kind;
        }
    }

    return skins::KIND_COUNT;
}

static const reskin_model_t *model_for( usize kind, usize row )
{
    if ( kind >= skins::KIND_COUNT || row == 0 || row >= s_model_counts[ kind ] )
    {
        return nullptr;
    }

    return &s_models[ kind ][ row ];
}

static i32 stock_for( i32 definition )
{
    for ( const twin_t &twin : TWINS )
    {
        if ( twin.m_upgradeable == definition )
        {
            return twin.m_stock;
        }
    }

    return definition;
}

static i32 upgradeable_for( i32 definition )
{
    for ( const twin_t &twin : TWINS )
    {
        if ( twin.m_stock == definition )
        {
            return twin.m_upgradeable;
        }
    }

    return definition;
}

static bool is_all_class( i32 definition )
{
    usize              melee_count{ };
    const all_class_t *melee_rows{ all_class_melees( melee_count ) };

    for ( usize index{ }; index < melee_count; ++index )
    {
        if ( melee_rows[ index ].m_definition == definition )
        {
            return true;
        }
    }

    return false;
}

static i32 key_for( i32 definition, i32 player_class )
{
    const i32 key{ stock_for( definition ) };

    usize           reskin_count{ };
    const reskin_t *reskin_rows{ reskins( reskin_count ) };

    for ( usize index{ }; index < reskin_count; ++index )
    {
        if ( reskin_rows[ index ].m_definition == key )
        {
            return reskin_rows[ index ].m_kind;
        }
    }

    if ( is_all_class( key ) )
    {
        const i32 kind{ melee_kind( player_class ) };
        return kind >= 0 ? kind : key;
    }

    return key;
}

static role_t weapon_role( i32 definition, i32 player_class )
{
    if ( is_all_class( definition ) )
    {
        return role_t::all_class;
    }

    const usize kind{ kind_for( key_for( definition, player_class ) ) };
    return kind < skins::KIND_COUNT ? kind_role( KINDS[ kind ] ) : role_t::none;
}

static const char *role_prefix( role_t role )
{
    switch ( role )
    {
        case role_t::melee:
            return ATLAS_XORSTR_STATIC( "ACT_MELEE_VM_" );
        case role_t::all_class:
            return ATLAS_XORSTR_STATIC( "ACT_MELEE_ALLCLASS_VM_" );
        case role_t::item1:
            return ATLAS_XORSTR_STATIC( "ACT_ITEM1_VM_" );
        default:
            return nullptr;
    }
}

static const char *festive_model_for( i32 definition )
{
    const dumped_item_t *item{ dumped( definition ) };
    return item ? dumped_text( item->m_festive ) : nullptr;
}

// a botkiller is its own item definition without australium or festive versions
static bool is_botkiller( const reskin_model_t *model )
{
    return model && model->m_head;
}

static bool australium_capable( i32 key )
{
    for ( const i32 candidate : AUSTRALIUM_KEYS )
    {
        if ( candidate == key )
        {
            return true;
        }
    }

    return false;
}

static bool festive_capable( i32 key )
{
    const i32 definition{ upgradeable_for( key ) };

    for ( const i32 candidate : s_festive_definitions )
    {
        if ( candidate == definition )
        {
            return true;
        }
    }

    return false;
}

constexpr i32   GOLD_DEFINITIONS[]{ 169, 423, 1071 };
constexpr float GOLD_LIFETIME{ 20.0f };

static bool turns_to_gold( i32 definition )
{
    for ( const i32 candidate : GOLD_DEFINITIONS )
    {
        if ( candidate == definition )
        {
            return true;
        }
    }

    return false;
}

static bool paintkit_capable( i32 key )
{
    const i32 definition{ upgradeable_for( key ) };

    for ( const i32 candidate : s_paintkit_definitions )
    {
        if ( candidate == definition )
        {
            return true;
        }
    }

    return false;
}

static u32 bits( float value )
{
    return __builtin_bit_cast( u32, value );
}

static const reskin_model_t *model_by_definition( usize kind, i32 definition )
{
    if ( kind >= skins::KIND_COUNT || definition < 0 )
    {
        return nullptr;
    }

    // row 0 is the weapon's own model
    for ( usize row{ 1 }; row < s_model_counts[ kind ]; ++row )
    {
        if ( s_models[ kind ][ row ].m_definition == definition )
        {
            return &s_models[ kind ][ row ];
        }
    }

    return nullptr;
}

template< usize count >
static bool listed( const u32 ( &values )[ count ], u32 value )
{
    for ( const u32 candidate : values )
    {
        if ( candidate == value )
        {
            return true;
        }
    }

    return false;
}

static bool listed( const atl::vector< row_t > &rows, u32 value )
{
    for ( const row_t &row : rows )
    {
        if ( row.m_value == value )
        {
            return true;
        }
    }

    return false;
}

static shared_weapon_t resolve( i32 key, const config::skin_t &skin )
{
    const reskin_model_t *model{ model_for( kind_for( key ), skin.m_model ) };
    usize                 paint_count{ };
    const row_t          *paint_rows{ paintkits( paint_count ) };
    shared_weapon_t       weapon{ };

    weapon.m_key          = key;
    weapon.m_model        = model ? model->m_definition : -1;
    weapon.m_paintkit     = paintkit_capable( key ) && skin.m_paintkit < paint_count ? paint_rows[ skin.m_paintkit ].m_value : 0u;
    weapon.m_wear         = static_cast< u32 >( skin.m_wear < skins::WEAR_COUNT ? skin.m_wear : 0 );
    weapon.m_seed         = static_cast< u32 >( skin.m_seed );
    weapon.m_australium   = skin.m_australium && !is_botkiller( model );
    weapon.m_festivized   = skin.m_festivized && !is_botkiller( model );
    weapon.m_unusual      = skin.m_unusual < skins::UNUSUAL_COUNT ? UNUSUAL_EFFECTS[ skin.m_unusual ] : 0u;
    weapon.m_killstreak   = static_cast< u32 >( skin.m_killstreak < skins::KILLSTREAK_COUNT ? skin.m_killstreak : 0 );
    weapon.m_sheen        = static_cast< u32 >( skin.m_sheen < skins::SHEEN_COUNT ? skin.m_sheen : 0 );
    weapon.m_killstreaker = skin.m_killstreaker < skins::KILLSTREAKER_COUNT ? KILLSTREAKERS[ skin.m_killstreaker ] : 0u;

    return weapon;
}

static const shared_weapon_t *shared_weapon( const shared_loadout_t &loadout, i32 key )
{
    for ( usize index{ }; index < loadout.m_weapon_count; ++index )
    {
        if ( loadout.m_weapons[ index ].m_key == key )
        {
            return &loadout.m_weapons[ index ];
        }
    }

    return nullptr;
}

static const shared_wearable_t *shared_wearable( const shared_loadout_t &loadout, i32 definition )
{
    for ( usize index{ }; index < loadout.m_wearable_count; ++index )
    {
        if ( loadout.m_wearables[ index ].m_definition == definition )
        {
            return &loadout.m_wearables[ index ];
        }
    }

    return nullptr;
}

// "models/weapons/c_models/c_rocketlauncher/c_rocketlauncher.mdl" -> "c_rocketlauncher"
static void model_stem( C_BaseEntity *entity, fixed_string< 64 > &out )
{
    const model_t *model{ entity->GetModel( ) };
    const char    *path{ model ? g_tf2.m_model_info->GetModelName( model ) : nullptr };

    if ( !path )
    {
        out.assign( string_view{ } );
        return;
    }

    const char *stem{ path };
    for ( const char *cursor{ path }; *cursor; ++cursor )
    {
        if ( *cursor == '/' || *cursor == '\\' )
        {
            stem = cursor + 1;
        }
    }

    usize length{ };
    while ( stem[ length ] && stem[ length ] != '.' )
    {
        ++length;
    }

    out.assign( string_view{ stem, length } );
}

static bool definition_name( i32 definition, fixed_string< 64 > &out )
{
    void       *schema{ g_tf2.m_GetItemSchema.call_as< void * >( ) };
    const u8   *item{ g_tf2.m_CEconItemSchema_GetItemDefinition.call_as< const u8 *, void *, i32 >( schema, definition ) };
    const char *name{ item ? *reinterpret_cast< const char *const * >( item + 0x180 ) : nullptr };

    if ( !name || !*name )
    {
        return false;
    }

    const char *upgradeable{ ATLAS_XORSTR_STATIC( "Upgradeable " ) };
    const char *weapon{ ATLAS_XORSTR_STATIC( "TF_WEAPON_" ) };
    usize       length{ str::length( name ) };

    if ( str::starts_with( name, length, upgradeable, str::length( upgradeable ) ) )
    {
        name += str::length( upgradeable );
        length -= str::length( upgradeable );
    }

    if ( !str::starts_with( name, length, weapon, str::length( weapon ) ) )
    {
        out.assign( string_view{ name, length } );
        return true;
    }

    name += str::length( weapon );
    length -= str::length( weapon );

    out.clear( );

    bool word_start{ true };
    for ( usize index{ }; index < length; ++index )
    {
        if ( name[ index ] == '_' )
        {
            out.push_back( ' ' );
            word_start = true;
            continue;
        }

        out.push_back( word_start ? name[ index ] : str::to_lower( name[ index ] ) );
        word_start = false;
    }

    return true;
}

static C_TFPlayer *local_player( ) // doing this because of when we update certain stuff, i used to crash on sentinel bcs of not doing this
{
    if ( !g_tf2.m_engine->IsInGame( ) )
    {
        return nullptr;
    }

    IClientEntity *client_entity{ g_tf2.m_entity_list->GetClientEntity( g_tf2.m_engine->GetLocalPlayer( ) ) };
    C_BaseEntity  *entity{ client_entity ? client_entity->GetBaseEntity( ) : nullptr };
    ClientClass   *client_class{ entity ? entity->GetClientClass( ) : nullptr };

    if ( !client_class || client_class->GetClassID( ) != ETFClassID::CTFPlayer )
    {
        return nullptr;
    }

    return entity->As< C_TFPlayer * >( );
}

static void release_viewmodel_attachment( C_TFWeaponBase *weapon )
{
    if ( C_BaseEntity * attachment{ weapon->m_hViewmodelAttachment( ).Get( ) } )
    {
        attachment->GetClientNetworkable( )->Release( );
    }
}

static void hide_particles( C_EconEntity *entity )
{
    i32          &effects{ entity->m_fEffects( ) };
    EHANDLE      &owner{ entity->m_hOwnerEntity( ) };
    const i32     effects_before{ effects };
    const EHANDLE owner_before{ owner };

    effects |= EF_NODRAW;
    owner = EHANDLE{ };

    entity->GetClientNetworkable( )->OnDataChanged( DATA_UPDATE_DATATABLE_CHANGED );

    effects = effects_before;
    owner   = owner_before;
}

static void release_material( IMaterial *&material )
{
    if ( material )
    {
        material->DecrementReferenceCount( );
        material = nullptr;
    }
}

static void forget_paintkit( C_EconEntity *entity )
{
    for ( IMaterial *&material : entity->m_MaterialOverrides( ).m_teams )
    {
        release_material( material );
    }

    item_material_overrides_t &item{ entity->m_ItemMaterialOverrides( ) };

    for ( usize team{ }; team < sizeof( item.m_teams ) / sizeof( item.m_teams[ 0 ] ); ++team )
    {
        release_material( item.m_teams[ team ] );
        item.m_initialized[ team ] = false;
    }

    entity->m_nWeaponSkinGeneration( ) = 0;
}

static CEconItemAttribute *find_attribute( CAttributeList &list, u16 index )
{
    for ( i32 entry{ }; entry < list.m_Size; ++entry )
    {
        if ( list.m_pMemory[ entry ].m_iAttributeDefinitionIndex == index )
        {
            return &list.m_pMemory[ entry ];
        }
    }

    return nullptr;
}

static bool write_attribute( CAttributeList &list, u16 index, u32 raw_value )
{
    void       *schema{ g_tf2.m_GetItemSchema.call_as< void * >( ) };
    const void *definition{
        g_tf2.m_CEconItemSchema_GetAttributeDefinition.call_as< const void *, void *, i32 >( schema, index )
    };

    if ( !definition )
    {
        return false;
    }

    g_tf2.m_CAttributeList_SetRuntimeAttributeValue.call_as< void, CAttributeList *, const void *, float >(
        &list, definition, __builtin_bit_cast( float, raw_value ) );

    return true;
}

static void remove_attribute( CAttributeList &list, u16 index )
{
    CEconItemAttribute *attribute{ find_attribute( list, index ) };

    if ( !attribute )
    {
        return;
    }

    write_attribute( list, index, attribute->m_iRawValue32 );

    for ( CEconItemAttribute *next{ attribute + 1 }; next < list.m_pMemory + list.m_Size; ++next )
    {
        next[ -1 ] = *next;
    }

    --list.m_Size;
}

static i32 bone_by_name( const studiohdr_t *studio, const char *name )
{
    for ( i32 index{ }; index < studio->numbones; ++index )
    {
        if ( str::equals( studio->pBone( index )->pszName( ), name ) )
        {
            return index;
        }
    }

    return -1;
}

static void build_jiggle( const mstudiojigglebone_t &info,
                          jiggle_bone_t             &state,
                          const matrix3x4            goal,
                          float                      step,
                          bool                       reset,
                          matrix3x4                  out )
{
    const vec3 base{ goal[ 0 ][ 3 ], goal[ 1 ][ 3 ], goal[ 2 ][ 3 ] };
    const vec3 left{ goal[ 0 ][ 0 ], goal[ 1 ][ 0 ], goal[ 2 ][ 0 ] };
    const vec3 up{ goal[ 0 ][ 1 ], goal[ 1 ][ 1 ], goal[ 2 ][ 1 ] };
    const vec3 forward{ goal[ 0 ][ 2 ], goal[ 1 ][ 2 ], goal[ 2 ][ 2 ] };
    const vec3 goal_tip{ base + forward * info.length };

    if ( reset )
    {
        state = jiggle_bone_t{ goal_tip, vec3{ }, base, base, vec3{ } };
    }

    atl::memcpy( out, goal, sizeof( matrix3x4 ) );

    if ( info.flags & JIGGLE_IS_FLEXIBLE )
    {
        if ( step > 0.0f )
        {
            const vec3 error{ goal_tip - state.m_tip };
            const f32  yaw{ info.yawStiffness * left.dot( error ) - info.yawDamping * left.dot( state.m_tip_velocity ) };
            const f32  pitch{ info.pitchStiffness * up.dot( error ) - info.pitchDamping * up.dot( state.m_tip_velocity ) };
            vec3       acceleration{ left * yaw + up * pitch };

            if ( !( info.flags & JIGGLE_HAS_LENGTH_CONSTRAINT ) )
            {
                acceleration += forward * ( info.alongStiffness * forward.dot( error ) -
                                            info.alongDamping * forward.dot( state.m_tip_velocity ) );
            }

            acceleration.z -= info.tipMass;

            state.m_tip_velocity += acceleration * step;
            state.m_tip += state.m_tip_velocity * step;
        }

        if ( info.flags & JIGGLE_HAS_PITCH_CONSTRAINT )
        {
            const vec3 along{ state.m_tip - base };
            const f32  pitch{ math::atan2( up.dot( along ), forward.dot( along ) ) };

            if ( pitch < info.minPitch || pitch > info.maxPitch )
            {
                const f32  limit{ pitch < info.minPitch ? info.minPitch : info.maxPitch };
                const vec3 limit_forward{ up * math::sin( limit ) + forward * math::cos( limit ) };

                state.m_tip          = base + left * left.dot( along ) + limit_forward * limit_forward.dot( along );
                state.m_tip_velocity = vec3{ };
            }
        }

        vec3 direction{ ( state.m_tip - base ).normalized( ) };

        if ( info.flags & JIGGLE_HAS_ANGLE_CONSTRAINT )
        {
            const f32 dot{ direction.dot( forward ) };
            const f32 between{ dot < 0.0f ? 2.0f * PI - math::acos( dot ) : math::acos( dot ) };

            if ( between > info.angleLimit )
            {
                state.m_tip = goal_tip - ( goal_tip - state.m_tip ).normalized( ) * ( info.length * math::sin( info.angleLimit ) );
                direction   = ( state.m_tip - base ).normalized( );
            }
        }

        if ( info.flags & JIGGLE_HAS_LENGTH_CONSTRAINT )
        {
            state.m_tip = base + direction * info.length;
            state.m_tip_velocity -= direction * state.m_tip_velocity.dot( direction );
        }

        const bool flipped{ left.cross( up ).dot( forward ) < 0.0f };
        const vec3 side{ ( flipped ? direction.cross( up ) : up.cross( direction ) ).normalized( ) };
        const vec3 lift{ flipped ? side.cross( direction ) : direction.cross( side ) };

        for ( i32 row{ }; row < 3; ++row )
        {
            out[ row ][ 0 ] = side[ row ];
            out[ row ][ 1 ] = lift[ row ];
            out[ row ][ 2 ] = direction[ row ];
        }
    }

    if ( info.flags & JIGGLE_HAS_BASE_SPRING )
    {
        if ( step > 0.0f )
        {
            vec3 acceleration{ ( base - state.m_base ) * info.baseStiffness - state.m_base_velocity * info.baseDamping };
            acceleration.z -= info.baseMass;

            state.m_base_velocity += acceleration * step;
            state.m_base += state.m_base_velocity * step;

            const vec3 error{ state.m_base - base };

            state.m_base = base + left * math::clamp( left.dot( error ), info.baseMinLeft, info.baseMaxLeft ) +
                           up * math::clamp( up.dot( error ), info.baseMinUp, info.baseMaxUp ) +
                           forward * math::clamp( forward.dot( error ), info.baseMinForward, info.baseMaxForward );

            state.m_base_velocity = ( state.m_base - state.m_base_last ) * ( 1.0f / step );
            state.m_base_last     = state.m_base;
        }

        out[ 0 ][ 3 ] = state.m_base.x;
        out[ 1 ][ 3 ] = state.m_base.y;
        out[ 2 ][ 3 ] = state.m_base.z;
    }
}

static void pose_head_bones( const studiohdr_t *head,
                             const studiohdr_t *weapon,
                             const matrix3x4   *weapon_bones,
                             jiggle_bone_t     *jiggles,
                             usize              jiggle_count,
                             float              step,
                             bool               reset,
                             matrix3x4         *out )
{
    usize jiggle{ };

    for ( i32 index{ }; index < head->numbones; ++index )
    {
        const mstudiobone_t *bone{ head->pBone( index ) };

        if ( const i32 shared{ bone_by_name( weapon, bone->pszName( ) ) }; shared >= 0 )
        {
            atl::memcpy( out[ index ], weapon_bones[ shared ], sizeof( matrix3x4 ) );
            continue;
        }

        const quaternion rotation{ bone->quat[ 0 ], bone->quat[ 1 ], bone->quat[ 2 ], bone->quat[ 3 ] };
        matrix3x4        local{ };
        matrix3x4        goal{ };

        math::quaternion_matrix( rotation, bone->pos, local );
        math::concat_transforms( bone->parent >= 0 ? out[ bone->parent ] : weapon_bones[ 0 ], local, goal );

        if ( ( bone->flags & BONE_ALWAYS_PROCEDURAL ) && bone->proctype == STUDIO_PROC_JIGGLE && jiggle < jiggle_count )
        {
            build_jiggle( *static_cast< const mstudiojigglebone_t * >( bone->pProcedure( ) ), jiggles[ jiggle++ ], goal,
                          step, reset, out[ index ] );
            continue;
        }

        atl::memcpy( out[ index ], goal, sizeof( matrix3x4 ) );
    }
}

static void match_viewmodel_role( C_BaseViewModel *viewmodel, role_t have, role_t want )
{
    const char *from{ role_prefix( have ) };
    const char *to{ role_prefix( want ) };

    if ( !from || !to )
    {
        return;
    }

    CStudioHdr *studio{ g_tf2.m_CBaseAnimating_GetModelPtr.call_as< CStudioHdr * >( viewmodel ) };
    const i32   count{ studio ? g_tf2.m_CStudioHdr_GetNumSeq.call_as< i32 >( studio ) : 0 };
    const i32   sequence{ viewmodel->m_nSequence( ) };

    if ( sequence < 0 || sequence >= count )
    {
        return;
    }

    const mstudioseqdesc_t *current{ g_tf2.m_CStudioHdr_pSeqdesc.call_as< const mstudioseqdesc_t * >( studio, sequence ) };
    const char             *activity{ current->pszActivityName( ) };
    const usize             length{ str::length( activity ) };
    const usize             from_length{ str::length( from ) };

    if ( !str::starts_with( activity, length, from, from_length ) )
    {
        return;
    }

    fixed_string< 64 > wanted{ };
    wanted.assign( string_view{ to } );
    wanted.append( string_view{ activity + from_length, length - from_length } );

    for ( i32 index{ }; index < count; ++index )
    {
        const mstudioseqdesc_t *candidate{ g_tf2.m_CStudioHdr_pSeqdesc.call_as< const mstudioseqdesc_t * >( studio, index ) };

        if ( str::equals( candidate->pszActivityName( ), wanted.c_str( ) ) )
        {
            viewmodel->m_nSequence( ) = index;
            return;
        }
    }
}

void skins::publish_lists( ) const
{
    const auto fill{ []( gui::dynamic_list_box *list, const char *const *labels, usize count )
    {
        if ( !list )
        {
            return;
        }

        for ( usize index{ }; index < count; ++index )
        {
            if ( !list->insert_element( labels[ index ] ) )
            {
                return;
            }
        }
    } };

    fill( this->m_effect_list, s_effect_labels.data( ), s_effect_labels.size( ) );
    fill( this->m_paintkit_list, s_paintkit_labels.data( ), s_paintkit_labels.size( ) );

    for ( usize kind{ }; kind < KIND_COUNT; ++kind )
    {
        fill( this->m_model_lists[ kind ], s_model_labels[ kind ], s_model_counts[ kind ] );
    }
}

void skins::service_game_tables( )
{
    if ( !this->m_tables_requested.load( ) || this->m_tables_ready.load( ) )
    {
        return;
    }

    build_game_tables( );
    build_model_rows( );
    this->publish_lists( );
    this->m_tables_ready.store( true );
}

void skins::think( )
{
    static const i32 s_attribute_list{ g_netvars.get_offset( HASH_CT( "DT_EconEntity" ), HASH_CT( "m_AttributeList" ) ) };
    static const i32 s_streaks{ g_netvars.get_offset( HASH_CT( "DT_TFPlayer" ), HASH_CT( "m_nStreaks" ) ) };

    if ( !s_attribute_list || !s_streaks )
    {
        return;
    }

    config::skins_t &cfg{ g_cfg.m_skins };
    const bool       changed{ this->refresh_wanted( ) };

    if ( changed || ( !this->m_own_ready && this->m_tables_ready.load( ) ) )
    {
        this->rebuild_own( );
    }

    if ( this->m_own_ready )
    {
        g_skin_share.update( );
    }

    C_TFPlayer *local{ local_player( ) };
    this->m_in_game         = local != nullptr;
    this->m_botkiller_count = 0;

    if ( !local )
    {
        this->m_held_key   = -1;
        this->m_held_kind  = KIND_COUNT;
        this->m_seen_count = 0;

        for ( usize slot{ }; slot < config::SKIN_WEARABLE_SLOTS; ++slot )
        {
            this->m_wearable_keys[ slot ] = -1;
        }

        return;
    }

    const i32 player_class{ local->m_iClass( ) };

    this->publish_wearables( local );

    this->m_seen_next_count = 0;

    const seen_t *active{ this->dress_player( local, cfg.m_enabled ? &this->m_own : nullptr ) };

    if ( cfg.m_shared && !g_skin_share.empty( ) )
    {
        this->dress_peers( );
    }

    this->undress_unclaimed( );

    for ( usize index{ }; index < this->m_seen_next_count; ++index )
    {
        this->m_seen[ index ] = this->m_seen_next[ index ];
    }

    this->m_seen_count = this->m_seen_next_count;

    this->publish_held( local, active );

    if ( !cfg.m_enabled )
    {
        return;
    }

    const config::skin_entry_t *held{ this->weapon_entry( this->m_held_key ) };
    if ( held && held->m_skin.m_killstreak )
    {
        local->m_nStreaks( )[ kTFStreak_Kills ] = this->m_streak;
    }

    if ( active )
    {
        const role_t  have{ weapon_role( active->m_server_definition, player_class ) };
        const role_t  want{ weapon_role( active->m_definition, player_class ) };
        C_BaseEntity *viewmodel{ local->m_hViewModel( ).Get( ) };

        if ( have != role_t::none && want != role_t::none && have != want && viewmodel )
        {
            match_viewmodel_role( viewmodel->As< C_BaseViewModel * >( ), have, want );
        }
    }
}

const skins::seen_t *skins::dress_player( C_TFPlayer *player, const shared_loadout_t *loadout )
{
    const i32     player_class{ player->m_iClass( ) };
    EHANDLE      *weapons{ player->m_hMyWeapons( ) };
    const EHANDLE active_handle{ player->m_hActiveWeapon( ) };
    const seen_t *active{ nullptr };

    for ( usize index{ }; index < MAX_WEAPONS && this->m_seen_next_count < SEEN_MAX; ++index )
    {
        C_BaseEntity *held{ weapons[ index ].Get( ) };
        if ( !held )
        {
            continue;
        }

        C_TFWeaponBase *weapon{ held->As< C_TFWeaponBase * >( ) };
        const seen_t   *before{ find_seen( this->m_seen, this->m_seen_count, weapons[ index ] ) };
        seen_t         &now{ this->m_seen_next[ this->m_seen_next_count++ ] };
        const i32       current{ weapon->m_iItemDefinitionIndex( ) };

        now                     = before ? *before : seen_t{ };
        now.m_handle            = weapons[ index ];
        now.m_server_definition = before && before->m_definition == current ? before->m_server_definition : current;
        now.m_definition        = current;

        if ( weapons[ index ] == active_handle )
        {
            active = &now;
        }

        const shared_weapon_t *skin{ loadout ? shared_weapon( *loadout, key_for( now.m_server_definition, player_class ) ) : nullptr };
        this->dress_weapon( weapon, skin ? *skin : shared_weapon_t{ }, now );
    }

    const handle_vector_t &wearables{ player->m_hMyWearables( ) };
    for ( i32 index{ }; index < wearables.m_count && this->m_seen_next_count < SEEN_MAX; ++index )
    {
        C_BaseEntity *entity{ wearables.m_handles[ index ].Get( ) };
        if ( !entity )
        {
            continue;
        }

        C_TFWearable            *wearable{ entity->As< C_TFWearable * >( ) };
        const seen_t            *before{ find_seen( this->m_seen, this->m_seen_count, wearables.m_handles[ index ] ) };
        seen_t                  &now{ this->m_seen_next[ this->m_seen_next_count++ ] };
        const shared_wearable_t *skin{ loadout ? shared_wearable( *loadout, wearable->m_iItemDefinitionIndex( ) ) : nullptr };

        now            = before ? *before : seen_t{ };
        now.m_handle   = wearables.m_handles[ index ];
        now.m_wearable = true;

        this->dress_wearable( wearable, skin ? skin->m_unusual : 0u, now );
    }

    return active;
}

void skins::dress_peers( )
{
    const i32 local{ g_tf2.m_engine->GetLocalPlayer( ) };
    const i32 maximum{ math::smaller( g_tf2.m_engine->GetMaxClients( ), MAX_TRACKED_PLAYERS - 1 ) };

    for ( i32 index{ 1 }; index <= maximum; ++index )
    {
        player_info_t  info{ };
        IClientEntity *client_entity{ index != local ? g_tf2.m_entity_list->GetClientEntity( index ) : nullptr };
        C_BaseEntity  *entity{ client_entity ? client_entity->GetBaseEntity( ) : nullptr };
        ClientClass   *client_class{ entity ? entity->GetClientClass( ) : nullptr };

        if ( !client_class || client_class->GetClassID( ) != ETFClassID::CTFPlayer || entity->IsDormant( ) ||
             !g_tf2.m_engine->GetPlayerInfo( index, &info ) )
        {
            continue;
        }

        if ( const shared_loadout_t *loadout{ g_skin_share.loadout( info.friendsID ) } )
        {
            this->dress_player( entity->As< C_TFPlayer * >( ), loadout );
        }
    }
}

void skins::undress_unclaimed( )
{
    for ( usize index{ }; index < this->m_seen_count; ++index )
    {
        seen_t       &item{ this->m_seen[ index ] };
        C_BaseEntity *entity{ item.dressed( ) ? item.m_handle.Get( ) : nullptr };

        if ( !entity || entity->IsDormant( ) || find_seen( this->m_seen_next, this->m_seen_next_count, item.m_handle ) )
        {
            continue;
        }

        if ( item.m_wearable )
        {
            this->dress_wearable( entity->As< C_TFWearable * >( ), 0u, item );
        } else
        {
            this->dress_weapon( entity->As< C_TFWeaponBase * >( ), shared_weapon_t{ }, item );
        }
    }
}

void skins::rebuild_own( )
{
    usize        effect_count{ };
    const row_t *effect_rows{ effects( effect_count ) };

    this->m_own       = shared_loadout_t{ };
    this->m_own_ready = this->m_tables_ready.load( );

    // the first entry for a key is the one the dress pass always read
    for ( const config::skin_entry_t &entry : this->m_wanted_weapons )
    {
        if ( entry.m_definition < 0 || shared_weapon( this->m_own, entry.m_definition ) )
        {
            continue;
        }

        this->m_own.m_weapons[ this->m_own.m_weapon_count++ ] = resolve( entry.m_definition, entry.m_skin );
    }

    for ( const config::wearable_entry_t &entry : this->m_wanted_wearables )
    {
        if ( entry.m_definition < 0 || !entry.m_skin.m_unusual || entry.m_skin.m_unusual >= effect_count ||
             shared_wearable( this->m_own, entry.m_definition ) )
        {
            continue;
        }

        this->m_own.m_wearables[ this->m_own.m_wearable_count++ ] = shared_wearable_t{ entry.m_definition,
                                                                                       effect_rows[ entry.m_skin.m_unusual ].m_value };
    }
}

void skins::sanitize( shared_weapon_t &weapon ) const
{
    const i32             key{ weapon.m_key };
    const reskin_model_t *model{ model_by_definition( kind_for( key ), weapon.m_model ) };

    weapon.m_model        = model ? weapon.m_model : -1;
    weapon.m_paintkit     = paintkit_capable( key ) && listed( s_paintkits, weapon.m_paintkit ) ? weapon.m_paintkit : 0u;
    weapon.m_wear         = weapon.m_wear < WEAR_COUNT ? weapon.m_wear : 0u;
    weapon.m_australium   = weapon.m_australium && australium_capable( key ) && !is_botkiller( model );
    weapon.m_festivized   = weapon.m_festivized && festive_capable( key ) && !is_botkiller( model );
    weapon.m_unusual      = listed( UNUSUAL_EFFECTS, weapon.m_unusual ) ? weapon.m_unusual : 0u;
    weapon.m_killstreak   = weapon.m_killstreak < KILLSTREAK_COUNT ? weapon.m_killstreak : 0u;
    weapon.m_sheen        = weapon.m_sheen < SHEEN_COUNT ? weapon.m_sheen : 0u;
    weapon.m_killstreaker = listed( KILLSTREAKERS, weapon.m_killstreaker ) ? weapon.m_killstreaker : 0u;
}

void skins::sanitize( shared_wearable_t &wearable ) const
{
    wearable.m_unusual = listed( s_effects, wearable.m_unusual ) ? wearable.m_unusual : 0u;
}

const skins::seen_t *skins::find_seen( const seen_t *records, usize count, const EHANDLE &handle )
{
    for ( usize index{ }; index < count; ++index )
    {
        if ( records[ index ].m_handle == handle )
        {
            return &records[ index ];
        }
    }

    return nullptr;
}

void skins::dress_weapon( C_TFWeaponBase *weapon, const shared_weapon_t &skin, seen_t &item )
{
    u16       &definition{ weapon->m_iItemDefinitionIndex( ) };
    const bool changed{ skin != item.m_skin };

    if ( changed )
    {
        this->undress( weapon, item );
        definition  = static_cast< u16 >( item.m_server_definition );
        item.m_skin = skin;
    }

    const reskin_model_t *model{ model_by_definition( kind_for( skin.m_key ), skin.m_model ) };

    // the models are loaded before the game resolves them, which happens in the PostDataUpdate
    // and CREATED passes right after this
    if ( model )
    {
        definition = static_cast< u16 >( model->m_definition );
        this->register_model( model->m_model );
        this->register_model( model->m_attached );

        if ( skin.m_festivized )
        {
            this->register_model( model->m_festive );
        }

        if ( model->m_head && this->m_botkiller_count < BOTKILLER_SLOTS )
        {
            botkiller_t &botkiller{ this->m_botkillers[ this->m_botkiller_count++ ] };

            botkiller.m_weapon  = item.m_handle;
            botkiller.m_head    = this->head_handle( model->m_head );
            botkiller.m_head_vm = model->m_head_vm ? this->head_handle( model->m_head_vm ) : MDLHANDLE_INVALID;
        }
    } else if ( skin.m_australium || skin.m_festivized || skin.m_paintkit )
    {
        definition = static_cast< u16 >( upgradeable_for( definition ) );

        if ( skin.m_festivized )
        {
            this->register_model( festive_model_for( definition ) );
        }
    }

    item.m_definition = definition;

    CAttributeList &list{ weapon->m_AttributeList( ) };
    const i32       style{ model && model->m_style >= 0 ? model->m_style : ( skin.m_australium ? 1 : -1 ) };

    if ( skin.m_australium )
    {
        this->set_attribute( list, item, ATTRIB_IS_AUSTRALIUM_ITEM, 1u );
    }

    if ( style >= 0 )
    {
        this->set_attribute( list, item, ATTRIB_ITEM_STYLE_OVERRIDE, bits( static_cast< float >( style ) ) );
    }

    if ( skin.m_festivized )
    {
        this->set_attribute( list, item, ATTRIB_IS_FESTIVIZED, bits( 1.0f ) );
    }

    if ( skin.m_paintkit )
    {
        Assert( skin.m_wear < WEAR_COUNT );
        this->set_attribute( list, item, ATTRIB_PAINTKIT_PROTO_DEF_INDEX, skin.m_paintkit );
        this->set_attribute( list, item, ATTRIB_SET_ITEM_TEXTURE_WEAR, bits( WEARS[ skin.m_wear ] ) );

        // https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/game/client/tf/c_tf_player.cpp#L3637
        if ( skin.m_seed )
        {
            this->set_attribute( list, item, ATTRIB_CUSTOM_PAINTKIT_SEED_LO, skin.m_seed );
            this->set_attribute( list, item, ATTRIB_CUSTOM_PAINTKIT_SEED_HI, 0u );
        }
    }

    if ( skin.m_unusual )
    {
        this->set_attribute( list, item, ATTRIB_SET_ATTACHED_PARTICLE, bits( static_cast< float >( skin.m_unusual ) ) );
    }

    if ( skin.m_killstreak )
    {
        this->set_attribute( list, item, ATTRIB_KILLSTREAK_TIER, bits( static_cast< float >( skin.m_killstreak ) ) );
    }

    if ( skin.m_sheen )
    {
        this->set_attribute( list, item, ATTRIB_KILLSTREAK_IDLEEFFECT, bits( static_cast< float >( skin.m_sheen ) ) );
    }

    if ( skin.m_killstreaker )
    {
        this->set_attribute( list, item, ATTRIB_KILLSTREAK_EFFECT, bits( static_cast< float >( skin.m_killstreaker ) ) );
    }

    if ( changed )
    {
        this->refresh_weapon( weapon, model ? model->m_model : nullptr );
    }
}

void skins::dress_wearable( C_TFWearable *wearable, u32 effect, seen_t &item )
{
    const bool changed{ effect != item.m_skin.m_unusual };

    if ( changed )
    {
        this->undress( wearable, item );
        item.m_skin.m_unusual = effect;
    }

    const bool written{ effect && this->set_attribute( wearable->m_AttributeList( ), item, ATTRIB_SET_ATTACHED_PARTICLE,
                                                       bits( static_cast< float >( effect ) ) ) };

    if ( changed || written )
    {
        this->refresh_wearable( wearable );
    }
}

void skins::undress( C_EconEntity *entity, seen_t &item )
{
    if ( !item.dressed( ) )
    {
        return;
    }

    hide_particles( entity );

    CAttributeList &list{ entity->m_AttributeList( ) };

    for ( usize index{ }; index < item.m_original_count; ++index )
    {
        const original_t &original{ item.m_originals[ index ] };

        if ( original.m_present )
        {
            write_attribute( list, original.m_index, original.m_raw_value );
        } else
        {
            remove_attribute( list, original.m_index );
        }
    }

    item.m_original_count = 0;
}

void skins::refresh_wearable( C_TFWearable *wearable )
{
    wearable->GetClientNetworkable( )->OnDataChanged( DATA_UPDATE_CREATED );
}

void skins::refresh_weapon( C_TFWeaponBase *weapon, const char *model )
{
    release_viewmodel_attachment( weapon );

    // GetWorldModelIndex fills this once and copies it into m_iWorldModelIndex from then on, so a
    // weapon that already exists keeps its old world model until the cache is cleared
    weapon->m_iCachedModelIndex( ) = 0;

    if ( model && weapon->GetClientClass( )->GetClassID( ) == ETFClassID::CTFWeaponBuilder )
    {
        const i32 index{ g_tf2.m_model_info->GetModelIndex( model ) };
        weapon->m_iCachedModelIndex( ) = index > 0 ? index : 0;
    }

    forget_paintkit( weapon );

    IClientNetworkable *networkable{ weapon->GetClientNetworkable( ) };

    networkable->PostDataUpdate( DATA_UPDATE_DATATABLE_CHANGED );
    networkable->OnDataChanged( DATA_UPDATE_CREATED );
}

// the game's setter notifies the attribute manager every time it runs, which flushes the hook
// cache and re-marks the item description dirty, so the list is read first and the call is only
// made for a value that is missing or different; the first write of an index also keeps what the
// server had there for undress
bool skins::set_attribute( CAttributeList &list, seen_t &item, u16 index, u32 raw_value )
{
    const CEconItemAttribute *current{ find_attribute( list, index ) };

    if ( current && current->m_iRawValue32 == raw_value )
    {
        return false;
    }

    bool recorded{ };
    for ( usize slot{ }; slot < item.m_original_count && !recorded; ++slot )
    {
        recorded = item.m_originals[ slot ].m_index == index;
    }

    if ( !recorded )
    {
        Assert( item.m_original_count < ORIGINAL_SLOTS );
        item.m_originals[ item.m_original_count++ ] = original_t{ index, current != nullptr, current ? current->m_iRawValue32 : 0u };
    }

    return write_attribute( list, index, raw_value );
}

// didn't precache the model yet, which means it doesn't load
i32 skins::register_model( const char *path )
{
    if ( !path )
    {
        return -1;
    }

    const u64 hash{ HASH_RT( path ) };

    for ( usize index{ }; index < this->m_model_count; ++index )
    {
        if ( this->m_models[ index ].m_hash == hash )
        {
            return this->m_models[ index ].m_model ? MODEL_INDEX_BASE + static_cast< i32 >( index ) : -1;
        }
    }

    if ( this->m_model_count >= MODEL_SLOTS || !g_tf2.m_model_loader )
    {
        return -1;
    }

    loaded_model_t &slot{ this->m_models[ this->m_model_count ] };

    slot.m_hash  = hash;
    slot.m_model = g_tf2.m_model_loader->GetModelForName( path, FMODELLOADER_CLIENT );

    const i32 index{ MODEL_INDEX_BASE + static_cast< i32 >( this->m_model_count++ ) };
    return slot.m_model ? index : -1;
}

void skins::draw_botkiller( CStudioRender      *studio_render,
                            DrawModelResults_t *results,
                            DrawModelInfo_t    *info,
                            matrix3x4          *bone_to_world,
                            const vec3         *model_origin,
                            i32                 flags )
{
    if ( !this->m_botkiller_count || !info || !info->m_pClientEntity )
    {
        return;
    }

    auto           *renderable{ static_cast< IClientRenderable * >( info->m_pClientEntity ) };
    IClientUnknown *unknown{ renderable->GetIClientUnknown( ) };
    C_BaseEntity   *entity{ unknown ? unknown->GetBaseEntity( ) : nullptr };

    if ( !entity )
    {
        return;
    }

    // the viewmodel draws the weapon through an attachment model the weapon owns
    const unsigned long self{ entity->GetRefEHandle( ).GetIndex( ) };
    const unsigned long owner{ entity->m_hOwnerEntity( ).GetIndex( ) };
    MDLHandle_t         handle{ MDLHANDLE_INVALID };

    for ( usize index{ }; index < this->m_botkiller_count && handle == MDLHANDLE_INVALID; ++index )
    {
        const botkiller_t  &botkiller{ this->m_botkillers[ index ] };
        const unsigned long weapon{ botkiller.m_weapon.GetIndex( ) };

        if ( weapon == self )
        {
            handle = botkiller.m_head;
        } else if ( weapon == owner && entity->GetClientClass( )->GetClassID( ) == ETFClassID::CBaseAnimating )
        {
            handle = botkiller.m_head_vm != MDLHANDLE_INVALID ? botkiller.m_head_vm : botkiller.m_head;
        }
    }

    if ( handle == MDLHANDLE_INVALID )
    {
        return;
    }

    studiohdr_t    *head{ g_tf2.m_mdl_cache->GetStudioHdr( handle ) };
    studiohwdata_t *hardware{ g_tf2.m_mdl_cache->GetHardwareData( handle ) };

    if ( !head || !hardware || head->numbones > static_cast< i32 >( HEAD_BONES_MAX ) )
    {
        return;
    }

    head_pose_t &pose{ this->head_pose( self ) };
    const float  now{ g_tf2.m_globals->realtime };
    const float  step{ now - pose.m_time };
    const bool   reset{ step > 1.0f / 20.0f };
    matrix3x4    bones[ HEAD_BONES_MAX ]{ };

    pose_head_bones( head, info->m_pStudioHdr, bone_to_world, pose.m_jiggles, HEAD_JIGGLE_MAX, reset ? 0.0f : step, reset, bones );
    pose.m_time = now;

    const DrawModelInfo_t weapon{ *info };

    info->m_pStudioHdr    = head;
    info->m_pHardwareData = hardware;
    info->m_Decals        = nullptr;
    info->m_Body          = 0;
    info->m_Lod           = hardware->m_RootLOD;

    g_hooks.m_draw_model.thiscall< void >( studio_render, results, info, bones, nullptr, nullptr, model_origin, flags );

    *info = weapon;
}

skins::head_pose_t &skins::head_pose( unsigned long owner )
{
    head_pose_t *oldest{ &this->m_head_poses[ 0 ] };

    for ( head_pose_t &pose : this->m_head_poses )
    {
        if ( pose.m_owner == owner )
        {
            return pose;
        }

        if ( pose.m_time < oldest->m_time )
        {
            oldest = &pose;
        }
    }

    oldest->m_owner = owner;
    oldest->m_time  = 0.0f;
    return *oldest;
}

MDLHandle_t skins::head_handle( const char *path )
{
    const u64 hash{ HASH_RT( path ) };

    for ( usize index{ }; index < this->m_head_count; ++index )
    {
        if ( this->m_heads[ index ].m_hash == hash )
        {
            return this->m_heads[ index ].m_handle;
        }
    }

    if ( this->m_head_count >= HEAD_SLOTS )
    {
        return MDLHANDLE_INVALID;
    }

    head_t &head{ this->m_heads[ this->m_head_count++ ] };

    head.m_hash   = hash;
    head.m_handle = g_tf2.m_mdl_cache->FindMDL( path );
    return head.m_handle;
}

void *skins::model_for_index( i32 index ) const
{
    const i32 slot{ index - MODEL_INDEX_BASE };

    if ( slot < 0 || slot >= static_cast< i32 >( this->m_model_count ) )
    {
        return nullptr;
    }

    return this->m_models[ slot ].m_model;
}

i32 skins::index_for_model( const char *name ) const
{
    const u64 hash{ HASH_RT( name ) };

    for ( usize index{ }; index < this->m_model_count; ++index )
    {
        if ( this->m_models[ index ].m_hash == hash && this->m_models[ index ].m_model )
        {
            return MODEL_INDEX_BASE + static_cast< i32 >( index );
        }
    }

    return -1;
}

bool skins::attribute_hook( C_BaseEntity *initiator, const char *hook, float &value ) const
{
    enum spell_t : usize
    {
        SPELL_FOOTPRINTS,
        SPELL_VOICE,
        SPELL_PUMPKINS,
        SPELL_FLAMES,
        SPELL_JINGLE,
        SPELL_COUNT,
    };

    const auto name{ HASH_RT( hook ) };
    usize      spell{ SPELL_COUNT };

    if ( name == HASH_CT( "halloween_footstep_type" ) )
    {
        spell = SPELL_FOOTPRINTS;
    } else if ( name == HASH_CT( "halloween_voice_modulation" ) )
    {
        spell = SPELL_VOICE;
    } else if ( name == HASH_CT( "halloween_pumpkin_explosions" ) )
    {
        spell = SPELL_PUMPKINS;
    } else if ( name == HASH_CT( "halloween_green_flames" ) )
    {
        spell = SPELL_FLAMES;
    } else if ( name == HASH_CT( "add_jingle_to_footsteps" ) )
    {
        spell = SPELL_JINGLE;
    }

    if ( spell == SPELL_COUNT )
    {
        return false;
    }

    C_TFPlayer   *local{ g_cl.m_local };
    C_BaseEntity *owner{ initiator->m_hOwnerEntity( ).Get( ) };

    if ( !local || ( initiator != local && owner != local ) )
    {
        return false;
    }

    if ( !g_cfg.m_skins.m_enabled )
    {
        return false;
    }

    const config::spells_t &spells{ this->m_wanted_spells };

    switch ( spell )
    {
        case SPELL_FOOTPRINTS:
            if ( !spells.m_footprints || spells.m_footprints >= FOOTPRINT_COUNT )
            {
                return false;
            }
            value = spells.m_footprint_custom
                        ? static_cast< float >( spells.m_footprint_color.d3d( ) & 0xFFFFFFu )
                        : FOOTPRINTS[ spells.m_footprints ];
            return true;

        case SPELL_VOICE:
            value = 1.0f;
            return spells.m_voices_from_below;

        case SPELL_PUMPKINS:
            value = 1.0f;
            return spells.m_pumpkin_bombs;

        case SPELL_FLAMES:
            value = 1.0f;
            return spells.m_green_flames;

        case SPELL_JINGLE:
            if ( !spells.m_jingle || spells.m_jingle >= JINGLE_COUNT )
            {
                return false;
            }

            value = static_cast< float >( spells.m_jingle );
            return true;

        default:
            return false;
    }
}

bool skins::holiday_active( i32 holiday ) const
{
    const config::spells_t &spells{ this->m_wanted_spells };

    return holiday == kHoliday_HalloweenOrFullMoon && g_cfg.m_skins.m_enabled && ( spells.m_full_moon || spells.halloween( ) );
}

void skins::tint_sheen( CProxyAnimatedWeaponSheen *proxy ) const
{
    const config::skins_t &cfg{ g_cfg.m_skins };

    if ( !cfg.m_enabled || !cfg.m_sheen_custom || !proxy )
    {
        return;
    }

    if ( !proxy->m_pTintVar )
    {
        return;
    }

    // the three component setter is the one the game itself uses on this var when a weapon has no
    // sheen, so the alpha the sheen table wrote stays put and an untiered weapon stays dark
    proxy->m_pTintVar->SetVecValue( static_cast< float >( cfg.m_sheen_color.r ) / 255.0f,
                                    static_cast< float >( cfg.m_sheen_color.g ) / 255.0f,
                                    static_cast< float >( cfg.m_sheen_color.b ) / 255.0f );
}

bool skins::mutes_sound( i32 entity_index, const char *name ) const
{
    const config::spells_t &spells{ this->m_wanted_spells };

    if ( !g_cfg.m_skins.m_enabled || !spells.m_replace_step || !spells.m_jingle || !name )
    {
        return false;
    }

    if ( entity_index != g_tf2.m_engine->GetLocalPlayer( ) )
    {
        return false;
    }

    return g_misc.is_footstep( name );
}

bool skins::refresh_wanted( )
{
    const config::skins_t &cfg{ g_cfg.m_skins };
    bool                   changed{ };

    for ( usize index{ }; index < config::SKIN_WEAPON_COUNT; ++index )
    {
        if ( cfg.m_weapons[ index ] == this->m_wanted_weapons[ index ] )
        {
            continue;
        }

        this->m_wanted_weapons[ index ] = cfg.m_weapons[ index ];
        changed                         = true;
    }

    for ( usize index{ }; index < config::SKIN_WEARABLE_COUNT; ++index )
    {
        if ( cfg.m_wearables[ index ] == this->m_wanted_wearables[ index ] )
        {
            continue;
        }

        this->m_wanted_wearables[ index ] = cfg.m_wearables[ index ];
        changed                           = true;
    }

    this->m_wanted_spells = cfg.m_spells;

    return changed;
}

void skins::publish_held( C_TFPlayer *local, const seen_t *active )
{
    if ( !active )
    {
        this->m_held_key  = -1;
        this->m_held_kind = KIND_COUNT;
        return;
    }

    const i32 key{ key_for( active->m_server_definition, local->m_iClass( ) ) };

    if ( key != this->m_held_key && !definition_name( active->m_server_definition, this->m_held_name ) )
    {
        if ( C_BaseEntity * weapon{ active->m_handle.Get( ) } )
        {
            model_stem( weapon, this->m_held_name );
        } else
        {
            this->m_held_name.clear( );
        }
    }

    this->m_held_key  = key;
    this->m_held_kind = kind_for( key );
}

void skins::publish_wearables( C_TFPlayer *local )
{
    handle_vector_t &wearables{ local->m_hMyWearables( ) };
    usize            slot{ };

    for ( i32 index{ }; index < wearables.m_count && slot < config::SKIN_WEARABLE_SLOTS; ++index )
    {
        C_BaseEntity *entity{ wearables.m_handles[ index ].Get( ) };
        if ( !entity )
        {
            continue;
        }

        const i32 key{ entity->As< C_TFWearable * >( )->m_iItemDefinitionIndex( ) };

        if ( key != this->m_wearable_keys[ slot ] && !definition_name( key, this->m_wearable_names[ slot ] ) )
        {
            model_stem( entity, this->m_wearable_names[ slot ] );
        }

        this->m_wearable_keys[ slot++ ] = key;
    }

    for ( ; slot < config::SKIN_WEARABLE_SLOTS; ++slot )
    {
        this->m_wearable_keys[ slot ] = -1;
    }
}

bool skins::held_australium_capable( ) const
{
    return australium_capable( this->m_held_key ) &&
           !is_botkiller( model_for( this->m_held_kind, g_cfg.m_skins.m_weapon_edit.m_model ) );
}

bool skins::held_festive_capable( ) const
{
    return this->m_held_key >= 0 && festive_capable( this->m_held_key ) &&
           !is_botkiller( model_for( this->m_held_kind, g_cfg.m_skins.m_weapon_edit.m_model ) );
}

bool skins::held_paintkit_capable( ) const
{
    return this->m_held_key >= 0 && paintkit_capable( this->m_held_key );
}

i32 skins::wearable_key( ) const
{
    const usize slot{ g_cfg.m_skins.m_wearable_slot };
    return slot < config::SKIN_WEARABLE_SLOTS ? this->m_wearable_keys[ slot ] : -1;
}

const char *skins::wearable_name( ) const
{
    const usize slot{ g_cfg.m_skins.m_wearable_slot };
    return slot < config::SKIN_WEARABLE_SLOTS ? this->m_wearable_names[ slot ].data( ) : "";
}

void skins::record_gold_kill( i32 victim, bool gold )
{
    const float       now{ g_tf2.m_globals->curtime };
    const gold_kill_t wanted{ victim, now + GOLD_LIFETIME };
    gold_kill_t      *free_slot{ };

    for ( gold_kill_t &kill : this->m_gold_kills )
    {
        if ( kill.m_victim == victim )
        {
            kill = gold ? wanted : gold_kill_t{ };
            return;
        }

        if ( ( kill.m_victim < 0 || kill.m_expiry <= now ) && !free_slot )
        {
            free_slot = &kill;
        }
    }

    if ( gold && free_slot )
    {
        *free_slot = wanted;
    }
}

bool skins::gold_victim( i32 victim ) const
{
    for ( const gold_kill_t &kill : this->m_gold_kills )
    {
        if ( kill.m_victim == victim )
        {
            return kill.m_expiry > g_tf2.m_globals->curtime;
        }
    }

    return false;
}

bool skins::on_ragdoll_created( C_TFRagdoll *ragdoll ) const
{
    const config::skins_t &cfg{ g_cfg.m_skins };

    if ( this->gold_victim( ragdoll->m_hPlayer( ).GetEntryIndex( ) ) && cfg.m_enabled )
    {
        ragdoll->m_bGoldRagdoll( )  = true;
        ragdoll->m_bGib( )          = false;
        ragdoll->m_bBecomeAsh( )    = false;
        ragdoll->m_bIceRagdoll( )   = false;
        ragdoll->m_iDamageCustom( ) = 0;
        return true;
    }

    const config::ragdolls_t &effects{ cfg.m_ragdolls };
    C_TFPlayer               *local{ g_cl.m_local };

    if ( !effects.m_enabled || !effects.any( ) || !local )
    {
        return false;
    }

    const usize target{ ragdoll->m_iTeam( ) == local->m_iTeamNum( ) ? config::RAGDOLL_TARGET_TEAMMATES
                                                                    : config::RAGDOLL_TARGET_ENEMIES };

    if ( !effects.m_targets[ target ] )
    {
        return false;
    }

    // every effect lives on the ragdoll path, so a death that would have gibbed gets a ragdoll
    ragdoll->m_bGib( ) = false;
    if ( effects.m_gold )
    {
        ragdoll->m_bGoldRagdoll( ) = true;
    }

    if ( effects.m_ice )
    {
        // Necessary, otherwise ice ragdolls don't get the actual ice effect.
        ragdoll->m_bGoldRagdoll( ) = true;
        ragdoll->m_bIceRagdoll( )  = true;
    }

    if ( effects.m_burning )
    {
        ragdoll->m_bBurning( ) = true;
    }

    if ( effects.m_electrocuted )
    {
        ragdoll->m_bElectrocuted( ) = true;
    }

    if ( effects.m_ash )
    {
        ragdoll->m_bBecomeAsh( ) = true;
    }

    if ( effects.m_dissolve && !effects.m_ash )
    {
        ragdoll->m_iDamageCustom( ) = TF_DMG_CUSTOM_PLASMA;
    } else if ( effects.m_gold || effects.m_ice || effects.m_ash )
    {
        ragdoll->m_iDamageCustom( ) = 0;
    }

    return true;
}

void skins::reset_streaks( )
{
    this->m_streak = 0;

    for ( weapon_streak_t &streak : this->m_weapon_streaks )
    {
        streak = weapon_streak_t{ };
    }
}

i32 *skins::weapon_streak( i32 key )
{
    weapon_streak_t *free_slot{ };

    for ( weapon_streak_t &streak : this->m_weapon_streaks )
    {
        if ( streak.m_key == key )
        {
            return &streak.m_kills;
        }

        if ( streak.m_key < 0 && !free_slot )
        {
            free_slot = &streak;
        }
    }

    if ( !free_slot )
    {
        return nullptr;
    }

    free_slot->m_key   = key;
    free_slot->m_kills = 0;
    return &free_slot->m_kills;
}

config::skin_entry_t *skins::weapon_entry( i32 key )
{
    if ( key < 0 )
    {
        return nullptr;
    }

    for ( config::skin_entry_t &entry : this->m_wanted_weapons )
    {
        if ( entry.m_definition == key )
        {
            return &entry;
        }
    }

    return nullptr;
}

void skins::on_event( IGameEvent *event )
{
    if ( !g_cfg.m_skins.m_enabled )
    {
        return;
    }

    const auto name{ HASH_RT( event->GetName( ) ) };

    if ( name == HASH_CT( "player_changeclass" ) )
    {
        this->on_class_change( event );
        return;
    }

    if ( name != HASH_CT( "player_death" ) )
    {
        return;
    }

    const i32 local{ g_tf2.m_engine->GetLocalPlayer( ) };
    const i32 victim{ g_tf2.m_engine->GetPlayerForUserID( event->GetInt( ATLAS_XORSTR( "userid" ).get( ) ) ) };
    const i32 attacker{ g_tf2.m_engine->GetPlayerForUserID( event->GetInt( ATLAS_XORSTR( "attacker" ).get( ) ) ) };

    if ( event->GetInt( ATLAS_XORSTR( "death_flags" ).get( ) ) & TF_DEATH_FEIGN_DEATH )
    {
        return;
    }

    if ( victim == local )
    {
        this->reset_streaks( );
        return;
    }

    if ( victim <= 0 )
    {
        return;
    }

    // somebody else's kill still has to clear the victim's slot, or his next corpse would wear
    // the gold of the life we ended
    if ( attacker != local )
    {
        this->record_gold_kill( victim, false );
        return;
    }

    C_TFPlayer *player{ local_player( ) };
    const i32   definition{ event->GetInt( ATLAS_XORSTR( "weapon_def_index" ).get( ), -1 ) };
    const i32   key{ definition >= 0 && player ? key_for( definition, player->m_iClass( ) ) : -1 };

    const config::skin_entry_t *entry{ this->weapon_entry( key ) };
    const reskin_model_t       *model{ entry ? model_for( kind_for( key ), entry->m_skin.m_model ) : nullptr };
    const bool                  gold{ model && turns_to_gold( model->m_definition ) };

    this->record_gold_kill( victim, gold );
    if ( gold || ( entry && entry->m_skin.m_australium && !is_botkiller( model ) ) )
    {
        event->SetInt( ATLAS_XORSTR( "death_flags" ).get( ),
                       event->GetInt( ATLAS_XORSTR( "death_flags" ).get( ) ) | TF_DEATH_AUSTRALIUM );
    }

    if ( !entry )
    {
        return;
    }

    if ( entry->m_skin.m_killstreak )
    {
        i32 *kills{ this->weapon_streak( key ) };

        ++this->m_streak;

        event->SetInt( ATLAS_XORSTR( "kill_streak_total" ).get( ), this->m_streak );
        event->SetInt( ATLAS_XORSTR( "kill_streak_wep" ).get( ), kills ? ++*kills : this->m_streak );
    }

    if ( model && model->m_icon )
    {
        event->SetString( ATLAS_XORSTR( "weapon" ).get( ), model->m_icon );
    }
}

void skins::on_class_change( IGameEvent *event )
{
    const i32 local{ g_tf2.m_engine->GetLocalPlayer( ) };
    const i32 chooser{ g_tf2.m_engine->GetPlayerForUserID( event->GetInt( ATLAS_XORSTR( "userid" ).get( ) ) ) };

    if ( chooser != local )
    {
        return;
    }

    const i32 chosen{ event->GetInt( ATLAS_XORSTR( "class" ).get( ) ) };

    if ( chosen == this->m_class )
    {
        return;
    }

    this->m_class = chosen;
    this->reset_streaks( );
}

void skins::on_level_shutdown( )
{
    this->reset_streaks( );

    for ( gold_kill_t &kill : this->m_gold_kills )
    {
        kill = gold_kill_t{ };
    }

    // the loader drops client-loaded models with the map; they are loaded again when asked for
    this->m_model_count = 0;

    this->m_seen_count      = 0;
    this->m_botkiller_count = 0;
    this->m_in_game         = false;
    this->m_class           = -1;
    this->m_held_key        = -1;
    this->m_held_kind       = KIND_COUNT;

    for ( usize slot{ }; slot < config::SKIN_WEARABLE_SLOTS; ++slot )
    {
        this->m_wearable_keys[ slot ] = -1;
    }
}

void skins::reload_editor( )
{
    this->m_edit_key          = -2;
    this->m_edit_wearable_key = -2;
}

void skins::sync_editor( )
{
    config::skins_t &cfg{ g_cfg.m_skins };

    const i32 key{ this->m_held_key };

    if ( key != this->m_edit_key )
    {
        this->m_edit_key = key;

        config::skin_entry_t *entry{ nullptr };

        for ( config::skin_entry_t &candidate : cfg.m_weapons )
        {
            if ( candidate.m_definition == key )
            {
                entry = &candidate;
                break;
            }
        }

        cfg.m_weapon_edit = entry ? entry->m_skin : config::skin_t{ };
    } else if ( key >= 0 )
    {
        config::skin_entry_t *entry{ nullptr };
        config::skin_entry_t *free_slot{ nullptr };

        for ( config::skin_entry_t &candidate : cfg.m_weapons )
        {
            if ( candidate.m_definition == key )
            {
                entry = &candidate;
                break;
            }

            if ( candidate.m_definition < 0 && !free_slot )
            {
                free_slot = &candidate;
            }
        }

        if ( entry )
        {
            entry->m_skin = cfg.m_weapon_edit;

            if ( entry->m_skin.off( ) )
            {
                entry->m_definition = -1;
            }
        } else if ( free_slot && !cfg.m_weapon_edit.off( ) )
        {
            free_slot->m_skin       = cfg.m_weapon_edit;
            free_slot->m_definition = key;
        }
    }

    const i32 wearable_key{ this->wearable_key( ) };

    if ( wearable_key != this->m_edit_wearable_key )
    {
        this->m_edit_wearable_key = wearable_key;

        config::wearable_entry_t *entry{ nullptr };

        for ( config::wearable_entry_t &candidate : cfg.m_wearables )
        {
            if ( candidate.m_definition == wearable_key )
            {
                entry = &candidate;
                break;
            }
        }

        cfg.m_wearable_edit = entry ? entry->m_skin : config::wearable_skin_t{ };
    } else if ( wearable_key >= 0 )
    {
        config::wearable_entry_t *entry{ nullptr };
        config::wearable_entry_t *free_slot{ nullptr };

        for ( config::wearable_entry_t &candidate : cfg.m_wearables )
        {
            if ( candidate.m_definition == wearable_key )
            {
                entry = &candidate;
                break;
            }

            if ( candidate.m_definition < 0 && !free_slot )
            {
                free_slot = &candidate;
            }
        }

        if ( entry )
        {
            entry->m_skin = cfg.m_wearable_edit;

            if ( !entry->m_skin.m_unusual )
            {
                entry->m_definition = -1;
            }
        } else if ( free_slot && cfg.m_wearable_edit.m_unusual )
        {
            free_slot->m_skin       = cfg.m_wearable_edit;
            free_slot->m_definition = wearable_key;
        }
    }
}
