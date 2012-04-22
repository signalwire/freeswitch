
#ifndef _CONST_THIS_H_
#define _CONST_THIS_H_

template < typename T >
struct ConstThis
{
    T const & constThis() const
    {
        // TODO: will this return the right reference?
        return static_cast<const T&>(*this);
    }
};

#endif /* _CONST_THIS_H_ */
