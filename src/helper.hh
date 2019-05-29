#ifndef _HELPER_HH
#define _HELPER_HH

struct SIMPL_CMD {
  char cmd[10];
  uint64_t cmd_seq;
  char data[];
};

struct CMPLX_CMD {
  char cmd[10];
  uint64_t cmd_seq;
  uint64_t param;
  char data[];
};

#endif // _HELPER_HH
