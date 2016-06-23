env = Environment(CCFLAGS = '-std=c99')
env.Program("survey", ["start.c", "logger.c", "stat_server.c", "/usr/local/lib/libev.a", "/usr/local/lib/libnanomsg.a"], LIBS = ["pthread", "m"])
