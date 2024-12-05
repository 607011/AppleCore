#ifndef __THOUSANDS_NUMPUNCT_HPP__
#define __THOUSANDS_NUMPUNCT_HPP__

#include <locale>
#include <string>

class thsds_numpunct : public std::numpunct<char>
{
  protected:
    virtual char do_thousands_sep() const
    {
        return ',';
    }
    virtual std::string do_grouping() const
    {
        return "\03";
    }
};


#endif