#include <ctype.h>

int
isalnum(int c)
{
  return isalpha(c) || isdigit(c);
}

int
isalpha(int c)
{
  return isupper(c) || islower(c);
}

int
isdigit(int c)
{
  return '0' <= c && c <= '9';
}

int
islower(int c)
{
  return 'a' <= c && c <= 'z';
}

int
isupper(int c)
{
  return 'A' <= c && c <= 'Z';
}

int
tolower(int c)
{
  if (isupper(c))
    return c + 'a' - 'A';
  else
    return c;
}

int
toupper(int c)
{
  if (islower(c))
    return c + 'A' - 'a';
  else
    return c;
}
