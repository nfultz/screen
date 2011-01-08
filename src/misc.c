/* Copyright (c) 2008, 2009
 *      Juergen Weigert (jnweiger@immd4.informatik.uni-erlangen.de)
 *      Michael Schroeder (mlschroe@immd4.informatik.uni-erlangen.de)
 *      Micah Cowan (micah@cowan.name)
 *      Sadrul Habib Chowdhury (sadrul@users.sourceforge.net)
 * Copyright (c) 1993-2002, 2003, 2005, 2006, 2007
 *      Juergen Weigert (jnweiger@immd4.informatik.uni-erlangen.de)
 *      Michael Schroeder (mlschroe@immd4.informatik.uni-erlangen.de)
 * Copyright (c) 1987 Oliver Laumann
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (see the file COPYING); if not, see
 * http://www.gnu.org/licenses/, or contact Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 *
 ****************************************************************
 */

#include <sys/types.h>
#include <sys/stat.h>	/* mkdir() declaration */
#include <signal.h>

#include "config.h"
#include "screen.h"
#include "extern.h"

#ifdef SVR4
# include <sys/resource.h>
#endif

#ifdef HAVE_FDWALK
static int close_func (void *, int);
#endif

char *
SaveStr(register const char *str)
{
  register char *cp;

  if ((cp = malloc(strlen(str) + 1)) == NULL)
    Panic(0, "%s", strnomem);
  else
    strlcpy(cp, str, strlen(str) + 1);
  return cp;
}

char *
SaveStrn(register const char *str, int n)
{
  register char *cp;

  if ((cp = malloc(n + 1)) == NULL)
    Panic(0, "%s", strnomem);
  else
    {
      memmove(cp, (char *)str, n);
      cp[n] = 0;
    }
  return cp;
}

/* cheap strstr replacement */
char *
InStr(char *str, const char *pat)
{
  int npat = strlen(pat);
  for (;*str; str++)
    if (!strncmp(str, pat, npat))
      return str;
  return 0;
}

#ifndef HAVE_STRERROR
char *
strerror(int err)
{
  extern int sys_nerr;
  extern char *sys_errlist[];

  static char er[20];
  if (err > 0 && err < sys_nerr)
    return sys_errlist[err];
  sprintf(er, "Error %d", err);
  return er;
}
#endif

void
centerline(char *str, int y)
{
  int l, n;

  ASSERT(flayer);
  n = strlen(str);
  if (n > flayer->l_width - 1)
    n = flayer->l_width - 1;
  l = (flayer->l_width - 1 - n) / 2;
  LPutStr(flayer, str, n, &mchar_blank, l, y);
}

void
leftline(char *str, int y, struct mchar *rend)
{
  int l, n;
  struct mchar mchar_dol;

  mchar_dol = mchar_blank;
  mchar_dol.image = '$';

  ASSERT(flayer);
  l = n = strlen(str);
  if (n > flayer->l_width - 1)
    n = flayer->l_width - 1;
  LPutStr(flayer, str, n, rend ? rend : &mchar_blank, 0, y);
  if (n != l)
    LPutChar(flayer, &mchar_dol, n, y);
}


char *
Filename(char *s)
{
  register char *p = s;

  if (p)
    while (*p)
      if (*p++ == '/')
        s = p;
  return s;
}

char *
stripdev(char *nam)
{
#ifdef apollo
  char *p;
  
  if (nam == NULL)
    return NULL;
# ifdef SVR4
  /* unixware has /dev/pts012 as synonym for /dev/pts/12 */
  if (!strncmp(nam, "/dev/pts", 8) && nam[8] >= '0' && nam[8] <= '9')
    {
      static char b[13];
      sprintf(b, "pts/%d", atoi(nam + 8));
      return b;
    }
# endif /* SVR4 */
  if (p = strstr(nam,"/dev/"))
    return p + 5;
#else /* apollo */
  if (nam == NULL)
    return NULL;
  if (strncmp(nam, "/dev/", 5) == 0)
    return nam + 5;
#endif /* apollo */
  return nam;
}


