#ifndef __PIPES_HPP
#define __PIPES_HPP

#ifdef _MSC_VER
  #include <stdlib.h>
  #define popen _popen
  #define pclose _pclose
#else
  #include <unistd.h>
#endif

#endif
