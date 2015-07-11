#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fts.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <utime.h>
#include <sys/wait.h>
#include <signal.h>

#define VIDEO_BITRATE "1M"
#define FFMPEG "/usr/bin/ffmpeg"

static int force_update = 0;

#define ERROR(_fmt_, ...) fprintf(stderr, "*** ERROR ***: %s:%d " _fmt_ "\n", __FILE__, __LINE__, ##__VA_ARGS__ )
#define WARNING(_fmt_, ...) fprintf(stderr, "*** WARNING ***: %s:%d " _fmt_ "\n", __FILE__, __LINE__, ##__VA_ARGS__ )
#define MESSAGE(_fmt_, ...) fprintf(stdout, _fmt_ "\n", ##__VA_ARGS__ )

static int
is_video(FTSENT *ftsent)
{
  const char * const ext[] = {
    ".avi", ".AVI",
    ".mov", ".MOV",
    ".3gp", ".3GP",
    ".mp4", ".MP4",
    ".asf", ".ASF"};
  unsigned int i;
  size_t len = ftsent->fts_namelen;
  if ( len <= 4 )
    return 0;
  for ( i = 0 ; i < sizeof(ext)/sizeof(*ext) ; ++i )
    {
      assert(strlen(ext[i]) == 4);
      if ( 0 == strcmp(ftsent->fts_name+(len-4),ext[i]) )
	return 1;
    }
  
  return 0;
}

static int
getmtime(const char *dir, time_t *timep)
{
  struct stat st;
  int ret;
  ret = stat(dir, &st);
  if ( ret < 0 )
    {
      ERROR("stat(%s): %s", dir, strerror(errno));
      return ret;
    }
  if (timep)
    *timep = st.st_mtim.tv_sec;
  return 0;
}

static int
setmtime(const char *dir, time_t time)
{
  struct stat st;
  int ret;
  ret = stat(dir, &st);
  if ( ret < 0 )
    {
      ERROR("stat(%s): %s", dir, strerror(errno));
      return ret;
    }
  struct utimbuf utim = {
    .actime = st.st_atim.tv_sec,
    .modtime = time,
  };
  
  ret = utime(dir, &utim);
  if ( ret < 0 )
    ERROR("utime(%s): %s", dir, strerror(errno));
  return ret;
}

static int
probe_setmtime(const char *dir, time_t *dt)
{
  int ret;
  ret = getmtime(dir, dt);
  if ( ret < 0 )
    return ret;
  ret = setmtime(dir, *dt + 1);
  if ( ret < 0 )
    return ret;
  ret = setmtime(dir, *dt);
  return ret;
}


const char *garbage = NULL;
const char *dir_to_restore = NULL;
time_t     time_to_restore = 0;

static void cleanup(int sig)
{
  if ( garbage )
    {
      unlink(garbage);
      ERROR("signal received: %s unlinked", garbage);
    }
  garbage = NULL;
  if ( dir_to_restore )
    {
      setmtime(dir_to_restore, time_to_restore);
      ERROR("signal received: %s timestamp restored", dir_to_restore);
    }
  dir_to_restore = NULL;
  abort();
}

static int
do_convert(const char * from, const char *to)
{
  const char * const argv[] =
    { FFMPEG, "-loglevel", "error", "-i", from, "-b:v", VIDEO_BITRATE, to, NULL };
  int pid;
  int cpid;
  int ret;
  int child_status;
  garbage = to;
  pid = fork();
  switch(pid)
    {
    case 0:
      ret = execv(FFMPEG, (char**)argv);
      if ( ret < 0 )
	{
	  ERROR("Failed to execute " FFMPEG ": %s", strerror(errno));
	}
      abort();
    case -1:
      ERROR("Failed to fork");
      return 1;
    default:
      do
	{
	  cpid = wait(&ret);
	  assert( pid == cpid );
	}
      while ( !WIFEXITED(ret) );
      child_status = WEXITSTATUS(ret);
      if ( child_status != 0 )
	{
	  ERROR("failed to convert %s", from);
	  unlink(to);
	}
      garbage = NULL;
      return child_status;
    }
  abort();
}

