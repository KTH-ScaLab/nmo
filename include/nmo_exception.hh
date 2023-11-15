#ifndef mt_NMO_EXCEPTION_H
#define mt_NMO_EXCEPTION_H

#include <string>

struct NmoException: public std::exception
{
    std::string m;
    NmoException(std::string msg)
    {
        m = msg;
    }

    const char* what () const throw ()
    {
        return m.data();
    }
};

#endif
