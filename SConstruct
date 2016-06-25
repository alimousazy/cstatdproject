env = Environment(CCFLAGS = '-std=c99')
env.Program("survey", ["main.c", "logger.c", "stat_server.c", "/usr/local/lib/libev.a", "/usr/local/lib/libnanomsg.a", "/usr/local/lib/libhashring.a"], LIBS = ["pthread", "m"])
