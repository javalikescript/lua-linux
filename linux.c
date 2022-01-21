#include <lua.h>
#include <lauxlib.h>

#include <signal.h>
#include <errno.h>
#include <string.h> /* strerror */
#include <sys/types.h> /* pid_t */
#include <sys/wait.h>
#include <unistd.h> /* getpid */
#include <fcntl.h>

#ifndef sighandler_t
typedef __sighandler_t sighandler_t;
#endif

static const char *const lua_linux_handler_names[] = {
  "ignore", "SIG_IGN", "default", "SIG_DFL", NULL
};

static const sighandler_t lua_linux_handler_values[] = {
  SIG_IGN, SIG_IGN, SIG_DFL, SIG_DFL
};

static int lua_linux_signal(lua_State *l) {
  int signum, opt;
  sighandler_t previousHandler, handler;
  signum = (int) luaL_checkinteger(l, 1);
  opt = luaL_checkoption(l, 2, NULL, lua_linux_handler_names);
  handler = lua_linux_handler_values[opt];
  previousHandler = signal(signum, handler);
  lua_pushboolean(l, previousHandler != SIG_ERR);
  return 1;
}

static int lua_linux_kill(lua_State *l) {
  pid_t pid;
  int signum, r;
  pid = (pid_t) luaL_checkinteger(l, 1);
  signum = (int) luaL_checkinteger(l, 2);
  r = kill(pid, signum);
  if ((r != 0) && (errno != 0)) {
    r = errno;
  }
  lua_pushinteger(l, r);
  return 1;
}

static int lua_linux_waitpid(lua_State *l) {
  pid_t pid, r;
  int options, status;
  pid = (pid_t) luaL_checkinteger(l, 1);
  options = (pid_t) lua_tointeger(l, 2);
  r = waitpid(pid, &status, options);
  if (r < 0) {
    r = errno;
		lua_pushnil(l);
    lua_pushinteger(l, r);
    return 2;
  }
  lua_pushinteger(l, r);
  lua_pushboolean(l, WIFEXITED(status));
  lua_pushinteger(l, WEXITSTATUS(status));
  return 3;
}

static int lua_linux_strerror(lua_State *l) {
  int errnum = (int) lua_tointeger(l, 1);
	lua_pushstring(l, strerror(errnum));
  return 1;
}

static int lua_linux_getpid(lua_State *l) {
  pid_t pid = getpid();
  lua_pushinteger(l, pid);
  return 1;
}

static int getFileDesc(lua_State *l, int arg) {
	int fd;
	luaL_Stream *pLuaStream;
	pLuaStream = (luaL_Stream *)luaL_testudata(l, arg, LUA_FILEHANDLE);
	if (pLuaStream != NULL) {
		fd = fileno(pLuaStream->f);
	} else {
		fd = luaL_checkinteger(l, arg);
	}
	return fd;
}

static int lua_linux_fcntl(lua_State *l) {
	int fd = getFileDesc(l, 1);
  int cmd, arg, r;
  cmd = (int) luaL_checkinteger(l, 2);
  arg = (int) lua_tointeger(l, 3);
  r = fcntl(fd, cmd, arg);
  if (r == -1) {
    r = errno;
		lua_pushnil(l);
    lua_pushinteger(l, r);
    return 2;
  }
  lua_pushinteger(l, r);
  return 1;
}

#define set_int_field(__LUA_STATE, __FIELD_NAME) \
  lua_pushinteger(__LUA_STATE, __FIELD_NAME); \
  lua_setfield(__LUA_STATE, -2, #__FIELD_NAME)

LUALIB_API int luaopen_linux(lua_State *l) {
  luaL_Reg reg[] = {
    { "strerror", lua_linux_strerror },
    { "getpid", lua_linux_getpid },
    { "signal", lua_linux_signal },
    { "fcntl", lua_linux_fcntl },
    { "kill", lua_linux_kill },
    { "waitpid", lua_linux_waitpid },
    { NULL, NULL }
  };
  lua_newtable(l);
  luaL_setfuncs(l, reg, 0);

  lua_newtable(l);
  // signals
  set_int_field(l, SIGHUP);
  set_int_field(l, SIGINT);
  set_int_field(l, SIGQUIT);
  set_int_field(l, SIGILL);
  set_int_field(l, SIGTRAP);
  set_int_field(l, SIGABRT);
  set_int_field(l, SIGFPE);
  set_int_field(l, SIGKILL);
  set_int_field(l, SIGSEGV);
  set_int_field(l, SIGPIPE);
  set_int_field(l, SIGALRM);
  set_int_field(l, SIGTERM);
  // errors
  set_int_field(l, EAGAIN);
  set_int_field(l, EDEADLK);
  set_int_field(l, ENAMETOOLONG);
  set_int_field(l, ENOLCK);
  set_int_field(l, ENOSYS);
  set_int_field(l, ENOTEMPTY);
  set_int_field(l, ELOOP);
  set_int_field(l, EWOULDBLOCK);
  set_int_field(l, ENOMSG);
  set_int_field(l, EIDRM);
  set_int_field(l, ECHRNG);
  set_int_field(l, EL2NSYNC);
  set_int_field(l, EL3HLT);
  set_int_field(l, EL3RST);
  set_int_field(l, ELNRNG);
  set_int_field(l, EUNATCH);
  set_int_field(l, ENOCSI);
  set_int_field(l, EL2HLT);
  set_int_field(l, EBADE);
  set_int_field(l, EBADR);
  set_int_field(l, EXFULL);
  set_int_field(l, ENOANO);
  set_int_field(l, EBADRQC);
  set_int_field(l, EBADSLT);
  // file cmd
  set_int_field(l, F_GETFD);
  set_int_field(l, F_SETFD);
  set_int_field(l, F_GETFL);
  set_int_field(l, F_SETFL);
  set_int_field(l, F_GETLK);
  set_int_field(l, F_SETLK);
  set_int_field(l, F_SETLKW);
  // file flags
  set_int_field(l, O_APPEND);
  set_int_field(l, O_ASYNC);
  //set_int_field(l, O_DIRECT);
  //set_int_field(l, O_NOATIME);
  set_int_field(l, O_NONBLOCK);
  lua_setfield(l, -2, "constants");

  lua_pushliteral(l, "Lua linux");
  lua_setfield(l, -2, "_NAME");
  lua_pushliteral(l, "0.2");
  lua_setfield(l, -2, "_VERSION");
  return 1;
}
