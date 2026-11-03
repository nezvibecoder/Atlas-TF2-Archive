#pragma once

#include "inline_hook.hpp"
#include "vmt_hook.hpp"

namespace atlashook
{
    inline vmt_hook create_vmt( void *object, u32 method_count )
    {
        return vmt_hook::create( object, method_count );
    }

    inline vmt_method_hook create_vmt_method( void *object, atlas_obfuscation::vfunc_u32 index, void *destination )
    {
        return vmt_method_hook::create( object, index.get( ), destination );
    }

    template< typename destination_t >
    inline vmt_method_hook create_vmt_method( void *object, atlas_obfuscation::vfunc_u32 index,
                                              destination_t destination )
    {
        return create_vmt_method( object, index, reinterpret_cast< void * >( destination ) );
    }

    inline vm_hook create_vm( vmt_hook &vmt, atlas_obfuscation::vfunc_u32 index, void *destination )
    {
        return vmt.hook_method( index.get( ), destination );
    }

    template< typename type >
    inline vm_hook create_vm( vmt_hook &vmt, atlas_obfuscation::vfunc_u32 index, type destination )
    {
        return create_vm( vmt, index, reinterpret_cast< void * >( destination ) );
    }

    inline inline_hook create_inline( void *target, void *destination,
                                      inline_hook::flags options = inline_hook::default_flags )
    {
        return inline_hook::create( target, destination, options );
    }

    template< typename target_t, typename destination_t >
    inline inline_hook create_inline( target_t target, destination_t destination,
                                      inline_hook::flags options = inline_hook::default_flags )
    {
        return create_inline( reinterpret_cast< void * >( target ), reinterpret_cast< void * >( destination ), options );
    }
} // namespace atlashook
