pkglibexec_PROGRAMS = msm-renice msm-kill

AM_CPPFLAGS = $(TOOLS_CFLAGS)

msm_renice_SOURCES = msm_execute_helper.c
msm_renice_LDADD = $(TOOLS_LIBS)
msm_renice_CFLAGS = -DCOMMAND=\"renice\" $(WARN_CFLAGS)

msm_kill_SOURCES = msm_execute_helper.c
msm_kill_LDADD = $(TOOLS_LIBS)
msm_kill_CFLAGS = -DCOMMAND=\"kill\" $(WARN_CFLAGS)

