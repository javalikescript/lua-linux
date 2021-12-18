#include <lua.h>
#include <lauxlib.h>

#include <signal.h>

#ifndef sighandler_t
typedef __sighandler_t sighandler_t;
#endif

static const char *const lua_linux_signal_names[] = {
  "SIGHUP", "SIGINT", "SIGQUIT", "SIGILL", "SIGTRAP", "SIGABRT", "SIGFPE", "SIGKILL", "SIGSEGV", "SIGPIPE", "SIGALRM", "SIGTERM", NULL
};

static const int lua_linux_signal_values[] = {
  SIGHUP, SIGINT, SIGQUIT, SIGILL, SIGTRAP, SIGABRT, SIGFPE, SIGKILL, SIGSEGV, SIGPIPE, SIGALRM, SIGTERM
};

static const char *const lua_linux_handler_names[] = {
  "ignore", "SIG_IGN", "default", "SIG_DFL", NULL
};

static const sighandler_t lua_linux_handler_values[] = {
  SIG_IGN, SIG_IGN, SIG_DFL, SIG_DFL
};

static int lua_linux_signal(lua_State *l) {
  int signum, opt;
  sighandler_t previousHandler, handler;
  if (lua_isstring(l, 1)) {
    opt = luaL_checkoption(l, 1, NULL, lua_linux_signal_names);
    signum = lua_linux_signal_values[opt];
  } else {
    signum = (int) lua_tointeger(l, 1);
  }
  opt = luaL_checkoption(l, 2, NULL, lua_linux_handler_names);
  handler = lua_linux_handler_values[opt];
  previousHandler = signal(signum, handler);
  lua_pushboolean(l, previousHandler != SIG_ERR);
  return 1;
}

LUALIB_API int luaopen_linux(lua_State *l) {
  luaL_Reg reg[] = {
    { "signal", lua_linux_signal },
    { NULL, NULL }
  };
  lua_newtable(l);
  luaL_setfuncs(l, reg, 0);
  lua_pushliteral(l, "Lua linux");
  lua_setfield(l, -2, "_NAME");
  lua_pushliteral(l, "0.1");
  lua_setfield(l, -2, "_VERSION");
  return 1;
}