/*
 *    Signal handling
 */

#ifdef POSIX
sigret_t (*xsignal(sig, func))
# ifndef __APPLE__
 SIGPROTOARG
# else
()
# endif
int sig;
sigret_t (*func) SIGPROTOARG;
{
  struct sigaction osa, sa;
  sa.sa_handler = func;
  (void)sigemptyset(&sa.sa_mask);
#ifdef SA_RESTART
  sa.sa_flags = (sig == SIGCHLD ? SA_RESTART : 0);
#else
  sa.sa_flags = 0;
#endif
  if (sigaction(sig, &sa, &osa))
    return (sigret_t (*)SIGPROTOARG)-1;
  return osa.sa_handler;
}

#else
# ifdef hpux
/*
 * hpux has berkeley signal semantics if we use sigvector,
 * but not, if we use signal, so we define our own signal() routine.
 */
void (*xsignal(sig, func)) SIGPROTOARG
int sig;
void (*func) SIGPROTOARG;
{
  struct sigvec osv, sv;

  sv.sv_handler = func;
  sv.sv_mask = sigmask(sig);
  sv.sv_flags = SV_BSDSIG;
  if (sigvector(sig, &sv, &osv) < 0)
    return (void (*)SIGPROTOARG)(BADSIG);
  return osv.sv_handler;
}
# endif	/* hpux */
#endif	/* POSIX */


/*
 *    uid/gid handling
 */

#ifdef HAVE_SETEUID

void
xseteuid(int euid)
{
  if (seteuid(euid) == 0)
    return;
  seteuid(0);
  if (seteuid(euid))
    Panic(errno, "seteuid");
}

void
xsetegid(int egid)
{
  if (setegid(egid))
    Panic(errno, "setegid");
}

#else /* HAVE_SETEUID */
# ifdef HAVE_SETREUID

void
xseteuid(int euid)
{
  int oeuid;

  oeuid = geteuid();
  if (oeuid == euid)
    return;
  if ((int)getuid() != euid)
    oeuid = getuid();
  if (setreuid(oeuid, euid))
    Panic(errno, "setreuid");
}

void
xsetegid(int egid)
{
  int oegid;

  oegid = getegid();
  if (oegid == egid)
    return;
  if ((int)getgid() != egid)
    oegid = getgid();
  if (setregid(oegid, egid))
    Panic(errno, "setregid");
}

# endif /* HAVE_SETREUID */
#endif /* HAVE_SETEUID */


void
bclear(char *p, int n)
{
  memmove(p, (char *)blank, n);
}


void
Kill(int pid, int sig)
{
  if (pid < 2)
    return;
  (void) kill(pid, sig);
}

#ifdef HAVE_FDWALK
/*
 * Modern versions of Solaris include fdwalk(3c) which allows efficient
 * implementation of closing open descriptors; this is helpful because
 * the default file descriptor limit has risen to 65k.
 */
static int
close_func(void *cb_data, int fd)
{
  int except = *(int *)cb_data;
  if (fd > 2 && fd != except)
    (void)close(fd);
  return (0);
}

void
closeallfiles(int except)
{
  (void)fdwalk(close_func, &except);
}

#else /* HAVE_FDWALK */

void
closeallfiles(int except)
{
  int f;
#ifdef SVR4
  struct rlimit rl;
  
  if ((getrlimit(RLIMIT_NOFILE, &rl) == 0) && rl.rlim_max != RLIM_INFINITY)
    f = rl.rlim_max;
  else
#endif /* SVR4 */
#if defined(SYSV) && defined(NOFILE) && !defined(ISC)
  f = NOFILE;
#else /* SYSV && !ISC */
  f = getdtablesize();
#endif /* SYSV && !ISC */
  while (--f > 2)
    if (f != except)
      close(f);
}

