/* shared.c */
#include "shared.h"
#include <stdio.h>
#include <string.h>

char *unit_to_string(unit u)
{
    switch(u)
    {
        case W:
            return "W";
        case Wh:
            return "Wh";
        case kWh:
            return "kWh";
        case A:
            return "A";
        case V:
            return "V";
        case C:
            return "C";
        default:
            return "?";
    }
}

double reabs(double x)
{
    if(x < 0)
        return -x;
    return x;
}

double zpow(double base, int32_t exponent)
{
    int32_t i;
    double result = 1;
    if(exponent > 0)
    {
        for(i=0;i < exponent;i++)
            result *= base;
        return result;
    }
    else if(exponent < 0)
    {
        for(i=0;i > exponent;i--)
            result /= base;
        return result;
    }
    else
        return 1;
}

void double_to_2ints(double val, int32_t *z_val, uint32_t *r_val)
{
    *z_val = (int32_t) val;
    *r_val = (uint32_t) ((val - (double)(*z_val)) * zpow(10,FIXED_POINT_PRECISION));
}

// time function

struct base_time
{
    int32_t sec;
    int32_t min;
    int32_t hour;
    int32_t day;
    int32_t month;
    int32_t year;
    uint64_t last_seconds;
};

int16_t is_leap_year(int32_t year)
{
    if(year % 400 == 0 || (year % 100 != 0 && year % 4 == 0))
        return 0;
    else
        return 1;
}

static struct base_time bt = {TIME_SEC, TIME_MIN, TIME_HOUR, TIME_DAY, TIME_MONTH, TIME_YEAR, 0};
    
int16_t ctime(uint64_t seconds, char *str, int32_t length)
{
    const int32_t month_days[13] = {0,31,28,31,30,31,30,31,31,30,31,30,31};
    uint64_t rel_seconds = 0;
    uint64_t si = 0;
    if(seconds >= bt.last_seconds)
    {
        rel_seconds = seconds - bt.last_seconds;
    }
    else
    {
        bt = (struct base_time) {TIME_SEC, TIME_MIN, TIME_HOUR, TIME_DAY, TIME_MONTH, TIME_YEAR, 0};
        rel_seconds = seconds;
    }
    
    int16_t ly = !is_leap_year(bt.year);
    uint64_t siy;
    if(ly)
    {
        siy = 31622400;
    }
    else
    {
        siy = 31536000;
    }
    //add years
    while(si + siy <= rel_seconds)
    {
        bt.year++;
        si += siy;
        ly = !is_leap_year(bt.year);
        if(ly)
        {
            siy = 31622400;
        }
        else
        {
            siy = 31536000;
        }
    }
    
    //add days
    while(si + 86400 <= rel_seconds)
    {
        bt.day++;
        if(bt.month == 2)
        {
            if(ly)
            {
                if(bt.day > 29)
                {
                    bt.month++;
                    bt.day = 1;
                }
            }
            else
            {
                if(bt.day > 28)
                {
                    bt.month++;
                    bt.day = 1;
                }
            }
        }
        else
        {
            if(bt.day > month_days[bt.month])
            {
                bt.month++;
                bt.day = 1;
            }
        }
        if(bt.month > 12)
        {
            bt.year++;
            bt.month = 1;
            ly = !is_leap_year(bt.year);
        }
        si += 86400;
    }
    
    // add hours
    
    while(si + 3600 <= rel_seconds)
    {
        bt.hour++;
        if(bt.hour == 24)
        {
            bt.day++;
            bt.hour = 0;
            if(bt.month == 2)
            {
                if(ly)
                {
                    if(bt.day > 29)
                    {
                        bt.month++;
                        bt.day = 1;
                    }
                }
                else
                {
                    if(bt.day > 28)
                    {
                        bt.month++;
                        bt.day = 1;
                    }
                }
            }
            else
            {
                if(bt.day > month_days[bt.month])
                {
                    bt.month++;
                    bt.day = 1;
                }
            }
            if(bt.month > 12)
            {
                bt.year++;
                bt.month = 1;
                ly = !is_leap_year(bt.year);
            }
        }
        si += 3600;
    }
    
    //add minutes
    while(si + 60 <= rel_seconds)
    {
        bt.min++;
        if(bt.min == 60)
        {
            bt.hour++;
            bt.min = 0;
            if(bt.hour == 24)
            {
                bt.day++;
                bt.hour = 0;
                if(bt.month == 2)
                {
                    if(ly)
                    {
                        if(bt.day > 29)
                        {
                            bt.month++;
                            bt.day = 1;
                        }
                    }
                    else
                    {
                        if(bt.day > 28)
                        {
                            bt.month++;
                            bt.day = 1;
                        }
                    }
                }
                else
                {
                    if(bt.day > month_days[bt.month])
                    {
                        bt.month++;
                        bt.day = 1;
                    }
                }
                if(bt.month > 12)
                {
                    bt.year++;
                    bt.month = 1;
                    ly = !is_leap_year(bt.year);
                }
            }
        }
        si += 60;
    }
    
    // add seconds
    bt.sec += rel_seconds - si;
    if(bt.sec >= 60)
    {
        bt.min++;
        bt.sec -= 60;
        if(bt.min == 60)
        {
            bt.hour++;
            bt.min = 0;
            if(bt.hour == 24)
            {
                bt.day++;
                bt.hour = 0;
                if(bt.month == 2)
                {
                    if(ly)
                    {
                        if(bt.day > 29)
                        {
                            bt.month++;
                            bt.day = 1;
                        }
                    }
                    else
                    {
                        if(bt.day > 28)
                        {
                            bt.month++;
                            bt.day = 1;
                        }
                    }
                }
                else
                {
                    if(bt.day > month_days[bt.month])
                    {
                        bt.month++;
                        bt.day = 1;
                    }
                }
                if(bt.month > 12)
                {
                    bt.year++;
                    bt.month = 1;
                    ly = !is_leap_year(bt.year);
                }
            }
        }
    }
    bt.last_seconds = seconds;
    memset(str,0,length);
    return snprintf(str, length, "%ld.%ld.%ld %.2ld:%.2ld:%.2ld", bt.day, bt.month, bt.year, bt.hour, bt.min, bt.sec);
}