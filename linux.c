#include <lua.h>
#include <lauxlib.h>

#include <signal.h>
#include <errno.h>
#include <string.h> /* strerror */
#include <sys/types.h> /* pid_t */
#include <sys/wait.h>
#include <sys/syscall.h>
#include <unistd.h> /* getpid */
#include <fcntl.h>
#include <sys/ioctl.h>
#ifdef SYS_pidfd_open
#include <poll.h>
#else
#include <time.h>
#endif

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
  int options, status, timoutMs, sleepTime;
  pid = (pid_t) luaL_checkinteger(l, 1);
  options = luaL_optinteger(l, 2, 0);
  timoutMs = luaL_optinteger(l, 3, options & WNOHANG ? 0 : -1);
  sleepTime = luaL_optinteger(l, 4, 500);
  status = 0;
  r = 1;
  if (timoutMs >= 0) {
    if (timoutMs > 0) {
#ifdef SYS_pidfd_open
      int fd = syscall(SYS_pidfd_open, pid, 0);
      if (fd > 0) {
        struct pollfd pfd = {fd, POLLIN, 0};
        r = poll(&pfd, 1, timoutMs);
      }
#endif
    }
    options |= WNOHANG;
  }
  if (r == 1) {
#ifdef SYS_pidfd_open
    r = waitpid(pid, &status, options);
#else
    struct timespec ts, rts;
    int sleepTotalTime = 0;
    for (;;) {
      r = waitpid(pid, &status, options);
      if ((r != 0) || ((timoutMs >= 0) && (sleepTotalTime >= timoutMs))) {
        break;
      }
      ts.tv_sec = sleepTime / 1000;
      ts.tv_nsec = (sleepTime % 1000) * 1000000;
      if (nanosleep(&ts, &rts) == -1) {
        sleepTotalTime += rts.tv_sec * 1000 + rts.tv_nsec / 1000000;
      } else {
        sleepTotalTime += sleepTime;
      }
    }
#endif
  }
  if (r < 0) { // error
    r = errno;
    lua_pushnil(l);
    lua_pushinteger(l, r);
    return 2;
  }
  const char *what = "na";
  if (r == 0) { // timeout
    what = "timeout";
    status = 0;
  } else {
    if (WIFEXITED(status)) {
      what = "exit";
      status = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
      what = "signal";
      status = WTERMSIG(status);
    }
  }
  lua_pushinteger(l, r);
  lua_pushstring(l, what);
  lua_pushinteger(l, status);
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

static int lua_linux_lockf(lua_State *l) {
  int fd = getFileDesc(l, 1);
  int op = (int) luaL_checkinteger(l, 2);
  int len = (int) luaL_checkinteger(l, 3);
  int r = lockf(fd, op, len);
  if (r == -1) {
    r = errno;
    lua_pushnil(l);
    lua_pushinteger(l, r);
    return 2;
  }
  lua_pushinteger(l, r);
  return 1;
}

#define LUA_TO_POINTER(_LS, _INDEX) ((char *) \
  (lua_isuserdata(_LS, _INDEX) ? lua_touserdata(_LS, _INDEX) : lua_tolstring(_LS, _INDEX, NULL)))

static int lua_linux_ioctl(lua_State *l) {
  int fd = getFileDesc(l, 1);
  unsigned long request = (unsigned long) luaL_checkinteger(l, 2);
  char *argp = LUA_TO_POINTER(l, 3);
  int r = ioctl(fd, request, argp);
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
    { "ioctl", lua_linux_ioctl },
    { "lockf", lua_linux_lockf },
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
  // wait
  set_int_field(l, WNOHANG);
  // lock
  set_int_field(l, F_LOCK);
  set_int_field(l, F_TLOCK);
  set_int_field(l, F_ULOCK);
  set_int_field(l, F_TEST);
  lua_setfield(l, -2, "constants");

  lua_pushliteral(l, "Lua linux");
  lua_setfield(l, -2, "_NAME");
  lua_pushliteral(l, "0.3");
  lua_setfield(l, -2, "_VERSION");
  return 1;
}
