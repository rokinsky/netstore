#ifndef _HELPER_HH
#define _HELPER_HH

struct __attribute__ ((packed)) SIMPL_CMD {
  char cmd[10];
  uint64_t cmd_seq;
  char data[];
};

struct __attribute__ ((packed)) CMPLX_CMD {
  char cmd[10];
  uint64_t cmd_seq;
  uint64_t param;
  char data[];
};

#endif // _HELPER_HH