static int
convert_video(FTSENT *ftsent)
{
  char dir[MAXPATHLEN];
  char webm_name[MAXPATHLEN] = {};
  int offset;
  char webm[MAXPATHLEN];
  char arch[MAXPATHLEN];
  char *last_slash;

  strncpy(dir, ftsent->fts_path, sizeof(dir));
  last_slash = rindex(dir, '/');
  assert( NULL != last_slash );
  *last_slash = '\0';
  
  time_t dt;
  int ret;

  ret = probe_setmtime(dir, &dt);
  if ( ret < 0 )
    return ret;
  dir_to_restore = dir;
  time_to_restore = dt;
  
  if ( '.' == ftsent->fts_name[0] )
    {
      offset = 1;
      strncpy(arch, ftsent->fts_path, sizeof(arch));
    }
  else
    {
      offset = 0;
      snprintf(arch, sizeof(arch), "%s/.%s", dir, ftsent->fts_name);
      struct stat st;
      ret = stat(arch, &st);
      if ( 0 == ret )
	{
	  char target[MAXPATHLEN];
	  snprintf(target, sizeof(target), "%s~", arch);
	  WARNING("%s already exists, %s moved as %s", arch, ftsent->fts_path, target);
	  ret = rename(ftsent->fts_path, target);
	  if ( ret < 0 )
	    {
	      ERROR("rename(%s, %s): %s", ftsent->fts_path, target, strerror(errno));
	      ret = 1;
	      goto end;
	    }
	  ret = 0;
	  goto end;
	}
      else if ( errno != ENOENT )
	{
	  ERROR("stat(%s): %s", arch, strerror(errno));
	  ret = 1;
	  goto end;
	}
      ret = rename(ftsent->fts_path, arch);
      if ( ret < 0 )
	{
	  ERROR("rename(%s, %s): %s", ftsent->fts_path, arch, strerror(errno));
	  ret = 1;
	  goto end;
	}
    }
  memcpy(webm_name, ftsent->fts_name + offset, ftsent->fts_namelen-4-offset);
  snprintf(webm, sizeof(webm), "%s/%s.webm", dir, webm_name);

  struct stat st_webm, st_arch;

  ret = stat(webm, &st_webm);
  if ( ret < 0 )
    {
      if ( errno != ENOENT )
	{
	  ERROR("stat(%s): %s", webm, strerror(errno));
	  ret = 1;
	  goto end;
	}
      memset(&st_webm, 0, sizeof(st_webm));
    }
  ret = stat(arch, &st_arch);
  if ( ret < 0 )
    {
      ERROR("stat(%s): %s", arch, strerror(errno));
      ret = 1;
      goto end;
    }

  if ( st_arch.st_mtim.tv_sec >= st_webm.st_mtim.tv_sec ||
       force_update )
    {
      MESSAGE("UPDATE OF: %s", arch);
      ret = do_convert(arch, webm);
    }
 end:
  if ( setmtime(dir, dt) != 0 )
    {
      ERROR("Failed to restore %s timestamp", dir);
      return 1;
    }
  dir_to_restore = NULL;
  time_to_restore = 0;
  return ret; 
}

static int
recurse(const char * path)
{
  FTS *fts;
  FTSENT *ftsent;
  char * const apath[] = { strdup(path), NULL };
  fts = fts_open(apath,FTS_NOCHDIR|FTS_NOSTAT|FTS_COMFOLLOW, NULL);
  if ( NULL == fts)
    {
      ERROR("fts_open(%s) failed", path);
      return EXIT_FAILURE;
    }
  while ((ftsent = fts_read(fts)))
    switch (ftsent->fts_info)
      {
      case FTS_F:
	if ( is_video(ftsent) )
	  if ( convert_video(ftsent) )
	    ERROR("conversion failed: %s", ftsent->fts_path);
	break;
      case FTS_SLNONE:
      case FTS_NS:
      case FTS_ERR:
	ERROR("fts_read: %s (%s)", ftsent->fts_path, strerror(errno));
      default:(void)0;
      }
  fts_close(fts);
  free(apath[0]);
  return EXIT_SUCCESS;
}

int main(int argc, char **argv)
{
  if ( 2 != argc )
    {
      ERROR("usage: %s <BASE PATH>\n", argv[0]);
      exit(EXIT_FAILURE);
    }

  if ( getenv("FORCE_UPDATE") )
    force_update = 1;
  
  signal(SIGTERM, cleanup);
  signal(SIGQUIT, cleanup);
  signal(SIGINT, cleanup);
  signal(SIGUSR1, cleanup);
  signal(SIGUSR2, cleanup);
  
  return recurse(argv[1]);
}
