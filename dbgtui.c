#define _GNU_SOURCE
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <argp.h>

#define inner_key 1000
#define ex_key 1001

const struct argp_option options[] = { { .name = "inner-mode",
                                         .key = inner_key,
                                         .doc = "Run as inner layer",
                                         .flags = OPTION_HIDDEN },
                                       {
                                           .name = "ex",
                                           .key = ex_key,
                                           .doc = "Execute a GDB command",
                                           .arg = "COMMAND",
                                       },
                                       { 0 } };

struct config
{
  bool inner_mode;
  size_t n_gdb_commands;
  char **gdb_commands;
};

error_t
parse_opt (int key, char *arg, struct argp_state *state)
{
  struct config *config = state->input;

  switch (key)
    {
    case inner_key:
      config->inner_mode = true;
      return 0;

    case ex_key:
      config->gdb_commands[config->n_gdb_commands] = arg;
      config->n_gdb_commands++;
      return 0;

    default:
      return ARGP_ERR_UNKNOWN;
    }
}

const struct argp parser = {
  .options = options,
  .parser = parse_opt,
  .args_doc = "-- DEBUGGEE ARGS",
  .doc = "Invoke a program with an attached GDB window",
};

void
inner_phase (char **debuggee_args)
{
  const int n = 3;

  for (int i = 0; i < n; i++)
    {
      int new_fd = i;
      int old_fd = i + n;
      int res = dup2 (old_fd, new_fd);
      if (res == -1)
        {
          perror ("dup2");
          exit (EXIT_FAILURE);
        }
      close (old_fd);
      if (res == -1)
        {
          perror ("close");
        }
    }

  execvp (debuggee_args[0], debuggee_args);
  exit (EXIT_FAILURE);
}

char *
terminal_name (void)
{
  char *ret = "gnome-terminal";

  int fd = open ("/etc/os-release", O_RDONLY | O_CLOEXEC);
  if (fd >= 0)
    {
      struct stat sb;

      if (fstat (fd, &sb) != -1)
        {
          char *s = mmap (NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);

          if (s != MAP_FAILED)
            {
              const char *needle = "Ubuntu";

              if (memmem (s, sb.st_size, needle, strlen (needle)) != NULL)
                {
                  ret = "gnome-terminal.real";
                }

              munmap (s, sb.st_size);
            }
        }

      close (fd);
    }

  return ret;
}

void
outer_phase (char *self_name, int n_debuggee_args, char **debuggee_args,
             int n_gdb_commands, char **gdb_commands)
{
  const int n = 3;

  for (int i = 0; i < n; i++)
    {
      int new_fd = i + n;
      int old_fd = i;
      int res = dup2 (old_fd, new_fd);
      if (res == -1)
        {
          perror ("dup2");
          exit (EXIT_FAILURE);
        }
    }

  char *extra_args1[] = { terminal_name (), "--wait", "--fd=3", "--fd=4",
                          "--fd=5",         "--",     "gdb",    "-ex",
                          "catch exec",     "-ex",    "run",    "-ex",
                          "delete 1" };

  char *extra_args2[]
      = { "delete 1", "--args", self_name, "--inner-mode", "--" };

  const unsigned n_extra_args1
      = sizeof (extra_args1) / sizeof (extra_args1[0]);
  const unsigned n_extra_args2
      = sizeof (extra_args2) / sizeof (extra_args2[0]);

  const unsigned n_extra_args
      = n_extra_args1 + 2 * n_gdb_commands + n_extra_args2;

  char *args[n_extra_args + n_debuggee_args + 1];

  unsigned i = 0;
  for (unsigned j = 0; j < n_extra_args1; j++)
    {
      args[i++] = extra_args1[j];
    }

  for (unsigned j = 0; j < n_gdb_commands; j++)
    {
      args[i++] = "-ex";
      args[i++] = gdb_commands[j];
    }

  for (unsigned j = 0; j < n_extra_args2; j++)
    {
      args[i++] = extra_args2[j];
    }

  for (unsigned j = 0; j < n_debuggee_args; j++)
    {
      args[i++] = debuggee_args[j];
    }

  args[n_extra_args + n_debuggee_args] = NULL;

  execvp (args[0], args);
  perror ("execvp");
  exit (EXIT_FAILURE);
}

int
main (int argc, char *argv[])
{
  /* The debuggee is wrapped into four layers:
     - An outer dbgtui layer
     - A gnome-terminal layer
     - A GDB layer
     - An inner dbgtui layer

     Each layer does some setup and then execs the next layer. */

  // The value argc should be an upper bound for the number of GDB commands
  // passed through -ex.
  char *gdb_commands[argc];
  struct config config = {
    .inner_mode = false,
    .n_gdb_commands = 0,
    .gdb_commands = gdb_commands,
  };

  int idx;

  argp_parse (&parser, argc, argv, ARGP_LONG_ONLY, &idx, &config);

  if (config.inner_mode)
    {
      inner_phase (&argv[idx]);
    }
  else
    {
      outer_phase (argv[0], argc - idx, &argv[idx], config.n_gdb_commands,
                   config.gdb_commands);
    }
}
