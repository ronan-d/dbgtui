#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <argp.h>

#define inner_key 1000

const struct argp_option options[] = { { .name = "inner-mode",
                                         .key = inner_key,
                                         .doc = "Run as inner layer",
                                         .flags = OPTION_HIDDEN },
                                       { 0 } };

error_t
parse_opt (int key, char *arg, struct argp_state *state)
{
  bool *inner_mode = state->input;

  switch (key)
    {
    case inner_key:
      *inner_mode = true;
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

void
outer_phase (char *self_name, int n_debuggee_args, char **debuggee_args)
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

  char *extra_args[] = { "gnome-terminal",
                         "--wait",
                         "--fd=3",
                         "--fd=4",
                         "--fd=5",
                         "--",
                         "gdb",
                         "-ex",
                         "catch exec",
                         "-ex",
                         "run",
                         "-ex",
                         "delete 1",
                         "--args",
                         self_name,
                         "--inner-mode",
                         "--" };

  const unsigned n_extra_args = sizeof (extra_args) / sizeof (extra_args[0]);

  char *args[n_extra_args + n_debuggee_args + 1];

  for (unsigned i = 0; i < n_extra_args; i++)
    {
      args[i] = extra_args[i];
    }

  for (unsigned i = 0; i < n_debuggee_args; i++)
    {
      args[i + n_extra_args] = debuggee_args[i];
    }

  args[n_extra_args + n_debuggee_args] = NULL;

  int res = execvp (args[0], args);
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

  bool inner_mode = false;

  int idx;

  argp_parse (&parser, argc, argv, 0, &idx, &inner_mode);

  if (inner_mode)
    {
      inner_phase (&argv[idx]);
    }
  else
    {
      outer_phase (argv[0], argc - idx, &argv[idx]);
    }
}