#endif /* HAVE_FDWALK */


/*
 *  Security - switch to real uid
 */

#ifndef USE_SETEUID
static int UserPID;
static sigret_t (*Usersigcld)SIGPROTOARG;
#endif
static int UserSTAT;

int
UserContext()
{
#ifndef USE_SETEUID
  if (eff_uid == real_uid && eff_gid == real_gid)
    return 1;
  Usersigcld = signal(SIGCHLD, SIG_DFL);
  debug("UserContext: forking.\n");
  switch (UserPID = fork())
    {
    case -1:
      Msg(errno, "fork");
      return -1;
    case 0:
      signal(SIGHUP, SIG_DFL);
      signal(SIGINT, SIG_IGN);
      signal(SIGQUIT, SIG_DFL);
      signal(SIGTERM, SIG_DFL);
# ifdef BSDJOBS
      signal(SIGTTIN, SIG_DFL);
      signal(SIGTTOU, SIG_DFL);
# endif
      setuid(real_uid);
      setgid(real_gid);
      return 1;
    default:
      return 0;
    }
#else
  xseteuid(real_uid);
  xsetegid(real_gid);
  return 1;
#endif
}

void
UserReturn(int val)
{
#ifndef USE_SETEUID
  if (eff_uid == real_uid && eff_gid == real_gid)
    UserSTAT = val;
  else
    _exit(val);
#else
  xseteuid(eff_uid);
  xsetegid(eff_gid);
  UserSTAT = val;
#endif
}

int
UserStatus()
{
#ifndef USE_SETEUID
  int i;
# ifdef BSDWAIT
  union wait wstat;
# else
  int wstat;
# endif

  if (eff_uid == real_uid && eff_gid == real_gid)
    return UserSTAT;
  if (UserPID < 0)
    return -1;
  while ((errno = 0, i = wait(&wstat)) != UserPID)
    if (i < 0 && errno != EINTR)
      break;
  (void) signal(SIGCHLD, Usersigcld);
  if (i == -1)
    return -1;
  return WEXITSTATUS(wstat);
#else
  return UserSTAT;
#endif
}

#ifndef HAVE_RENAME
int
rename (char *old, char *new)
{
  if (link(old, new) < 0)
    return -1;
  return unlink(old);
}
#endif


int
AddXChar(char *buf, int ch)
{
  char *p = buf;

  if (ch < ' ' || ch == 0x7f)
    {
      *p++ = '^';
      *p++ = ch ^ 0x40;
    }
  else if (ch >= 0x80)
    {
      *p++ = '\\';
      *p++ = (ch >> 6 & 7) + '0';
      *p++ = (ch >> 3 & 7) + '0';
      *p++ = (ch >> 0 & 7) + '0';
    }
  else
    *p++ = ch;
  return p - buf;
}

int
AddXChars(char *buf, int len, char *str)
{
  char *p;

  if (str == 0)
    {
      *buf = 0;
      return 0;
    }
  len -= 4;     /* longest sequence produced by AddXChar() */
  for (p = buf; p < buf + len && *str; str++)
    {
      if (*str == ' ')
        *p++ = *str;
      else
        p += AddXChar(p, *str);
    }
  *p = 0;
  return p - buf;
}


#ifdef DEBUG
void
opendebug(int new, int shout)
{
  char buf[256];

#ifdef _MODE_T
  mode_t oumask = umask(0);
#else
  int oumask = umask(0);
#endif

  ASSERT(!dfp);

  (void) mkdir(DEBUGDIR, 0777);
  sprintf(buf, shout ? "%s/SCREEN.%d" : "%s/screen.%d", DEBUGDIR, getpid());
  if (!(dfp = fopen(buf, new ? "w" : "a")))
    dfp = stderr;
  else
    (void)chmod(buf, 0666);

  (void)umask(oumask);
  debug("opendebug: done.\n");
}
#endif /* DEBUG */

