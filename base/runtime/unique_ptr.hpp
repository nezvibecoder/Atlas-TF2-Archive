#pragma once

#include <new>

#include "memory.hpp"
#include "util/typedefs.hpp"

namespace atl
{
    template< typename Type >
    class unique_ptr
    {
    public:
        unique_ptr( ) = default;
        constexpr unique_ptr( Type *obj ) : obj( obj )
        {
        }

        ~unique_ptr( )
        {
            delete obj;
        }

        inline constexpr unique_ptr &operator=( unique_ptr &&other )
        {
            if ( this != &other )
            {
                delete obj;
                obj       = other.obj;
                other.obj = nullptr;
            }

            return *this;
        }

        inline constexpr unique_ptr &operator=( nullptr_t )
        {
            delete obj;
            obj = nullptr;
            return *this;
        }

        inline constexpr unique_ptr &operator=( const unique_ptr & ) = delete;

        inline constexpr Type *release( )
        {
            Type *_obj = obj;
            obj        = nullptr;
            return _obj;
        }

        inline void swap( unique_ptr &other )
        {
            Type *cur_obj = obj;
            obj           = other.obj;
            other.obj     = cur_obj;
        }

        inline constexpr Type *get( ) const
        {
            return obj;
        }

        inline constexpr operator bool( ) const
        {
            return obj != nullptr;
        }

        inline constexpr Type &operator*( ) const
        {
            return *obj;
        }

        inline constexpr Type *operator->( ) const
        {
            return obj;
        }

    private:
        Type *obj;
    };

    template< typename Type, typename... Args >
    inline constexpr unique_ptr< Type > make_unique( Args &&...args )
    {
        return unique_ptr< Type >( new Type( args... ) );
    }
} // namespace atl
