#pragma once

static inline int isdigit( int value )
{
    return value >= '0' && value <= '9';
}

static inline int isupper( int value )
{
    return value >= 'A' && value <= 'Z';
}

static inline int islower( int value )
{
    return value >= 'a' && value <= 'z';
}

static inline int isalpha( int value )
{
    return isupper( value ) || islower( value );
}

static inline int isalnum( int value )
{
    return isalpha( value ) || isdigit( value );
}

static inline int isxdigit( int value )
{
    return isdigit( value ) || ( value >= 'a' && value <= 'f' ) || ( value >= 'A' && value <= 'F' );
}

static inline int isspace( int value )
{
    return value == ' ' || ( value >= '\t' && value <= '\r' );
}

static inline int iscntrl( int value )
{
    return ( value >= 0 && value < 0x20 ) || value == 0x7F;
}

static inline int isgraph( int value )
{
    return value > 0x20 && value < 0x7F;
}

static inline int ispunct( int value )
{
    return isgraph( value ) && !isalnum( value );
}

static inline int tolower( int value )
{
    return isupper( value ) ? value + ( 'a' - 'A' ) : value;
}

static inline int toupper( int value )
{
    return islower( value ) ? value - ( 'a' - 'A' ) : value;
}