void
sleep1000(int msec)
{
  struct timeval t;

  t.tv_sec = (long) (msec / 1000);
  t.tv_usec = (long) ((msec % 1000) * 1000);
  select(0, (fd_set *)0, (fd_set *)0, (fd_set *)0, &t);
}


/*
 * This uses either setenv() or putenv(). If it is putenv() we cannot dare
 * to free the buffer after putenv(), unless it it the one found in putenv.c
 */
void
xsetenv(char *var, char *value)
{
#ifndef USESETENV
  char *buf;
  int l;

  if ((buf = (char *)malloc((l = strlen(var)) +
			    strlen(value) + 2)) == NULL)
    {
      Msg(0, strnomem);
      return;
    }
  strlcpy(buf, var, strlen(var) + 1);
  buf[l] = '=';
  strlcpy(buf + l + 1, value, strlen(value) + 1);
  putenv(buf);
# ifdef NEEDPUTENV
  /*
   * we use our own putenv(), knowing that it does a malloc()
   * the string space, we can free our buf now.
   */
  free(buf);
# else /* NEEDPUTENV */
  /*
   * For all sysv-ish systems that link a standard putenv()
   * the string-space buf is added to the environment and must not
   * be freed, or modified.
   * We are sorry to say that memory is lost here, when setting
   * the same variable again and again.
   */
# endif /* NEEDPUTENV */
#else /* USESETENV */
# if HAVE_SETENV_3
  setenv(var, value, 1);
# else
  setenv(var, value);
# endif /* HAVE_SETENV_3 */
#endif /* USESETENV */
}

#ifndef HAVE_STRLCPY

/*	$OpenBSD: strlcpy.c,v 1.11 2006/05/05 15:27:38 millert Exp $	*/

/*
 * Copyright (c) 1998 Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Copy src to string dst of size siz.  At most siz-1 characters
 * will be copied.  Always NUL terminates (unless siz == 0).
 * Returns strlen(src); if retval >= siz, truncation occurred.
 */
size_t
strlcpy(char *dst, const char *src, size_t siz)
{
	char *d = dst;
	const char *s = src;
	size_t n = siz;

	/* Copy as many bytes as will fit */
	if (n != 0) {
		while (--n != 0) {
			if ((*d++ = *s++) == '\0')
				break;
		}
	}

	/* Not enough room in dst, add NUL and traverse rest of src */
	if (n == 0) {
		if (siz != 0)
			*d = '\0';		/* NUL-terminate dst */
		while (*s++)
			;
	}

	return(s - src - 1);	/* count does not include NUL */
}

#endif

#ifndef HAVE_STRLCAT

/*	$OpenBSD: strlcat.c,v 1.13 2005/08/08 08:05:37 espie Exp $	*/

/*
 * Copyright (c) 1998 Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Appends src to string dst of size siz (unlike strncat, siz is the
 * full size of dst, not space left).  At most siz-1 characters
 * will be copied.  Always NUL terminates (unless siz <= strlen(dst)).
 * Returns strlen(src) + MIN(siz, strlen(initial dst)).
 * If retval >= siz, truncation occurred.
 */
size_t
strlcat(char *dst, const char *src, size_t siz)
{
	char *d = dst;
	const char *s = src;
	size_t n = siz;
	size_t dlen;

	/* Find the end of dst and adjust bytes left but don't go past end */
	while (n-- != 0 && *d != '\0')
		d++;
	dlen = d - dst;
	n = siz - dlen;

	if (n == 0)
		return(dlen + strlen(s));
	while (*s != '\0') {
		if (n != 1) {
			*d++ = *s;
			n--;
		}
		s++;
	}
	*d = '\0';

	return(dlen + (s - src));	/* count does not include NUL */
}

#endif

# define xva_arg(s, t, tn) va_arg(s, t)
# define xva_list va_list

