// from xv6/ulib.c
int strcmp(const char *p, const char *q)
{
  while(*p && *p == *q)
    p++, q++;
  return (unsigned char)*p - (unsigned char)*q;
}